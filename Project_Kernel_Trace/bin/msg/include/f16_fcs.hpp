/*
 * f16_fcs.hpp
 */
#ifndef F16_FCS_HPP
#define F16_FCS_HPP

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

typedef struct f16_fcs_parameters {
  char name[50];
  void (*function)(void *instance, int parameter);
  int period;
  int parameter;
  void *instance;
  long int deadline;
  bool print;
} t_f16_fcs_par;

void *PeriodicTask_FCS(void *ptr);

void fcs_load(int cost);

#ifdef __cplusplus
}
#endif

#endif
