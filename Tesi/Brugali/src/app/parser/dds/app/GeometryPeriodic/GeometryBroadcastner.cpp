/*
 * GeometryBroadcastner.cpp
 *
 * Architettura a 3 thread:
 *
 *   Thread 1 "DDS_Publish"  (periodico)
 *       - Calcola il carico + prepara il messaggio Point
 *       - Passa il messaggio al thread di comunicazione e torna a dormire
 *
 *   Thread 2 "Activity_1"  (periodico)
 *       - Carico computazionale puro (come Activity_2 di app10h)
 *
 *
 *
 */

#include "GeometryBroadcastner.hpp"

#include "/home/leonardo/eprosima_projects/flight_sensor/flight_sensor/Tesi/Brugali/src/lib/trace_marker.h"
#include <cstdint>
#include <cstring>
#include <fastrtps/rtps/common/Time_t.h>
#include <geometry_msgsPubSubTypes.h>
#include <pthread.h>
#include <sched.h>


static Broadcastner g_broadcastner;
static double g_counter = 0.0;

/* ========================================================================
 * Comunicazione tra DDS_Publish e DDS_Comm 
 * ======================================================================== */
static pthread_mutex_t dds_comm_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dds_comm_cond = PTHREAD_COND_INITIALIZER;
static bool dds_comm_pending = false;
static Point dds_comm_msg;

/* ========================================================================
 * Thread 3: DDS_Comm — thread dedicato alla comunicazione DDS
 *
 * Questo thread scrive i marker DDS_MSG_START / DDS_MSG_END.
 * Poiche' li scrive LUI, ftrace li associa al TID di DDS_Comm.
 * thread_analysis li legge e genera gli intervalli DDS_MSG sulla
 * corsia "DDS_Comm" nel CSV e nel grafico.
 * ======================================================================== */
void *DDS_CommThread(void *arg) {
  (void)arg;
  pthread_setname_np(pthread_self(), "DDS_Comm");

  while (1) {
    
    pthread_mutex_lock(&dds_comm_mutex);
    while (!dds_comm_pending) {
      pthread_cond_wait(&dds_comm_cond, &dds_comm_mutex);
    }

    
    Point msg = dds_comm_msg;
    dds_comm_pending = false;
    pthread_mutex_unlock(&dds_comm_mutex);

   
    write_trace_marker("DDS_MSG_START_DDS_Comm");

    g_broadcastner.publish(&msg);

    write_trace_marker("DDS_MSG_END_DDS_Comm");

    printf("DDS_Comm:    sent Point(%.1f, %.1f, %.1f)\n",
           msg.x(), msg.y(), msg.z());
  }

  return NULL;
}

/* ========================================================================
 * Thread 1: DDS_Publish — task periodico
 * Prepara il messaggio e lo passa a DDS_Comm
 * ======================================================================== */
void dds_publish_function(void *instance, int parameter) {
  (void)instance;

  /* carico computazionale (questa parte resta su DDS_Publish) */
  activity_load(parameter);

  /* prepara il messaggio */
  g_counter += 0.1;
  Point point_msg;
  point_msg.x(1.0 * g_counter);
  point_msg.y(2.0 * g_counter);
  point_msg.z(3.0 * g_counter);

  /* passa il messaggio al thread DDS_Comm */
  pthread_mutex_lock(&dds_comm_mutex);
  dds_comm_msg = point_msg;
  dds_comm_pending = true;
  pthread_cond_signal(&dds_comm_cond);
  pthread_mutex_unlock(&dds_comm_mutex);
}

/* ========================================================================
 * Thread 2: Computation — task periodico
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

  /* ── Thread 3: DDS_Comm (event-driven, SCHED_FIFO) ────────────── */
  pthread_t thread_comm;
  pthread_attr_t attr_comm;
  struct sched_param param_comm;
  cpu_set_t cpuset;

  pthread_attr_init(&attr_comm);
  pthread_attr_setinheritsched(&attr_comm, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_comm, SCHED_FIFO);

  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  pthread_attr_setaffinity_np(&attr_comm, sizeof(cpu_set_t), &cpuset);

  /* Priorita' alta: deve reagire subito quando il task periodico segnala */
  param_comm.sched_priority = 90;
  pthread_attr_setschedparam(&attr_comm, &param_comm);

  int ret_err =
      pthread_create(&thread_comm, &attr_comm, DDS_CommThread, NULL);
  handle_error(ret_err, "Error in creating DDS_Comm thread");
  pthread_setname_np(thread_comm, "DDS_Comm");
  pthread_attr_destroy(&attr_comm);

  /* ── Thread 1: DDS_Publish (periodico ) ─────────────────── */
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

  ret_err =
      pthread_create(&thread1, &attr1, PeriodicTask, (void *)&activity_1);
  handle_error(ret_err, "Error in creating PeriodicTask DDS_Publish");
  pthread_setname_np(thread1, "DDS_Publish");
  pthread_attr_destroy(&attr1);

  /* ── Thread 2: Computation (periodico) ─────────────────── */
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

  ret_err =
      pthread_create(&thread2, &attr2, PeriodicTask, (void *)&activity_2);
  handle_error(ret_err, "Error in creating PeriodicTask Activity_1");
  pthread_setname_np(thread2, "Activity_1");
  pthread_attr_destroy(&attr2);

  /* ── Attesa completamento ──────────────────────────────────────── */
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
  /* DDS_Comm gira per sempre, terminato da Ctrl-C assieme ai periodici */

  close_tracing();
  return 0;
}
