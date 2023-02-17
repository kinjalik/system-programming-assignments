#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "libsort.h"
#include "libutil.h"

#define printf(...)

static size_t split_range(const size_t start, const size_t end)
{
    return start + (end - start) / 2;
}

static size_t calculate_left_interval(const size_t start, const size_t mid, const size_t end)
{
    return mid - start + 1;
}

static size_t calculate_right_interval(const size_t start, const size_t mid, const size_t end)
{
    return end - mid;
}

static void transfer_numbers(FILE * const from, FILE * const to, const size_t len)
{
    int tmp;
    for (size_t i = 0; i < len - 1; i++) {
        fscanf(from, "%d", &tmp);
        fprintf(to, "%d ", tmp);
    }
    fscanf(from, "%d", &tmp);
    fprintf(to, "%d", tmp);
    fflush(to);
}

static void print_numbers(FILE * const file, const size_t len)
{
    int tmp;
    for (size_t i = 0; i < len; i++) {
        fscanf(file, "%d", &tmp);
        printf("%d ", tmp);
    }
    printf("\n");
}

// ASSUMPTION: There are no more than 10000 list sorting procedures.
// If it was prod code, I would like to use map trace_id -> {target_latency, timestamp} with erasure at end of sorting
// But it is edu purposes code, so I would like omit it to save time (I wrote it in 3:40 PM :D)
#define MAX_SORTING_PROCESSES 10000
static uint64_t last_yield_time[MAX_SORTING_PROCESSES] = {0};
static uint64_t target_latency = 0;

/* Achtung: this macro uses trace_id and ctx_switch_count from outer scope! */

#define YIELD()                                                                \
do {                                                                           \
    if (get_time_in_microsec() - last_yield_time[trace_id] > target_latency) { \
        *ctx_switch_count = *ctx_switch_count + 1;                               \
        coro_yield();                                                          \
        last_yield_time[trace_id] = get_time_in_microsec();                    \
    }                                                                          \
} while(0);

static FILE *merge_files(const int trace_id, FILE *const file_left, FILE *const file_right,
                         size_t left_len, size_t right_len)
{
    FILE *output = tmpfile();
    if (file_left == NULL && file_right == NULL) {
        printf("[RUN %d][MERGE] Both parts are empty.\n", trace_id);
        goto EXIT;
    }
    if (file_left == NULL) {
        printf("[RUN %d][MERGE] Left part is empty, right part (len=%lu):\n", trace_id, right_len);
        print_numbers(file_right, right_len); rewind(file_right);
        transfer_numbers(file_right, output, right_len);
        goto EXIT;
    }
    if (file_right == NULL) {
        printf("[RUN %d][MERGE] Right part is empty, left part (len=%lu):\n", trace_id, left_len);
        print_numbers(file_left, left_len); rewind(file_left);
        transfer_numbers(file_left, output, left_len);
        goto EXIT;
    }

    printf("[RUN %d][MERGE] Left part (len=%lu):  ", trace_id, left_len);
    print_numbers(file_left, left_len);
    printf("[RUN %d] MERGE] Right part (len=%lu): ", trace_id, right_len);
    print_numbers(file_right, right_len);
    rewind(file_left);
    rewind(file_right);

    int left_num, right_num;
    fscanf(file_left, "%d", &left_num);
    fscanf(file_right, "%d", &right_num);

    size_t left_cnt = 0, right_cnt = 0;
    while (left_cnt < left_len || right_cnt < right_len) {
        int to_print = 0;

        if (left_cnt == left_len && right_cnt < right_len) {
            to_print = right_num;
            fscanf(file_right, "%d", &right_num);
            right_cnt++;
        } else if (right_cnt == right_len && left_cnt < left_len) {
            to_print = left_num;
            fscanf(file_left, "%d", &left_num);
            left_cnt++;
        } else if (left_num <= right_num && left_cnt < left_len) {
            to_print = left_num;
            fscanf(file_left, "%d", &left_num);
            left_cnt++;
        } else if (left_num > right_num && right_cnt < right_len) {
            to_print = right_num;
            fscanf(file_right, "%d", &right_num);
            right_cnt++;
        }

        if (left_cnt == left_len && right_cnt == right_len) {
            fprintf(output, "%d", to_print);
        } else {
            fprintf(output, "%d ", to_print);
        }
    }

    EXIT:
    fflush(output);
    rewind(output);
    return output;
}

static FILE *merge(const int trace_id, FILE *file_left, FILE *file_right, size_t start, size_t mid, size_t end)
{
    size_t left_len  = calculate_left_interval(start, mid, end);
    size_t right_len = calculate_right_interval(start, mid, end);

    return merge_files(trace_id, file_left, file_right, left_len, right_len);
}

