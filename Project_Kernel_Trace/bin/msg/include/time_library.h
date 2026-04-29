#ifndef TIME_LIBRARY_H
#define TIME_LIBRARY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>

#define NSEC_PER_SEC 1000000000L

void time_add_millisecs(struct timespec *ts, uint64_t ms);
uint64_t time_to_millisecs(const struct timespec *ts);
uint64_t time_current_millisecs();

#ifdef __cplusplus
}
#endif

#endif