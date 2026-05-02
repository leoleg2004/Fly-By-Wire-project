#ifndef F16_ENGINE_FADEC_HPP
#define F16_ENGINE_FADEC_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct f16_engine_fadec_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
} t_f16_engine_fadec_par;

void *PeriodicTask_ENGINE_FADEC(void *ptr);

#ifdef __cplusplus
}
#endif
#endif
