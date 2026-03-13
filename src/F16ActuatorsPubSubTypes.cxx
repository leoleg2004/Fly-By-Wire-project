// F16ActuatorsPubSubTypes.cxx
// Manual CDR serialization — no fastddsgen required
// Layout (CDR LE): [4 encap] [4 packet_id] [4*5 floats] = 32 bytes total
#include "F16ActuatorsPubSubTypes.hpp"
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

using SerializedPayload_t    = eprosima::fastdds::rtps::SerializedPayload_t;
using InstanceHandle_t       = eprosima::fastdds::rtps::InstanceHandle_t;
using DataRepresentationId_t = eprosima::fastdds::dds::DataRepresentationId_t;

static constexpr uint32_t WIRE_SIZE = 32;

F16ActuatorsPubSubType::F16ActuatorsPubSubType() {
  set_name("F16Actuators");
  max_serialized_type_size = WIRE_SIZE;
  is_compute_key_provided  = false;
}

F16ActuatorsPubSubType::~F16ActuatorsPubSubType() {}

bool F16ActuatorsPubSubType::serialize(const void *const data,
                                       SerializedPayload_t &payload,
                                       DataRepresentationId_t) {
  const F16Actuators *s = static_cast<const F16Actuators *>(data);
  eprosima::fastcdr::FastBuffer fb(
      reinterpret_cast<char *>(payload.data), payload.max_size);
  eprosima::fastcdr::Cdr ser(fb);
  try {
    ser.serialize_encapsulation();
    ser << s->packet_id;
    ser << s->ele;  ser << s->ail;  ser << s->rud;
    ser << s->lef;  ser << s->thr;
  } catch (...) { return false; }
  payload.length = static_cast<uint32_t>(ser.get_serialized_data_length());
  return true;
}

bool F16ActuatorsPubSubType::deserialize(SerializedPayload_t &payload,
                                         void *data) {
  F16Actuators *s = static_cast<F16Actuators *>(data);
  eprosima::fastcdr::FastBuffer fb(
      reinterpret_cast<char *>(payload.data), payload.length);
  eprosima::fastcdr::Cdr deser(fb);
  try {
    deser.read_encapsulation();
    deser >> s->packet_id;
    deser >> s->ele;  deser >> s->ail;  deser >> s->rud;
    deser >> s->lef;  deser >> s->thr;
  } catch (...) { return false; }
  return true;
}

uint32_t F16ActuatorsPubSubType::calculate_serialized_size(
    const void *const, DataRepresentationId_t) {
  return WIRE_SIZE;
}

bool F16ActuatorsPubSubType::compute_key(SerializedPayload_t &,
                                         InstanceHandle_t &, bool) {
  return false;
}
bool F16ActuatorsPubSubType::compute_key(const void *const,
                                         InstanceHandle_t &, bool) {
  return false;
}

void *F16ActuatorsPubSubType::create_data() {
  return reinterpret_cast<void *>(new F16Actuators());
}
void F16ActuatorsPubSubType::delete_data(void *data) {
  delete reinterpret_cast<F16Actuators *>(data);
}
