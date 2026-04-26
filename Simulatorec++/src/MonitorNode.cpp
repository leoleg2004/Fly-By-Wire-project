#include "F16KinematicsPubSubTypes.hpp"
#include "F16AeroStatePubSubTypes.hpp"
#include "F16ActuatorsPubSubTypes.hpp"
#include "MonitorDisplay.hpp"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/attributes/PropertyPolicy.hpp>
#include <fastdds/statistics/dds/domain/DomainParticipant.hpp>
#include <fastdds/statistics/topic_names.hpp>
#include <fastdds/statistics/dds/publisher/qos/DataWriterQos.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <vector>

using namespace eprosima::fastdds::dds;

// =========================================================================
// Shared telemetry state (populated from the 3 DDS topics)s
// =========================================================================
static PlaneData    shared_aereo;
static float        shared_jitter    = 0.0f;
static float        shared_cycle_ms  = 0.0f;
static std::mutex   aereo_mutex;

// =========================================================================
// KinematicsListener — F16KinematicsTopic
// =========================================================================
class KinematicsListener : public DataReaderListener {
  std::chrono::steady_clock::time_point last_pkt;
  bool first = true;
  std::vector<float> jitter_history;
  long total = 0, missed = 0;

public:
  void on_data_available(DataReader *reader) override {
    F16Kinematics kin;
    SampleInfo    info;
    if (reader->take_next_sample(&kin, &info) != RETCODE_OK || !info.valid_data)
      return;

    auto  now        = std::chrono::steady_clock::now();
    float cycle_ms   = 0.0f;
    float jitter_ms  = 0.0f;

    if (!first) {
      long us  = std::chrono::duration_cast<std::chrono::microseconds>(
                     now - last_pkt).count();
      cycle_ms  = us / 1000.0f;
      jitter_ms = std::abs(cycle_ms - 50.0f);
      jitter_history.push_back(jitter_ms);
      if (jitter_history.size() > 20) jitter_history.erase(jitter_history.begin());
    }
    last_pkt = now;
    first    = false;
    total++;

    float avg_jitter = 0.0f;
    if (!jitter_history.empty()) {
      float sum = std::accumulate(jitter_history.begin(),
                                  jitter_history.end(), 0.0f);
      avg_jitter = sum / jitter_history.size();
    }

    {
      std::lock_guard<std::mutex> lock(aereo_mutex);
      shared_aereo.roll       = kin.roll;
      shared_aereo.pitch      = kin.pitch;
      shared_aereo.yaw        = kin.yaw;
      shared_aereo.roll_rate  = kin.roll_rate;
      shared_aereo.pitch_rate = kin.pitch_rate;
      shared_aereo.yaw_rate   = kin.yaw_rate;
      shared_aereo.u          = kin.u;
      shared_aereo.v          = kin.v;
      shared_aereo.w          = kin.w;
      shared_aereo.x          = kin.x;
      shared_aereo.z          = kin.z;
      shared_aereo.altitude   = kin.altitude;
      shared_aereo.speed      = std::sqrt(kin.u * kin.u +
                                          kin.v * kin.v +
                                          kin.w * kin.w);
      shared_jitter           = avg_jitter;
      shared_cycle_ms         = cycle_ms;
    }

    float loss = (total > 0) ? (float)missed / total * 100.0f : 0.0f;
    std::cout << "\033[2J\033[1;1H";
    std::cout << "\033[1;44m"
              << "========== F-16 TORRE DI CONTROLLO — MONITORAGGIO REAL-TIME ==========\n"
              << "\033[0m\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << " KINEMATICS | roll=" << kin.roll * 57.3f << "° "
              << "pitch=" << kin.pitch * 57.3f << "° "
              << "yaw=" << kin.yaw * 57.3f << "°\n";
    std::cout << "            | p=" << kin.roll_rate << " q=" << kin.pitch_rate
              << " r=" << kin.yaw_rate << " rad/s\n";
    std::cout << "            | alt=" << kin.altitude << " m  "
              << "x=" << kin.x << " z=" << kin.z << " m\n";
    std::cout << " JITTER     | cycle=" << cycle_ms << " ms  "
              << "avg_jitter=" << avg_jitter << " ms  loss=" << loss << "%\n";
  }
};

// =========================================================================
// AeroStateListener — F16AeroStateTopic
// =========================================================================
class AeroStateListener : public DataReaderListener {
public:
  void on_data_available(DataReader *reader) override {
    F16AeroState aero;
    SampleInfo   info;
    if (reader->take_next_sample(&aero, &info) != RETCODE_OK || !info.valid_data)
      return;

    {
      std::lock_guard<std::mutex> lock(aereo_mutex);
      shared_aereo.alpha         = aero.alpha;
      shared_aereo.beta          = aero.beta;
      shared_aereo.mach          = aero.mach;
      shared_aereo.speed_kts     = aero.speed_kts;
      shared_aereo.nz            = aero.nz;
      shared_aereo.system_active = aero.system_active;
      shared_aereo.landing_mode  = aero.landing_mode;
      snprintf(shared_aereo.status_msg, sizeof(shared_aereo.status_msg),
               "%s", aero.status_msg.c_str());
    }

    bool alarm_crit = (aero.status_msg.find("PULL UP") != std::string::npos ||
                       aero.status_msg.find("ALARM")   != std::string::npos ||
                       aero.status_msg.find("HIGH ALT") != std::string::npos);
    bool alarm_warn = (aero.status_msg.find("WARN") != std::string::npos ||
                       shared_jitter > 5.0f);

    if      (alarm_crit)         std::cout << "\033[1;31m";
    else if (alarm_warn)         std::cout << "\033[1;33m";
    else if (aero.landing_mode)  std::cout << "\033[1;35m";
    else                         std::cout << "\033[1;32m";

    std::cout << std::fixed << std::setprecision(3);
    std::cout << " AERO STATE | alpha=" << aero.alpha * 57.3f << "°  "
              << "beta=" << aero.beta * 57.3f << "°  "
              << "Mach=" << aero.mach << "\n";
    std::cout << "            | CAS=" << aero.speed_kts << " kt  "
              << "Nz=" << aero.nz << " g\n";
    std::cout << " COND VOLO  | " << aero.status_msg
              << (aero.landing_mode ? " [LANDING]" : "") << "\033[0m\n";
  }
};

