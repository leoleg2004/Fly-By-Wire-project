#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Includiamo le librerie
#include "../lib/time_library.h"
#include "../lib/trace_marker.h"
// INCLUDIAMO LA NUOVA LIBRERIA DINAMICA
#include "../lib/activity_library_dyn.h"

#define handle_error(en, msg)                                                  \
  if (en != 0) {                                                               \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  }

int main(int argc, char *argv[]) {
  init_tracing();

  pthread_t thread1, thread2;
  pthread_attr_t attr1, attr2;
  struct sched_param param1, param2; 
  int ret_err;

  cpu_set_t cpuset1, cpuset2;

  pthread_attr_init(&attr1);
  pthread_attr_init(&attr2);

  pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);

  pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);

  // ==========================================
  // THREAD 1 
  // ==========================================
  CPU_ZERO(&cpuset1);
  CPU_SET(0, &cpuset1); // Core 0 per Thread 1
  pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);
  
  t_activity_par activity_1;
  memset(&activity_1, 0, sizeof(t_activity_par));
  sprintf(activity_1.name, "Activity_1");
  activity_1.function = ActivityIncrementDyn; // Presa dalla nuova libreria
  activity_1.period = 1000;
  activity_1.alternate_period = 1300; 
  activity_1.alternate_core = 1;
  activity_1.parameter = 4;
  activity_1.deadline = 1000;
  activity_1.alternate_deadline = 1300; 

  param1.sched_priority = 99 - (activity_1.period / 10);
  if (param1.sched_priority < 1) param1.sched_priority = 1;
  pthread_attr_setschedparam(&attr1, &param1);

  ret_err = pthread_create(&thread1, &attr1, PeriodicTaskDyn, (void *)&activity_1);
  handle_error(ret_err, "Error in creating PeriodicTask 1");
  pthread_setname_np(thread1, "Activity_1");

  // ==========================================
  // THREAD 2
  // ==========================================
  CPU_ZERO(&cpuset2);
  CPU_SET(1, &cpuset2); // Core 1 per Thread 2
  pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset2);

  t_activity_par activity_2;
  memset(&activity_2, 0, sizeof(t_activity_par));
  sprintf(activity_2.name, "Activity_2");
  activity_2.function = ActivityIncrementDyn;
  activity_2.period = 300;
  activity_2.alternate_period = 800;
  activity_2.alternate_core = 7;
  activity_2.parameter = 2;
  activity_2.deadline = 300;
  activity_2.alternate_deadline = 800; 

  param2.sched_priority = 99 - (activity_2.period / 10);
  if (param2.sched_priority < 1) param2.sched_priority = 1;
  pthread_attr_setschedparam(&attr2, &param2);

  ret_err = pthread_create(&thread2, &attr2, PeriodicTaskDyn, (void *)&activity_2);
  handle_error(ret_err, "Error in creating PeriodicTask 2");
  pthread_setname_np(thread2, "Activity_2");

  pthread_attr_destroy(&attr1);
  pthread_attr_destroy(&attr2);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  close_tracing();

  exit(0);
}
