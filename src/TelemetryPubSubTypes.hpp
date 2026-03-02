#ifndef _FAST_DDS_GENERATED_TELEMETRY_PUBSUBTYPES_HPP_
#define _FAST_DDS_GENERATED_TELEMETRY_PUBSUBTYPES_HPP_

#include <fastdds/dds/topic/TopicDataType.hpp>
#include "Telemetry.hpp"

class SystemStatsPubSubType : public eprosima::fastdds::dds::TopicDataType
{
public:
    SystemStatsPubSubType();
    virtual ~SystemStatsPubSubType() override;
    bool serialize(const void* const data, eprosima::fastdds::rtps::SerializedPayload_t& payload, eprosima::fastdds::dds::DataRepresentationId_t data_representation) override;
    bool deserialize(eprosima::fastdds::rtps::SerializedPayload_t& payload, void* data) override;
    uint32_t calculate_serialized_size(const void* const data, eprosima::fastdds::dds::DataRepresentationId_t data_representation) override;
    
    // Le due funzioni obbligatorie di FastDDS 3.x
    bool compute_key(eprosima::fastdds::rtps::SerializedPayload_t& payload, eprosima::fastdds::rtps::InstanceHandle_t& ihandle, bool force_md5) override;
    bool compute_key(const void* const data, eprosima::fastdds::rtps::InstanceHandle_t& ihandle, bool force_md5) override;
    
    void* create_data() override;
    void delete_data(void* data) override;
};
#endif
