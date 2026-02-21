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

    

    if(argc < 2) {

        printf("\nWARNING: missing arguments (sleep_1   sleep_2  ... )\n");

        exit(1);

    }


    /* Initialize and set thread detached attribute */

    pthread_attr_t attributes;

    pthread_attr_init(&attributes);

    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE);


    int NUM_THREADS = argc-1;

    

    /* Create the arguments for NUM_THREADS independent threads  */

    t_arg *arg = malloc(NUM_THREADS * sizeof(t_arg));

    for(unsigned int i=0; i < NUM_THREADS; i++) {

        sprintf(arg[i].message, "Thread_%d", i+1);

        arg[i].sleep = atoi(argv[i+1]);

    }

    


    /* Create NUM_THREADS threads each of which will execute the Task with different messages */

    pthread_t *thread = malloc(NUM_THREADS * sizeof(pthread_t));


    int  ret_err;

    char* error_message;


    for(unsigned int i=0; i < NUM_THREADS; i++) {

        ret_err =  pthread_create( &(thread[i]), &attributes, Task, (void*) &(arg[i]));

        sprintf(error_message, "Thread_%d", i+1);


        handle_error(ret_err, error_message);

    }




     /* Wait till threads are complete before main continues.  */

     /* Unless we wait, we run the risk of executing an exit which will terminate   */

     /* the process and all threads before the threads have completed.   */

    for(unsigned int i=0; i < NUM_THREADS; i++) {

        pthread_join( thread[i], NULL); 

    }

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
