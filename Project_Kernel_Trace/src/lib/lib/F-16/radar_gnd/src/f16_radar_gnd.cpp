#include <pthread.h>
#include "f16_radar_gnd.hpp"
#include "time_library.h"
#include "trace_marker.h"
#include <math.h>

void f16_radar_gnd_load(int parameter) {
  // F-16 Ground Radar Mapping
  volatile double ground_elevation = 0;
  for (long i = 0; i < parameter * 2 * 1000; i++) {
    ground_elevation += log(i + 1) * cos(i);
  }
}

void *PeriodicTask_RADAR_GND(void *ptr) {
  t_f16_radar_gnd_par activity = *((t_f16_radar_gnd_par *)ptr);

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
    
    f16_radar_gnd_load(activity.parameter);

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
