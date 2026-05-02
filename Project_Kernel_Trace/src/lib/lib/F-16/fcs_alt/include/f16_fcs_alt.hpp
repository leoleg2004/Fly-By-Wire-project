#ifndef F16_FCS_ALT_HPP
#define F16_FCS_ALT_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct f16_fcs_alt_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
} t_f16_fcs_alt_par;

void *PeriodicTask_FCS_ALT(void *ptr);

#ifdef __cplusplus
}
#endif
#endif
