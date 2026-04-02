// F16KinematicsPubSubTypes.cxx
// Manual CDR serialization — no fastddsgen required
// Layout (CDR LE): [4 encap] [4 packet_id] [4*12 floats] = 56 bytes total
#include "F16KinematicsPubSubTypes.hpp"
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

using SerializedPayload_t   = eprosima::fastdds::rtps::SerializedPayload_t;
using InstanceHandle_t      = eprosima::fastdds::rtps::InstanceHandle_t;
using DataRepresentationId_t = eprosima::fastdds::dds::DataRepresentationId_t;

// Fixed on-wire size: 4 (encap) + 4 (id) + 12*4 (floats) = 56
static constexpr uint32_t WIRE_SIZE = 56;

F16KinematicsPubSubType::F16KinematicsPubSubType() {
  set_name("F16Kinematics");
  max_serialized_type_size = WIRE_SIZE;
  is_compute_key_provided  = false;
}

F16KinematicsPubSubType::~F16KinematicsPubSubType() {}

bool F16KinematicsPubSubType::serialize(const void *const data,
                                        SerializedPayload_t &payload,
                                        DataRepresentationId_t) {
  const F16Kinematics *s =
      static_cast<const F16Kinematics *>(data);
  eprosima::fastcdr::FastBuffer fb(
      reinterpret_cast<char *>(payload.data), payload.max_size);
  eprosima::fastcdr::Cdr ser(fb);
  try {
    ser.serialize_encapsulation();
    ser << s->packet_id;
    ser << s->roll;       ser << s->pitch;      ser << s->yaw;
    ser << s->roll_rate;  ser << s->pitch_rate; ser << s->yaw_rate;
    ser << s->u;          ser << s->v;          ser << s->w;
    ser << s->x;          ser << s->z;          ser << s->altitude;
  } catch (...) { return false; }
  payload.length = static_cast<uint32_t>(ser.get_serialized_data_length());
  return true;
}

bool F16KinematicsPubSubType::deserialize(SerializedPayload_t &payload,
                                          void *data) {
  F16Kinematics *s = static_cast<F16Kinematics *>(data);
  eprosima::fastcdr::FastBuffer fb(
      reinterpret_cast<char *>(payload.data), payload.length);
  eprosima::fastcdr::Cdr deser(fb);
  try {
    deser.read_encapsulation();
    deser >> s->packet_id;
    deser >> s->roll;       deser >> s->pitch;      deser >> s->yaw;
    deser >> s->roll_rate;  deser >> s->pitch_rate; deser >> s->yaw_rate;
    deser >> s->u;          deser >> s->v;          deser >> s->w;
    deser >> s->x;          deser >> s->z;          deser >> s->altitude;
  } catch (...) { return false; }
  return true;
}

uint32_t F16KinematicsPubSubType::calculate_serialized_size(
    const void *const, DataRepresentationId_t) {
  return WIRE_SIZE;
}

bool F16KinematicsPubSubType::compute_key(SerializedPayload_t &,
                                          InstanceHandle_t &, bool) {
  return false;
}
bool F16KinematicsPubSubType::compute_key(const void *const,
                                          InstanceHandle_t &, bool) {
  return false;
}

void *F16KinematicsPubSubType::create_data() {
  return reinterpret_cast<void *>(new F16Kinematics());
}
void F16KinematicsPubSubType::delete_data(void *data) {
  delete reinterpret_cast<F16Kinematics *>(data);
}
