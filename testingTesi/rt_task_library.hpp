#ifndef RT_TASK_LIBRARY_HPP
#define RT_TASK_LIBRARY_HPP

#include <string>
#include <vector>

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
  int parameter;
  int iterations_to_run;
  long deadline_ms; // optional: used for reporting only
  std::vector<ActivityResult> results;

  // DDS fields
  bool use_dds{false};
  bool is_publisher{false};
  void *dds_writer{nullptr};
  void *dds_reader{nullptr};
};

// Run the simplified app09-like workload with two threads
int RunTestMem(const std::string &policy, const std::string &affinity,
               const std::string &config_xml = "config.xml");

int RunTestDDS(const std::string &policy, const std::string &affinity,
               const std::string &config_xml = "config.xml");

#endif // RT_TASK_LIBRARY_HPP
