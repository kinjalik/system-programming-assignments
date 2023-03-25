#include "userfs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

enum {
    BLOCK_SIZE = 512,
    MAX_FILE_SIZE = 1024 * 1024 * 100,
    RETVAL_SUCCESS = 0,
    RETVAL_FAILURE = -1,
    RETVAL_EOF = 0
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
    /** Block memory. */
    char *memory;
    /** Next block in the file. */
    struct block *next;
    /** Previous block in the file. */
    struct block *prev;

    /* PUT HERE OTHER MEMBERS */
};

struct file {
    /** Double-linked list of file blocks. */
    struct block *block_list;
    /**
     * Last block in the list above for fast access to the end
     * of file.
     */
    struct block *last_block;
    /** How many file descriptors are opened on the file. */
    int refs;
    /** File name. */
    char *name;
    /** Files are stored in a double-linked list. */
    struct file *next;
    struct file *prev;

    /* PUT HERE OTHER MEMBERS */
    bool marked_for_deletion;
    size_t total_size;
};

struct filedesc {
    struct file *file;

    /* PUT HERE OTHER MEMBERS */
    bool read_allowed;
    bool write_allowed;
    struct block *cur_block;
    size_t total_offset;
};

/** Block maintaining functions */
static struct block *
ufs_add_block(struct block *last_block);
static void
ufs_free_blocks(struct block *block_list);

/** File list and maintaining functions */
static struct file *file_list = NULL;
static struct file *file_list_last = NULL;

static struct file *
ufs_create_file(const char *name);

static struct file *
ufs_find_file(const char *name);

static void
ufs_mark_file_for_delete(struct file *const file);

static void
ufs_real_delete_file(struct file *const file);

static bool
ufs_extend_file(struct file *const file, size_t new_size);

static bool
ufs_reduce_file(struct file *const file, size_t new_size);

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static ssize_t file_descriptor_count = 0;
static ssize_t file_descriptor_capacity = 0;
static ssize_t file_descriptors_rr_ptr = 0;

static int
ufs_create_fd(struct file *file, int flags);

static void
ufs_delete_fd(int idx);

static bool
ufs_extend_fd_list_if_needed();

static bool
ufs_check_valid_fd_idx(int idx);

static void
ufs_move_overflowing_fds(const struct file * file);

/** PUBLIC INTERFACE START */

enum ufs_error_code
ufs_errno()
{
    return ufs_error_code;
}

int
ufs_open(const char *filename, int flags)
{
    ufs_error_code = UFS_ERR_NO_ERR;
    if (!(flags & UFS_READ_WRITE) && !(flags & UFS_READ_ONLY) && !(flags & UFS_WRITE_ONLY)) {
        flags |= UFS_READ_WRITE;
    }

    struct file *cur_file = ufs_find_file(filename);
    if (cur_file == NULL && flags & UFS_CREATE) {
        cur_file = ufs_create_file(filename);
    }
    if (cur_file == NULL) {
        ufs_error_code = ufs_error_code == UFS_ERR_NO_ERR ? UFS_ERR_NO_FILE : ufs_error_code;
        return RETVAL_FAILURE;
    }

    int newFd = ufs_create_fd(cur_file, flags);
    if (newFd == RETVAL_FAILURE) {
        return RETVAL_FAILURE;
    }
    return newFd + 1; // +1 because indices starts from zero, but FDs starts from 1
}

