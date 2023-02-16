#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
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

    file_list *cur = g_sorted_files_head, *next;
    FILE *tmp_output = tmpfile();
    while (cur != NULL) {
        assert(cur->status == SORTING_FINISHED);

        FILE *a = tmp_output;
        FILE *b = cur->sorted_output;
        rewind(a); rewind(b);
        tmp_output = merge_sorted_files(a, b);
        fclose(a);
        fclose(b);

        next = cur->next;
        free(cur);
        cur = next;
    }

    rewind(tmp_output);
    FILE *output = fopen("output.txt", "w");
    char a;
    while( (a = fgetc(tmp_output)) != EOF )
    {
        fputc(a, output);
    }

    fflush(output);
    fclose(tmp_output);
    fclose(output);

    uint64_t end_time = get_time_in_microsec();
    printf("Execution time: ");
    print_time_diff(start_time, end_time);
    return 0;
}