// =========================================================================
// ActuatorsListener — F16ActuatorsTopic
// =========================================================================
class ActuatorsListener : public DataReaderListener {
public:
  void on_data_available(DataReader *reader) override {
    F16Actuators act;
    SampleInfo   info;
    if (reader->take_next_sample(&act, &info) != RETCODE_OK || !info.valid_data)
      return;

    {
      std::lock_guard<std::mutex> lock(aereo_mutex);
      shared_aereo.act_ele = act.ele;
      shared_aereo.act_ail = act.ail;
      shared_aereo.act_rud = act.rud;
      shared_aereo.act_lef = act.lef;
      shared_aereo.act_thr = act.thr;
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << " ACTUATORS  | ele=" << act.ele << "°  "
              << "ail=" << act.ail << "°  "
              << "rud=" << act.rud << "°  "
              << "lef=" << act.lef << "°  "
              << "thr=" << act.thr * 100.0f << "%\n";
    std::cout << "====================================================================\n";
  }
};

// =========================================================================
// main
// =========================================================================
int main() {
  DomainParticipantQos pqos;
  pqos.name("F16_Monitor_Node");
  pqos.properties().properties().emplace_back("fastdds.statistics",
      "HISTORY_LATENCY;NETWORK_LATENCY;PUBLICATION_THROUGHPUT;"
      "SUBSCRIPTION_THROUGHPUT;HEARTBEAT_COUNT;ACKNACK_COUNT;"
      "DISCOVERY_STATISTICS;PHYSICAL_DATA_STATISTICS");

  DomainParticipant *participant =
      DomainParticipantFactory::get_instance()->create_participant(0, pqos);
  if (participant == nullptr) return 1;

  auto *stat_p =
      eprosima::fastdds::statistics::dds::DomainParticipant::narrow(participant);
  if (stat_p != nullptr) {
    stat_p->enable_statistics_datawriter(
        eprosima::fastdds::statistics::SUBSCRIPTION_THROUGHPUT_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_p->enable_statistics_datawriter(
        eprosima::fastdds::statistics::ACKNACK_COUNT_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_p->enable_statistics_datawriter(
        eprosima::fastdds::statistics::HISTORY_LATENCY_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_p->enable_statistics_datawriter(
        eprosima::fastdds::statistics::NETWORK_LATENCY_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
  }

  // ── Register 3 types ──────────────────────────────────────────────────
  TypeSupport type_kin (new F16KinematicsPubSubType());
  TypeSupport type_aero(new F16AeroStatePubSubType());
  TypeSupport type_act (new F16ActuatorsPubSubType());
  type_kin .register_type(participant);
  type_aero.register_type(participant);
  type_act .register_type(participant);

  // ── Subscriber + 3 DataReaders ────────────────────────────────────────
  Subscriber *sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);

  DataReaderQos dr_qos = DATAREADER_QOS_DEFAULT;
  dr_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
  dr_qos.durability().kind  = VOLATILE_DURABILITY_QOS;
  dr_qos.properties().properties().emplace_back(
      "fastdds.statistics", "SUBSCRIPTION_THROUGHPUT;HISTORY_LATENCY;ACKNACK_COUNT");

  Topic *topic_kin  = participant->create_topic(
      "F16KinematicsTopic",  type_kin .get_type_name(), TOPIC_QOS_DEFAULT);
  Topic *topic_aero = participant->create_topic(
      "F16AeroStateTopic",   type_aero.get_type_name(), TOPIC_QOS_DEFAULT);
  Topic *topic_act  = participant->create_topic(
      "F16ActuatorsTopic",   type_act .get_type_name(), TOPIC_QOS_DEFAULT);

  KinematicsListener listener_kin;
  AeroStateListener  listener_aero;
  ActuatorsListener  listener_act;

  DataReader *reader_kin  = sub->create_datareader(topic_kin,  dr_qos, &listener_kin);
  DataReader *reader_aero = sub->create_datareader(topic_aero, dr_qos, &listener_aero);
  DataReader *reader_act  = sub->create_datareader(topic_act,  dr_qos, &listener_act);

  MonitorDisplay display(900, 750, "Torre di Controllo — F-16 Telemetria");

  while (display.IsActive()) {
    PlaneData local;
    {
      std::lock_guard<std::mutex> lock(aereo_mutex);
      local = shared_aereo;
    }
    display.Draw(local);
  }

  sub->delete_datareader(reader_kin);
  sub->delete_datareader(reader_aero);
  sub->delete_datareader(reader_act);
  participant->delete_subscriber(sub);
  participant->delete_topic(topic_kin);
  participant->delete_topic(topic_aero);
  participant->delete_topic(topic_act);
  DomainParticipantFactory::get_instance()->delete_participant(participant);

  return 0;
}
