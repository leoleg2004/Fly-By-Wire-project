/*
 * Forwarder.cpp
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


#include "Forwarder.hpp"

#include "Listener.hpp"

#include <service_msgsPubSubTypes.h>


/*
 * method to start the broadcastners
 */
void Forwarder::startBroadcastners(int domain_id) {
	// create the message type
    eprosima::fastdds::dds::TypeSupport msg_type(new RequestPubSubType());

	// start the two broadcastners
    logarithm_broadcastner.start(&msg_type, "Request", "LogarithmicRequest", domain_id);
    division_broadcastner.start(&msg_type, "Request", "DivisionRequest", domain_id);
}

/*
 * this method is called when a connection is established
 */
void Forwarder::on_subscription_matched(
        eprosima::fastdds::dds::DataReader*,
        const SubscriptionMatchedStatus& info) {

    if (info.current_count_change == 1) {
        std::cout << "Forwarder matched." << std::endl;
    }
    else if (info.current_count_change == -1) {
        std::cout << "Forwarder unmatched." << std::endl;
    }
    else {
        std::cout << info.current_count_change
           << " is not a valid value for PublicationMatchedStatus" << std::endl;
    }

}

/*
 * this method is called when a new message is available
 */
void Forwarder::on_data_available(DataReader* reader) {
    SampleInfo info;
    Request request_msg;

    if (reader->take_next_sample(&request_msg, &info) == ReturnCode_t::RETCODE_OK) {
        if (info.instance_state == ALIVE_INSTANCE_STATE) {
            // Print the message
            std::cout << "Forwarder Received Request[" << request_msg.id() << "] " <<
            								request_msg.operation() << "("<<
            								request_msg.data_a() << ", " <<
											request_msg.data_b() << ")" << std::endl;
			if(request_msg.operation().compare("logarithm")==0)
				logarithm_broadcastner.publish(&request_msg);
			else if(request_msg.operation().compare("division")==0)
				division_broadcastner.publish(&request_msg);
			else
				std::cout << "ERROR: wrong operation" << std::endl;											
        }
    }
}

int main(int argc, char** argv) {
    eprosima::fastdds::dds::TypeSupport msg_type(new RequestPubSubType());

	// create an instance of the Forwarder class
	Forwarder forwarder;

    // start the broadcastners
    forwarder.startBroadcastners(1);

	// start the listner
    Listener listener(&forwarder, &msg_type, "Request");
    listener.start("ClientRequest", 1);
    

    return 0;
}
