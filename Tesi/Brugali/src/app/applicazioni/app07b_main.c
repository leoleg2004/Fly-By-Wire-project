/*
 * Main program
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#define handle_error(en, msg) \
        if(en != 0) {errno = en; perror(msg); exit(EXIT_FAILURE);}

// data structure for thread argument
typedef struct arg {
	char message[10];
	int sleep;
} t_arg;


void *Task( void *ptr );


int sleep_millisec(long msec);

int main(int argc, char* argv[]) {
	
	/* Initialize and set thread detached attribute */
	pthread_attr_t attributes;
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE);

	
    // Create two independent threads each of which will execute the Task function with different messages
	// and different periods

	pthread_t thread1;
	t_arg arg1;
	sprintf(arg1.message,	"Thread_1");
	arg1.sleep  = 500;
	
	pthread_t thread2;
	t_arg arg2;
	sprintf(arg2.message,	"Thread_2");
	arg2.sleep  = 1500;


    /* Create two independent threads each of which will execute the Task with different messages */
     int  ret_err;
     ret_err = pthread_create( &thread1, &attributes, Task, (void*) &arg1);
     handle_error(ret_err, "Error in creating Task 1");

     ret_err = pthread_create( &thread2, &attributes, Task, (void*) &arg2);
     handle_error(ret_err, "Error in creating Task 2");


     /* Wait till threads are complete before main continues.  */
     /* Unless we wait, we run the risk of executing an exit which will terminate   */
     /* the process and all threads before the threads have completed.   */
     pthread_join( thread1, NULL);
     pthread_join( thread2, NULL); 

     printf("\nTasks completed correctly\n");
     	   	
     exit(0);
}

void *Task( void *ptr ) {
     t_arg *arg;
     arg = (t_arg *) ptr;
     
     for(int i=0; i < 10; i++) {
     	printf("%s %d\n", arg->message, i+1);
	sleep_millisec(arg->sleep);
     }
     
     pthread_exit((void*) ptr);
}


/* sleep_millisec(): Sleep for the requested number of milliseconds. */
int sleep_millisec(long msec) {
    struct timespec ts;
    int res;

    if (msec < 0)  {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

/*
	nanosleep(duration, rem) suspends the execution of the calling 
	thread until either at least the time specified in *duration has elapsed, 
	or the delivery of a signal that triggers the invocation of a
    handler in the calling thread or that terminates the process.

    If the call is interrupted by a signal handler, nanosleep()
    returns -1, sets errno to EINTR, and writes the remaining time
    into the structure pointed to by rem unless rem is NULL.  The
    value of *rem can then be used to call nanosleep() again and
    complete the specified pause.
*/
    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}
