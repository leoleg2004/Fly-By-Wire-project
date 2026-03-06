#include "TaskDataPubSubTypes.hpp"

#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

using SerializedPayload_t = eprosima::fastdds::rtps::SerializedPayload_t;
using InstanceHandle_t = eprosima::fastdds::rtps::InstanceHandle_t;
using DataRepresentationId_t = eprosima::fastdds::dds::DataRepresentationId_t;

TaskDataPubSubType::TaskDataPubSubType() {
  set_name("TaskData");
  // fixed size: 4 (packet_id) + 4 (workload) + 1 (bool) + padding (3) + 4
  uint32_t type_size = 16;
  max_serialized_type_size = type_size + 4; // encapsulation
  is_compute_key_provided = false;
}

TaskDataPubSubType::~TaskDataPubSubType() {}

bool TaskDataPubSubType::serialize(const void *const data, SerializedPayload_t &payload,
                                   DataRepresentationId_t /*data_representation*/) {
  const TaskData *td = static_cast<const TaskData *>(data);

  eprosima::fastcdr::FastBuffer fastbuffer(reinterpret_cast<char *>(payload.data),
                                           payload.max_size);
  eprosima::fastcdr::Cdr ser(fastbuffer);

  try {
    ser.serialize_encapsulation();
    ser << td->packet_id;
    ser << td->workload;
    ser << static_cast<uint8_t>(td->deadline_missed ? 1 : 0);
    ser << td->latency_ms;
  } catch (...) {
    return false;
  }

  payload.length = static_cast<uint32_t>(ser.get_serialized_data_length());
  return true;
}

bool TaskDataPubSubType::deserialize(SerializedPayload_t &payload, void *data) {
  TaskData *td = static_cast<TaskData *>(data);

  eprosima::fastcdr::FastBuffer fastbuffer(reinterpret_cast<char *>(payload.data),
                                           payload.length);
  eprosima::fastcdr::Cdr deser(fastbuffer);

  try {
    deser.read_encapsulation();
    deser >> td->packet_id;
    deser >> td->workload;
    uint8_t missed = 0;
    deser >> missed;
    td->deadline_missed = missed != 0;
    deser >> td->latency_ms;
  } catch (...) {
    return false;
  }

  return true;
}

uint32_t TaskDataPubSubType::calculate_serialized_size(
    const void *const /*data*/, DataRepresentationId_t /*data_representation*/) {
  return max_serialized_type_size;
}

bool TaskDataPubSubType::compute_key(SerializedPayload_t &, InstanceHandle_t &, bool) {
  return false;
}

bool TaskDataPubSubType::compute_key(const void *const, InstanceHandle_t &, bool) { return false; }

void *TaskDataPubSubType::create_data() { return new TaskData(); }

void TaskDataPubSubType::delete_data(void *data) { delete static_cast<TaskData *>(data); }
