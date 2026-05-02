#ifndef F16_DISPLAYS_HPP
#define F16_DISPLAYS_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct f16_displays_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
} t_f16_displays_par;

void *PeriodicTask_DISPLAYS(void *ptr);

#ifdef __cplusplus
}
#endif
#endif
