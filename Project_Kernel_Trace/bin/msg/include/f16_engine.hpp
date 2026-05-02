/*
 * f16_engine.hpp
 */
#ifndef F16_ENGINE_HPP
#define F16_ENGINE_HPP

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

typedef struct f16_engine_parameters {
  char name[50];
  void (*function)(void *instance, int parameter);
  int period;
  int parameter;
  void *instance;
  long int deadline;
  bool print;
} t_f16_engine_par;

void *PeriodicTask_ENGINE(void *ptr);

void engine_load(int cost);

#ifdef __cplusplus
}
#endif

#endif
