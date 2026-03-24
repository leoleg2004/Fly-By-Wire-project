/*
 * Client.cpp
 */

#include "Client.hpp"
#include <service_msgsPubSubTypes.h>
#include <random>

	// init the random generation
	std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(-5, 5); // define the range of the random generator


Client::Client() {
}

/*
 * method to start the broadcastner of request messages
 */
void Client::start(std::string topic_name, int domain_id) {
    eprosima::fastdds::dds::TypeSupport msg_type(new RequestPubSubType());
    broadcastner.start(&msg_type, "Request", topic_name, domain_id);
    
    std::cout << "\nPress Ctrl-C to stop the Client" << std::endl;

	/* init the activity parameters */
	sprintf(activity_parameters.name,	"Client");
	activity_parameters.function = activity_function;
	activity_parameters.period = 1000;
	activity_parameters.instance = this;
	activity_parameters.print = false;

	/* Create the periodic thread */
    int ret_err = pthread_create( &periodic_thread, NULL, PeriodicTask, (void*) &activity_parameters);
    handle_error(ret_err, "Error in creating Client PeriodicTask");

    pthread_join( periodic_thread, NULL);
}


/*
 * this method is called periodically to publish a request message
 */
void Client::publish_request() {
    Request request_msg;
	int random;

	// increment the message id
	request_msg.id(++counter);

	// generate a number to select the operation
	random = distr(gen);
	if(random >= 0)
		request_msg.operation("division");
	else
		request_msg.operation("logarithm");

	// generate a number for the first operand
	random = distr(gen);
	request_msg.data_a(random);
	
	// generate a number for the second operand
	random = distr(gen);
	request_msg.data_b(random);
   
	std::cout << "Client send Request["<<counter<<"] " << request_msg.operation() << "(" << 
										 request_msg.data_a() << ", " <<
										 request_msg.data_b() << ")" << std::endl;
	broadcastner.publish(&request_msg);
}

/*
 * this method is called when a connection is established
 */
void Client::on_subscription_matched(
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
 * this method is called when a new reply message is available
 */
void Client::on_data_available(DataReader* reader) {
    SampleInfo info;
    Reply reply_msg;

    if (reader->take_next_sample(&reply_msg, &info) == ReturnCode_t::RETCODE_OK) {
        if (info.instance_state == ALIVE_INSTANCE_STATE) {
            // Print the message
            std::cout << "--- Received Reply[" << reply_msg.id() << "] " <<
            								reply_msg.operation();
			if(reply_msg.correct()) {
				std::cout << " CORRECT result = " << reply_msg.result() << std::endl;
			else
				std::cout << " ERROR: wrong operation" << std::endl;											
        }
    }
}


int main(int argc, char** argv) {
    Client client;
    client.start("ClientRequest", 1);
    

    return 0;
}
