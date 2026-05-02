/*
 * f16_library.hpp
 */
#ifndef F16_LIBRARY_HPP
#define F16_LIBRARY_HPP

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
 * data structure for f16 parameters
 */
typedef struct f16_parameters {
  char name[50];
  void (*function)(void *instance, int parameter);
  int period;
  int parameter;
  void *instance;
  long int deadline;
  bool print;
} t_f16_par;

/*
 * periodic task that executes an f16 task
 */
void *PeriodicTask_F16(void *ptr);

/*
 * function that executes long mathematical operations
 */
void f16_load(int cost);

#ifdef __cplusplus
}
#endif

#endif
