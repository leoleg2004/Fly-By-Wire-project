/*
 * time_library.c
 */
#include "time_library.h"
#include <math.h>

// return current time in milliseconds
uint64_t time_current_millisecs() {
    struct timespec time_stamp;

    clock_gettime(CLOCK_MONOTONIC, &time_stamp);

	uint64_t ms;
	ms = time_stamp.tv_sec * 1000;
    ms += round(time_stamp.tv_nsec / 1000000); // Convert nanoseconds to milliseconds

	return ms;
}


// add time in milliseconds
void time_add_millisecs(struct timespec *initial_time,	uint64_t addition) {
	// Add the corresponding time
	initial_time->tv_nsec += addition * 1000*1000;

	// Convert the resulting nanoseconds into seconds
	while (initial_time->tv_nsec >= NSEC_PER_SEC) {

		// Decrement the nanoseconds (-1e9)
		initial_time->tv_nsec -= NSEC_PER_SEC;

		// Increment the seconds
		initial_time->tv_sec++;
	}
}


// return time_stamp in milliseconds
uint64_t time_to_millisecs(struct timespec *time_stamp)  {
	uint64_t ms;

	ms = (uint64_t)((time_stamp->tv_sec * 1000) + 
	 				        (time_stamp->tv_nsec / 1000000));
	return ms;
}