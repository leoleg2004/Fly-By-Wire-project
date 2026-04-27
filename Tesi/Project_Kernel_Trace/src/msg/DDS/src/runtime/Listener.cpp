/*
 * Listener.cpp
 */

#include "Listener.hpp"

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

using namespace eprosima::fastdds::dds;

Listener::Listener(eprosima::fastdds::dds::DataReaderListener* listener, eprosima::fastdds::dds::TypeSupport* msg_type, std::string msg_name) : participant_(nullptr)
, subscriber_(nullptr)
, topic_(nullptr)
, reader_(nullptr)

{
	listener_ = listener;
	//type_ = new eprosima::fastdds::dds::TypeSupport(msg_type);
	type_ = msg_type;
	msg_name_ = msg_name;
}



bool Listener::start(std::string topic, int domain_id) {
    DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    pqos.name("Participant_pub");
    auto factory = DomainParticipantFactory::get_instance();

    participant_ = factory->create_participant(domain_id, pqos);

    if (participant_ == nullptr)
        return false;

    //REGISTER THE TYPE
    type_->register_type(participant_);

    //CREATE THE SUBSCRIBER
    SubscriberQos sqos = SUBSCRIBER_QOS_DEFAULT;

    subscriber_ = participant_->create_subscriber(sqos, nullptr);

    if (subscriber_ == nullptr)
        return false;

    //CREATE THE TOPIC
    TopicQos tqos = TOPIC_QOS_DEFAULT;

    topic_ = participant_->create_topic(topic, msg_name_, tqos);

    if (topic_ == nullptr)
        return false;

    // CREATE THE READER
    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;

    reader_ = subscriber_->create_datareader(topic_, rqos, listener_);

    if (reader_ == nullptr)
        return false;

	return true;
}


Listener::~Listener() {
    if (reader_ != nullptr)
        subscriber_->delete_datareader(reader_);
    if (topic_ != nullptr)
        participant_->delete_topic(topic_);
    if (subscriber_ != nullptr)
        participant_->delete_subscriber(subscriber_);
    DomainParticipantFactory::get_instance()->delete_participant(participant_);
}
