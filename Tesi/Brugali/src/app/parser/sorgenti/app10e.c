#define _GNU_SOURCE
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../lib/time_library.h"
#include "../lib/trace_marker.h"

#define handle_error(en, msg)                                                  \
  if (en != 0) {                                                               \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  }

/*
 * data structure for activity parameters
 */
typedef struct activity_parameters {
  char name[15];
  int period;
  int parameter;
  long int deadline;
  void (*function)(int); // Pointer to maintain original logic
} t_activity_par;

/*
 * Function that implements the activity
 * the input parameter "cost" is used to change the computational cost
 */
void ActivityIncrement(int parameter) {
  double result = 0;
  for (long i = 0; i < parameter * 1000 * 1000; i++) {
    result = result + sin(i) + cos(i) + i * tan(i);
  }
}

/*
 * periodic task that executes an activity
 */
void *PeriodicTask(void *ptr) {
  t_activity_par activity;
  activity = *((t_activity_par *)ptr); // [ms]

  struct timespec exec_release_time;

  uint64_t exec_next_release_time;
  uint64_t exec_start_time;
  uint64_t exec_end_time;
  uint64_t computational_cost;

  bool skip = false;

  // Obtain the current time: this is the beginning of the first period
  clock_gettime(CLOCK_MONOTONIC, &exec_release_time);
  uint64_t sim_start_time = time_to_millisecs(&exec_release_time);

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
      if (activity.function != NULL) {
        activity.function(activity.parameter);
      } else {
        ActivityIncrement(activity.parameter);
      }
      exec_end_time = time_current_millisecs();

      snprintf(marker, sizeof(marker), "FUNCTION_END_%s", activity.name);
      write_trace_marker(marker);

      computational_cost = exec_end_time - exec_start_time;

      // === STAMPE A TERMINALE RIPRISTINATE ===
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

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);

    snprintf(marker, sizeof(marker), "PERIOD_END_%s", activity.name);
    write_trace_marker(marker);
  }

  pthread_exit((void *)ptr);
}

int main(int argc, char *argv[]) {
  // Inizializza il file descriptor per i marker di tracciamento
  init_tracing();

  // Setup per un singolo thread
  pthread_t thread1;
  pthread_attr_t attr1;
  struct sched_param param1; // Struttura per la priorità
  int ret_err;

  cpu_set_t cpuset1;

  pthread_attr_init(&attr1);

  // Policy RM (SCHED_FIFO)
  pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);

  // Affinità Core
  CPU_ZERO(&cpuset1);
  CPU_SET(1, &cpuset1);
  pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);

  t_activity_par activity_1;
  sprintf(activity_1.name, "Activity_1");
  activity_1.function = ActivityIncrement;
  activity_1.period = 800;
  activity_1.parameter = 10;
  // Applichiamo la deadline che mi hai richiesto
  activity_1.deadline = 800;

  // Calcolo priorità RM (Periodo 800)
  param1.sched_priority = 99 - (activity_1.period / 10);
  if (param1.sched_priority < 1)
    param1.sched_priority = 1;
  pthread_attr_setschedparam(&attr1, &param1);

  // Creazione thread 1
  ret_err = pthread_create(&thread1, &attr1, PeriodicTask, (void *)&activity_1);
  handle_error(ret_err, "Error in creating PeriodicTask 1");
  pthread_setname_np(thread1, "Activity_1");

  pthread_attr_destroy(&attr1);

  // Attesa completamento
  pthread_join(thread1, NULL);

  // Chiude il file descriptor dei marker
  close_tracing();

  exit(0);
}
