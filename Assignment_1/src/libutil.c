#include <assert.h>
#include <printf.h>
#include "libutil.h"
#include "time.h"

#define MAGNITUDE 1000
#define MICROSEC_IN_SEC (MAGNITUDE * MAGNITUDE)

uint64_t get_time_in_microsec()
{
    struct timespec time = {0};
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t result = 0;
    result += time.tv_nsec / 1000;
    result += time.tv_sec * 1000000;
    return result;
}

void print_time_diff(const uint64_t start, const uint64_t end)
{
    assert(end >= start);
    uint64_t diff = end - start;

    uint64_t seconds = diff / MICROSEC_IN_SEC;
    uint32_t milliseconds = (diff % MICROSEC_IN_SEC) / MAGNITUDE;
    uint32_t microseconds = (diff % MICROSEC_IN_SEC) % MAGNITUDE;
    printf("%llu seconds, %u milliseconds, %u microseconds", seconds, milliseconds, microseconds);
}

