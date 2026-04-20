#ifndef ACTIVITY_LIBRARY_DYN_H
#define ACTIVITY_LIBRARY_DYN_H

#include <stdbool.h>

/*
 * Data structure for dynamic activity parameters
 */
typedef struct activity_parameters {
  char name[15];
  int period;
  int alternate_period;  
  int parameter;
  long int deadline;
  long int alternate_deadline; 
  void (*function)(int); 
} t_activity_par;

// Firme delle funzioni
void *PeriodicTaskDyn(void *ptr);
void ActivityIncrementDyn(int parameter);

#endif
