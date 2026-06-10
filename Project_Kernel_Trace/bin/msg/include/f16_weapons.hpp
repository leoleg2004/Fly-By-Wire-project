#ifndef F16_WEAPONS_HPP
#define F16_WEAPONS_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct f16_weapons_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
} t_f16_weapons_par;

void *PeriodicTask_WEAPONS(void *ptr);
void f16_weapons_load(int parameter);

#ifdef __cplusplus
}
#endif
#endif
