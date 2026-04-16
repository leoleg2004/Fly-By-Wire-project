/*
 * ThreadAnalysisDDS.hpp
 *
 * Subscriber DDS per app10e: riceve ThreadEvent e calcola
 * statistiche di thread analysis in tempo reale.
 */

#ifndef THREAD_ANALYSIS_DDS_H_
#define THREAD_ANALYSIS_DDS_H_

#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <map>
#include <string>
#include <vector>

using namespace eprosima::fastdds::dds;

/*
 * Statistiche per un singolo task
 */
struct TaskStats {
  std::string name;
  long period_ms;

  double wcet_ms; // Worst-Case Execution Time
  double avg_cost_ms;
  double sum_cost_ms;
  int job_count;
  int miss_count;

  // Per calcolo jitter
  double last_start_ms;
  std::vector<double> periods_measured;

  TaskStats()
      : period_ms(0), wcet_ms(0), avg_cost_ms(0), sum_cost_ms(0), job_count(0),
        miss_count(0), last_start_ms(-1.0) {}
};

/*
 * Listener DDS: uno per topic, gestisce tutti i thread di app10e
 */
class ThreadAnalysisDDS : public eprosima::fastdds::dds::DataReaderListener {
public:
  ThreadAnalysisDDS();
  ~ThreadAnalysisDDS() override {}

  void on_subscription_matched(eprosima::fastdds::dds::DataReader *,
                               const SubscriptionMatchedStatus &info) override;

  void on_data_available(eprosima::fastdds::dds::DataReader *reader) override;

  void print_summary() const;

private:
  std::map<std::string, TaskStats> stats_;
};

#endif /* THREAD_ANALYSIS_DDS_H_ */
