#pragma once
// F16KinematicsPubSubTypes.hpp
// Serialization support for F16Kinematics over FastDDS 3.x
// Pattern: testingTesi/TaskDataPubSubTypes — plain struct, manual Cdr
#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/rtps/common/InstanceHandle.hpp>
#include <fastdds/rtps/common/SerializedPayload.hpp>
#include "F16Kinematics.hpp"

class F16KinematicsPubSubType : public eprosima::fastdds::dds::TopicDataType {
public:
  using type = F16Kinematics;

  F16KinematicsPubSubType();
  ~F16KinematicsPubSubType() override;

  bool serialize(const void *const data,
                 eprosima::fastdds::rtps::SerializedPayload_t &payload,
                 eprosima::fastdds::dds::DataRepresentationId_t) override;

  bool deserialize(eprosima::fastdds::rtps::SerializedPayload_t &payload,
                   void *data) override;

  uint32_t calculate_serialized_size(
      const void *const data,
      eprosima::fastdds::dds::DataRepresentationId_t) override;

  bool compute_key(eprosima::fastdds::rtps::SerializedPayload_t &payload,
                   eprosima::fastdds::rtps::InstanceHandle_t &ihandle,
                   bool force_md5 = false) override;
  bool compute_key(const void *const data,
                   eprosima::fastdds::rtps::InstanceHandle_t &ihandle,
                   bool force_md5 = false) override;

  void *create_data() override;
  void  delete_data(void *data) override;
};
