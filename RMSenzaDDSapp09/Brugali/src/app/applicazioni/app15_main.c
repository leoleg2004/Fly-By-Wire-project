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

// mutex on the queue of cars waiting at the semaphore
 pthread_mutex_t  mutexOnQueue  = PTHREAD_MUTEX_INITIALIZER;

// mutex on the traffic light
 pthread_mutex_t  mutexOnTrafficLight  = PTHREAD_MUTEX_INITIALIZER;

// condition variable on the traffic light
pthread_cond_t condOnTrafficLight =  PTHREAD_COND_INITIALIZER; 

// shared variables : number of cars waiting at the semaphore along the two directions
int queue_NS = 0;				// north-south direction
int queue_EW = 0;			// east-west direction

int direction_green = 1;		// 1: North-South	-1: East-West
int traffic_light_timer = 0;  // used to count the number of periods the traffic light is green or red

bool ambulance_arriving = false;

void print_queues() {
	if(ambulance_arriving) {
			printf("---  AMBULANCE  ---\n");
	}
	else {
		if(direction_green == 1)
			printf("-----  GO N-S  ----\n");
		else
			printf("-----  GO E-W  ----\n");
	}
	printf("    N-S       E-W ");
	printf("\n     %d         %d\n\n", queue_NS, queue_EW);
}

/*
 * Function that implements the random arrival of cars along the two directions
 * the input parameter "rate" is used to specify the cars arrivale rate
 */
void ActivityCarsArriving(int rate) {
	int r_num = rand() % 100 + 1;	// random number between 1 and 100
	
	pthread_mutex_lock( &mutexOnQueue );
	if(r_num <= 50)	// cars arrive along the NS direction
		queue_NS += rate;
	else						// cars arrive along the EW direction
		queue_EW += rate;
	print_queues();
	pthread_mutex_unlock( &mutexOnQueue );    	
}

/*
 * Function that implements the random arrivale of an ambulance
 * the input parameter "probability" is used to specify the ambulance arrival probability
 */
void ActivityAmbulanceArriving(int probability) {
	pthread_mutex_lock( &mutexOnTrafficLight );
	if(ambulance_arriving) {
		ambulance_arriving = false;
		print_queues();
		pthread_cond_signal(&condOnTrafficLight);//NOTIFY
	}
	else {
		int r_num = rand() % 100 + 1;	// random number between 1 and 100
		if(r_num < probability)	{ // an ambulance is arriving
				ambulance_arriving = true;
				print_queues();
		}
	}
	pthread_mutex_unlock( &mutexOnTrafficLight );    	
}

/*
 * Function that implements the random departure of cars along the two directions
 * the input parameter "duration" is used to specify the interval of time for red and green
 */
void ActivityTrafficLight(int duration) {
	// if an ambulance is arriving the traffic light is red in both directions
	// and cars must wait
	pthread_mutex_lock( &mutexOnTrafficLight );
	if(ambulance_arriving)
		pthread_cond_wait(&condOnTrafficLight, &mutexOnTrafficLight);
	pthread_mutex_unlock( &mutexOnTrafficLight );    	

	if(traffic_light_timer > duration) {
		// switches the direction
		direction_green *= -1;
		// reset the timer
		traffic_light_timer = 0;
	}
	else
		traffic_light_timer++;
	
	// the traffic light is green in the N-S or E-W direction
	pthread_mutex_lock( &mutexOnQueue );
	if(direction_green == 1) {	// greed along the NS direction
		if(queue_NS > 0)  
			queue_NS --;
	}
	else	 {
		if(queue_EW > 0)	// green along the EW direction
		 	queue_EW --;
	}
	print_queues();
	pthread_mutex_unlock( &mutexOnQueue );    	
}




int main(int argc, char* argv[]) {
    pthread_t thread1, thread2, thread3;
    int ret_err;
	
	// Initialization of the random generator. It should only be called once.
	srand(time(NULL));   
	
	/* Create the periodic thread for the Cars Arriving activity*/
	t_activity_par activity_1;
	sprintf(activity_1.name,	"ActivityCarsArriving");
	activity_1.function = ActivityCarsArriving;
	activity_1.period = 600;
	activity_1.parameter = 2;
	activity_1.print = false;
    ret_err = pthread_create( &thread1, NULL, PeriodicTask, (void*) &activity_1);
    handle_error(ret_err, "Error in creating PeriodicTask 1");

	/* Create the periodic thread for the Ambulance Arriving activity*/
	t_activity_par activity_2;
	sprintf(activity_2.name,	"ActivityAmbulanceArriving");
	activity_2.function = ActivityAmbulanceArriving;
	activity_2.period = 3000;
	activity_2.parameter = 50;
	activity_2.print = false;
    ret_err = pthread_create( &thread3, NULL, PeriodicTask, (void*) &activity_2);
    handle_error(ret_err, "Error in creating PeriodicTask 2");

	/* Create the periodic thread for the TrafficLight activity*/
	t_activity_par activity_3;
	sprintf(activity_3.name,	"ActivityTrafficLight ");
	activity_3.function = ActivityTrafficLight;
	activity_3.period = 300;
	activity_3.parameter = 10;
	activity_3.print = false;
    ret_err = pthread_create( &thread2, NULL, PeriodicTask, (void*) &activity_3);
    handle_error(ret_err, "Error in creating PeriodicTask 3");

    /* Wait till threads are complete before main continues.  */
    pthread_join( thread1, NULL);
	pthread_join( thread2, NULL);
	pthread_join( thread3, NULL);
       	   	
    exit(0);
}


