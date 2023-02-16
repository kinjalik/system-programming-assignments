//
// Created by Albert Akmukhametov on 16.02.2023.
//

#include <time.h>
#include "libsort.h"

#define printf(...)

#define SPLIT_RANGE(start_expr, end_expr, mid_lvalue)           \
do {                                                            \
    size_t d_start_val = (start_expr);                          \
    size_t d_end_val = (end_expr);                              \
    (mid_lvalue) = d_start_val + (d_end_val - d_start_val) / 2; \
} while (0)

#define CALCULATE_INTERVALS(start_expr, mid_expr, end_expr, left_len_lvalue, right_len_lvalue) \
do {                                                                                           \
    size_t d_start_val = (start_expr);                                                         \
    size_t d_end_val = (end_expr);                                                             \
    size_t d_mid_val = (mid_expr);                                                             \
    (left_len_lvalue) = d_mid_val - d_start_val + 1;                                           \
    (right_len_lvalue) = d_end_val - d_mid_val;                                                \
} while (0)

#define TRANSFER_NUMBERS(file_from, file_to, len) \
do {                                              \
    FILE *d_from = (file_from);                   \
    FILE *d_to = (file_to);                       \
    int d_tmp;                                    \
    size_t d_len = (len);                         \
    for (size_t i = 0; i < d_len; i++) {          \
        fscanf(d_from, "%d", &d_tmp);             \
        fprintf(d_to, "%d ", d_tmp);              \
    }                                             \
    fflush(d_to);                                 \
} while (0)

#define PRINT_NUMBERS(file, len)         \
do {                                     \
    FILE *d_file = (file);               \
    size_t d_len = (len);                \
    int d_tmp;                           \
    for (size_t i = 0; i < d_len; i++) { \
        fscanf(d_file, "%d", &d_tmp);    \
        printf("%d ", d_tmp);            \
    }                                    \
    printf("\n");                        \
} while(0)

// ASSUMPTION: There are no more than 10000 list sorting procedures.
// If it was prod code, I would like to use map trace_id -> {target_latency, timestamp} with erasure at end of sorting
// But it is edu purposes code, so I would like omit it to save time (I wrote it in 3:40 PM :D)
#define MAX_SORTING_PROCESSES 10000
static uint64_t last_yield_time[MAX_SORTING_PROCESSES] = {0};
static uint64_t target_latency = 0;

uint64_t get_time_in_microsec() {
    struct timespec time = {0};
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t result = 0;
    result += time.tv_nsec / 1000;
    result += time.tv_sec * 1000000;
    return result;
}

#define YIELD()                                                                \
do {                                                                           \
    if (get_time_in_microsec() - last_yield_time[trace_id] > target_latency) { \
        coro_yield();                                                          \
        last_yield_time[trace_id] = get_time_in_microsec();                    \
    }                                                                          \
} while(0);

static FILE *merge_files(int trace_id, FILE *file_left, FILE *file_right, size_t left_len, size_t right_len)
{
    FILE *output = tmpfile();
    if (file_left == NULL && file_right == NULL) {
        printf("[RUN %d][MERGE] Both parts are empty.\n", trace_id);
        goto EXIT;
    }
    if (file_left == NULL) {
        printf("[RUN %d][MERGE] Left part is empty, right part (len=%lu): ", trace_id, right_len);
        PRINT_NUMBERS(file_right, right_len); rewind(file_right);
        TRANSFER_NUMBERS(file_right, output, right_len);
        goto EXIT;
    }
    if (file_right == NULL) {
        printf("[RUN %d][MERGE] Right part is empty, left part (len=%lu): ", trace_id, left_len);
        PRINT_NUMBERS(file_left, left_len); rewind(file_left);
        TRANSFER_NUMBERS(file_left, output, left_len);
        goto EXIT;
    }

    printf("[RUN %d][MERGE] Left part (len=%lu):  ", trace_id, left_len);
    PRINT_NUMBERS(file_left, left_len);
    printf("[RUN %d] MERGE] Right part (len=%lu): ", trace_id, right_len);
    PRINT_NUMBERS(file_right, right_len);
    rewind(file_left);
    rewind(file_right);

    int left_num, right_num;
    fscanf(file_left, "%d", &left_num);
    fscanf(file_right, "%d", &right_num);

    while (left_len > 0 || right_len > 0) {
        // Case 1: right side is empty, need to write left
        // Case 2: both not empty, left greater
        int to_print = 0;
        if (right_len == 0 || (left_len != 0 && left_num < right_num)) {
            to_print = left_num;
            fscanf(file_left, "%d", &left_num);
            left_len--;
        } else {
            assert(left_len == 0 || (right_len != 0 && right_num <= left_num));
            to_print = right_num;
            fscanf(file_right, "%d", &right_num);
            right_len--;
        }
        fprintf(output, "%d ", to_print);
//        printf("Printing: %d\n", to_print);
    }

    EXIT:
    fflush(output);
    rewind(output);
    return output;
}

static FILE *merge(int trace_id, FILE *file_left, FILE *file_right, size_t start, size_t mid, size_t end)
{
    size_t left_len, right_len;
    CALCULATE_INTERVALS(start, mid, end, left_len, right_len);

    return merge_files(trace_id, file_left, file_right, left_len, right_len);

}

static FILE *split(int trace_id, FILE *input, size_t start, size_t end) {
    if (end - start == 0) {
        FILE *output = tmpfile();
        TRANSFER_NUMBERS(input, output, 1);
        rewind(output);
        return output;
    }

    size_t mid;
    SPLIT_RANGE(start, end, mid);
    int left_len, right_len;
    CALCULATE_INTERVALS(start, mid, end, left_len, right_len);

    FILE *left = tmpfile();
    FILE *right = tmpfile();

    TRANSFER_NUMBERS(input, left, left_len);
    TRANSFER_NUMBERS(input, right, right_len);
    rewind(left); rewind(right);

    FILE *left_output = split(trace_id, left, start, mid);
    YIELD();
    FILE *right_output = split(trace_id, right, mid + 1, end);
    YIELD();

    fclose(left); fclose(right);
    if (left_output != NULL) rewind(left_output);
    if (right_output != NULL) rewind(right_output);

    FILE *merged_output = merge(trace_id, left_output, right_output, start, mid, end);
    printf("[RUN %d][SPLIT] Result of merge: ", trace_id);
    PRINT_NUMBERS(merged_output, end - start + 1);
    fclose(left_output); fclose(right_output);
    YIELD();

    rewind(merged_output);
    return merged_output;
}

static size_t count_numbers_in_file(FILE *file)
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
FILE *sort_file(uint64_t latency, const char *const name)
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
    FILE *output = split(trace_id, file, 0, numbers_in_file);
    printf("[RUN %d] Result of sort: ", trace_id);
    PRINT_NUMBERS(output, numbers_in_file);
    fclose(file);
    rewind(output);
    YIELD();
    return output;
}

/* This function doesn't require yield! */
FILE *merge_sorted_files(FILE *a, FILE *b)
{
    trace_id++;

    size_t a_len = count_numbers_in_file(a);
    size_t b_len = count_numbers_in_file(b);
    return merge_files(trace_id, a, b, a_len, b_len);
}
