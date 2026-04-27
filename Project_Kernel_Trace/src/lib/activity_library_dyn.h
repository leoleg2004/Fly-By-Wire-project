#ifndef ACTIVITY_LIBRARY_DYN_H
#define ACTIVITY_LIBRARY_DYN_H

#include <stdbool.h>

/* Struttura dati per parametri attività dinamici */
typedef struct activity_parameters {
    char name[15];
    int period;
    int alternate_period;  //periodo nuovo a run time
    int parameter;
    long int deadline;
    long int alternate_deadline; //deadline nuova a run-time
    int alternate_core;    //core sui si vuole migrare
    void (*function)(int); 
} t_activity_par;

void *PeriodicTaskDyn(void *ptr);
void ActivityIncrementDyn(int parameter);

#endif
