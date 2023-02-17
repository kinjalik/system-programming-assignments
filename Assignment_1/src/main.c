#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include "libcoro.h"
#include "libsort.h"
#include "libutil.h"

#define COROUTINE_NAME_LEN 16

typedef enum {
    SORTING_WAITING,
    SORTING_IN_PROGRESS,
    SORTING_FINISHED,
} sorting_status;

typedef struct file_list {
    const char *filename;
    FILE *sorted_output;
    sorting_status status;

    struct file_list *next;
} file_list;

static file_list *g_sorted_files_head = NULL;
static file_list *g_sorted_files_tail = NULL;

uint64_t g_target_latency = 0;

static int coroutine_func_f(void *context) {
    const char *const coro_name = (char *) context;
    uint64_t start_time = get_time_in_microsec();

    struct coro *this = coro_this();
    size_t switch_cnt = 0;

    file_list *cur = g_sorted_files_head;
    while (cur != NULL) {
        if (cur->status != SORTING_WAITING) {
            cur = cur->next;
            continue;
        }

        cur->status = SORTING_IN_PROGRESS;
        cur->sorted_output = sort_file(g_target_latency, cur->filename, &switch_cnt);
        cur->status = SORTING_FINISHED;
    }

    uint64_t end_time = get_time_in_microsec();
    assert(end_time >= start_time);
    printf("%s finished with %lu context switches. Execution time: ", coro_name, switch_cnt);
    print_time_diff(start_time, end_time);
    printf("\n");

    free(context);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    uint64_t start_time = get_time_in_microsec();

    /* Read coroutine pool, read all files to sort */
    long long coroutine_pool_size = strtol(argv[1], NULL, 10);
    g_target_latency = strtol(argv[2], NULL, 10);
    for (int i = 3; i < argc; i++) {
        if (g_sorted_files_head == NULL) {
            g_sorted_files_head = g_sorted_files_tail = calloc(1, sizeof(file_list));
        } else {
            g_sorted_files_tail->next = calloc(1, sizeof(file_list));
            g_sorted_files_tail = g_sorted_files_tail->next;
        }
        g_sorted_files_tail->status = SORTING_WAITING;
        g_sorted_files_tail->filename = argv[i]; // Since main will live during execution time, I use argv safely
    }

    /* Initialize our coroutine global cooperative scheduler. */
    coro_sched_init();

    /* Start several coroutines. */
    for (int i = 0; i < coroutine_pool_size; i++) {
        char coro_name[COROUTINE_NAME_LEN];
        sprintf(coro_name, "Coroutine-%d", i);

        coro_new(coroutine_func_f, strdup(coro_name));
    }
    /* Wait for all the coroutines to end. */
    struct coro *c;
    while ((c = coro_sched_wait()) != NULL) {
        /*
         * Each 'wait' returns a finished coroutine with which you can
         * do anything you want. Like check its exit status, for
         * example. Don't forget to free the coroutine afterwards.
         */
        coro_delete(c);
    }
    /* All coroutines have finished. */

    size_t file_cnt = argc - 3;
    FILE **files = (FILE **) calloc(file_cnt, sizeof(FILE *));
    size_t *file_lens = (size_t *) calloc(file_cnt, sizeof(size_t));
    int *cur_value = (int *) calloc(file_cnt, sizeof(int));


    file_list *cur = g_sorted_files_head, *next;
    size_t k = 0;
    while (cur != NULL) {
        assert(cur->status == SORTING_FINISHED);

        files[k] = cur->sorted_output;
        rewind(files[k]);
        file_lens[k] = count_numbers_in_file(files[k]);
        rewind(files[k]);

        next = cur->next;
        free(cur);
        cur = next;
        k++;
    }

    FILE *output = fopen("output.txt", "w");

    for (size_t i = 0; i < file_cnt; i++) {
        fscanf(files[i], "%d", &cur_value[i]);
    }

    bool merged = false;
    do {
        int to_write = INT32_MAX;
        size_t idx = file_cnt + 1;

        for (size_t i = 0; i < file_cnt; i++) {
            if (file_lens[i] == 0) {
                continue;
            }

            if (idx == file_cnt + 1 || cur_value[i] < to_write) {
                to_write = cur_value[i];
                idx = i;
            }
        }

        fprintf(output, "%d ", to_write);
        fscanf(files[idx], "%d", &cur_value[idx]);
        file_lens[idx]--;

        merged = true;
        for (size_t i = 0; merged && i < file_cnt; i++) {
            merged = merged && file_lens[i] == 0;
        }
    } while(!merged);

    fflush(output);
    fclose(output);

    uint64_t end_time = get_time_in_microsec();
    printf("Execution time: ");
    print_time_diff(start_time, end_time);
    return 0;
}