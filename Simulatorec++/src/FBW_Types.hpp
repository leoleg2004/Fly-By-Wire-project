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
  float stick_roll;     // [-1.0, +1.0]: input cloche asse rollio (comando p)
  float stick_pitch;    // [-1.0, +1.0]: input cloche asse beccheggio (comando q)
  float rudder;         // [-1.0, +1.0]: pedale timone (comando imbardata r)
  float throttle_input; // [0.0, 1.0]: comando manetta fisica (spinta) [NASA TP-1538]
  bool engines_on;      // Stato motori (toggle con E)
  bool engine_ready;    // false durante warmup di 20s
  bool landing_mode;    // Modalità ILS/atterraggio
  bool gear_deploy;     // Stato carrello
};
// Deflessioni delle superfici di controllo — output del FCC verso il FCS
// Riferimento NASA TP-1538: superfici di controllo F-16
struct ControlSurfaces {
  float stabilator_deflection; // [deg] δh - Stabilatore orizzontale intero (beccheggio e rollio asimmetrico)
  float flaperon_deflection;   // [deg] δa - Flaperoni (alettone + flap)
  float rudder_deflection;     // [deg] δr - Timone verticale
  float leading_edge_flap;     // [deg] δlef - Flap di bordo d'attacco (automatici in base a Mach e Alpha)
  float thrust_normalized;     // [0.0, 1.0] - Comando netto alla turbina per calcolo Spinta
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
  // Atteggiamento (Euler, rad) - Riferimento body-to-earth [NASA TP-1538]
  float roll;  // Φ (Phi)
  float pitch; // Θ (Theta)
  float yaw;   // Ψ (Psi)

  // Tassi angolari (rad/s) nel body frame
  float roll_rate;  // p
  float pitch_rate; // q
  float yaw_rate;   // r

  // Velocità lineari nel body frame (m/s) [Sostituiscono lo speed scalare]
  float u; // asse X (avanti)
  float v; // asse Y (destra)
  float w; // asse Z (basso)

  // Angoli aerodinamici (rad) calcolati da u, v, w [NASA TP-1538]
  float alpha; // Angolo di Attacco (α)
  float beta;  // Angolo di Derapata (β)

  // Navigazione
  float x;        // m — posizione Est/Ovest
  float z;        // m — posizione Nord/Sud
  float altitude; // m (posizione su asse Z earth-frame invertita)

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
    u = v = w = 0.0f;
    alpha = beta = 0.0f;
    x = z = 0.0f;
    altitude = 0.0f;
    mode = FBWMode::NORMAL_LAW;
    protections = {};
    system_active = false;
    landing_mode = false;
    memset(status_msg, 0, sizeof(status_msg));
    packet_id = 0;
  }
};
