#ifndef F16_NAV_INS_HPP
#define F16_NAV_INS_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct f16_nav_ins_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
} t_f16_nav_ins_par;

void *PeriodicTask_NAV_INS(void *ptr);
void f16_nav_ins_load(int parameter);

#ifdef __cplusplus
}
#endif
#endif
