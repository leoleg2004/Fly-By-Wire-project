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
void ActivityIncrementDyn(int parameter) {
  volatile volatile double result = 0;
  for (long i = 0; i < parameter * 1000 * 1000; i++) {
    result = result + sin(i) + cos(i) + i * tan(i);
  }
}

/*
 * Periodic task that executes an activity with dynamic inversion
 */
void *PeriodicTaskDyn(void *ptr) {
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
      printf("\n%s:    *SKIP* \n\n\n", activity.name);
      skip = false;
    } else {
      snprintf(marker, sizeof(marker), "FUNCTION_START_%s", activity.name);
      write_trace_marker(marker);

      exec_start_time = time_current_millisecs();
      
      // Esecuzione del carico
      if (activity.function != NULL) {
        activity.function(activity.parameter);
      } else {
        ActivityIncrementDyn(activity.parameter); // <--- Chiamata con Dyn
      }
      
      exec_end_time = time_current_millisecs();

      snprintf(marker, sizeof(marker), "FUNCTION_END_%s", activity.name);
      write_trace_marker(marker);

      computational_cost = exec_end_time - exec_start_time;

      printf("%s:             exec_start_time         = %ld millisecs\n", activity.name, exec_start_time);
      printf("%s:             exec_end_time           = %ld millisecs\n", activity.name, exec_end_time);
             
      if (exec_end_time > exec_next_release_time) {
        printf("%s:   -MISS-   cost                    = %ld millisecs\n", activity.name, computational_cost);
        skip = true;
      } else {
        printf("%s:   DO JOB   cost                    = %ld millisecs\n", activity.name, computational_cost);
        execution_count++;
      }
      
      printf("%s:             period                  = %d millisecs\n", activity.name, activity.period);
      printf("%s:             exec_next_release_time  = %ld millisecs\n\n", activity.name, exec_next_release_time);

     
      // logica di cambio priorità a run-time e cambio di core 
     //5 è scelto a caso al momento di potrebbe mettere anche dopo una possibile deadline miss
      if (execution_count == 5) {
          printf("\n==========================================================\n");
          printf(">> [%s] 5 ESECUZIONI RAGGIUNTE INVERSIONE IN CORSO...\n", activity.name);
          
          snprintf(marker, sizeof(marker), "INVERSION_START_%s", activity.name);
          write_trace_marker(marker);
          
          // 1. Cambio Periodo e Deadline
          activity.period = activity.alternate_period;
          activity.deadline = activity.alternate_deadline; 
          
          // 2. Cambio Priorità e ricalcolo con rm la priorità
          struct sched_param sp;
          sp.sched_priority = 99 - (activity.period / 10);
          if (sp.sched_priority < 1) sp.sched_priority = 1;
          if (sp.sched_priority > 99) sp.sched_priority = 99;

          if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
              perror("Errore nell'aggiornamento della priorita");
          }
          
          // 3. CAMBIO CORE 
          cpu_set_t cpuset;
          CPU_ZERO(&cpuset);
          CPU_SET(activity.alternate_core, &cpuset); // Imposta il nuovo core
          
          // MARKER INIZIO CAMBIO CORE 
          snprintf(marker, sizeof(marker), "CORE_MIGRATION_START_%s_TO_%d", activity.name, activity.alternate_core);
          write_trace_marker(marker);

          if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
              perror("Errore nell'aggiornamento del core (Affinity)");
          }

          //  MARKER FINE MIGRAZIONE CORE 
          snprintf(marker, sizeof(marker), "CORE_MIGRATION_END_%s_TO_%d", activity.name, activity.alternate_core);
          write_trace_marker(marker);

          
          printf(">> [%s] Nuovo Periodo: %d ms | Nuova Priorita: %d | Migrato su Core: %d\n", 
                 activity.name, activity.period, sp.sched_priority, activity.alternate_core);
          
          snprintf(marker, sizeof(marker), "INVERSION_DONE_%s_PRIO_%d_PER_%d_CORE_%d", 
                   activity.name, sp.sched_priority, activity.period, activity.alternate_core);
          write_trace_marker(marker);
          
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
