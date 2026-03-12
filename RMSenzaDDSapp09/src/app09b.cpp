/*
 * Main program
 */
#define _GNU_SOURCE // Fondamentale per le funzioni di CPU affinity
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <sched.h> // Aggiunto per le policy di scheduling (SCHED_FIFO)

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
    activity = *((t_activity_par *) ptr);    // [ms]

    struct timespec current_time;
    struct timespec exec_release_time;

    uint64_t exec_next_release_time;
    uint64_t exec_start_time;
    uint64_t exec_end_time;
    uint64_t computational_cost;

    bool skip = false;

    // Obtain the current time: this is the beginning of the first period
    clock_gettime(CLOCK_MONOTONIC, &exec_release_time);

    while(1) {
        time_add_millisecs(&exec_release_time, activity.period);

        exec_next_release_time  = time_to_millisecs(&exec_release_time);

        exec_start_time = time_current_millisecs();
        if( ! skip) {
            Activity(activity.parameter);
        }
        exec_end_time = time_current_millisecs();

        computational_cost = exec_end_time - exec_start_time;

        printf("%s:            exec_start_time         = %ld millisecs\n", activity.name, exec_start_time);
        printf("%s:            exec_end_time           = %ld millisecs\n", activity.name, exec_end_time);
        if(skip) {
            printf("%s:   *SKIP* cost                    =    %ld millisecs\n", activity.name, computational_cost);
            skip = false;
        }
        else {
            if(exec_end_time > exec_next_release_time) {
                printf("%s:   -MISS-   cost                    = %ld millisecs\n", activity.name, computational_cost);
                skip = true;
            }
            else
                printf("%s:   DO JOB   cost                    = %ld millisecs\n", activity.name, computational_cost);
        }
        printf("%s:            period                  = %d millisecs\n", activity.name, activity.period);
        printf("%s:            exec_next_release_time  = %ld millisecs\n\n", activity.name, exec_next_release_time);

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);

    }

    pthread_exit((void*) ptr);
}

int main(int argc, char* argv[]) {
    pthread_t thread1, thread2;
    int ret_err;

    //Dichiaro gli attributi, i parametri di scheduling e il set di CPU
    pthread_attr_t attr1, attr2;
    struct sched_param param1, param2;

    //serve per istanziare i set di core su cui devono girare i thread
    // CREO DUE CPU_SET DISTINTI PER ASSEGNARE CORE DIVERSI
    cpu_set_t cpuset1, cpuset2;

    // Inizializzo gli attributi
    pthread_attr_init(&attr1);
    pthread_attr_init(&attr2);

    // Stacco la policy di scheduling da quella del processo padre (main)
    pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);

    // Imposto la policy Real-Time FIFO (necessaria per Rate Monotonic) su entrambi i thread
    pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);
    pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);

    // Configuro il primo CPU set per bloccare il thread 1 sul CORE 0
    CPU_ZERO(&cpuset1);
    CPU_SET(0, &cpuset1);
    pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);

    // Configuro il secondo CPU set per bloccare il thread 2 sul CORE 1
    CPU_ZERO(&cpuset2);
    CPU_SET(1, &cpuset2); // 1 indica il secondo core
    pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset2);

    /* Create the first periodic thread */
    t_activity_par activity_1;
    sprintf(activity_1.name,    "Activity_1");
    activity_1.period = 1800;
    activity_1.parameter = 20;

    // Imposto la priorità (Periodo 1800ms a Priorità più bassa)
    param1.sched_priority = 99 - (activity_1.period/10);
    if (param1.sched_priority < 1) param1.sched_priority = 1;
    pthread_attr_setschedparam(&attr1, &param1);

    // Passo attr1 invece di NULL qua modifico gli attributi quindi non vengono messi quelli di default ma quelli modificati per la schedulazione RM
    ret_err = pthread_create( &thread1, &attr1, PeriodicTask, (void*) &activity_1);
    // Dopo pthread_create di thread1 cosi su kernel linux vedo nei task Activity1
        pthread_setname_np(thread1, "Activity_1");


    handle_error(ret_err, "Error in creating PeriodicTask 1");

    /* Create the second periodic thread */
    t_activity_par activity_2;
    sprintf(activity_2.name,    "Activity_2");
    activity_2.period = 800;
    activity_2.parameter = 10;

    // Imposto la priorità (Periodo 800ms -> Priorità più alta)
    param2.sched_priority = 99 - (activity_2.period / 10);
    if (param2.sched_priority < 1) param2.sched_priority = 1;

    //passo i miei attributi &attr2 personalizzati secondo la schedulazione RM
    pthread_attr_setschedparam(&attr2, &param2);

    // Passo attr2 invece di NULL
    ret_err = pthread_create( &thread2, &attr2, PeriodicTask, (void*) &activity_2);

    // Dopo pthread_create di thread2 cosi su kernel linux vedo nei task Activity2
         pthread_setname_np(thread2, "Activity_2");
    handle_error(ret_err, "Error in creating PeriodicTask 2");

    // Pulizia degli attributi
    pthread_attr_destroy(&attr1);
    pthread_attr_destroy(&attr2);

    /* Wait till threads are complete before main continues.  */
    pthread_join( thread1, NULL);
    pthread_join( thread2, NULL);

    printf("\nPeriodic Tasks completed correctly\n");

    exit(0);
}
