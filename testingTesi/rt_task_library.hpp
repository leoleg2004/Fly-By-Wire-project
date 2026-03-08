#ifndef RT_TASK_LIBRARY_HPP
#define RT_TASK_LIBRARY_HPP

#include <string>
#include <vector>
#include <sched.h>

#include <cstdint>

// Execution metrics captured for every iteration
struct ActivityResult {
  int iteration;
  long start_time_ms;
  long end_time_ms;
  long cost_ms;
  bool missed_deadline;
  bool skipped;
};

// Task configuration parsed from XML and consumed by threads
struct TaskConfig {
  std::string name;
  long period_ms;
  int job;
  int iterations_to_run;
  long deadline_ms; // optional: used for reporting only
  std::vector<ActivityResult> results;

  // DDS fields
  bool use_dds{false};
  bool is_publisher{false};
  void *dds_writer{nullptr};
  void *dds_reader{nullptr};

  // Scheduling policy info
  int sched_policy{SCHED_OTHER};
  int sched_priority{0};
  bool use_deadline{false};
  uint64_t runtime_ns{0};
  uint64_t deadline_ns{0};
  uint64_t period_ns{0};
};

// Run the simplified app09-like workload with two threads
int RunTestMem(const std::string &policy, const std::string &affinity,
               const std::string &config_xml = "config.xml");

int RunTestDDS(const std::string &policy, const std::string &affinity,
               const std::string &config_xml = "config.xml");

#endif // RT_TASK_LIBRARY_HPP