static void quick_sort(const int trace_id, size_t *const ctx_switch_count,
                       int * const numbers, const size_t first, const size_t last)
{
    int temp;
    size_t pivot, i, j;
    if (first >= last) {
        return;
    }

    pivot = first;
    i = first;
    j = last;

    while (i < j){
        while (numbers[i] <= numbers[pivot] && i < last)
            i++;

        while (numbers[j] > numbers[pivot])
            j--;

        if (i < j){
            temp = numbers[i];
            numbers[i] = numbers[j];
            numbers[j] = temp;
        }
    }

    temp = numbers[pivot];
    numbers[pivot] = numbers[j];
    numbers[j] = temp;

    YIELD();

    if (j != 0) {
        quick_sort(trace_id, ctx_switch_count, numbers, first, j - 1);
    }
    quick_sort(trace_id, ctx_switch_count, numbers, j + 1, last);
}

static FILE *quick_sort_prepare(const int trace_id, size_t *const ctx_switch_count, FILE *const input, const size_t len)
{
    printf("[RUN %d][QUICK_SORT] Sorting %lu numbers using quick-sort\n", trace_id, len);
    int *numbers = (int *) calloc(len, sizeof(int));
    for (size_t i = 0; i < len; i++) {
        fscanf(input, "%d", &numbers[i]);
    }

    quick_sort(trace_id, ctx_switch_count, numbers, 0, len - 1);

    FILE *output = tmpfile();
    for (size_t i = 0; i < len - 1; i++) {
        fprintf(output, "%d ", numbers[i]);
    }
    fprintf(output, "%d", numbers[len - 1]);

    free(numbers);
    return output;
}

static FILE *split(const int trace_id, size_t *const ctx_switch_count,
                   FILE *const input, const size_t start, const size_t end) {
    printf("[RUN %d][MERGE_SORT] Sorting from %lu to %lu numbers using merge-sort\n", trace_id, start, end);
    if (end - start == 0) {
        FILE *output = tmpfile();
        transfer_numbers(input, output, 1);
        rewind(output);
        return output;
    }

    size_t mid = split_range(start, end - 1);
    size_t left_len  = calculate_left_interval(start, mid, end - 1);
    size_t right_len = calculate_right_interval(start, mid, end - 1);

    FILE *left = tmpfile();
    FILE *right = tmpfile();

    transfer_numbers(input, left, left_len);
    transfer_numbers(input, right, right_len);
    rewind(left); rewind(right);

    printf("From %lu to %lu, mid=%lu, left_len=%lu, right_len=%lu\n", start, end, mid, left_len, right_len);
    FILE *left_output = left_len > MAX_NUMBERS_LOADED ?
                        split(trace_id, ctx_switch_count, left, start, mid) :
                        quick_sort_prepare(trace_id, ctx_switch_count, left, left_len);
    YIELD();
    FILE *right_output = right_len > MAX_NUMBERS_LOADED ?
                         split(trace_id, ctx_switch_count, right, mid + 1, end) :
                         quick_sort_prepare(trace_id, ctx_switch_count, right, right_len);
    YIELD();

    fclose(left); fclose(right);
    if (left_output != NULL) rewind(left_output);
    if (right_output != NULL) rewind(right_output);

    FILE *merged_output = merge(trace_id, left_output, right_output, start, mid, end - 1);
    printf("[RUN %d][SPLIT] Result of merge: ", trace_id);
    print_numbers(merged_output, end - start + 1);
    fclose(left_output); fclose(right_output);
    YIELD();

    rewind(merged_output);
    return merged_output;
}

size_t count_numbers_in_file(FILE * const file)
{
    size_t output = 0;
    char symbol = 0;
    while (symbol != EOF) {
        symbol = (char) fgetc(file);
        if (symbol == ' ' || symbol == '\n' || symbol == '\0' || symbol == EOF) {
            output++;
        }
    };
    rewind(file);
    return output;
}

static int trace_id = 0;
FILE *sort_file(const uint64_t latency, const char *const name, size_t *const ctx_switch_count)
{
    trace_id++;
    last_yield_time[trace_id] = get_time_in_microsec();
    target_latency = latency;

    FILE *file = fopen(name, "r");
    if (file == NULL) {
        printf("[RUN %d] Failed to open the file, errno=%u, error is: %s", trace_id, errno, strerror(errno));
        return NULL;
    }

    size_t numbers_in_file = count_numbers_in_file(file);
    printf("[RUN %d] Numbers in file: %lu\n", trace_id, numbers_in_file);
    rewind(file);
    FILE *output = numbers_in_file > MAX_NUMBERS_LOADED ?
            split(trace_id, ctx_switch_count, file, 0, numbers_in_file) :
                   quick_sort_prepare(trace_id, ctx_switch_count, file, numbers_in_file);
    printf("[RUN %d] Result of sort: ", trace_id);
    print_numbers(output, numbers_in_file);
    rewind(output);
    numbers_in_file = count_numbers_in_file(output);
    rewind(output);
    printf("[RUN %d] Numbers in sorted: %lu\n", trace_id, numbers_in_file);
    fclose(file);
    rewind(output);
    YIELD();
    return output;
}

/* This function doesn't require yield! */
FILE *merge_sorted_files(FILE *const a, FILE *const b)
{
    trace_id++;

    size_t a_len = count_numbers_in_file(a);
    size_t b_len = count_numbers_in_file(b);
    return merge_files(trace_id, a, b, a_len, b_len);
}
