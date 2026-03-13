#include "FlightControlComputer.hpp"
#include "FlightDisplay.hpp"
#include "F16KinematicsPubSubTypes.hpp"
#include "F16AeroStatePubSubTypes.hpp"
#include "F16ActuatorsPubSubTypes.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/attributes/PropertyPolicy.hpp>
#include <fastdds/statistics/dds/domain/DomainParticipant.hpp>
#include <fastdds/statistics/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/statistics/topic_names.hpp>
#include <thread>

using namespace eprosima::fastdds::dds;

static FlightControlComputer g_fcc;
static std::atomic<bool>     g_running{true};

// =========================================================================
// Helper — computes derived quantities from FlightState
// =========================================================================
static inline float compute_mach(float speed, float altitude) {
  float T_K   = (altitude < 11000.0f) ? (288.15f - 0.0065f * altitude) : 216.65f;
  float a     = std::sqrt(1.4f * 287.05f * T_K);
  return speed / a;
}

static inline float compute_nz(const FlightState &s, float V) {
  float nz = std::cos(s.pitch) * std::cos(s.roll)
             + s.pitch_rate * V / 9.806f;
  return std::clamp(nz, -3.0f, 9.0f);
}

// =========================================================================
// DDS Publish thread — 3 topics at ~20 Hz
// =========================================================================
struct F16Writers {
  DataWriter *kin;   // F16KinematicsTopic
  DataWriter *aero;  // F16AeroStateTopic
  DataWriter *act;   // F16ActuatorsTopic
};

