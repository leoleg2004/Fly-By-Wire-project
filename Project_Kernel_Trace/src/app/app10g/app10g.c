#define _GNU_SOURCE
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "time_library.h"
#include "trace_marker.h"
#include "activity_library.h"

#define handle_error(en, msg)                                                  \
  if (en != 0) {                                                               \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  }

int main(int argc, char *argv[]) {
  // Inizializza il file descriptor per i marker di tracciamento
  init_tracing();

  // Setup per i thread 1 e 2
  pthread_t thread1, thread2;
  pthread_attr_t attr1, attr2;
  struct sched_param param1, param2;
  int ret_err;

  // Definiamo due maschere diverse per i core
  cpu_set_t cpuset_core1;
  cpu_set_t cpuset_core2;

  pthread_attr_init(&attr1);
  pthread_attr_init(&attr2);

  // Impostazione della policy SCHED_FIFO (Real-Time) per entrambi
  pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);

  pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);

  // ==========================================
  // CONFIGURAZIONE AFFINITÀ CORE
  // ==========================================
  
  // Set per il Thread 1 -> CORE 1
  CPU_ZERO(&cpuset_core1);
  CPU_SET(1, &cpuset_core1);
  pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset_core1);

  // Set per il Thread 2 -> CORE 2
  CPU_ZERO(&cpuset_core2);
  CPU_SET(2, &cpuset_core2);
  pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset_core2);

  // ==========================================
  // -- THREAD 1 -- (Core 1)a
  // ==========================================
  t_activity_par activity_1;
  snprintf(activity_1.name, 15, "Activity_1");
  activity_1.function = (void (*)(void *, int))activity_load; // Funzione importata dalla tua libreria
  activity_1.period = 1000;
  activity_1.parameter = 10;
  activity_1.deadline = 1000;

  param1.sched_priority = 99 - (activity_1.period / 10);
  if (param1.sched_priority < 1) param1.sched_priority = 1;
  pthread_attr_setschedparam(&attr1, &param1);

  ret_err = pthread_create(&thread1, &attr1, PeriodicTask, (void *)&activity_1);
  handle_error(ret_err, "Error in creating PeriodicTask 1");
  pthread_setname_np(thread1, "Activity_1");

  // ==========================================
  // -- THREAD 2 -- (Core 2)
  // ==========================================
  t_activity_par activity_2;
  snprintf(activity_2.name, 15, "Activity_2");
  activity_2.function = (void (*)(void *, int))activity_load; // Funzione importata dalla tua libreria
  activity_2.period = 1200;
  activity_2.parameter = 20;
  activity_2.deadline = 1200;

  param2.sched_priority = 99 - (activity_2.period / 10);
  if (param2.sched_priority < 1) param2.sched_priority = 1;
  pthread_attr_setschedparam(&attr2, &param2);

  ret_err = pthread_create(&thread2, &attr2, PeriodicTask, (void *)&activity_2);
  handle_error(ret_err, "Error in creating PeriodicTask 2");
  pthread_setname_np(thread2, "Activity_2");

  // Pulizia attributi
  pthread_attr_destroy(&attr1);
  pthread_attr_destroy(&attr2);

  // Attesa completamento (in un sistema reale girano all'infinito)
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  // Chiude il file descriptor dei marker
  close_tracing();

  exit(0);
}
