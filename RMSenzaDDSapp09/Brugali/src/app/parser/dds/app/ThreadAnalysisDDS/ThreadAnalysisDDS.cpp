/*
 * ThreadAnalysisDDS.cpp
 *
 * Subscriber DDS per l'analisi in tempo reale dei thread di app10e.
 *
 * Uso:
 *   Terminale 1: ./ThreadAnalysisDDS      (avviare PRIMA)
 *   Terminale 2: ./app10e                 (avviare DOPO)
 *
 * Il subscriber riceve ThreadEvent pubblicati da ogni thread di app10e
 * e calcola in tempo reale: WCET, periodo medio, jitter, deadline miss.
 */

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/attributes/SubscriberAttributes.h>

#include "Listener.hpp"
#include "ThreadAnalysisDDS.hpp"
#include <thread_msgsPubSubTypes.hpp>

#define DOMAIN_ID 1
#define TOPIC_NAME "App10eTopic"

// =========================================================================
// Implementazione ThreadAnalysisDDS
// =========================================================================

ThreadAnalysisDDS::ThreadAnalysisDDS() {}

void ThreadAnalysisDDS::on_subscription_matched(
    eprosima::fastdds::dds::DataReader *,
    const SubscriptionMatchedStatus &info) {
  if (info.current_count_change == 1) {
    std::cout << "[DDS] app10e publisher collegato." << std::endl;
  } else if (info.current_count_change == -1) {
    std::cout << "[DDS] app10e publisher scollegato." << std::endl;
    print_summary();
  }
}

void ThreadAnalysisDDS::on_data_available(
    eprosima::fastdds::dds::DataReader *reader) {
  SampleInfo info;
  ThreadEvent event;

  while (reader->take_next_sample(&event, &info) == ReturnCode_t::RETCODE_OK) {
    if (info.instance_state != ALIVE_INSTANCE_STATE)
      continue;

    const std::string name = event.task_name();
    TaskStats &s = stats_[name];
    s.name = name;
    s.period_ms = event.period_ms();

    // Aggiorna WCET e media costo
    double cost = event.cost_ms();
    s.sum_cost_ms += cost;
    s.job_count++;
    if (cost > s.wcet_ms)
      s.wcet_ms = cost;
    s.avg_cost_ms = s.sum_cost_ms / s.job_count;

    // Conta deadline miss
    if (event.missed_deadline())
      s.miss_count++;

    // Calcola periodo misurato (jitter)
    double t_start = event.start_time_ms();
    if (s.last_start_ms >= 0.0) {
      double meas_period = t_start - s.last_start_ms;
      s.periods_measured.push_back(meas_period);
    }
    s.last_start_ms = t_start;

    // Stampa riga per questo job
    std::cout << std::fixed << std::setprecision(2) << "[" << name << "] "
              << "job #" << s.job_count << "  cost=" << std::setw(8) << cost
              << " ms"
              << "  WCET=" << std::setw(8) << s.wcet_ms << " ms"
              << "  miss=" << s.miss_count
              << (event.missed_deadline() ? "  <<MISS>>" : "") << std::endl;
  }
}

void ThreadAnalysisDDS::print_summary() const {
  std::cout
      << "\n============================================================\n";
  std::cout << "   RIEPILOGO FINALE THREAD ANALYSIS (DDS)\n";
  std::cout << "============================================================\n";

  for (const auto &[name, s] : stats_) {
    // Calcola jitter
    double avg_period = 0.0, jitter = 0.0;
    if (!s.periods_measured.empty()) {
      for (double p : s.periods_measured)
        avg_period += p;
      avg_period /= s.periods_measured.size();
      for (double p : s.periods_measured)
        jitter += (p - avg_period) * (p - avg_period);
      jitter = std::sqrt(jitter / s.periods_measured.size());
    }

    std::cout
        << "\n----------------------------------------------------------\n";
    std::cout << " TASK: " << name << "\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << " [Esecuzione] WCET      : " << std::setw(10) << s.wcet_ms
              << " ms"
              << "  |  Avg: " << std::setw(10) << s.avg_cost_ms << " ms\n";
    std::cout << " [Periodo]    Configurato: " << std::setw(10) << s.period_ms
              << " ms"
              << "  |  Misurato: " << std::setw(10) << avg_period << " ms\n";
    std::cout << " [Jitter]                : " << std::setw(10) << jitter
              << " ms\n";
    std::cout << " [Deadline]   Miss       : " << std::setw(10) << s.miss_count
              << "  /  " << s.job_count << " job totali\n";
  }
  std::cout
      << "\n============================================================\n";
}

// =========================================================================
// main
// =========================================================================
int main(int argc, char **argv) {
  std::cout << "========================================================="
            << std::endl;
  std::cout << "  ThreadAnalysisDDS — Subscriber in ascolto su:" << std::endl;
  std::cout << "  Topic: " << TOPIC_NAME << "  |  Domain: " << DOMAIN_ID
            << std::endl;
  std::cout << "  Avvia app10e in un altro terminale." << std::endl;
  std::cout << "  Premi Invio per fermare e vedere il riepilogo." << std::endl;
  std::cout << "========================================================="
            << std::endl;

  eprosima::fastdds::dds::TypeSupport msg_type(new ThreadEventPubSubType());

  ThreadAnalysisDDS *analysis_listener = new ThreadAnalysisDDS();
  Listener listener(analysis_listener, &msg_type, "ThreadEvent");
  listener.start(TOPIC_NAME, DOMAIN_ID);

  analysis_listener->print_summary();
  delete analysis_listener;

  return 0;
}
