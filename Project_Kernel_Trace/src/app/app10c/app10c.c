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

#include "time_library.h"
#include "activity_library.h"
#include "trace_marker.h"



void ActivityLight(int parameter) {
    volatile double result = 0;
    for (long i = 0; i < parameter * 1000 * 1000; i++) {
        result += sin(i);
    }
}

void ActivityMedium(int parameter) {
    volatile double result = 0;
    for (long i = 0; i < parameter * 1000 * 1000; i++) {
        result += sin(i) * cos(i);
    }
}

void ActivityHeavy(int parameter) {
    volatile double result = 0;
    for (long i = 0; i < parameter * 1000 * 1000; i++) {
        result += sin(i) + cos(i) + i * tan(i);
    }
}

void ActivityExtreme(int parameter) {
    volatile double result = 0;
    for (long i = 0; i < parameter * 1000 * 1000; i++) {
        result += sqrt(i) * log(i + 1) * sin(i);
    }
}


int main(int argc, char* argv[]) {
    
    init_tracing();
    
    pthread_t threads[8];
    pthread_attr_t attrs[8];
    cpu_set_t cpusets[8];
    t_activity_par activities[8];
    
    int ret_err;

    // ==========================================================
    // --- CONFIGURAZIONE ESPLICITA DEGLI 8 THREAD E DEI CORE ---
    // ==========================================================

    // --- Thread 1: Veloce (Assegnato al Core 0) ---
    pthread_attr_init(&attrs[0]);
    CPU_ZERO(&cpusets[0]);
    CPU_SET(0, &cpusets[0]);
    pthread_attr_setaffinity_np(&attrs[0], sizeof(cpu_set_t), &cpusets[0]);

    activities[0].function = ActivityLight;
    activities[0].period = 200;
    activities[0].parameter = 5;
    sprintf(activities[0].name, "Activity_1");

    // --- Thread 2: Medio (Assegnato al Core 1) ---
    pthread_attr_init(&attrs[1]);
    CPU_ZERO(&cpusets[1]);
    CPU_SET(1, &cpusets[1]);
    pthread_attr_setaffinity_np(&attrs[1], sizeof(cpu_set_t), &cpusets[1]);

    activities[1].function = ActivityMedium;
    activities[1].period = 500;
    activities[1].parameter = 15;
    sprintf(activities[1].name, "Activity_2");

    // --- Thread 3: Pesante (Assegnato al Core 0) ---
    pthread_attr_init(&attrs[2]);
    CPU_ZERO(&cpusets[2]);
    CPU_SET(0, &cpusets[2]);
    pthread_attr_setaffinity_np(&attrs[2], sizeof(cpu_set_t), &cpusets[2]);

    activities[2].function = ActivityHeavy;
    activities[2].period = 400;
    activities[2].parameter = 35;
    sprintf(activities[2].name, "Activity_3");

    // --- Thread 4: Estremo (Assegnato al Core 1) ---
    pthread_attr_init(&attrs[3]);
    CPU_ZERO(&cpusets[3]);
    CPU_SET(1, &cpusets[3]);
    pthread_attr_setaffinity_np(&attrs[3], sizeof(cpu_set_t), &cpusets[3]);

    activities[3].function = ActivityExtreme;
    activities[3].period = 300;
    activities[3].parameter = 50;
    sprintf(activities[3].name, "Activity_4");

    // --- Thread 5: Medio (Assegnato al Core 0) ---
    pthread_attr_init(&attrs[4]);
    CPU_ZERO(&cpusets[4]);
    CPU_SET(0, &cpusets[4]);
    pthread_attr_setaffinity_np(&attrs[4], sizeof(cpu_set_t), &cpusets[4]);

    activities[4].function = ActivityMedium;
    activities[4].period = 600;
    activities[4].parameter = 20;
    sprintf(activities[4].name, "Activity_5");

    // --- Thread 6: Veloce (Assegnato al Core 1) ---
    pthread_attr_init(&attrs[5]);
    CPU_ZERO(&cpusets[5]);
    CPU_SET(1, &cpusets[5]);
    pthread_attr_setaffinity_np(&attrs[5], sizeof(cpu_set_t), &cpusets[5]);

    activities[5].function = ActivityLight;
    activities[5].period = 150;
    activities[5].parameter = 8;
    sprintf(activities[5].name, "Activity_6");

    // --- Thread 7: Pesante (Assegnato al Core 0) ---
    pthread_attr_init(&attrs[6]);
    CPU_ZERO(&cpusets[6]);
    CPU_SET(0, &cpusets[6]);
    pthread_attr_setaffinity_np(&attrs[6], sizeof(cpu_set_t), &cpusets[6]);

    activities[6].function = ActivityHeavy;
    activities[6].period = 1000;
    activities[6].parameter = 40;
    sprintf(activities[6].name, "Activity_7");

    // --- Thread 8: Estremo (Assegnato al Core 1) ---
    pthread_attr_init(&attrs[7]);
    CPU_ZERO(&cpusets[7]);
    CPU_SET(1, &cpusets[7]);
    pthread_attr_setaffinity_np(&attrs[7], sizeof(cpu_set_t), &cpusets[7]);

    activities[7].function = ActivityExtreme;
    activities[7].period = 450;
    activities[7].parameter = 45;
    sprintf(activities[7].name, "Activity_8");


    // ==========================================================
    // APPLICAZIONE RM E AVVIO DEI THREAD
    // ==========================================================
    
    for (int i = 0; i < 8; i++) {
        // 1. Impostazioni per Rate Monotonic (SCHED_FIFO)
        pthread_attr_setinheritsched(&attrs[i], PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attrs[i], SCHED_FIFO);

        // 2. Calcolo Dinamico Priorità RM
        struct sched_param param;
        param.sched_priority = 99 - (activities[i].period / 10);
        if (param.sched_priority < 1) param.sched_priority = 1;
        pthread_attr_setschedparam(&attrs[i], &param);
        
        // 3. Creazione Thread
        ret_err = pthread_create(&threads[i], &attrs[i], PeriodicTask, (void*) &activities[i]);
        if (ret_err != 0) {
            errno = ret_err;
            perror("Errore creazione thread");
            exit(EXIT_FAILURE);
        }
        
        // Assegna il nome al thread a livello di sistema operativo
        pthread_setname_np(threads[i], activities[i].name);
    }

    // --- PULIZIA E ATTESA ---
    for (int i = 0; i < 8; i++) {
        pthread_attr_destroy(&attrs[i]);
    }

    for (int i = 0; i < 8; i++) {
        pthread_join(threads[i], NULL);
    }
    
    close_tracing();             
    
    return 0;
}
