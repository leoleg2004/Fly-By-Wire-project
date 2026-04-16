/*
 * activity.c
 */
#include "activity_library.h"

#include <stdio.h>
#include <string.h>

#include "/home/leonardo/eprosima_projects/flight_sensor/flight_sensor/Tesi/Brugali/src/lib/trace_marker.h"


void *PeriodicTask(void *ptr) {
  t_activity_par activity;
  activity = *((t_activity_par *)ptr); // [ms]

  // Renaming thread explicitly for ftrace
  pthread_setname_np(pthread_self(), activity.name);

  struct timespec exec_release_time;

  uint64_t exec_next_release_time;
  uint64_t exec_start_time;
  uint64_t exec_end_time;
  uint64_t computational_cost;

  bool skip = false;
  char marker[128];

  // Obtain the current time: this is the beginning of the first period
  clock_gettime(CLOCK_MONOTONIC, &exec_release_time);

  /*
   * periodic loop
   */
  while (1) {
    snprintf(marker, sizeof(marker), "PERIOD_START_%s", activity.name);
    write_trace_marker(marker);

    // Calculate the time for the next periodic activation: period (millisecs)
    time_add_millisecs(&exec_release_time, activity.period);

    // get the next activation time in microseconds
    exec_next_release_time = time_to_millisecs(&exec_release_time);

    if (skip) {
      printf("\n%s:   *SKIP*   \n\n\n", activity.name);
      skip = false;
    } else {
      // execute the computation
      // ---------------------------------------------------------
      snprintf(marker, sizeof(marker), "FUNCTION_START_%s", activity.name);
      write_trace_marker(marker);

      exec_start_time = time_current_millisecs();
      activity.function(activity.instance, activity.parameter);
      exec_end_time = time_current_millisecs();

      snprintf(marker, sizeof(marker), "FUNCTION_END_%s", activity.name);
      write_trace_marker(marker);
      // ---------------------------------------------------------

      // compute the difference between end_time and start_time
      computational_cost = exec_end_time - exec_start_time;

      // === STAMPE A TERMINALE (identiche ad app10h) ===
      printf("%s:            exec_start_time         = %ld millisecs\n",
             activity.name, exec_start_time);
      printf("%s:            exec_end_time           = %ld millisecs\n",
             activity.name, exec_end_time);

      if (exec_end_time > exec_next_release_time) {
        printf("%s:   -MISS-   cost                    = %ld millisecs\n",
               activity.name, computational_cost);
        skip = true;
      } else {
        printf("%s:   DO JOB   cost                    = %ld millisecs\n",
               activity.name, computational_cost);
      }
      printf("%s:            period                  = %d millisecs\n",
             activity.name, activity.period);
      printf("%s:            exec_next_release_time  = %ld millisecs\n\n",
             activity.name, exec_next_release_time);

      snprintf(marker, sizeof(marker), "SLEEP_START_%s", activity.name);
      write_trace_marker(marker);
    }

    // sleep until the end of the period
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);

    snprintf(marker, sizeof(marker), "PERIOD_END_%s", activity.name);
    write_trace_marker(marker);
  } // end while(alive)

  pthread_exit((void *)ptr);
}

/*
 * function that executes long mathematical operations
 */
void activity_load(int cost) {
  volatile double result = 0;
  for (long i = 0; i < cost * 1000 * 1000; i++) {
    result = result + sin(i) + cos(i) + i * tan(i);
  }
}
