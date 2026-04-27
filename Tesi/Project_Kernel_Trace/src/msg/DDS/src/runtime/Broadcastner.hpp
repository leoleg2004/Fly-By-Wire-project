/*
 * Broadcastner.hpp
 */

#ifndef ERTS_DDS_PUBLISHER_H_
#define ERTS_DDS_PUBLISHER_H_

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>

class Broadcastner
{
public:

	Broadcastner();

    virtual ~Broadcastner();

    // Initialize
    bool start(eprosima::fastdds::dds::TypeSupport* msg_type, std::string msg_name, std::string topic, int domain_id);

    //!Publish a sample
    bool publish(void* const msg_data);


private:

    eprosima::fastdds::dds::DomainParticipant* participant_;

    eprosima::fastdds::dds::Publisher* publisher_;

    eprosima::fastdds::dds::Topic* topic_;

    eprosima::fastdds::dds::DataWriter* writer_;

    eprosima::fastdds::dds::TypeSupport* type_;

    std::string msg_name_;

    class PubListener : public eprosima::fastdds::dds::DataWriterListener {
    public:
        PubListener() : matched_(0), firstConnected_(false) { }

        ~PubListener() override { }

        void on_publication_matched(
                eprosima::fastdds::dds::DataWriter* writer,
                const eprosima::fastdds::dds::PublicationMatchedStatus& info) override;

        int matched_;

        bool firstConnected_;
    } listener_;

};



#endif
