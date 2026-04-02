/*
 * GeometryListener.cpp
 */

#include <limits>
#include <sstream>
#include <iostream>

#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/attributes/SubscriberAttributes.h>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>


#include "GeometryListener.hpp"
#include "Listener.hpp"
#include <geometry_msgsPubSubTypes.hpp>


#define DOMAIN_ID 1

void GeometryListener::on_subscription_matched(
        eprosima::fastdds::dds::DataReader*,
        const SubscriptionMatchedStatus& info) {

    if (info.current_count_change == 1) {
        std::cout << "Publisher matched." << std::endl;
    }
    else if (info.current_count_change == -1) {
        std::cout << "Publisher unmatched." << std::endl;
    }
    else {
        std::cout << info.current_count_change
           << " is not a valid value for PublicationMatchedStatus" << std::endl;
    }

}

void GeometryListener::on_data_available(DataReader* reader) {
    SampleInfo info;
    Point point_msg;

    if (reader->take_next_sample(&point_msg, &info) == ReturnCode_t::RETCODE_OK) {
        if (info.instance_state == ALIVE_INSTANCE_STATE) {
            // Print the message
            std::cout << "Received Point(" 	<< point_msg.x() << ", "
            								<< point_msg.y() << ", "
											<< point_msg.z() << ")" << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    eprosima::fastdds::dds::TypeSupport msg_type(new PointPubSubType());

    Listener listener(new GeometryListener(), &msg_type, "Point");
    listener.start("PointTopic", DOMAIN_ID);

    return 0;
}
