#pragma once

#include <cstdint>

struct TaskData {
  uint32_t packet_id{0};
  int32_t workload{0};
  bool deadline_missed{false};
  float latency_ms{0.0f};
};
