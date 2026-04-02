/*
 * activity_library.c
 */
#include "activity_library.h"
#include <stdio.h>

/*
 * periodic task that executes an activity
 */
void *PeriodicTask( void *ptr ) {
    t_activity_par activity;
    activity = *((t_activity_par *) ptr);	// [ms]
 
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
		time_add_millisecs(&exec_release_time, activity.period); 

		// get the next activation time in microseconds
		exec_next_release_time  = time_to_millisecs(&exec_release_time);

		if(skip) {
			if(activity.print) printf("\n%s:   *SKIP*   \n\n\n", activity.name);
			skip = false;
		}
		else {
			// execute the computation
			// ---------------------------------------------------------
			exec_start_time = time_current_millisecs();
			activity.function(activity.parameter);
			exec_end_time = time_current_millisecs();
			// ---------------------------------------------------------
			
			// compute the difference between end_time and start_time
			computational_cost = exec_end_time - exec_start_time;

			if(exec_end_time > exec_next_release_time) {
				if(activity.print) printf("%s:   -MISS-   cost            = %ld millisecs\n", activity.name, computational_cost);
				// skipp one job
				skip = true;
			}
			else
				if(activity.print) printf("%s:   JOB OK  cost            = %ld millisecs\n", activity.name, computational_cost);
			if(activity.print) printf("%s:   period                  = %d millisecs\n", activity.name, activity.period);
			if(activity.print) printf("%s:   exec_next_release_time  = %ld millisecs\n\n", activity.name, exec_next_release_time);


		}
		// sleep until the end of the period
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);
		
    } // end while(alive)

    pthread_exit((void*) ptr);
}



/*
 * function that executes long mathematical operations
 */
void activity_load(int cost) {
    double result = 0;
    for (long i=0; i<cost*1000*1000; i++) {
		result = result + sin(i) + cos(i) + i *tan(i);
    }
}