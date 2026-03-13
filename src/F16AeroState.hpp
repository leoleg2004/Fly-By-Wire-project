#pragma once
// F16AeroState.hpp — plain struct, no fastddsgen dependency
// Matches F16AeroState.idl field for field
#include <cstdint>
#include <string>

struct F16AeroState {
  uint32_t    packet_id{0};
  float       alpha{0.0f};       // [rad] AoA
  float       beta{0.0f};        // [rad] sideslip
  float       mach{0.0f};
  float       speed_kts{0.0f};
  float       nz{1.0f};          // [g] load factor
  bool        system_active{false};
  bool        landing_mode{false};
  std::string status_msg;
};
