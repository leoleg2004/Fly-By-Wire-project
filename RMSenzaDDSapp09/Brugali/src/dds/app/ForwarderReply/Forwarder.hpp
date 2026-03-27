/*
 * Forwarder.hpp
 */

#ifndef ERTS_DDS_FORWARDER_H_
#define ERTS_DDS_FORWARDER_H_

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>

#include "Broadcastner.hpp"

class Forwarder : public eprosima::fastdds::dds::DataReaderListener  {
public:
	Forwarder() {};

	~Forwarder() override { }

	// method to start the broadcastners
	void startBroadcastners(int domain_id);
	
	// method to receive messages
	void on_data_available(
		eprosima::fastdds::dds::DataReader* reader) override;

	// method to receive connection notifications
	void on_subscription_matched(
			eprosima::fastdds::dds::DataReader* reader,
			const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;

private:
	// broadcastner used to forward messages to the Logarithmic Server
	Broadcastner logarithm_broadcastner; 

	// broadcastner used to forward messages to the Division Server
	Broadcastner division_broadcastner;
		
};

#endif
