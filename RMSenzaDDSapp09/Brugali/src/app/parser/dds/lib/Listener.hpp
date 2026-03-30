/*
 * Listener.hpp
 */

#ifndef ERTS_DDS_READER_H_
#define ERTS_DDS_READER_H_

#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>

using namespace eprosima::fastdds::dds;

class Listener
{
public:

	Listener(eprosima::fastdds::dds::DataReaderListener* listener, eprosima::fastdds::dds::TypeSupport* msg_type, std::string msg_name);

    virtual ~Listener();

    // Initialize
    bool start(std::string topic, int domain_id);

private:
    eprosima::fastdds::dds::DomainParticipant* participant_;

    eprosima::fastdds::dds::Subscriber* subscriber_;

    eprosima::fastdds::dds::Topic* topic_;

    eprosima::fastdds::dds::DataReader* reader_;

    eprosima::fastdds::dds::TypeSupport* type_;

    eprosima::fastdds::dds::DataReaderListener* listener_;

    std::string msg_name_;
};



#endif /* HELLOWORLDPUBLISHER_H_ */
