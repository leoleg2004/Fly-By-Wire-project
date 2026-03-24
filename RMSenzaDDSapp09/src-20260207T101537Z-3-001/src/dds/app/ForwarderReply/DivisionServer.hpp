/*
 * DivisionServer.hpp
 */

#ifndef ERTS_DDS_DIVISION_SERVER_H_
#define ERTS_DDS_DIVISION_SERVER_H_

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>

#include "Broadcastner.hpp"

class DivisionServer : public eprosima::fastdds::dds::DataReaderListener  {
public:
	DivisionServer() { }

	~DivisionServer() override { }

	// method to start the broadcastner of reply messages
	void startBroadcastner(std::string topic_name, int domain_id);

	// method to receive messages
	void on_data_available(
			eprosima::fastdds::dds::DataReader* reader) override;

	// method to receive connection notifications
	void on_subscription_matched(
			eprosima::fastdds::dds::DataReader* reader,
			const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;
			
private:
	// broadcastner used to publish reply messages
	Broadcastner broadcastner; 

};

#endif
