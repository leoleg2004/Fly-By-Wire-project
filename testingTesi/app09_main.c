/*
 * Main program
 */
//boost xml parser fai file xml per la lettura di tutti i parametri e un fie con i risultati d amattere in csv 
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

/*
 * data structure for activity parameters
 */
typedef struct activity_parameters {
	char name[15];
	int period;
	int parameter;
} t_activity_par;


/*
 * Function that implements the activity
 * the input parameter is used to change the computational cost
 */
void Activity(int parameter) {
    double result = 0;
    for (long i=0; i<parameter*1000*1000; i++) {
		result = result + sin(i) + cos(i) + i *tan(i);
    }
}

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

		// execute the computation
		// ---------------------------------------------------------
		exec_start_time = time_current_millisecs();
		if( ! skip) {
			Activity(activity.parameter);
		}
		exec_end_time = time_current_millisecs();
		// ---------------------------------------------------------

		// compute the difference between end_time and start_time
		computational_cost = exec_end_time - exec_start_time;

		printf("%s:            exec_start_time         = %ld millisecs\n", activity.name, exec_start_time);
		printf("%s:            exec_end_time           = %ld millisecs\n", activity.name, exec_end_time);
		if(skip) {
			printf("%s:   *SKIP*   cost                    =    %ld millisecs\n", activity.name, computational_cost);
			skip = false;
		}
		else {
			if(exec_end_time > exec_next_release_time) {
				printf("%s:   -MISS-   cost                    = %ld millisecs\n", activity.name, computational_cost);
				// skipp one job
				skip = true;
			}
			else
				printf("%s:   DO JOB   cost                    = %ld millisecs\n", activity.name, computational_cost);
		}
		printf("%s:            period                  = %d millisecs\n", activity.name, activity.period);
		printf("%s:            exec_next_release_time  = %ld millisecs\n\n", activity.name, exec_next_release_time);

		// sleep until the end of the period
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);
		
    } // end while(alive)

    pthread_exit((void*) ptr);
}



int main(int argc, char* argv[]) {
    pthread_t thread1, thread2;
    int ret_err;
	
    /* Create the first periodic thread */
    t_activity_par activity_1;
    sprintf(activity_1.name,	"Activity_1");
    activity_1.period = 1800;
    activity_1.parameter = 20;
    ret_err = pthread_create( &thread1, NULL, PeriodicTask, (void*) &activity_1);
    handle_error(ret_err, "Error in creating PeriodicTask 1");

	/* Create the second periodic thread */
	t_activity_par activity_2;
	sprintf(activity_2.name,	"Activity_2");
	activity_2.period = 800;
	activity_2.parameter = 10;
    ret_err = pthread_create( &thread2, NULL, PeriodicTask, (void*) &activity_2);
    handle_error(ret_err, "Error in creating PeriodicTask 2");

    /* Wait till threads are complete before main continues.  */
    pthread_join( thread1, NULL);
	pthread_join( thread2, NULL);
    
	printf("\nPeriodic Tasks completed correctly\n");
     	   	
    exit(0);
}


