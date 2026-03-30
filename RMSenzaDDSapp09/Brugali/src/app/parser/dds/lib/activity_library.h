/*
 * activity_library.h
 */
#ifndef ACTIVITY_LIBRARY
#define ACTIVITY_LIBRARY


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
	char name[50];
	void (*function)(void* instance);
	int period;
	void* instance;
	bool print;
} t_activity_par;


/*
 * periodic task that executes an activity
 */
void *PeriodicTask( void *ptr ) ;


/*
 * function that executes long mathematical operations
 */
void activity_load(int cost);
#endif