ssize_t
ufs_write(int fdN, const char *buf, size_t size)
{
    int idx = fdN - 1;

    if (!ufs_check_valid_fd_idx(idx)) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return RETVAL_FAILURE;
    }

    struct filedesc *fd = file_descriptors[idx];
    struct file *file = fd->file;

    if (!fd->write_allowed) {
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return RETVAL_FAILURE;
    }

    if (fd->total_offset + size> MAX_FILE_SIZE) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return RETVAL_FAILURE;
    }

    const char *cur_buf = buf;
    size_t size_remaining = size;
    while (size_remaining != 0) {
        struct block *cur_block = fd->cur_block;
        size_t fd_offset_in_block = fd->total_offset % BLOCK_SIZE;
        size_t available_in_block = BLOCK_SIZE - fd_offset_in_block;

        size_t to_write = size_remaining;
        if (available_in_block < to_write) {
            to_write = available_in_block;
        }

        memcpy(&cur_block->memory[fd_offset_in_block], cur_buf, to_write);
        cur_buf += to_write;
        size_remaining -= to_write;
        fd->total_offset += to_write;
        if (fd->total_offset % BLOCK_SIZE == 0) {
            struct block *next_block = cur_block->next;
            if (next_block == NULL) {
                next_block = ufs_add_block(cur_block);
                file->last_block = next_block;
            }
            fd->cur_block = next_block;
        }
    }

    if (fd->total_offset > file->total_size) {
        file->total_size = fd->total_offset;
    }

    return size;
}

ssize_t
ufs_read(int fdN, char *buf, size_t size)
{
    int idx = fdN - 1;

    if (!ufs_check_valid_fd_idx(idx)) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return RETVAL_FAILURE;
    }

    struct filedesc *fd = file_descriptors[idx];
    struct file *file = fd->file;

    if (!fd->read_allowed) {
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return RETVAL_FAILURE;
    }

    if (fd->total_offset >= fd->file->total_size) {
        return RETVAL_EOF;
    }

    char *cur_buf = buf;
    size_t size_remaining = size;
    size_t read_total = 0;
    while (size_remaining > 0 && fd->total_offset < fd->file->total_size) {
        struct block *cur_block = fd->cur_block;
        size_t fd_offset_in_block = fd->total_offset % BLOCK_SIZE;
        size_t remaining_in_block = BLOCK_SIZE - fd_offset_in_block;
        size_t unread_from_file = file->total_size - fd->total_offset;

        size_t to_read = unread_from_file < remaining_in_block ? unread_from_file : remaining_in_block;
        if (to_read > size_remaining) {
            to_read = size_remaining;
        }
        assert(to_read <= BLOCK_SIZE);
        assert(to_read <= remaining_in_block);

        memcpy(cur_buf, &cur_block->memory[fd_offset_in_block], to_read);

        cur_buf += to_read;
        size_remaining -= to_read;
        read_total += to_read;

        fd->total_offset += to_read;

        if (fd->total_offset >= fd->file->total_size) {
            break;
        }

        if (fd->total_offset % BLOCK_SIZE != 0) {
            break;
        }

        struct block *next_block = cur_block->next;
        if (next_block == NULL) {
            printf("ACHTUNG! Next block doesn't even exist!");
            break;
        }
        fd->cur_block = next_block;

    }
    assert(read_total <= size);
    return read_total;
}

int
ufs_close(int fd)
{
    int idx = fd - 1;

    if (!ufs_check_valid_fd_idx(idx)) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return RETVAL_FAILURE;
    }

    ufs_delete_fd(idx);
    return RETVAL_SUCCESS;
}

int
ufs_delete(const char *filename)
{
    struct file * const file = ufs_find_file(filename);
    if (file == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return RETVAL_FAILURE;
    }

    ufs_mark_file_for_delete(file);
    if (file->refs == 0)
        ufs_real_delete_file(file);

    return RETVAL_SUCCESS;
}

int
ufs_resize(int fdN, size_t new_size)
{
    int idx = fdN - 1;

    if (!ufs_check_valid_fd_idx(idx)) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return RETVAL_FAILURE;
    }

    if (new_size > MAX_FILE_SIZE) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return RETVAL_FAILURE;
    }

    struct filedesc *fd = file_descriptors[idx];
    struct file *file = fd->file;

    if (new_size == file->total_size) {
        return RETVAL_SUCCESS;
    }

    bool ret = true;
    if (new_size > file->total_size) {
        ret = ufs_extend_file(file, new_size);
    } else {
        ret = ufs_reduce_file(file, new_size);
        ufs_move_overflowing_fds(file);
    }
    if (!ret) {
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}

