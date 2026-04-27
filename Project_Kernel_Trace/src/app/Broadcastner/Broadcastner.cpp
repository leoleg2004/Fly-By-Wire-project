/*
 * Broadcastner.cpp
 */

#include "../../lib/communication_library/Broadcastner.hpp"
#include "../../lib/activity_library.h"
#include "../../lib/trace_marker.h"

#include <cstdint>
#include <cstring>
#include <fastrtps/rtps/common/Time_t.h>
#include <geometry_msgsPubSubTypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

// Usiamo il namespace generato da fastddsgen v2+
using namespace geometry_msgs::msg::dds_;

static Broadcastner g_broadcastner;
static double g_counter = 0.0;

void dds_publish_function(void *instance, int parameter) {
  (void)instance;
  activity_load(parameter);

  g_counter += 0.1;
  Point_ point_msg; // Rinominato da Point
  point_msg.x(1.0 * g_counter);
  point_msg.y(2.0 * g_counter);
  point_msg.z(3.0 * g_counter);

  write_trace_marker("DDS_MSG_START_DDS_Publish");
  g_broadcastner.publish(&point_msg);
  write_trace_marker("DDS_MSG_END_DDS_Publish");

  printf("DDS_Publish: sent Point(%.1f, %.1f, %.1f)\n", point_msg.x(),
         point_msg.y(), point_msg.z());
}

void computation_function(void *instance, int parameter) {
  (void)instance;
  activity_load(parameter);
}

int main(int argc, char **argv) {
  init_tracing();

  // Rinominato da PointPubSubType
  eprosima::fastdds::dds::TypeSupport msg_type(new Point_PubSubType());
  g_broadcastner.start(&msg_type, "Point", "PointTopic", 1);
  printf("\nDDS inizializzato. Press Ctrl-C to stop.\n\n");

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);

  pthread_t thread1;
  pthread_attr_t attr1;
  struct sched_param param1;

  pthread_attr_init(&attr1);
  pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);
  pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset);

  t_activity_par activity_1;
  snprintf(activity_1.name, sizeof(activity_1.name), "DDS_Publish");
  activity_1.function = dds_publish_function;
  activity_1.period = 600;
  activity_1.parameter = 6;
  activity_1.deadline = 600;
  activity_1.instance = NULL;
  activity_1.print = true;

  param1.sched_priority = 99 - (activity_1.period / 10);
  if (param1.sched_priority < 1)
    param1.sched_priority = 1;
  pthread_attr_setschedparam(&attr1, &param1);

  int ret_err =
      pthread_create(&thread1, &attr1, PeriodicTask, (void *)&activity_1);
  if (ret_err != 0) {
    perror("Error creating thread 1");
    exit(1);
  }
  pthread_setname_np(thread1, "DDS_Publish");
  pthread_attr_destroy(&attr1);

  pthread_t thread2;
  pthread_attr_t attr2;
  struct sched_param param2;

  pthread_attr_init(&attr2);
  pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
  pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset);

  t_activity_par activity_2;
  snprintf(activity_2.name, sizeof(activity_2.name), "Activity_1");
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
  if (ret_err != 0) {
    perror("Error creating thread 2");
    exit(1);
  }
  pthread_setname_np(thread2, "Activity_1");
  pthread_attr_destroy(&attr2);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  close_tracing();
  return 0;
}
