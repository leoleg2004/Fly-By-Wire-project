/*
 * Listener.cpp
 */

#include "Listener.hpp"
#include "activity_library.h"
#include "trace_marker.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/attributes/SubscriberAttributes.h>
#include <fastrtps/rtps/common/Time_t.h>
#include <geometry_msgsPubSubTypes.h>

using namespace eprosima::fastdds::dds;
using namespace geometry_msgs::msg::dds_;

class GeometryListener : public DataReaderListener {
public:
  GeometryListener() {}
  ~GeometryListener() override {}

  void on_data_available(DataReader *reader) override {
    write_trace_marker("DDS_READ_START");
    SampleInfo info;
    Point_ point_msg; // Rinominato da Point

    if (reader->take_next_sample(&point_msg, &info) ==
        ReturnCode_t::RETCODE_OK) {
      if (info.instance_state == ALIVE_INSTANCE_STATE) {
        std::cout << "Received Point(" << point_msg.x() << ", " << point_msg.y()
                  << ", " << point_msg.z() << ")" << std::endl;
      }
    }
    write_trace_marker("DDS_READ_END");
  }

  void on_subscription_matched(DataReader *reader,
                               const SubscriptionMatchedStatus &info) override {
    if (info.current_count_change == 1) {
      std::cout << "Publisher matched." << std::endl;
    } else if (info.current_count_change == -1) {
      std::cout << "Publisher unmatched." << std::endl;
    }
  }
};

int main(int argc, char **argv) {
  init_tracing();
  // Rinominato da PointPubSubType
  eprosima::fastdds::dds::TypeSupport msg_type(new Point_PubSubType());

  Listener listener(new GeometryListener(), &msg_type, "Point");
  listener.start("PointTopic", 1);

  close_tracing();
  return 0;
}
