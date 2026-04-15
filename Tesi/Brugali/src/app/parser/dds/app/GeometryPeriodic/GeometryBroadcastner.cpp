/*
 * GeometryBroadcastner.cpp
 */

#include "GeometryBroadcastner.hpp"

#include <cstdint>
#include <fastrtps/rtps/common/Time_t.h>
#include <geometry_msgsPubSubTypes.h>
#include <pthread.h>
#include <sched.h>

GeometryBroadcastner::GeometryBroadcastner() {}

void GeometryBroadcastner::start(std::string topic_name, int domain_id) {
  eprosima::fastdds::dds::TypeSupport msg_type(new PointPubSubType());
  broadcastner.start(&msg_type, "Point", topic_name, domain_id);

  std::cout << "\nPress Ctrl-C to stop the GeometryBroadcastner" << std::endl;

  /* init the activity parameters */
  sprintf(activity_parameters.name, "GeometryPublish");
  activity_parameters.function = activity_function;
  activity_parameters.period = 800;
  activity_parameters.deadline = 800;
  activity_parameters.instance = this;

  /* Create the periodic thread using Rate Monotonic (SCHED_FIFO) */
  pthread_attr_t attr;
  struct sched_param param;
  cpu_set_t cpuset;

  pthread_attr_init(&attr);
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

  // RM Priority: Inversely proportional to period
  param.sched_priority = 99 - (activity_parameters.period / 10);
  if (param.sched_priority < 1)
    param.sched_priority = 1;
  pthread_attr_setschedparam(&attr, &param);

  int ret_err = pthread_create(&periodic_thread, &attr, PeriodicTask,
                               (void *)&activity_parameters);

  pthread_setname_np(periodic_thread, "GeometryPublish");
  pthread_attr_destroy(&attr);

  handle_error(ret_err, "Error in creating PeriodicTask 1");

  pthread_join(periodic_thread, NULL);
}

void GeometryBroadcastner::publish_point() {
  Point point_msg;

  counter += 0.1;
  point_msg.x(1.0 * counter);
  point_msg.y(2.0 * counter);
  point_msg.z(3.0 * counter);

  std::cout << "GeometryBroadcastner send Point(" << point_msg.x() << ", "
            << point_msg.y() << ", " << point_msg.z() << ")" << std::endl;
  broadcastner.publish(&point_msg);
}

int main(int argc, char **argv) {
  GeometryBroadcastner geometry;
  geometry.start("PointTopic", 1);

  return 0;
}