void dds_publish_thread(F16Writers w) {
  while (g_running.load(std::memory_order_relaxed)) {
    FlightState state = g_fcc.get_state();
    ActuatorOut act   = g_fcc.get_actuator_state();

    float speed = std::sqrt(state.u * state.u +
                            state.v * state.v +
                            state.w * state.w);

    // ── F16KinematicsTopic ─────────────────────────────────
    F16Kinematics kin_msg;
    kin_msg.packet_id  = state.packet_id;
    kin_msg.roll       = state.roll;
    kin_msg.pitch      = state.pitch;
    kin_msg.yaw        = state.yaw;
    kin_msg.roll_rate  = state.roll_rate;
    kin_msg.pitch_rate = state.pitch_rate;
    kin_msg.yaw_rate   = state.yaw_rate;
    kin_msg.u          = state.u;
    kin_msg.v          = state.v;
    kin_msg.w          = state.w;
    kin_msg.x          = state.x;
    kin_msg.z          = state.z;
    kin_msg.altitude   = state.altitude;
    w.kin->write(&kin_msg);

    // ── F16AeroStateTopic ──────────────────────────────────
    F16AeroState aero_msg;
    aero_msg.packet_id     = state.packet_id;
    aero_msg.alpha         = state.alpha;
    aero_msg.beta          = state.beta;
    aero_msg.mach          = compute_mach(speed, state.altitude);
    aero_msg.speed_kts     = speed * 1.94384f;
    aero_msg.nz            = compute_nz(state, speed);
    aero_msg.system_active = state.system_active;
    aero_msg.landing_mode  = state.landing_mode;
    aero_msg.status_msg    = state.status_msg; // char[64] → std::string
    w.aero->write(&aero_msg);

    // ── F16ActuatorsTopic ──────────────────────────────────
    F16Actuators act_msg;
    act_msg.packet_id = state.packet_id;
    act_msg.ele       = act.ele;
    act_msg.ail       = act.ail;
    act_msg.rud       = act.rud;
    act_msg.lef       = act.lef;
    act_msg.thr       = act.thr;
    w.act->write(&act_msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // ~20 Hz
  }
}

// =========================================================================
// main
// =========================================================================
int main() {
  // ── 1. DDS Participant ────────────────────────────────────────────────
  DomainParticipantQos pqos;
  pqos.name("F16_Pilot_Node");
  pqos.properties().properties().emplace_back(
      "fastdds.statistics",
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
        eprosima::fastdds::statistics::PUBLICATION_THROUGHPUT_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_p->enable_statistics_datawriter(
        eprosima::fastdds::statistics::NETWORK_LATENCY_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_p->enable_statistics_datawriter(
        eprosima::fastdds::statistics::HISTORY_LATENCY_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_p->enable_statistics_datawriter(
        eprosima::fastdds::statistics::HEARTBEAT_COUNT_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
  }

  // ── 2. Register 3 types ───────────────────────────────────────────────
  TypeSupport type_kin (new F16KinematicsPubSubType());
  TypeSupport type_aero(new F16AeroStatePubSubType());
  TypeSupport type_act (new F16ActuatorsPubSubType());
  type_kin .register_type(participant);
  type_aero.register_type(participant);
  type_act .register_type(participant);

  // ── 3. Publisher + 3 Topics + 3 DataWriters ───────────────────────────
  Publisher *pub = participant->create_publisher(PUBLISHER_QOS_DEFAULT);

  // DEADLINE QoS: 50 ms (= 20 Hz publish cycle)
  DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
  wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
  wqos.deadline().period  = Duration_t(0, 50'000'000);
  wqos.properties().properties().emplace_back(
      "fastdds.statistics", "PUBLICATION_THROUGHPUT;HISTORY_LATENCY");

  Topic *topic_kin  = participant->create_topic(
      "F16KinematicsTopic",  type_kin .get_type_name(), TOPIC_QOS_DEFAULT);
  Topic *topic_aero = participant->create_topic(
      "F16AeroStateTopic",   type_aero.get_type_name(), TOPIC_QOS_DEFAULT);
  Topic *topic_act  = participant->create_topic(
      "F16ActuatorsTopic",   type_act .get_type_name(), TOPIC_QOS_DEFAULT);

  DataWriter *writer_kin  = pub->create_datawriter(topic_kin,  wqos);
  DataWriter *writer_aero = pub->create_datawriter(topic_aero, wqos);
  DataWriter *writer_act  = pub->create_datawriter(topic_act,  wqos);
  if (!writer_kin || !writer_aero || !writer_act) return 1;

  // ── 4. FCC initial state (grounded, engines off) ──────────────────────
  FlightState initial_state;
  initial_state.system_active = false;
  initial_state.landing_mode  = true;
  initial_state.altitude      = 0.0f;
  strncpy(initial_state.status_msg, "ENGINES SHUT DOWN", 63);
  g_fcc.set_initial_state(initial_state);
  std::cout << "[FBW INIT] landing_mode=true, engines=off, alt=0\n";

  // ── 5. DDS publish thread (20 Hz, non-blocking for render) ───────────
  std::thread dds_thread(dds_publish_thread,
                         F16Writers{writer_kin, writer_aero, writer_act});

  // ── 6. Render loop — 60 FPS ───────────────────────────────────────────
  // Flow per frame:
  //   A. Read FCC state → populate PlaneData (for audio in HandleInput)
  //   B. HandleInput → keys, audio, writes PilotInput
  //   C. g_fcc.step(pilot, dt) → physics + control law
  //   D. Read updated FCC state → populate PlaneData for rendering
  //   E. Draw
  FlightDisplay display(1000, 900, "F-16 Leo Flight System FBW");
  PlaneData     Aereo;
  PilotInput    pilot_input{};

  int   debug_frame_count = 0;
  float debug_elapsed     = 0.0f;

  // Lambda: populate Aereo from FCC state (deduplicates A and D)
  auto populate_plane_data = [&]() {
    FlightState state = g_fcc.get_state();
    ActuatorOut act   = g_fcc.get_actuator_state();

    Aereo.roll       = state.roll;
    Aereo.pitch      = state.pitch;
    Aereo.yaw        = state.yaw;
    Aereo.roll_rate  = state.roll_rate;
    Aereo.pitch_rate = state.pitch_rate;
    Aereo.yaw_rate   = state.yaw_rate;
    Aereo.u          = state.u;
    Aereo.v          = state.v;
    Aereo.w          = state.w;
    Aereo.alpha      = state.alpha;
    Aereo.beta       = state.beta;
    Aereo.altitude   = state.altitude;
    Aereo.x          = state.x;
    Aereo.z          = state.z;
    Aereo.speed      = std::sqrt(state.u * state.u +
                                 state.v * state.v +
                                 state.w * state.w);
    Aereo.mach       = compute_mach(Aereo.speed, state.altitude);
    Aereo.speed_kts  = Aereo.speed * 1.94384f;
    Aereo.nz         = compute_nz(state, Aereo.speed);

    Aereo.act_ele = act.ele;
    Aereo.act_ail = act.ail;
    Aereo.act_rud = act.rud;
    Aereo.act_lef = act.lef;
    Aereo.act_thr = act.thr;

    Aereo.system_active = state.system_active;
    Aereo.landing_mode  = state.landing_mode;
    strncpy(Aereo.status_msg, state.status_msg, 63);
    Aereo.status_msg[63] = '\0';
  };

  while (display.IsActive()) {
    float dt = GetFrameTime();
    if (std::abs(dt - (1.0f / 60.0f)) < 0.005f) dt = 1.0f / 60.0f;
    else if (dt > 0.1f)                           dt = 0.1f;

    debug_elapsed += dt;
    debug_frame_count++;
    if (debug_elapsed >= 2.0f) {
      g_fcc.debug_print();
      std::cout << "  [INPUT] roll=" << pilot_input.stick_roll
                << " pitch=" << pilot_input.stick_pitch
                << " thr=" << pilot_input.throttle_input
                << " eng=" << pilot_input.engines_on
                << " rdy=" << pilot_input.engine_ready
                << " land=" << pilot_input.landing_mode
                << " frame=" << debug_frame_count
                << " dt=" << std::fixed << std::setprecision(4) << dt << '\n';
      debug_elapsed     = 0.0f;
      debug_frame_count = 0;
    }

    populate_plane_data();            // A
    display.HandleInput(Aereo, pilot_input); // B
    g_fcc.step(pilot_input, dt);      // C
    populate_plane_data();            // D
    display.Draw(Aereo);              // E
  }

  // ── 7. Orderly shutdown ───────────────────────────────────────────────
  g_running.store(false, std::memory_order_relaxed);
  dds_thread.join();

  pub->delete_datawriter(writer_kin);
  pub->delete_datawriter(writer_aero);
  pub->delete_datawriter(writer_act);
  participant->delete_publisher(pub);
  participant->delete_topic(topic_kin);
  participant->delete_topic(topic_aero);
  participant->delete_topic(topic_act);
  DomainParticipantFactory::get_instance()->delete_participant(participant);

  return 0;
}
