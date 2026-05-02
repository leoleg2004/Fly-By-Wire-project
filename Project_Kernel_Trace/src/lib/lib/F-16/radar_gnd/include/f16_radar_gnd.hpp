#ifndef F16_RADAR_GND_HPP
#define F16_RADAR_GND_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct f16_radar_gnd_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
} t_f16_radar_gnd_par;

void *PeriodicTask_RADAR_GND(void *ptr);

#ifdef __cplusplus
}
#endif
#endif