/** PUBLIC INTERFACE END */

static struct file *
ufs_find_file(const char *name)
{
    struct file *cur_file = file_list;

    while (cur_file != NULL && (strcmp(name, cur_file->name) != 0 || cur_file->marked_for_deletion)) {
        cur_file = cur_file->next;
    }

    return cur_file;
}

static struct block *
ufs_add_block(struct block *last_block)
{
    struct block *new_block = (struct block *) calloc(sizeof(struct block), 1);
    if (new_block == NULL) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return NULL;
    }
    new_block->prev = NULL;
    new_block->next = NULL;
    new_block->memory = (char *) calloc(sizeof(char), BLOCK_SIZE);
    if (new_block->memory == NULL) {
        free(new_block);
        ufs_error_code = UFS_ERR_NO_MEM;
        return NULL;
    }

    if (last_block != NULL) {
        new_block->prev = last_block;
        last_block->next = new_block;
    }

    return new_block;
}

static void
ufs_free_blocks(struct block *block_list)
{
    struct block *cur = block_list;
    while (cur != NULL) {
        struct block *next = cur->next;
        free(cur->memory);
        free(cur);
        cur = next;
    }
}

static struct file *
ufs_create_file(const char *name)
{
    struct file *new_file = (struct file *) calloc(sizeof(struct file), 1);
    if (new_file == NULL) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return NULL;
    }
    new_file->name = (char *) calloc(sizeof(char), strlen(name) + 1);
    if (new_file->name == NULL) {
        ufs_error_code = UFS_ERR_NO_MEM;
        free(new_file);
        return NULL;
    }

    strcpy(new_file->name, name);

    new_file->block_list = ufs_add_block(NULL);
    new_file->last_block = new_file->block_list;
    if (new_file->block_list == NULL) {
        free(new_file);
        return NULL;
    }

    new_file->marked_for_deletion = false;
    new_file->refs = 0;

    new_file->prev = file_list_last;
    new_file->next = NULL;
    if (file_list == NULL) {
        file_list = new_file;
    }
    if (file_list_last == NULL) {
        file_list_last = new_file;
    } else {
        file_list_last->next = new_file;
        file_list_last = new_file;
    }
    return new_file;
}

static void
ufs_real_delete_file(struct file *const file)
{
    ufs_free_blocks(file->block_list);
    file->block_list = NULL;

    free(file->name);
    file->name = NULL;

    if (file->prev != NULL)
        file->prev->next = file->next;

    if (file_list == file)
        file_list = file->next;

    if (file_list_last == file) {
        file_list_last = file->prev;
    }
    if (file->next != NULL) {
        file->next->prev = file->prev;
    }

    free(file);
}

static bool
ufs_extend_file(struct file *const file, size_t new_size)
{
    size_t need_to_add = new_size - file->total_size;
    while (need_to_add != 0) {
        struct block *cur_block = file->last_block;
        size_t offsetInBlock = file->total_size % BLOCK_SIZE;
        size_t available_in_block = BLOCK_SIZE - offsetInBlock;

        size_t to_write = need_to_add;
        if (available_in_block < to_write) {
            to_write = available_in_block;
        }

        memset(&cur_block->memory[offsetInBlock], 0, to_write);
        need_to_add -= to_write;
        file->total_size += to_write;
        if (file->total_size % BLOCK_SIZE == 0) {
            struct block *new_block = ufs_add_block(cur_block);
            if (new_block == NULL) {
                return false;
            }
            file->last_block = new_block;
        }
    }
    return true;
}

