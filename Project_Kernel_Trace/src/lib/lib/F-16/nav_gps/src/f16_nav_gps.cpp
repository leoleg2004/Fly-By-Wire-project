#include <pthread.h>
#include "f16_nav_gps.hpp"
#include "time_library.h"
#include "trace_marker.h"
#include <math.h>

void *PeriodicTask_NAV_GPS(void *ptr) {
  t_f16_nav_gps_par activity = *((t_f16_nav_gps_par *)ptr);

  struct timespec exec_release_time;
  uint64_t exec_next_release_time;
  uint64_t exec_start_time;
  uint64_t exec_end_time;
  uint64_t computational_cost;
  bool skip = false;

  clock_gettime(CLOCK_MONOTONIC, &exec_release_time);

  while (1) {
    char marker[128];
    snprintf(marker, sizeof(marker), "PERIOD_START_%s", activity.name);
    write_trace_marker(marker);

    time_add_millisecs(&exec_release_time, activity.period);
    exec_next_release_time = time_to_millisecs(&exec_release_time);

    if (skip) {
      printf("\n%s:   *SKIP* \n\n\n", activity.name);
      skip = false;
    } else {
      snprintf(marker, sizeof(marker), "FUNCTION_START_%s", activity.name);
      write_trace_marker(marker);

      exec_start_time = time_current_millisecs();
      
      
      // F-16 GPS Kalman Filter
      volatile double pos_x = 0;
      for (long i = 0; i < activity.parameter * 2 * 1000; i++) {
        pos_x += sqrt(i) * cos(i*0.1) + sin(i*0.2);
      }


      exec_end_time = time_current_millisecs();

      snprintf(marker, sizeof(marker), "FUNCTION_END_%s", activity.name);
      write_trace_marker(marker);

      computational_cost = exec_end_time - exec_start_time;

      printf("%s:            exec_start_time         = %ld millisecs\n", activity.name, exec_start_time);
      printf("%s:            exec_end_time           = %ld millisecs\n", activity.name, exec_end_time);
      if (exec_end_time > exec_next_release_time) {
        printf("%s:   -MISS-   cost                    = %ld millisecs\n", activity.name, computational_cost);
        skip = true;
      } else {
        printf("%s:   DO JOB   cost                    = %ld millisecs\n", activity.name, computational_cost);
      }
      printf("%s:            period                  = %d millisecs\n", activity.name, activity.period);
      printf("%s:            exec_next_release_time  = %ld millisecs\n\n", activity.name, exec_next_release_time);

      snprintf(marker, sizeof(marker), "SLEEP_START_%s", activity.name);
      write_trace_marker(marker);
    }

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);

    snprintf(marker, sizeof(marker), "PERIOD_END_%s", activity.name);
    write_trace_marker(marker);
  }
  pthread_exit((void *)ptr);
}
