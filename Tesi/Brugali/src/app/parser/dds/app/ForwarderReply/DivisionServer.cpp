/*
 * DivisionServer.cpp
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


#include "DivisionServer.hpp"
#include "Listener.hpp"

#include <service_msgsPubSubTypes.h>

#include <math.h>

/*
 * method to start the broadcastner
 */
void DivisionServer::startBroadcastner(std::string topic_name, int domain_id) {
	// create the message type
    eprosima::fastdds::dds::TypeSupport msg_type(new ReplyPubSubType());

	// start the two broadcastners
    broadcastner.start(&msg_type, "Reply", topic_name, domain_id);
}


/*
 * this method is called when a connection is established
 */
void DivisionServer::on_subscription_matched(
        eprosima::fastdds::dds::DataReader*,
        const SubscriptionMatchedStatus& info) {

    if (info.current_count_change == 1) {
        std::cout << "DivisionServer matched." << std::endl;
    }
    else if (info.current_count_change == -1) {
        std::cout << "DivisionServer unmatched." << std::endl;
    }
    else {
        std::cout << info.current_count_change
           << " is not a valid value for PublicationMatchedStatus" << std::endl;
    }
}

/*
 * this method is called when a new message is available
 */
void DivisionServer::on_data_available(DataReader* reader) {
    SampleInfo info;
    Request request_msg;
    Reply reply_msg;
    double numerator;		// input value
    double denominator;		// input value
    double result;			// result
    
    if (reader->take_next_sample(&request_msg, &info) == ReturnCode_t::RETCODE_OK) {
        if (info.instance_state == ALIVE_INSTANCE_STATE) {
        	numerator = request_msg.data_a();
        	denominator = request_msg.data_b();
            // Print the message
            std::cout << "DivisionServer Received Request[" << request_msg.id() << "] " <<
            					request_msg.operation() << " numerator= "<< numerator << " denominator= " 	
            					<< denominator << std::endl;

			// copy the request id into the reply id			
			reply_msg.id( request_msg.id() );					
			
			// check if the received data are correct
			if(denominator == 0.0) {
				reply_msg.correct(false);
			}
			else {
				result = numerator / denominator;
				reply_msg.correct(true);
				reply_msg.result(result);
			}
											
			broadcastner.publish(&reply_msg);
        }
    }
}

int main(int argc, char** argv) {
	// create an instance of the LogServer class
	DivisionServer server;

    // start the broadcastner
    server.startBroadcastner("ServerReply", 1);

	// start the listener
    eprosima::fastdds::dds::TypeSupport msg_type(new RequestPubSubType());
    Listener listener(&server, &msg_type, "Request");
    listener.start("DivisionRequest", 1);

    return 0;
}
