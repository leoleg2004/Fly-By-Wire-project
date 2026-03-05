#pragma once
#include <cstdint>
#include <cstring>

// FBW_Types.hpp
// Core data structures for the Fly-By-Wire system

// Legge di controllo attiva
enum class FBWMode : uint8_t {
  NORMAL_LAW = 0,    // Protezioni complete, rate command (C* law)
  ALTERNATE_LAW = 1, // Protezioni ridotte, comando superfici diretto
  DIRECT_LAW = 2     // Proporzionale grezzo, nessuna protezione
};

// Raw pilot input (Rate demand)
struct PilotInput {
  float stick_roll;  // [-1.0, +1.0]: comanda roll rate in Normal Law
  float stick_pitch; // [-1.0, +1.0]: comanda pitch rate in Normal Law
  float rudder;      // [-1.0, +1.0]: pedale timone
  float speed_delta; // [KPH/frame]
  bool engines_on;   // Stato motori (toggle con E)
  bool engine_ready; // false durante warmup di 20s
  bool landing_mode; // Modalità ILS/atterraggio
  bool gear_deploy;  // Stato carrello
};

// Deflessioni delle superfici di controllo — output del FCC verso il FCS
struct ControlSurfaces {
  float aileron_deflection;  // [deg]  ±25° max
  float elevator_deflection; // [deg]  ±30° max
  float rudder_deflection;   // [deg]  ±30° max
  float thrust_normalized;   // [0.0, 1.0]
};

// Flag protezioni envelope di volo attive nell'ultimo ciclo FCC
struct ProtectionStatus {
  bool alpha_floor;       // Stall protection (bassa velocità)
  bool overspeed;         // VMO protection (alta velocità)
  bool high_altitude;     // Quota operativa massima superata
  bool terrain_avoidance; // GPWS pull-up
  bool bank_angle_limit;  // Limitatore bank angle (67° in Normal Law)
  bool load_factor_limit; // Limitatore G (2.5G)
};

// Stato di volo completo — prodotto dal FCC ogni ciclo da 100 Hz
struct FlightState {
  // Assetto (Euler, rad)
  float roll;
  float pitch;
  float yaw;

  // Tassi angolari (rad/s) — calcolati dal FCC via control surfaces
  float roll_rate;
  float pitch_rate;
  float yaw_rate;

  // Navigazione
  float x;        // m — posizione Est/Ovest
  float z;        // m — posizione Nord/Sud
  float altitude; // m
  float speed;    // KPH

  // Stato sistema FBW
  FBWMode mode;
  ProtectionStatus protections;
  bool system_active;
  bool landing_mode;
  char status_msg[64];
  uint32_t packet_id;

  FlightState() {
    roll = pitch = yaw = 0.0f;
    roll_rate = pitch_rate = yaw_rate = 0.0f;
    x = z = 0.0f;
    altitude = 0.0f;
    speed = 0.0f;
    mode = FBWMode::NORMAL_LAW;
    protections = {};
    system_active = false;
    landing_mode = false;
    memset(status_msg, 0, sizeof(status_msg));
    packet_id = 0;
  }
};
