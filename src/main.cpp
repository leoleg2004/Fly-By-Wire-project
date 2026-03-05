#include "FlightControlComputer.hpp"
#include "FlightDisplay.hpp"
#include "TelemetryPubSubTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
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

// Shared variables between render and DDS threads
static FlightControlComputer g_fcc; // FCC — il render lo chiama, DDS lo legge
static std::atomic<bool> g_running{true}; // Flag shutdown ordinato

// DDS Telemetry publisher thread (~20Hz)
// Reads FCC state and publishes it without blocking the render loop
void dds_publish_thread(DataWriter *writer) {
  while (g_running.load(std::memory_order_relaxed)) {
    FlightState state = g_fcc.get_state();

    SystemStats stats;
    stats.packet_id(state.packet_id);
    stats.roll(state.roll);
    stats.pitch(state.pitch);
    stats.yaw(state.yaw);
    stats.altitude(state.altitude);
    stats.speed(state.speed);
    stats.x(state.x);
    stats.z(state.z);
    stats.landing_mode(state.landing_mode);
    stats.system_active(state.system_active);
    stats.status_msg(state.status_msg);
    writer->write(&stats);

    // ~20 Hz target rate
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

int main() {
  // 1. DDS Setup
  DomainParticipantQos pqos;
  pqos.name("Pilot_Node_F35");
  pqos.properties().properties().emplace_back(
      "fastdds.statistics",
      "HISTORY_LATENCY;NETWORK_LATENCY;PUBLICATION_THROUGHPUT;"
      "SUBSCRIPTION_THROUGHPUT;HEARTBEAT_COUNT;ACKNACK_COUNT;"
      "DISCOVERY_STATISTICS;PHYSICAL_DATA_STATISTICS");

  DomainParticipant *participant =
      DomainParticipantFactory::get_instance()->create_participant(0, pqos);
  if (participant == nullptr)
    return 1;

  auto *stat_participant =
      eprosima::fastdds::statistics::dds::DomainParticipant::narrow(
          participant);
  if (stat_participant != nullptr) {
    stat_participant->enable_statistics_datawriter(
        eprosima::fastdds::statistics::PUBLICATION_THROUGHPUT_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_participant->enable_statistics_datawriter(
        eprosima::fastdds::statistics::NETWORK_LATENCY_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_participant->enable_statistics_datawriter(
        eprosima::fastdds::statistics::HISTORY_LATENCY_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    stat_participant->enable_statistics_datawriter(
        eprosima::fastdds::statistics::HEARTBEAT_COUNT_TOPIC,
        eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
  }

  TypeSupport type(new SystemStatsPubSubType());
  type.register_type(participant);

  Publisher *pub = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
  Topic *topic = participant->create_topic(
      "TelemetryTopic", type.get_type_name(), TOPIC_QOS_DEFAULT);

  DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
  wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
  // DEADLINE QoS: il middleware avvisa se l'aggiornamento supera i 50ms
  wqos.deadline().period = eprosima::fastdds::dds::Duration_t(0, 50'000'000);
  wqos.properties().properties().emplace_back(
      "fastdds.statistics", "PUBLICATION_THROUGHPUT;HISTORY_LATENCY");

  DataWriter *writer = pub->create_datawriter(topic, wqos);
  if (writer == nullptr)
    return 1;

  // =========================================================================
  // 2. Inizializzazione FCC — Assetto "Grounded" corretto
  //
  // BUG FIX: lo stato iniziale deve rispecchiare un aereo FERMO IN PISTA:
  //   - landing_mode = true  → ILS/ground logic attiva, GPWS disabilitato
  //   - system_active = false → motori SPENTI (il pilota preme E per accendere)
  //   - altitude = 0          → a terra
  // =========================================================================
  FlightState initial_state;
  initial_state.system_active = false; // Motori spenti all'avvio — premi E
  initial_state.landing_mode = true;   // ILS + ground mode attivo
  initial_state.altitude = 0.0f;
  strncpy(initial_state.status_msg, "ENGINES SHUT DOWN", 63);
  g_fcc.set_initial_state(initial_state);
  std::cout << "[FBW INIT] Stato iniziale spawn: landing_mode=true, "
               "engines=off, alt=0"
            << std::endl;

  // =========================================================================
  // 3. Avvio thread DDS (solo publish, senza fisica)
  // =========================================================================
  std::thread dds_thread(dds_publish_thread, writer);

  // =========================================================================
  // 4. Loop di rendering — 60 FPS
  //
  // Il FCC viene chiamato QUI, non in un thread separato.
  // Questo elimina i salti discreti di posizione che causavano il
  // movimento "a scatti" quando fisica (100Hz) e render (60Hz) erano
  // su thread diversi.
  //
  // Flusso per ogni frame:
  //   A. Leggi stato FCC → aggiorna PlaneData per audio e rendering
  //   B. HandleInput → cattura tasti, audio, scrive PilotInput
  //   C. g_fcc.step(pilot, dt) → FCC esegue physics + control law
  //   D. Leggi stato FCC aggiornato → aggiorna PlaneData per disegno
  //   E. Draw
  // =========================================================================
  FlightDisplay display(1000, 900, "Leonardo Flight System - FBW");
  PlaneData Aereo;
  PilotInput pilot_input{};

  Aereo.roll = 0.0f;
  Aereo.pitch = 0.0f;
  Aereo.yaw = 0.0f;
  Aereo.altitude = 0.0f;
  Aereo.x = 0.0f;
  Aereo.z = 0.0f;
  Aereo.speed = 0.0f;
  Aereo.system_active = false; // Motori spenti, coerente con spawn state
  Aereo.landing_mode = true; // Ground mode attivo: carrello giù, flap estratti

  // ------------------------------------------------------------------
  // DEBUG LOOP: stampa il stato FBW ogni 2 secondi per tracciare freeze
  // Utile per vedere il momento esatto in cui i comandi smettono di
  // rispondere — guarda i flag PROT (GPWS, BANK, ALPHA) nell'output.
  // ------------------------------------------------------------------
  int debug_frame_count = 0;
  float debug_elapsed = 0.0f;

  while (display.IsActive()) {
    float dt = GetFrameTime();
    debug_elapsed += dt;
    debug_frame_count++;

    // Debug ogni 2 secondi
    if (debug_elapsed >= 2.0f) {
      g_fcc.debug_print();
      // Log aggiuntivo: input corrente del pilota
      std::cout << "  [INPUT] roll=" << pilot_input.stick_roll
                << " pitch=" << pilot_input.stick_pitch
                << " spd_d=" << pilot_input.speed_delta
                << " eng=" << pilot_input.engines_on
                << " rdy=" << pilot_input.engine_ready
                << " land=" << pilot_input.landing_mode
                << " frame=" << debug_frame_count << " dt=" << std::fixed
                << std::setprecision(4) << dt << std::endl;
      debug_elapsed = 0.0f;
    }

    // A. Leggi stato pre-step dal FCC (per condizioni audio in HandleInput)
    {
      FlightState state = g_fcc.get_state();
      Aereo.roll = state.roll;
      Aereo.pitch = state.pitch;
      Aereo.yaw = state.yaw;
      Aereo.altitude = state.altitude;
      Aereo.speed = state.speed;
      Aereo.x = state.x;
      Aereo.z = state.z;
      Aereo.system_active = state.system_active;
      Aereo.landing_mode = state.landing_mode;
      strncpy(Aereo.status_msg, state.status_msg, 63);
      Aereo.status_msg[63] = '\0';
    }

    // B. HandleInput: tasti, audio, animazioni → scrive in pilot_input
    display.HandleInput(Aereo, pilot_input);

    // C. FCC step con dt reale del renderer — fisica sincrona al rendering
    g_fcc.step(pilot_input, dt);

    // D. Leggi stato aggiornato per il disegno
    {
      FlightState state = g_fcc.get_state();
      Aereo.roll = state.roll;
      Aereo.pitch = state.pitch;
      Aereo.yaw = state.yaw;
      Aereo.altitude = state.altitude;
      Aereo.speed = state.speed;
      Aereo.x = state.x;
      Aereo.z = state.z;
      Aereo.system_active = state.system_active;
      Aereo.landing_mode = state.landing_mode;
      strncpy(Aereo.status_msg, state.status_msg, 63);
      Aereo.status_msg[63] = '\0';
    }

    // E. Rendering 3D e HUD
    display.Draw(Aereo);
  }

  // =========================================================================
  // 5. Shutdown ordinato
  // =========================================================================
  g_running.store(false, std::memory_order_relaxed);
  dds_thread.join();

  pub->delete_datawriter(writer);
  participant->delete_publisher(pub);
  participant->delete_topic(topic);
  DomainParticipantFactory::get_instance()->delete_participant(participant);

  return 0;
}
