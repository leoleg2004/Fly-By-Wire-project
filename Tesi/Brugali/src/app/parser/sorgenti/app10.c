#define _GNU_SOURCE
#include <sched.h>
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
#include "../lib/trace_marker.h"

/*
 * Function that implements the activity
 */
void ActivityIncrement(int parameter) {
    double result = 0;
    for (long i=0; i<parameter*1000*1000; i++) {
        result = result + sin(i) + cos(i) + i *tan(i);
    }
}

void ActivityDecrement(int parameter) {
    double result = 0;
    for (long i=0; i<parameter*1000*1000; i++) {
        result = result + sin(i) * cos(i) * i *tan(i);
    }
}

int main(int argc, char* argv[]) {
    // Inizializza il file descriptor per i marker di tracciamento
    init_tracing();
    
    pthread_t thread1, thread2;
    pthread_attr_t attr1, attr2;
    struct sched_param param1, param2; // Aggiunto per gestire le priorità RM
    int ret_err;
    
    cpu_set_t cpuset1, cpuset2;

    // --- SETUP THREAD 1 (SU CORE 1) ---
    pthread_attr_init(&attr1);
    
    // 1. Impostazioni per Rate Monotonic (SCHED_FIFO)
    pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);

    // 2. Affinità Core
    CPU_ZERO(&cpuset1);
    CPU_SET(1, &cpuset1); 
    pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);
  
    // 3. Parametri Activity
    t_activity_par activity_1;
    sprintf(activity_1.name, "Activity_1");
    activity_1.function = ActivityIncrement;
    activity_1.period = 1800;
    activity_1.parameter = 20;
    // Se la tua libreria lo richiede, scommenta la riga sotto:
    // activity_1.deadline = 1800;

    // 4. Calcolo Priorità RM: Periodo maggiore = Priorità minore
    param1.sched_priority = 99 - (activity_1.period / 10);
    if (param1.sched_priority < 1) param1.sched_priority = 1;
    pthread_attr_setschedparam(&attr1, &param1);
    
    // Creazione thread
    ret_err = pthread_create(&thread1, &attr1, PeriodicTask, (void*) &activity_1);
    handle_error(ret_err, "Error in creating PeriodicTask 1");
    pthread_setname_np(thread1, "Activity_1");
    
    // --- SETUP THREAD 2 (SU CORE 0) ---
    pthread_attr_init(&attr2);

    // 1. Impostazioni per Rate Monotonic (SCHED_FIFO)
    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);

    // 2. Affinità Core
    CPU_ZERO(&cpuset2);
    CPU_SET(0, &cpuset2);
    pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset2);
    
    // 3. Parametri Activity
    t_activity_par activity_2;
    sprintf(activity_2.name, "Activity_2");
    activity_2.function = ActivityDecrement;
    activity_2.period = 800;
    activity_2.parameter = 10;
    // activity_2.deadline = 800;

    // 4. Calcolo Priorità RM: Periodo minore = Priorità maggiore
    param2.sched_priority = 99 - (activity_2.period / 10);
    if (param2.sched_priority < 1) param2.sched_priority = 1;
    pthread_attr_setschedparam(&attr2, &param2);

    // Creazione thread
    ret_err = pthread_create(&thread2, &attr2, PeriodicTask, (void*) &activity_2);
    handle_error(ret_err, "Error in creating PeriodicTask 2");
    pthread_setname_np(thread2, "Activity_2");
    
    // --- PULIZIA ATTRIBUTI E ATTESA ---
    pthread_attr_destroy(&attr1);
    pthread_attr_destroy(&attr2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    close_tracing();             
    
    exit(0);
}
