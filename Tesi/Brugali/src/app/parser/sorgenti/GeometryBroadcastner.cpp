/*
 * GeometryBroadcastner.cpp
 *
 * Architettura a 2 thread:
 *
 *   Thread 1 "DDS_Publish"  (periodico, SCHED_FIFO)
 *       - Calcola il carico matematico
 *       - Prepara il messaggio Point
 *       - Chiama direttamente broadcastner.publish() con i marker DDS_MSG
 *       - Tiene TUTTI i marker: PERIOD_START, FUNCTION_START,
 *         DDS_MSG_START, DDS_MSG_END, FUNCTION_END, SLEEP_START, PERIOD_END
 *
 *   Thread 2 "Activity_1"  (periodico, SCHED_FIFO)
 *       - Carico computazionale puro separato
 */

#include "GeometryBroadcastner.hpp"

#include "/home/leonardo/eprosima_projects/flight_sensor/flight_sensor/Tesi/Brugali/src/lib/trace_marker.h"
#include <cstdint>
#include <cstring>
#include <fastrtps/rtps/common/Time_t.h>
#include <geometry_msgsPubSubTypes.h>
#include <pthread.h>
#include <sched.h>

/* ========================================================================
 * Stato globale condiviso tra main e le funzioni dei thread
 * ======================================================================== */
static Broadcastner g_broadcastner;
static double g_counter = 0.0;

/* ========================================================================
 * Funzione del Thread 1: DDS_Publish
 *
 * Viene chiamata da PeriodicTask() ogni periodo.
 * Scrive TUTTI i marker in sequenza — inclusi DDS_MSG_START/END —
 * cosi' ftrace li associa al TID di DDS_Publish.
 * ======================================================================== */
void dds_publish_function(void *instance, int parameter) {
  (void)instance;

  /* 1. Carico computazionale */
  activity_load(parameter);

  /* 2. Prepara il messaggio */
  g_counter += 0.1;
  Point point_msg;
  point_msg.x(1.0 * g_counter);
  point_msg.y(2.0 * g_counter);
  point_msg.z(3.0 * g_counter);

  /* 3. Marker inizio comunicazione DDS (scritto da DDS_Publish) */
  write_trace_marker("DDS_MSG_START_DDS_Publish");

  /* 4. Invio reale tramite Fast-DDS */
  g_broadcastner.publish(&point_msg);

  /* 5. Marker fine comunicazione DDS */
  write_trace_marker("DDS_MSG_END_DDS_Publish");

  printf("DDS_Publish: sent Point(%.1f, %.1f, %.1f)\n", point_msg.x(),
         point_msg.y(), point_msg.z());
}

/* ========================================================================
 * Funzione del Thread 2: Activity_1
 * Carico puro, nessuna comunicazione DDS
 * ======================================================================== */
void computation_function(void *instance, int parameter) {
  (void)instance;
  activity_load(parameter);
}

/* ========================================================================
 * main()
 * ======================================================================== */
int main(int argc, char **argv) {
  init_tracing();

  /* ── Inizializzazione DDS ──────────────────────────────────────── */
  eprosima::fastdds::dds::TypeSupport msg_type(new PointPubSubType());
  g_broadcastner.start(&msg_type, "Point", "PointTopic", 1);
  printf("\nDDS inizializzato. Press Ctrl-C to stop.\n\n");

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);

  /* ── Thread 1: DDS_Publish (periodico, SCHED_FIFO) ────────────── */
  pthread_t thread1;
  pthread_attr_t attr1;
  struct sched_param param1;

  pthread_attr_init(&attr1);
  pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);
  pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset);

  t_activity_par activity_1;
  sprintf(activity_1.name, "DDS_Publish");
  activity_1.function = dds_publish_function;
  activity_1.period = 500;
  activity_1.parameter = 2;
  activity_1.deadline = 500;
  activity_1.instance = NULL;
  activity_1.print = true;

  param1.sched_priority = 99 - (activity_1.period / 10);
  if (param1.sched_priority < 1)
    param1.sched_priority = 1;
  pthread_attr_setschedparam(&attr1, &param1);

  int ret_err =
      pthread_create(&thread1, &attr1, PeriodicTask, (void *)&activity_1);
  handle_error(ret_err, "Error in creating PeriodicTask DDS_Publish");
  pthread_setname_np(thread1, "DDS_Publish");
  pthread_attr_destroy(&attr1);

  /* ── Thread 2: Activity_1 (periodico, SCHED_FIFO) ─────────────── */
  pthread_t thread2;
  pthread_attr_t attr2;
  struct sched_param param2;

  pthread_attr_init(&attr2);
  pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
  pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset);

  t_activity_par activity_2;
  sprintf(activity_2.name, "Activity_1");
  activity_2.function = computation_function;
  activity_2.period = 1000;
  activity_2.parameter = 4;
  activity_2.deadline = 1000;
  activity_2.instance = NULL;
  activity_2.print = true;

  param2.sched_priority = 99 - (activity_2.period / 10);
  if (param2.sched_priority < 1)
    param2.sched_priority = 1;
  pthread_attr_setschedparam(&attr2, &param2);

  ret_err = pthread_create(&thread2, &attr2, PeriodicTask, (void *)&activity_2);
  handle_error(ret_err, "Error in creating PeriodicTask Activity_1");
  pthread_setname_np(thread2, "Activity_1");
  pthread_attr_destroy(&attr2);

  /* ── Attesa completamento ──────────────────────────────────────── */
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  close_tracing();
  return 0;
}
