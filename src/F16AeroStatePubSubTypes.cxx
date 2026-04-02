// F16AeroStatePubSubTypes.cxx
// Manual CDR serialization — no fastddsgen required
// Layout (CDR LE): [4 encap] [4 id] [4*5 floats] [1+1 bools] [4+N string] — variable
#include "F16AeroStatePubSubTypes.hpp"
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

using SerializedPayload_t    = eprosima::fastdds::rtps::SerializedPayload_t;
using InstanceHandle_t       = eprosima::fastdds::rtps::InstanceHandle_t;
using DataRepresentationId_t = eprosima::fastdds::dds::DataRepresentationId_t;

// Max: 4 (encap) + 4 (id) + 20 (5 floats) + 2 (2 bools) + 2 (align pad)
//    + 4 (str len) + 64 (status_msg) = 100 → use 256 for safety
static constexpr uint32_t MAX_WIRE_SIZE = 256;

F16AeroStatePubSubType::F16AeroStatePubSubType() {
  set_name("F16AeroState");
  max_serialized_type_size = MAX_WIRE_SIZE;
  is_compute_key_provided  = false;
}

F16AeroStatePubSubType::~F16AeroStatePubSubType() {}

bool F16AeroStatePubSubType::serialize(const void *const data,
                                       SerializedPayload_t &payload,
                                       DataRepresentationId_t) {
  const F16AeroState *s = static_cast<const F16AeroState *>(data);
  eprosima::fastcdr::FastBuffer fb(
      reinterpret_cast<char *>(payload.data), payload.max_size);
  eprosima::fastcdr::Cdr ser(fb);
  try {
    ser.serialize_encapsulation();
    ser << s->packet_id;
    ser << s->alpha;     ser << s->beta;
    ser << s->mach;      ser << s->speed_kts;  ser << s->nz;
    ser << static_cast<uint8_t>(s->system_active ? 1 : 0);
    ser << static_cast<uint8_t>(s->landing_mode  ? 1 : 0);
    ser << s->status_msg;
  } catch (...) { return false; }
  payload.length = static_cast<uint32_t>(ser.get_serialized_data_length());
  return true;
}

bool F16AeroStatePubSubType::deserialize(SerializedPayload_t &payload,
                                         void *data) {
  F16AeroState *s = static_cast<F16AeroState *>(data);
  eprosima::fastcdr::FastBuffer fb(
      reinterpret_cast<char *>(payload.data), payload.length);
  eprosima::fastcdr::Cdr deser(fb);
  try {
    deser.read_encapsulation();
    deser >> s->packet_id;
    deser >> s->alpha;    deser >> s->beta;
    deser >> s->mach;     deser >> s->speed_kts;  deser >> s->nz;
    uint8_t sa = 0, lm = 0;
    deser >> sa; s->system_active = (sa != 0);
    deser >> lm; s->landing_mode  = (lm != 0);
    deser >> s->status_msg;
  } catch (...) { return false; }
  return true;
}

uint32_t F16AeroStatePubSubType::calculate_serialized_size(
    const void *const data, DataRepresentationId_t) {
  const F16AeroState *s = static_cast<const F16AeroState *>(data);
  // 4 encap + 4 id + 20 floats + 2 bools + 4 str_len + str_data
  return static_cast<uint32_t>(
      4 + 4 + 20 + 2 + 4 + s->status_msg.size() + 1);
}

bool F16AeroStatePubSubType::compute_key(SerializedPayload_t &,
                                         InstanceHandle_t &, bool) {
  return false;
}
bool F16AeroStatePubSubType::compute_key(const void *const,
                                         InstanceHandle_t &, bool) {
  return false;
}

void *F16AeroStatePubSubType::create_data() {
  return reinterpret_cast<void *>(new F16AeroState());
}
void F16AeroStatePubSubType::delete_data(void *data) {
  delete reinterpret_cast<F16AeroState *>(data);
}
