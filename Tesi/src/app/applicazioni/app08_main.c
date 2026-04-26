/*
 * Main program
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>  	
#include <time.h>       
#include <math.h>
#include <stdbool.h>

#include "../lib/time_library.h"

#define handle_error(en, msg) \
        if(en != 0) {errno = en; perror(msg); exit(EXIT_FAILURE);}



int period1 = 1100; // [ms]

void Activity_1() {
    double result = 0;
    for (long i=0; i<20*1000*1000; i++) {
		result = result + sin(i) + cos(i) + i *tan(i);
    }
}


void *PeriodicTask1( void *ptr ) {
    int period;
    period = *((int *) ptr);	// [ms]
 
    struct timespec current_time; 
    struct timespec exec_release_time; 

    uint64_t exec_next_release_time;
    uint64_t exec_start_time;
    uint64_t exec_end_time;
    uint64_t computational_cost;
	
	bool skip = false;
	
    // Obtain the current time: this is the beginning of the first period
	clock_gettime(CLOCK_MONOTONIC, &exec_release_time);

    /*
     * periodic loop
     */
    while(1) {
		// Calculate the time for the next periodic activation: period (millisecs)
		time_add_millisecs(&exec_release_time, period); 

		// get the next activation time in microseconds
		exec_next_release_time  = time_to_millisecs(&exec_release_time);

		// execute the computation
		exec_start_time = time_current_millisecs();
		if( ! skip) {
			Activity_1();
		}
		exec_end_time = time_current_millisecs();

		// compute the difference between end_time and start_time
		computational_cost = exec_end_time - exec_start_time;

		printf("Activity_1:            exec_start_time         = %ld millisecs\n",exec_start_time);
		printf("Activity_1:            exec_end_time           = %ld millisecs\n",exec_end_time);
		if(skip) {
			printf("Activity_1:   *SKIP*   cost =    %ld millisecs\n",computational_cost);
			skip = false;
		}
		else {
			if(exec_end_time > exec_next_release_time) {
				printf("Activity_1:   -MISS-   cost      = %ld millisecs\n",computational_cost);
				// skipp one job
				skip = true;
			}
			else
				printf("Activity_1:   DO JOB   cost      = %ld millisecs\n",computational_cost);
		}
		printf("Activity_1:            period                  = %d millisecs\n",(period));
		printf("Activity_1:            exec_next_release_time  = %ld millisecs\n\n",exec_next_release_time);

		// sleep until the end of the period
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);
		
    } // end while(alive)

    pthread_exit((void*) ptr);
}



int main(int argc, char* argv[]) {
    pthread_t thread1;
    int ret_err;

    /* Create the periodic thread */
    ret_err = pthread_create( &thread1, NULL, PeriodicTask1, (void*) &period1);
    handle_error(ret_err, "Error in creating PeriodicTask 1");

    /* Wait till threads are complete before main continues.  */
    pthread_join( thread1, NULL);

    printf("\nPeriodic Tasks completed correctly\n");
     	   	
    exit(0);
}


