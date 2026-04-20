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

#include "../lib/time_library.h"
#include "../lib/trace_marker.h"
// Include the library header so we use ITS t_activity_par and PeriodicTask
#include "../lib/activity_library.h"

#define handle_error(en, msg)                                                  \
  if (en != 0) {                                                               \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  }

/*
 * Function that implements the activity
 * The signature must match the library's expectation: void (*)(void*, int)
 */
void ActivityIncrement(void* instance, int parameter) {
  double result = 0;
  for (long i = 0; i < parameter * 1000 * 1000; i++) {
    result = result + sin(i) + cos(i) + i * tan(i);
  }
}

int main(int argc, char *argv[]) {
  // Inizializza il file descriptor per i marker di tracciamento
  init_tracing();

  // Setup per DUE thread!
  pthread_t thread1, thread2;
  pthread_attr_t attr1, attr2;
  struct sched_param param1, param2; // Strutture per la priorità
  int ret_err;

  cpu_set_t cpuset1, cpuset2;

  pthread_attr_init(&attr1);
  pthread_attr_init(&attr2);

  // Policy RM (SCHED_FIFO)
  pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);

  pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);

  // ==========================================
  // -- THREAD 1 -- (Meno prioritario)
  // ==========================================
  // Affinità Core: Thread 1 sul CORE 0
  CPU_ZERO(&cpuset1);
  CPU_SET(0, &cpuset1);
  pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);

  t_activity_par activity_1;
  memset(&activity_1, 0, sizeof(t_activity_par)); // Clean initialization
  sprintf(activity_1.name, "Activity_1");
  activity_1.function = (void (*)(void*, int))ActivityIncrement;
  activity_1.period = 2000;
  activity_1.parameter = 5;
  activity_1.deadline = 2000;
  activity_1.instance = NULL;

  // Calcolo priorità RM (Periodo 2000)
  param1.sched_priority = 99 - (activity_1.period / 10);
  if (param1.sched_priority < 1)
    param1.sched_priority = 1;
  pthread_attr_setschedparam(&attr1, &param1);

  ret_err = pthread_create(&thread1, &attr1, PeriodicTask, (void *)&activity_1);
  handle_error(ret_err, "Error in creating PeriodicTask 1");

  // ==========================================
  // -- THREAD 2 -- (Più prioritario)
  // ==========================================
  // Affinità Core: Thread 2 sul CORE 1
  CPU_ZERO(&cpuset2);
  CPU_SET(1, &cpuset2);
  pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset2);

  t_activity_par activity_2;
  memset(&activity_2, 0, sizeof(t_activity_par));
  sprintf(activity_2.name, "Activity_2");
  activity_2.function = (void (*)(void*, int))ActivityIncrement;
  activity_2.period = 300;
  activity_2.parameter = 2;
  activity_2.deadline = 300;
  activity_2.instance = NULL;

  // Calcolo priorità RM (Periodo 300)
  param2.sched_priority = 99 - (activity_2.period / 10);
  if (param2.sched_priority < 1)
    param2.sched_priority = 1;
  pthread_attr_setschedparam(&attr2, &param2);

  ret_err = pthread_create(&thread2, &attr2, PeriodicTask, (void *)&activity_2);
  handle_error(ret_err, "Error in creating PeriodicTask 2");

  pthread_attr_destroy(&attr1);
  pthread_attr_destroy(&attr2);

  // Attesa completamento
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  // Chiude il file descriptor dei marker
  close_tracing();

  exit(0);
}
