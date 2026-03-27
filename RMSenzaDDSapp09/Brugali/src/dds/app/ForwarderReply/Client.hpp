/*
 * Client.hpp
 */

#ifndef ERTS_DDS_CLIENT_H_
#define ERTS_DDS_CLIENT_H_

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>

#include "Broadcastner.hpp"
#include "activity_library.h"

class Client : public eprosima::fastdds::dds::DataReaderListener  {
public:
	Client() {};

	~Client() override { }

	// method to start the request broadcastner and the reply listener
	void start(std::string topic_name, int domain_id);
	
	// method to publish a request message
	void publish_request();
	
	// method to receive reply messages
	void on_data_available(
		eprosima::fastdds::dds::DataReader* reader) override;

	// method to receive connection notifications
	void on_subscription_matched(
			eprosima::fastdds::dds::DataReader* reader,
			const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;

private:

	Broadcastner broadcastner;
    int counter = 0;

    pthread_t periodic_thread;
	t_activity_par activity_parameters;};

void activity_function(void* instance) {
	Client* class_instance = (Client*) instance;
	class_instance->publish_request();
}

#endif
