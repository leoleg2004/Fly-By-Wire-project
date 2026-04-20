#define _GNU_SOURCE
#include "activity_library_dyn.h"
#include "trace_marker.h"
#include "time_library.h"

#include <math.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

/*
 * Function that implements the activity
 */
void ActivityIncrement(int parameter) {
  double result = 0;
  for (long i = 0; i < parameter * 1000 * 1000; i++) {
    result = result + sin(i) + cos(i) + i * tan(i);
  }
}

/*
 * Periodic task that executes an activity with dynamic inversion
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
  int execution_count = 0;

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
      
      // Esecuzione del carico
      if (activity.function != NULL) {
        activity.function(activity.parameter);
      } else {
        ActivityIncrement(activity.parameter);
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
        execution_count++;
      }
      
      printf("%s:            period                  = %d millisecs\n", activity.name, activity.period);
      printf("%s:            exec_next_release_time  = %ld millisecs\n\n", activity.name, exec_next_release_time);

      // ========================================================
      // LOGICA DI INVERSIONE A 5 ESECUZIONI
      // ========================================================
      if (execution_count == 5) {
          printf("\n==========================================================\n");
          printf(">> [%s] 5 ESECUZIONI RAGGIUNTE INVERSIONE IN CORSO...\n", activity.name);
          
          snprintf(marker, sizeof(marker), "INVERSION_START_%s", activity.name);
          write_trace_marker(marker);
          
          activity.period = activity.alternate_period;
          activity.deadline = activity.alternate_deadline; 
          
          struct sched_param sp;
          sp.sched_priority = 99 - (activity.period / 10);
          if (sp.sched_priority < 1) sp.sched_priority = 1;
          if (sp.sched_priority > 99) sp.sched_priority = 99;

          if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
              perror("Errore nell'aggiornamento della priorita");
          } else {
              printf(">> [%s] Nuovo Periodo: %d ms | Nuova Deadline: %ld ms | Nuova Priorita: %d\n", 
                     activity.name, activity.period, activity.deadline, sp.sched_priority);
              
              snprintf(marker, sizeof(marker), "INVERSION_DONE_%s_PRIO_%d_PER_%d_DEAD_%ld", 
                       activity.name, sp.sched_priority, activity.period, activity.deadline);
              write_trace_marker(marker);
          }
          printf("==========================================================\n\n");
          
          execution_count++; // Incrementiamo per non ripetere l'inversione
      }

      snprintf(marker, sizeof(marker), "SLEEP_START_%s", activity.name);
      write_trace_marker(marker);
    }

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);

    snprintf(marker, sizeof(marker), "PERIOD_END_%s", activity.name);
    write_trace_marker(marker);
  }

  pthread_exit((void *)ptr);
}
