/*
 * activity_library.h
 */
#ifndef ACTIVITY_LIBRARY
#define ACTIVITY_LIBRARY

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../lib/time_library.h"

#define handle_error(en, msg)                                                  \
  if (en != 0) {                                                               \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  }

/*
 * data structure for activity parameters
 */
typedef struct activity_parameters {
  char name[50];
  void (*function)(int cost);
  int period;
  int parameter;
  long int deadline; // Aggiunto per tolleranza parser
  bool print;
} t_activity_par;

/*
 * periodic task that executes an activity
 */
void *PeriodicTask(void *ptr);

/*
 * function that executes long mathematical operations
 */
void activity_load(int cost);
#endif
