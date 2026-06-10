#ifndef F16_FCS_LAT_HPP
#define F16_FCS_LAT_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct f16_fcs_lat_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
} t_f16_fcs_lat_par;

void *PeriodicTask_FCS_LAT(void *ptr);
void f16_fcs_lat_load(int parameter);

#ifdef __cplusplus
}
#endif
#endif