static bool
ufs_reduce_file(struct file *const file, size_t new_size)
{
    size_t need_to_reduce = file->total_size - new_size;

    while (need_to_reduce != 0) {
        struct block *cur_block = file->last_block;

        size_t to_reduce = file->total_size % BLOCK_SIZE;
        if ((to_reduce == 0 && need_to_reduce < BLOCK_SIZE) || need_to_reduce < to_reduce) {
            to_reduce = need_to_reduce;
        }
        // Free entire block at once
        if (to_reduce == 0) {
            file->last_block = cur_block->prev;
            if (file->last_block != NULL)
                file->last_block->next = NULL;

            ufs_free_blocks(cur_block);
            file->total_size -= BLOCK_SIZE;
            need_to_reduce -= BLOCK_SIZE;
        } else {
            size_t full_blocks_before = file->total_size / BLOCK_SIZE;
            file->total_size -= to_reduce;
            need_to_reduce -= to_reduce;
            size_t full_blocks_after = file->total_size / BLOCK_SIZE;
            if (full_blocks_after < full_blocks_before) { // Case - when we staying on first byte of new block
                file->last_block = cur_block->prev;
                if (file->last_block != NULL)
                    file->last_block->next = NULL;

                ufs_free_blocks(cur_block);
            }
        }
    }

    return true;
}

static void
ufs_mark_file_for_delete(struct file *const file) {
    file->marked_for_deletion = true;
}

static bool
ufs_extend_fd_list_if_needed()
{
    if (file_descriptor_count < file_descriptor_capacity){
        return true;
    }

    ssize_t new_capacity = file_descriptor_capacity != 0 ? file_descriptor_capacity * 2 : 1;

    struct filedesc **new_mem = (struct filedesc **) calloc(sizeof(struct filedesc *), new_capacity);
    if (new_mem == NULL) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return false;
    }
    memcpy(new_mem, file_descriptors, file_descriptor_capacity * sizeof(struct filedesc *));
    free(file_descriptors);
    file_descriptors = new_mem;

    file_descriptor_capacity = new_capacity;
    return true;
}

static int
ufs_create_fd(struct file *file, int flags)
{
    struct filedesc *newFd = (struct filedesc *) calloc(1, sizeof(struct filedesc));
    if (newFd == NULL) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return RETVAL_FAILURE;
    }

    if (!ufs_extend_fd_list_if_needed()) {
        free(newFd);
        return RETVAL_FAILURE;
    }

    newFd->file = file;
    newFd->cur_block = file->block_list;
    newFd->total_offset = 0;
    file->refs++;
    newFd->write_allowed = flags & UFS_WRITE_ONLY || flags & UFS_READ_WRITE;
    newFd->read_allowed = flags & UFS_READ_ONLY || flags & UFS_READ_WRITE;

    while (file_descriptors[file_descriptors_rr_ptr] != NULL) {
        file_descriptors_rr_ptr++;
        if (file_descriptors_rr_ptr == file_descriptor_capacity)
            file_descriptors_rr_ptr = 0;
    }

    file_descriptors[file_descriptors_rr_ptr] = newFd;
    file_descriptor_count++;
    return file_descriptors_rr_ptr;
}

static void
ufs_delete_fd(int idx)
{
    struct filedesc * fd = file_descriptors[idx];
    file_descriptors[idx] = NULL;

    struct file * const file = fd->file;
    file->refs--;
    if (file->marked_for_deletion && file->refs == 0) {
        ufs_real_delete_file(file);
    }

    free(fd);
}

static bool
ufs_check_valid_fd_idx(int idx)
{
    if (idx < 0 || idx >= file_descriptor_capacity) {
        return false;
    }

    return file_descriptors[idx] != NULL;
}

static void
ufs_move_overflowing_fds(const struct file * const file)
{
    size_t descriptors_found = 0;
    for (size_t i = 0; i < file_descriptor_capacity; i++) {
        if (file_descriptors[i] == NULL|| file_descriptors[i]->file != file) {
            continue;
        }
        struct filedesc *fd = file_descriptors[i];
        descriptors_found++;

        if (fd->total_offset <= file->total_size) {
            continue;
        }

        fd->total_offset = file->total_size;
        fd->cur_block = file->last_block;
    }

    assert(descriptors_found == file->refs);
}