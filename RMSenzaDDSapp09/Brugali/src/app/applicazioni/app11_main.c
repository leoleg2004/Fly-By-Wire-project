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
#include "../lib/activity_library.h"

/*
 * init a mutex
 */
pthread_mutex_t  mutex_activity  = PTHREAD_MUTEX_INITIALIZER;

/*
 * Function that implements the activity
 * the input parameter is used to change the computational cost
 */
void Activity(int parameter) {
	// get the lock
	pthread_mutex_lock( &mutex_activity );

	// access the critical section
    double result = 0;
    for (long i=0; i<parameter*1000*1000; i++) {
		result = result + sin(i) + cos(i) + i *tan(i);
    }
	
	// release the lock
	pthread_mutex_unlock( &mutex_activity );    	
}




int main(int argc, char* argv[]) {
    pthread_t thread1, thread2;
    int ret_err;
	
	/* Create the first periodic thread */
	t_activity_par activity_1;
	sprintf(activity_1.name,	"Activity_1");
	activity_1.function = Activity;
	activity_1.period = 800;
	activity_1.parameter = 10;
    ret_err = pthread_create( &thread1, NULL, PeriodicTask, (void*) &activity_1);
    handle_error(ret_err, "Error in creating PeriodicTask 1");

	/* Create the second periodic thread */
	t_activity_par activity_2;
	sprintf(activity_2.name,	"Activity_2");
	activity_2.function = Activity;
	activity_2.period = 800;
	activity_2.parameter = 10;
    ret_err = pthread_create( &thread2, NULL, PeriodicTask, (void*) &activity_2);
    handle_error(ret_err, "Error in creating PeriodicTask 2");

    /* Wait till threads are complete before main continues.  */
    pthread_join( thread1, NULL);
	pthread_join( thread2, NULL);
       	   	
    exit(0);
}


