/*
 * time_library.h
 */
#ifndef TIME_LIBRARY
#define TIME_LIBRARY

#include <time.h>
#include <stdint.h>  	

#define USEC_PER_NSEC		1000L					// 10^3
#define USEC_PER_SEC		1000000L			// 10^6
#define NSEC_PER_SEC		1000000000L		// 10^9


// return current time in milliseconds
uint64_t time_current_millisecs();

// add time in milliseconds
void time_add_millisecs(struct timespec *initial_time,	uint64_t addition) ;

// return time_stamp in milliseconds
uint64_t time_to_millisecs(struct timespec *time_stamp) ;


#endif