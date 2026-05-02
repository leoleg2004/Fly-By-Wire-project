#ifndef F16_NAV_GPS_HPP
#define F16_NAV_GPS_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct f16_nav_gps_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
} t_f16_nav_gps_par;

void *PeriodicTask_NAV_GPS(void *ptr);

#ifdef __cplusplus
}
#endif
#endif
