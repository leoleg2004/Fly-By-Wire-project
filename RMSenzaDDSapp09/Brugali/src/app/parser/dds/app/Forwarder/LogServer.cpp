/*
 * LogServer.cpp
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


#include "LogServer.hpp"

#include "Listener.hpp"

#include <service_msgsPubSubTypes.h>


/*
 * this method is called when a connection is established
 */
void LogServer::on_subscription_matched(
        eprosima::fastdds::dds::DataReader*,
        const SubscriptionMatchedStatus& info) {

    if (info.current_count_change == 1) {
        std::cout << "LogarithmicServer matched." << std::endl;
    }
    else if (info.current_count_change == -1) {
        std::cout << "LogarithmicServer unmatched." << std::endl;
    }
    else {
        std::cout << info.current_count_change
           << " is not a valid value for PublicationMatchedStatus" << std::endl;
    }

}

/*
 * this method is called when a new message is available
 */
void LogServer::on_data_available(DataReader* reader) {
    SampleInfo info;
    Request request_msg;

    if (reader->take_next_sample(&request_msg, &info) == ReturnCode_t::RETCODE_OK) {
        if (info.instance_state == ALIVE_INSTANCE_STATE) {
            // Print the message
            std::cout << "LogarithmicServer Received Request[" << request_msg.id() << "] " <<
            								request_msg.operation() << "("<<
            								request_msg.data_a() << ", " <<
											request_msg.data_b() << ")" << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    eprosima::fastdds::dds::TypeSupport msg_type(new RequestPubSubType());

    Listener listener(new LogServer(), &msg_type, "Request");
    listener.start("LogarithmicRequest", 1);

    return 0;
}
