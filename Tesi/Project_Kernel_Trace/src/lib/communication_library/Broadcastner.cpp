/*
 * Broadcastner.cpp
 */

#include "Broadcastner.hpp"
#include "/home/leonardo/eprosima_projects/flight_sensor/flight_sensor/Tesi/src/lib/trace_marker.h"

#include <iostream>
#include <limits>
#include <sstream>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>

using namespace eprosima::fastdds::dds;

Broadcastner::Broadcastner()
    : participant_(nullptr), publisher_(nullptr), topic_(nullptr),
      writer_(nullptr) {}

bool Broadcastner::start(eprosima::fastdds::dds::TypeSupport *msg_type,
                         std::string msg_name, std::string topic,
                         int domain_id) {
  type_ = msg_type;
  msg_name_ = msg_name;

  DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
  pqos.name("Participant_pub");

  pqos.properties().properties().emplace_back("fastdds.statistics",
                                              "HISTORY_LATENCY_TOPIC;"
                                              "NETWORK_LATENCY_TOPIC;"
                                              "PUBLICATION_THROUGHPUT_TOPIC;"
                                              "SUBSCRIPTION_THROUGHPUT_TOPIC;"
                                              "RTPS_SENT_TOPIC;"
                                              "RTPS_LOST_TOPIC;"
                                              "RESENT_DATAS_TOPIC;"
                                              "DISCOVERY_TOPIC;"
                                              "PHYSICAL_DATA_TOPIC");

  auto factory = DomainParticipantFactory::get_instance();

  participant_ = factory->create_participant(domain_id, pqos);

  if (participant_ == nullptr) {
    return false;
  }

  // REGISTER THE TYPE
  type_->register_type(participant_, msg_name_);

  // CREATE THE PUBLISHER
  PublisherQos pubqos = PUBLISHER_QOS_DEFAULT;

  publisher_ = participant_->create_publisher(pubqos, nullptr);

  if (publisher_ == nullptr) {
    return false;
  }

  // CREATE THE TOPIC
  TopicQos tqos = TOPIC_QOS_DEFAULT;

  topic_ = participant_->create_topic(topic, msg_name_, tqos);

  if (topic_ == nullptr) {
    return false;
  }

  // CREATE THE WRITER
  DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;

  writer_ = publisher_->create_datawriter(topic_, wqos, &listener_);

  if (writer_ == nullptr)
    return false;

  std::cout << "\Broadcastner running." << std::endl;

  return true;
}

bool Broadcastner::publish(void *const msg_data) {
  if (listener_.firstConnected_) {
    write_trace_marker("DDS_WRITE_START");
    writer_->write(msg_data);
    write_trace_marker("DDS_WRITE_END");
    return true;
  }
  return false;
}

Broadcastner::~Broadcastner() {
  if (writer_ != nullptr) {
    publisher_->delete_datawriter(writer_);
  }
  if (publisher_ != nullptr) {
    participant_->delete_publisher(publisher_);
  }
  if (topic_ != nullptr) {
    participant_->delete_topic(topic_);
  }
  DomainParticipantFactory::get_instance()->delete_participant(participant_);
}

void Broadcastner::PubListener::on_publication_matched(
    eprosima::fastdds::dds::DataWriter *,
    const eprosima::fastdds::dds::PublicationMatchedStatus &info) {
  if (info.current_count_change == 1) {
    matched_ = info.total_count;
    firstConnected_ = true;
    std::cout << "Publisher matched." << std::endl;
  } else if (info.current_count_change == -1) {
    matched_ = info.total_count;
    std::cout << "Publisher unmatched." << std::endl;
  } else {
    std::cout << info.current_count_change
              << " is not a valid value for PublicationMatchedStatus current "
                 "count change"
              << std::endl;
  }
}
