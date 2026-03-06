#pragma once

#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/rtps/common/InstanceHandle.hpp>
#include <fastdds/rtps/common/SerializedPayload.hpp>
#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/utils/md5.hpp>

#include "TaskData.hpp"

class TaskDataPubSubType : public eprosima::fastdds::dds::TopicDataType {
public:
  using type = TaskData;

  TaskDataPubSubType();
  ~TaskDataPubSubType() override;

  bool serialize(const void *const data,
                 eprosima::fastdds::rtps::SerializedPayload_t &payload,
                 eprosima::fastdds::dds::DataRepresentationId_t data_representation) override;

  bool deserialize(eprosima::fastdds::rtps::SerializedPayload_t &payload,
                   void *data) override;

  uint32_t calculate_serialized_size(
      const void *const data,
      eprosima::fastdds::dds::DataRepresentationId_t data_representation) override;

  bool compute_key(eprosima::fastdds::rtps::SerializedPayload_t &payload,
                   eprosima::fastdds::rtps::InstanceHandle_t &ihandle,
                   bool force_md5 = false) override;

  bool compute_key(const void *const data,
                   eprosima::fastdds::rtps::InstanceHandle_t &ihandle,
                   bool force_md5 = false) override;

  void *create_data() override;
  void delete_data(void *data) override;

#ifdef TOPIC_DATA_TYPE_API_HAS_IS_BOUNDED
  inline bool is_bounded() const override { return true; }
#endif

#ifdef TOPIC_DATA_TYPE_API_HAS_IS_PLAIN
  inline bool is_plain(
      eprosima::fastdds::dds::DataRepresentationId_t /*data_representation*/) const override {
    return true;
  }
#endif

#ifdef TOPIC_DATA_TYPE_API_HAS_CONSTRUCT_SAMPLE
  inline bool construct_sample(void *memory) const override {
    new (memory) TaskData();
    return true;
  }
#endif
};
