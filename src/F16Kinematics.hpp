#pragma once
// F16Kinematics.hpp — plain struct, no fastddsgen dependency
// Matches F16Kinematics.idl field for field (serialized manually in PubSubTypes)
#include <cstdint>

struct F16Kinematics {
  uint32_t packet_id{0};
  float roll{0.0f}, pitch{0.0f}, yaw{0.0f};
  float roll_rate{0.0f}, pitch_rate{0.0f}, yaw_rate{0.0f};
  float u{0.0f}, v{0.0f}, w{0.0f};
  float x{0.0f}, z{0.0f}, altitude{0.0f};
};
