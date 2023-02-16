#ifndef ASSIGNMENT_1_LIBSORT_H
#define ASSIGNMENT_1_LIBSORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "libcoro.h"

FILE *sort_file(uint64_t target_latency, const char *const name);
FILE *merge_sorted_files(FILE *a, FILE *b);

#endif //ASSIGNMENT_1_LIBSORT_H
