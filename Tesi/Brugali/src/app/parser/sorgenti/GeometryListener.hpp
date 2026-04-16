/*
 * GeometryListener.hpp
 */

#ifndef ERTS_DDS_GEOMETRY_LISTENER_H_
#define ERTS_DDS_GEOMETRY_LISTENER_H_

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>


class GeometryListener : public eprosima::fastdds::dds::DataReaderListener  {
public:
	GeometryListener() { }

	~GeometryListener() override { }

	void on_data_available(
			eprosima::fastdds::dds::DataReader* reader) override;

	void on_subscription_matched(
			eprosima::fastdds::dds::DataReader* reader,
			const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;
};

#endif
