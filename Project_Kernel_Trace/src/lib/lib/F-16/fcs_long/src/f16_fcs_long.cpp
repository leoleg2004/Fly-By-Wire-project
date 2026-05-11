#include <pthread.h>
#include "f16_fcs_long.hpp"
#include "time_library.h"
#include "trace_marker.h"
#include <math.h>

void f16_fcs_long_load(int parameter) {
  // F-16 Pitch Autopilot Logic
  double pitch_error = 0.5;
  double kp = 1.2, ki = 0.05, kd = 0.1;
  volatile double elevator_deflection = 0;
  for (long i = 0; i < parameter * 2 * 1000; i++) {
    elevator_deflection += (kp * pitch_error) + (ki * sin(i)) + (kd * cos(i));
  }
}

void *PeriodicTask_FCS_LONG(void *ptr) {
  t_f16_fcs_long_par activity = *((t_f16_fcs_long_par *)ptr);

  struct timespec exec_release_time;
  uint64_t exec_start_time;
  uint64_t exec_end_time;
  char marker[128];

  clock_gettime(CLOCK_MONOTONIC, &exec_release_time);

  while (1) {
    snprintf(marker, sizeof(marker), "PERIOD_START_%s", activity.name);
    write_trace_marker(marker);

    time_add_millisecs(&exec_release_time, activity.period);

    snprintf(marker, sizeof(marker), "FUNCTION_START_%s", activity.name);
    write_trace_marker(marker);

    exec_start_time = time_current_millisecs();
    
    f16_fcs_long_load(activity.parameter);

    exec_end_time = time_current_millisecs();

    snprintf(marker, sizeof(marker), "FUNCTION_END_%s", activity.name);
    write_trace_marker(marker);

    snprintf(marker, sizeof(marker), "SLEEP_START_%s", activity.name);
    write_trace_marker(marker);

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);

    snprintf(marker, sizeof(marker), "PERIOD_END_%s", activity.name);
    write_trace_marker(marker);
  }
  pthread_exit((void *)ptr);
}
