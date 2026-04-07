
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

// --- FUNZIONI DI CARICO COMPUTAZIONALE ---

void ActivityLight(int parameter) {
    double result = 0;
    for (long i = 0; i < parameter * 1000 * 1000; i++) {
        result += sin(i);
    }
}

void ActivityMedium(int parameter) {
    double result = 0;
    for (long i = 0; i < parameter * 1000 * 1000; i++) {
        result += sin(i) * cos(i);
    }
}

void ActivityHeavy(int parameter) {
    double result = 0;
    for (long i = 0; i < parameter * 1000 * 1000; i++) {
        result += sin(i) + cos(i) + i * tan(i);
    }
}

void ActivityExtreme(int parameter) {
    double result = 0;
    for (long i = 0; i < parameter * 1000 * 1000; i++) {
        result += sqrt(i) * log(i + 1) * sin(i);
    }
}

int main(int argc, char* argv[]) {
    
    init_tracing();
    
    // Array per gestire agevolmente gli 8 thread
    pthread_t threads[8];
    pthread_attr_t attrs[8];
    cpu_set_t cpusets[8];
    t_activity_par activities[8];
    
    int ret_err;

    // --- CONFIGURAZIONE DEGLI 8 THREAD ---

    // Thread 1: Core 0, Veloce
    activities[0].function = ActivityLight;
    activities[0].period = 200;
    activities[0].parameter = 5;
    sprintf(activities[0].name, "Activity_1");

    // Thread 2: Core 1, Medio
    activities[1].function = ActivityMedium;
    activities[1].period = 500;
    activities[1].parameter = 15;
    sprintf(activities[1].name, "Activity_2");

    // Thread 3: Core 0, Pesante
    activities[2].function = ActivityHeavy;
    activities[2].period = 400;
    activities[2].parameter = 35;
    sprintf(activities[2].name, "Activity_3");

    // Thread 4: Core 1, Estremo (Sicuro Miss)
    activities[3].function = ActivityExtreme;
    activities[3].period = 300;
    activities[3].parameter = 50;
    sprintf(activities[3].name, "Activity_4");

    // Thread 5: Core 0, Medio
    activities[4].function = ActivityMedium;
    activities[4].period = 600;
    activities[4].parameter = 20;
    sprintf(activities[4].name, "Activity_5");

    // Thread 6: Core 1, Veloce
    activities[5].function = ActivityLight;
    activities[5].period = 150;
    activities[5].parameter = 8;
    sprintf(activities[5].name, "Activity_6");

    // Thread 7: Core 0, Pesante
    activities[6].function = ActivityHeavy;
    activities[6].period = 1000;
    activities[6].parameter = 40;
    sprintf(activities[6].name, "Activity_7");

    // Thread 8: Core 1, Estremo
    activities[7].function = ActivityExtreme;
    activities[7].period = 800;
    activities[7].parameter = 45;
    sprintf(activities[7].name, "Activity_8");


    // --- ASSEGNAZIONE CORE E CREAZIONE (CON SCHEDULING RM) ---
    
    for (int i = 0; i < 8; i++) {
        pthread_attr_init(&attrs[i]);

        // 1. Impostazioni per Rate Monotonic (SCHED_FIFO)
        pthread_attr_setinheritsched(&attrs[i], PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attrs[i], SCHED_FIFO);

        // 2. Calcolo Dinamico Priorità RM
        struct sched_param param;
        param.sched_priority = 99 - (activities[i].period / 10);
        if (param.sched_priority < 1) param.sched_priority = 1;
        pthread_attr_setschedparam(&attrs[i], &param);

        // (Opzionale) Se la tua struct in time_library.h richiede anche la deadline esplicita:
        // activities[i].deadline = activities[i].period;

        // 3. Impostazione CPU Affinity
        CPU_ZERO(&cpusets[i]);
        // I thread pari vanno sul Core 0, i dispari sul Core 1
        if (i % 2 == 0) {
            CPU_SET(0, &cpusets[i]);
        } else {
            CPU_SET(1, &cpusets[i]);
        }
        pthread_attr_setaffinity_np(&attrs[i], sizeof(cpu_set_t), &cpusets[i]);
        
        // 4. Creazione Thread
        ret_err = pthread_create(&threads[i], &attrs[i], PeriodicTask, (void*) &activities[i]);
        // Se non hai la macro handle_error attiva, usa un if standard:
        if (ret_err != 0) {
            errno = ret_err;
            perror("Errore creazione thread");
            exit(EXIT_FAILURE);
        }
        
        // Assegna il nome al thread a livello di sistema operativo
        pthread_setname_np(threads[i], activities[i].name);
    }


    
    for (int i = 0; i < 8; i++) {
        pthread_attr_destroy(&attrs[i]);
    }

    for (int i = 0; i < 8; i++) {
        pthread_join(threads[i], NULL);
    }
    
    close_tracing();             
    
    return 0;
}
