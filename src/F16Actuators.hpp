#pragma once
// F16Actuators.hpp — plain struct, no fastddsgen dependency
// Matches F16Actuators.idl field for field
#include <cstdint>

struct F16Actuators {
  uint32_t packet_id{0};
  float ele{0.0f};   // [deg] stabilatore
  float ail{0.0f};   // [deg] flaperon
  float rud{0.0f};   // [deg] timone
  float lef{0.0f};   // [deg] LEF
  float thr{0.0f};   // [0,1] throttle
};
