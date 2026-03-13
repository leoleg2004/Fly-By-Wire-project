#pragma once
#include "FBW_Types.hpp"
#include "FlightControlLaw.hpp"
#include <cstring>
#include <mutex>

// =========================================================================
// FlightControlComputer — FCC
//
// Catena di Comando (ogni step):
//   1. PilotInput → FlightControlLaw (Outer PID placeholder + Inner SAS)
//   2. δ_cmd → Actuator Model (rate-limited first-order) → δ_actual
//   3. δ_actual → F16AeroFM (NASA TP-1538 lookup tables) → Forze/Momenti
//   4. Forze/Momenti + Gravità + Spinta → EOM 6-DOF (RK4) → nuovo stato
//
// Ref: NASA TP-1538, Stevens & Lewis "Aircraft Control and Simulation"
// =========================================================================
// Actuator positions exposed to rendering layer
struct ActuatorOut {
  float ele; // [deg] stabilatore
  float ail; // [deg] flaperon
  float rud; // [deg] timone
  float lef; // [deg] LEF
  float thr; // [0,1] throttle
};

class FlightControlComputer {
public:
  FlightControlComputer();

  void step(const PilotInput &input, float dt);
  FlightState get_state() const;
  ActuatorOut get_actuator_state() const;
  void set_initial_state(const FlightState &state);
  void debug_print() const;

private:
  FlightControlLaw m_law;
  FlightState m_state;
  mutable std::mutex m_mtx;

  // --- Hysteresis autopilota ---
  bool m_terrain_recovery_active = false;
  bool m_high_alt_recovery_active = false;
  bool m_bank_recovery_active = false;

  // =====================================================================
  // Actuator Model — Dinamica attuatori (first-order + rate limit)
  //
  // Ogni attuatore è modellato come:
  //   δ_dot = clamp((δ_cmd - δ_actual) / τ, -rate_max, +rate_max)
  //   δ_actual += δ_dot * dt
  //   δ_actual = clamp(δ_actual, -pos_max, +pos_max)
  //
  // Rate limits da trim_and_linearize.m:
  //   dele: 60 deg/s,  dail: 80 deg/s,  drud: 120 deg/s,  dlef: 25 deg/s
  //   T:    10000 lbs/s → normalizzato [0,1] ~0.556/s
  // =====================================================================
  struct ActuatorState {
    float ele  = 0.0f; // [deg] stabilatore attuale
    float ail  = 0.0f; // [deg] flaperon attuale
    float rud  = 0.0f; // [deg] timone attuale
    float lef  = 0.0f; // [deg] LEF attuale
    float thr  = 0.0f; // [0,1] throttle attuale
  };
  ActuatorState m_actuator{};

  // Rate limits [deg/s] (trim_and_linearize.m)
  static constexpr float ACT_RATE_ELE = 60.0f;
  static constexpr float ACT_RATE_AIL = 80.0f;
  static constexpr float ACT_RATE_RUD = 120.0f;
  static constexpr float ACT_RATE_LEF = 25.0f;
  static constexpr float ACT_RATE_THR = 0.556f; // 10000 lbf/s / 18000 lbf range

  // Time constant [s] (tipico attuatore idraulico F-16)
  static constexpr float ACT_TAU = 0.05f; // 20 Hz bandwidth

  // Position limits [deg]
  static constexpr float MAX_ELE = 25.0f;
  static constexpr float MAX_AIL = 21.5f;
  static constexpr float MAX_RUD = 30.0f;
  static constexpr float MAX_LEF = 25.0f;

  void update_actuators(const ControlSurfaces &cmd, float dt);

  // =====================================================================
  // Costanti F-16 da load_F16_params.m (NASA TP-1538, SI)
  // =====================================================================
  static constexpr float SLUG2KG = 14.5939f;
  static constexpr float FT2M = 0.3048f;
  static constexpr float SLUG_FT2_TO_KG_M2 = SLUG2KG * FT2M * FT2M;

  static constexpr float MASS_KG = 636.94f * SLUG2KG;          // 9298.6 kg

  static constexpr float I_XX = 9496.0f  * SLUG_FT2_TO_KG_M2; // 12874 kg*m^2
  static constexpr float I_YY = 55814.0f * SLUG_FT2_TO_KG_M2; // 75674 kg*m^2
  static constexpr float I_ZZ = 63100.0f * SLUG_FT2_TO_KG_M2; // 85552 kg*m^2
  static constexpr float I_XZ = 982.0f   * SLUG_FT2_TO_KG_M2; // 1331  kg*m^2

  static constexpr float S_WING = 300.0f * FT2M * FT2M;       // 27.87 m^2
  static constexpr float B_SPAN = 30.0f * FT2M;                //  9.144 m
  static constexpr float C_BAR  = 11.32f * FT2M;               //  3.45 m
  static constexpr float XC_G_REF    = 0.35f;
  static constexpr float XC_G_ACTUAL = 0.30f;

  static constexpr float GRAVITY = 9.806f;
  static constexpr float RHO_SEA_LEVEL = 1.225f;
};
