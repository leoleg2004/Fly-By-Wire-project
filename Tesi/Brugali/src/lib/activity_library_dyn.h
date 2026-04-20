/*
 * activity_library.c
 */
#include "activity_library.h"
#include "trace_marker.h"
#include <math.h> // Necessario per sin, cos, tan
#include <stdio.h>

/*
 * periodic task that executes an activity
 */
void *PeriodicTask(void *ptr) {
  t_activity_par activity;
  activity = *((t_activity_par *)ptr); // [ms]

  struct timespec current_time;
  struct timespec exec_release_time;

  uint64_t exec_next_release_time;
  uint64_t exec_start_time;
  uint64_t exec_end_time;
  uint64_t computational_cost;

  bool skip = false;
  char marker[128]; // Buffer riutilizzabile per i marker

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
      if (activity.print)
        printf("\n%s:   *SKIP* \n\n\n", activity.name);
      skip = false;
    } else {

      snprintf(marker, sizeof(marker), "FUNCTION_START_%s", activity.name);
      write_trace_marker(marker);

      // execute the computation
      // ---------------------------------------------------------
      exec_start_time = time_current_millisecs();

      activity.function(activity.parameter); // qua fa il puntatore alla
                                             // funzione che devono eseguire

      exec_end_time = time_current_millisecs();
      // ---------------------------------------------------------

      // MARKER FINE CALCOLO
      snprintf(marker, sizeof(marker), "FUNCTION_END_%s", activity.name);
      write_trace_marker(marker);

      // compute the difference between end_time and start_time
      computational_cost = exec_end_time - exec_start_time;

      // MARKER FINE PERIODO
      snprintf(marker, sizeof(marker), "SLEEP_START_%s", activity.name);
      write_trace_marker(marker);

      // sleep until the end of the period
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);

      // MARKER FINE PERIODO
      snprintf(marker, sizeof(marker), "PERIOD_END_%s", activity.name);
      write_trace_marker(marker);
    }
  } // end while(alive)

  pthread_exit((void *)ptr);
}

/*
 * function that executes long mathematical operations
 */
void activity_load(int cost) {
  double result = 0;
  for (long i = 0; i < cost * 1000 * 1000; i++) {
    result = result + sin(i) + cos(i) + i * tan(i);
  }
}
