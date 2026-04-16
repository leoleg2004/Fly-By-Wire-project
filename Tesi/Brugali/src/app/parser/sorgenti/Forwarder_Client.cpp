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



int main(int argc, char** argv) {
    Client client;
    client.start("ClientRequest", 1);
    

    return 0;
}
