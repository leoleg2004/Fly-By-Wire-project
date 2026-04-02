#include "TelemetryPubSubTypes.hpp"
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>
#include <fastdds/rtps/common/SerializedPayload.hpp>

SystemStatsPubSubType::SystemStatsPubSubType()
{
    set_name("SystemStats");
    auto type_size = SystemStats::getMaxCdrSerializedSize();
    type_size += eprosima::fastcdr::Cdr::alignment(type_size, 4);
    max_serialized_type_size = static_cast<uint32_t>(type_size) + 4;
}

SystemStatsPubSubType::~SystemStatsPubSubType() {}

bool SystemStatsPubSubType::serialize(const void* const data, eprosima::fastdds::rtps::SerializedPayload_t& payload, eprosima::fastdds::dds::DataRepresentationId_t data_representation)
{
    SystemStats* p_type = const_cast<SystemStats*>(static_cast<const SystemStats*>(data));
    eprosima::fastcdr::FastBuffer fastbuffer(reinterpret_cast<char*>(payload.data), payload.max_size);
    eprosima::fastcdr::Cdr ser(fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN);
    payload.encapsulation = ser.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
    ser.serialize_encapsulation();
    p_type->serialize(ser);
    payload.length = static_cast<uint32_t>(ser.get_serialized_data_length());
    return true;
}

bool SystemStatsPubSubType::deserialize(eprosima::fastdds::rtps::SerializedPayload_t& payload, void* data)
{
    SystemStats* p_type = static_cast<SystemStats*>(data);
    eprosima::fastcdr::FastBuffer fastbuffer(reinterpret_cast<char*>(payload.data), payload.length);
    eprosima::fastcdr::Cdr deser(fastbuffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN);
    deser.read_encapsulation();
    payload.encapsulation = deser.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
    p_type->deserialize(deser);
    return true;
}

uint32_t SystemStatsPubSubType::calculate_serialized_size(const void* const data, eprosima::fastdds::dds::DataRepresentationId_t data_representation)
{
    (void)data_representation;
    SystemStats* p_type = const_cast<SystemStats*>(static_cast<const SystemStats*>(data));
    return static_cast<uint32_t>(SystemStats::getCdrSerializedSize(*p_type)) + 4;
}

bool SystemStatsPubSubType::compute_key(eprosima::fastdds::rtps::SerializedPayload_t& payload, eprosima::fastdds::rtps::InstanceHandle_t& ihandle, bool force_md5)
{
    (void)payload; (void)ihandle; (void)force_md5;
    return false;
}

bool SystemStatsPubSubType::compute_key(const void* const data, eprosima::fastdds::rtps::InstanceHandle_t& ihandle, bool force_md5)
{
    (void)data; (void)ihandle; (void)force_md5;
    return false;
}

void* SystemStatsPubSubType::create_data()
{
    return reinterpret_cast<void*>(new SystemStats());
}

void SystemStatsPubSubType::delete_data(void* data)
{
    delete(reinterpret_cast<SystemStats*>(data));
}
