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

#define NUM_THREADS 8

void print_the_table();

/*
 * shared variables
 */
bool philosopher[NUM_THREADS];		// for each philosopher	true: eating          false: thinking
bool fork[NUM_THREADS];			 		// for each fork               	true: available   	false: taken
int lunch[NUM_THREADS];					// the number of times each philosopher has eaten

/*
 * init a mutex
 */
pthread_mutex_t  mutex_table  = PTHREAD_MUTEX_INITIALIZER;

/*
 * Function that implements the activity
 * the input parameter "cost" is used to change the computational cost
 */
void Philosopher(int id) {
	int left, right;  // forks position
	bool left_available;
	bool right_available;

	// Select the forks based on the thread ID
    if(id == NUM_THREADS-1) {
		left = id;
		right = 0;
    }
    else {
		left = id;
		right = id+1;    
    }

	if(philosopher[id]  == false) {
		/* 
		 *  If the philosopher was sleeping, he tries to get both forks at once
	     */
		// try to get the lock on the table
		pthread_mutex_lock( &mutex_table );   
    
		// check if both forks are available
		if(fork[left]== true && fork[right]== true) {
			fork[left] = false;
			fork[right] = false;
			// The philosopher can eat
			philosopher[id] = true;
			lunch[id]++;
		}
		else {
			// The philosopher cannot eat
			philosopher[id] = false;
		}
	
		left_available = fork[left];
		right_available = fork[right];
		print_the_table();
	
		// release the lock on the table
		pthread_mutex_unlock( &mutex_table );    
	 }
	else {
		/*
		* The philospher has eaten and now he sleeps
		*/
		// The philosopher releases both forks
    	pthread_mutex_lock( &mutex_table );   
       	fork[left] = true;
       	fork[right] = true;

        // The philosopher think
        philosopher[id] = false;       
    	pthread_mutex_unlock( &mutex_table );   
	}		
}




int main(int argc, char* argv[]) {
    pthread_t thread[NUM_THREADS];
	t_activity_par activity[NUM_THREADS];
    int ret_err;

	// init the table
    for(unsigned int i=0; i < NUM_THREADS; i++) {
		fork[i] = true;					// all the forks are available
		philosopher[i] = false;		// all the philosophers are thinking
	}

	for(unsigned int i=0; i < NUM_THREADS; i++) {
		sprintf(activity[i].name,	"Philosopher_%d", i+1);
		activity[i].function = Philosopher;
		activity[i].period = 1000 + 200*i;
		activity[i].parameter = i;
		activity[i].print = false;
		ret_err = pthread_create( &thread[i], NULL, PeriodicTask, (void*) &activity[i]);
		handle_error(ret_err, "Error in creating PeriodicTask 1");
	}

    /* Wait till threads are complete before main continues.  */
    for(unsigned int i=0; i < NUM_THREADS; i++)
		pthread_join( thread[i], NULL);
	
    exit(0);
}


void print_the_table() {
    int count = 0;
    printf("\n");
    for(int i=0; i < NUM_THREADS; i++) {
        if(philosopher[i]==true) {
            printf("v   ");
            count++;
        }
        else
            printf("-   ");
    }
    printf("     %d\n", count);
    
    for(int i=0; i < NUM_THREADS; i++) {
        if(lunch[i] > 9)
            printf("%d  ", lunch[i]);
        else
            printf("%d   ", lunch[i]);        
    }
    printf("\n\n");
}
