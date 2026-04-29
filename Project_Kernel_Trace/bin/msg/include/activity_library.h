/*
 * activity_library.h
 */
#ifndef ACTIVITY_LIBRARY
#define ACTIVITY_LIBRARY

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "time_library.h"

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
  void (*function)(void *instance, int parameter);
  int period;
  int parameter;
  void *instance;
  long int deadline;
  bool print;
} t_activity_par;

/*
 * periodic task that executes an activity a
 */
void *PeriodicTask(void *ptr);

/*
 * function that executes long mathematical operationss
 */
void activity_load(int cost);

#ifdef __cplusplus
}
#endif

#endif
