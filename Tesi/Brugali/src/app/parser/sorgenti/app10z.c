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
 * Data structure for activity parameters
 * MODIFICA: Aggiunto "alternate_period" per sapere quale periodo assumere dopo 5 esecuzioni
 */
typedef struct activity_parameters {
  char name[15];
  int period;
  int alternate_period;//variabile per il cambio di prioirità 
  int parameter;
  long int deadline;
  void (*function)(int); 
} t_activity_par;

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
 * Periodic task that executes an activity
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
  int execution_count = 0; // conto quante volte viene contto la periodi task

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
        
       
        execution_count++;
      }
      
      printf("%s:            period                  = %d millisecs\n",
             activity.name, activity.period);
      printf("%s:            exec_next_release_time  = %ld millisecs\n\n",
             activity.name, exec_next_release_time);

     if (execution_count == 5) {
          printf("\n==========================================================\n");
          printf(">> [%s] 5 ESECUZIONI RAGGIUNTE INVERSIONE IN CORSO...\n", activity.name);
          
         
          snprintf(marker, sizeof(marker), "INVERSION_START_%s", activity.name);
          write_trace_marker(marker);
          
         
          activity.period = activity.alternate_period;
          
          // Ricalcola la priorità Rate Monotonic per il nuovo periodo
          struct sched_param sp;
          sp.sched_priority = 99 - (activity.period / 10);
          if (sp.sched_priority < 1) sp.sched_priority = 1;
          if (sp.sched_priority > 99) sp.sched_priority = 99;

          // Applica la nuova priorità allo scheduler (SCHED_FIFO)
          if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
              perror("Errore nell'aggiornamento della priorita");
          } else {
              printf(">> [%s] Nuovo Periodo: %d ms | Nuova Priorita: %d\n", 
                     activity.name, activity.period, sp.sched_priority);
              
             
              snprintf(marker, sizeof(marker), "INVERSION_DONE_%s_PRIO_%d_PER_%d", 
                       activity.name, sp.sched_priority, activity.period);
              write_trace_marker(marker);
             
          }
          printf("==========================================================\n\n");
          
          // Incrementiamo per non ripetere l'inversione
          execution_count++; 
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

int main(int argc, char *argv[]) {
  init_tracing();

  pthread_t thread1, thread2;
  pthread_attr_t attr1, attr2;
  struct sched_param param1, param2; 
  int ret_err;

  cpu_set_t cpuset1;

  pthread_attr_init(&attr1);
  pthread_attr_init(&attr2);

  pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);

  pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);

  // ==========================================
  //THREAD 1 
  // ==========================================
  CPU_ZERO(&cpuset1);
  CPU_SET(0, &cpuset1); 
  pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);
  
  t_activity_par activity_1;
  sprintf(activity_1.name, "Activity_1");
  activity_1.function = ActivityIncrement;
  activity_1.period = 2000;
  activity_1.alternate_period = 300; // MODIFICA: Periodo da scambiare
  activity_1.parameter = 5;
  activity_1.deadline = 2000;

  param1.sched_priority = 99 - (activity_1.period / 10);
  if (param1.sched_priority < 1) param1.sched_priority = 1;
  pthread_attr_setschedparam(&attr1, &param1);

  ret_err = pthread_create(&thread1, &attr1, PeriodicTask, (void *)&activity_1);
  handle_error(ret_err, "Error in creating PeriodicTask 1");
  pthread_setname_np(thread1, "Activity_1");

  // ==========================================
  // THREAD 2
  // ==========================================
  
  CPU_ZERO(&cpuset1);
  CPU_SET(1, &cpuset1);
  pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);
  pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset1);

  t_activity_par activity_2;
  sprintf(activity_2.name, "Activity_2");
  activity_2.function = ActivityIncrement;
  activity_2.period = 300;
  activity_2.alternate_period = 2000; 
  activity_2.parameter = 2;
  activity_2.deadline = 300;

  param2.sched_priority = 99 - (activity_2.period / 10);
  if (param2.sched_priority < 1) param2.sched_priority = 1;
  pthread_attr_setschedparam(&attr2, &param2);

  ret_err = pthread_create(&thread2, &attr2, PeriodicTask, (void *)&activity_2);
  handle_error(ret_err, "Error in creating PeriodicTask 2");
  pthread_setname_np(thread2, "Activity_2");

  pthread_attr_destroy(&attr1);
  pthread_attr_destroy(&attr2);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  close_tracing();

  exit(0);
}
