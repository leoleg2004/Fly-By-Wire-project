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

#include "../../lib/time_library.h"
#include "../../lib/activity_library.h"
// Manteniamo questa libreria solo per init_tracing() e close_tracing()
#include "../../lib/trace_marker.h"

/*
 * Function that implements the activity
 * the input parameter "cost" is used to change the computational cost
 */
void ActivityIncrement(int parameter) {
    volatile double result = 0;
    for (long i=0; i<parameter*1000*1000; i++) {
        result = result + sin(i) + cos(i) + i *tan(i);
    }
}

void ActivityDecrement(int parameter) {
    volatile double result = 0;
    for (long i=0; i<parameter*1000*1000; i++) {
        result = result + sin(i) * cos(i) * i *tan(i);
    }
}

// Nuova funzione per la terza activity
void ActivityMixed(int parameter) {
    volatile double result = 0;
    for (long i=0; i<parameter*1000*1000; i++) {
        result = result + sqrt(i) * sin(i);
    }
}

int main(int argc, char* argv[]) {
    // Inizializza il file descriptor per i marker di tracciamento
    init_tracing();
    
    // Aggiunto thread3
    pthread_t thread1, thread2, thread3;
    pthread_attr_t attr1, attr2, attr3;
    struct sched_param param1, param2, param3; // Strutture per le priorità
    int ret_err;
    
    cpu_set_t cpuset1, cpuset2, cpuset3;

    // ==========================================
    // --- SETUP THREAD 1 (SU CORE 1) ---
    // ==========================================
    pthread_attr_init(&attr1);
    
    // Policy RM (SCHED_FIFO)
    pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);

    // Affinità Core
    CPU_ZERO(&cpuset1);
    CPU_SET(1, &cpuset1); 
    pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);
  
    t_activity_par activity_1;
    sprintf(activity_1.name, "Activity_1");
    activity_1.function = ActivityIncrement;
    activity_1.period = 800;
    activity_1.parameter = 20;
    // Se la tua struct ha il campo deadline, scommenta:
    // activity_1.deadline = 800;
    
    // Calcolo priorità RM (Periodo 800)
    param1.sched_priority = 99 - (activity_1.period / 10);
    if (param1.sched_priority < 1) param1.sched_priority = 1;
    pthread_attr_setschedparam(&attr1, &param1);

    // Creazione thread 1
    ret_err = pthread_create(&thread1, &attr1, PeriodicTask, (void*) &activity_1);
    handle_error(ret_err, "Error in creating PeriodicTask 1");
    pthread_setname_np(thread1, "Activity_1");

#ifdef OLD
    // ==========================================
    // --- SETUP THREAD 2 (SU CORE 0) ---
    // ==========================================
    pthread_attr_init(&attr2);

    // Policy RM (SCHED_FIFO)
    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);

    // Affinità Core
    CPU_ZERO(&cpuset2);
    CPU_SET(0, &cpuset2);
    pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset2);
    
    t_activity_par activity_2;
    sprintf(activity_2.name, "Activity_2");
    activity_2.function = ActivityDecrement;
    activity_2.period = 800;
    activity_2.parameter = 10;
    // activity_2.deadline = 800;

    // Calcolo priorità RM (Periodo 800)
    param2.sched_priority = 99 - (activity_2.period / 10);
    if (param2.sched_priority < 1) param2.sched_priority = 1;
    pthread_attr_setschedparam(&attr2, &param2);

    // Creazione thread 2
    ret_err = pthread_create(&thread2, &attr2, PeriodicTask, (void*) &activity_2);
    handle_error(ret_err, "Error in creating PeriodicTask 2");
    pthread_setname_np(thread2, "Activity_2");

    // ==========================================
    // --- SETUP THREAD 3 (SU CORE 0) ---
    // ==========================================
    pthread_attr_init(&attr3);

    // Policy RM (SCHED_FIFO)
    pthread_attr_setinheritsched(&attr3, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr3, SCHED_FIFO);

    // Affinità Core
    CPU_ZERO(&cpuset3);
    CPU_SET(0, &cpuset3);
    pthread_attr_setaffinity_np(&attr3, sizeof(cpu_set_t), &cpuset3);
    
    t_activity_par activity_3;
    sprintf(activity_3.name, "Activity_3");
    activity_3.function = ActivityMixed;
    activity_3.period = 500;
    activity_3.parameter = 15;
    // activity_3.deadline = 500;

    // Calcolo priorità RM (Periodo 500 -> Priorità più ALTA del Thread 2)
    param3.sched_priority = 99 - (activity_3.period / 10);
    if (param3.sched_priority < 1) param3.sched_priority = 1;
    pthread_attr_setschedparam(&attr3, &param3);

    // Creazione thread 3
    ret_err = pthread_create(&thread3, &attr3, PeriodicTask, (void*) &activity_3);
    handle_error(ret_err, "Error in creating PeriodicTask 3");
    pthread_setname_np(thread3, "Activity_3");
#endif
    // ==========================================
    // --- PULIZIA ATTRIBUTI E ATTESA ---
    // ==========================================
    pthread_attr_destroy(&attr1);
 //   pthread_attr_destroy(&attr2);
 //   pthread_attr_destroy(&attr3);

    // Attesa completamento
    pthread_join(thread1, NULL);
   // pthread_join(thread2, NULL);
   // pthread_join(thread3, NULL);
    
    // Chiude il file descriptor dei marker
    close_tracing();             
    
    exit(0);
}
