#ifndef ASSIGNMENT_1_LIBSORT_H
#define ASSIGNMENT_1_LIBSORT_H

#include <stdio.h>
#include <stdint.h>
#include "libcoro.h"

// Restriction: No more than 2 MB of numbers from one file may be loaded simultaneously
// Assumption: int is 2^2=4 bytes (i.e. int32_t)
#define MAX_NUMBERS_LOADED (2 << 10 << 10 >> 2)

FILE *sort_file(const uint64_t latency, const char *const name, size_t *const ctx_switch_count,
                uint64_t *const execTime);
FILE *merge_sorted_files(FILE *a, FILE *b);
size_t count_numbers_in_file(FILE * const file);

#endif //ASSIGNMENT_1_LIBSORT_H
