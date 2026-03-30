#pragma once
#include "FBW_Types.hpp"
#include <cmath>

// =========================================================================
// FlightControlLaw — SAS Inner Loop + Outer Loop Placeholder (prof.Russo)
//
// Catena di Comando:
//   PilotInput → [Outer Loop PID (placeholder)] → [Inner Loop SAS] → δ_cmd
//   δ_cmd → FCC Actuator Model → δ_actual → F16AeroFM → EOM
//
// Inner Loop (SAS — Stability Augmentation System):
//   Smorzamento artificiale su p,q,r per compensare l'instabilità
//   longitudinale naturale dell'F-16 (CG a 0.30, ref 0.35).
//   δ_sas = δ_pilot + Kp*p_err + Kq*q_err + Kr*r_err
//
// Outer Loop (Attitude Hold — PLACEHOLDER per tesi prof.Russo):
//   PID su Φ,Θ per tracking degli angoli di assetto.
//   Attualmente bypassed: output = 0. Le strutture dati e i metodi
//   sono predisposti per l'inserimento delle leggi del prof. Russo.
// =========================================================================
class FlightControlLaw {
public:
  FlightControlLaw();

  // Output: deflessioni COMANDATE (pre-attuatore) [deg]
  ControlSurfaces compute(const PilotInput &pilot, const FlightState &state,
                          float dt);

  FBWMode evaluate_mode(const FlightState &state) const;
  const ProtectionStatus &protection_status() const { return m_prot; }

private:
  // --- Flusso Normal Law ---
  ControlSurfaces normal_law(const PilotInput &p, const FlightState &s, float dt);
  ControlSurfaces alternate_law(const PilotInput &p);
  ControlSurfaces direct_law(const PilotInput &p);
  void apply_protections(ControlSurfaces &surf, const FlightState &s,
                         const PilotInput &p);

  // =====================================================================
  // INNER LOOP — SAS (Stability Augmentation System)
  // Feedback proporzionale sui body rates per smorzamento artificiale.
  // Ref: Stevens & Lewis, "Aircraft Control and Simulation", Cap. 5
  // =====================================================================
  struct SASOutput {
    float delta_ele; // [deg] incremento stabilatore da SAS
    float delta_ail; // [deg] incremento flaperon da SAS
    float delta_rud; // [deg] incremento timone da SAS
  };
  SASOutput compute_sas(const FlightState &s) const;

  // =====================================================================
  // Gain SAS inner loop — FEEDBACK NEGATIVO (smorzamento)
  //
  // Formula: δ_sas = K * (rate_cmd - rate_actual)
  // Con rate_cmd=0 (loop manuale): δ_sas = -K * rate_actual
  // Per smorzare (feedback negativo): K > 0, così δ_sas si oppone al rate.
  //
  // NOTA: i gain devono essere POSITIVI per feedback negativo.
  // Con K>0: quando rate>0 → δ_sas<0 → momento correttivo opposto. ✓
  // =====================================================================
  static constexpr float KQ_PITCH = +1.5f;  // [deg/(rad/s)] pitch damper (era -2.0, segno corretto)
  static constexpr float KP_ROLL  = +0.3f;  // [deg/(rad/s)] roll damper  (era -0.4, segno corretto)
  static constexpr float KR_YAW   = +1.2f;  // [deg/(rad/s)] yaw damper   (era -1.5, segno corretto)

  // Virata coordinata
  // K_COORD_RUD: frazione di stick_roll → timone (virata coordinata)
  // K_TURN_PITCH: compensazione portanza nella virata [deg/rad^2]
  static constexpr float K_COORD_RUD  = 0.15f; // [adim]
  static constexpr float K_TURN_PITCH = 1.0f;  // [deg/rad^2]

  // =====================================================================
  // OUTER LOOP — PID Attitude Hold (PLACEHOLDER — Tesi prof.Russo)
  //
  // Quando attivato, genera comandi di rateo (p_cmd, q_cmd) a partire
  // dagli errori di assetto (Φ_ref - Φ, Θ_ref - Θ).
  // Il SAS inner loop usa poi questi comandi come riferimento.
  //
  // TODO prof.Russo: Inserire qui le equazioni PID dal documento di tesi.
  //   q_cmd = Kp_theta*(theta_ref - theta) + Ki_theta*integral + Kd_theta*d_err
  //   p_cmd = Kp_phi*(phi_ref - phi) + Ki_phi*integral + Kd_phi*d_err
  // =====================================================================
  struct OuterLoopState {
    // Riferimenti di assetto [rad]
    float phi_ref   = 0.0f;   // Roll reference
    float theta_ref = 0.0f;   // Pitch reference

    // Errori PID roll
    float phi_error      = 0.0f;
    float phi_integral   = 0.0f;
    float phi_error_prev = 0.0f;

    // Errori PID pitch
    float theta_error      = 0.0f;
    float theta_integral   = 0.0f;
    float theta_error_prev = 0.0f;

    // Output: comandi di rateo generati dal PID [rad/s]
    float p_cmd = 0.0f;  // Roll rate command
    float q_cmd = 0.0f;  // Pitch rate command
  };

  // Gain PID outer loop — PLACEHOLDER (tutti a zero: loop inattivo)
  // TODO prof.Russo: Assegnare i gain dal documento di tesi
  static constexpr float KP_PHI   = 0.0f;  // [rad/s / rad]
  static constexpr float KI_PHI   = 0.0f;  // [rad/s / (rad*s)]
  static constexpr float KD_PHI   = 0.0f;  // [rad/s / (rad/s)]
  static constexpr float KP_THETA = 0.0f;  // [rad/s / rad]
  static constexpr float KI_THETA = 0.0f;  // [rad/s / (rad*s)]
  static constexpr float KD_THETA = 0.0f;  // [rad/s / (rad/s)]

  // Anti-windup: limiti integrali [rad*s]
  static constexpr float INTEGRAL_MAX = 1.0f;

  OuterLoopState m_outer{};
  bool m_outer_loop_engaged  = false; // false = bypass (solo manuale + SAS)
  bool m_prev_landing_mode   = true;  // per rilevare la transizione landing→volo

  // TODO prof.Russo: Implementare la logica PID qui
  void compute_outer_loop(const FlightState &s, float dt);
  void reset_outer_loop();

  // =====================================================================
  // SAFETY PITCH HOLD — attivo all'uscita da landing mode
  //
  // Il problema: uscendo da landing_mode con m_pitch_integrator=0,
  // il comando elevatore diventa neutro. L'F-16 ha margine statico
  // NEGATIVO → senza trim, il muso cade per gravità → schianto.
  //
  // Soluzione: al momento della transizione si attiva un controller P
  // sull'angolo di pitch che mantiene l'aereo in assetto sicuro
  // (almeno SAFETY_THETA_MIN gradi di cabrata) finché:
  //   a) il pilota prende il controllo (|stick_pitch| > 0.05)
  //   b) l'aereo raggiunge quota sicura (alt > 3000m)
  // =====================================================================
  struct SafetyPitchHold {
    bool  active    = false;
    float theta_ref = 0.0f; // [rad] pitch target
  };
  SafetyPitchHold m_safety{};

  // Guadagno proporzionale del safety hold: 10 deg di elevatore per rad di errore
  static constexpr float K_SAFETY_PITCH  = 10.0f;  // [deg/rad]
  // Pitch minimo di riferimento al momento dell'uscita da landing mode
  static constexpr float SAFETY_THETA_MIN = 0.08f; // [rad] ≈ 4.5° nose-up

  // =====================================================================
  // Stato interno e costanti
  // =====================================================================
  ProtectionStatus m_prot{};
  float m_pitch_integrator = 0.0f; // C* law integrator

  // Limiti superfici F-16 (trim_and_linearize.m / NASA TP-1538)
  // Comandi di rateo massimi in Normal Law — aumentati per miglior risposta
  static constexpr float MAX_ROLL_RATE  = 2.0f;   // [rad/s] ~115 deg/s (era 1.5)
  static constexpr float MAX_PITCH_RATE = 0.75f;  // [rad/s] ~43 deg/s  (era 0.5)
  static constexpr float MAX_AIL_DEG  = 21.5f;     // δa max
  static constexpr float MAX_ELEV_DEG = 25.0f;     // δh max
  static constexpr float MAX_RUD_DEG  = 30.0f;     // δr max
  static constexpr float MAX_LEF_DEG  = 25.0f;     // δlef max

  // Envelope protection thresholds
  static constexpr float BANK_LIMIT      = 1.047f;  // 60° [rad] — soglia allarme bank angle F-16
  static constexpr float BANK_HARD_LIMIT = 1.309f;  // 75° [rad] — limite assoluto Normal Law
  static constexpr float ALPHA_MIN_SPEED = 60.0f;   // [m/s]
  static constexpr float VMO = 290.0f;              // [m/s]
  static constexpr float ALT_MAX = 13000.0f;        // [m]
  static constexpr float GPWS_ALT = 300.0f;         // [m] — GPWS pull-up hardware

  // Soglie quota per allarmi e autopilota
  static constexpr float ALT_WARN_LOW    = 3000.0f;  // [m] allarme bassa quota
  static constexpr float ALT_AP_PULLUP   = 1000.0f;  // [m] autopilota pitch-up
  static constexpr float ALT_WARN_HIGH   = 12000.0f; // [m] allarme alta quota
  static constexpr float ALT_AP_PUSHDOWN = 15000.0f; // [m] autopilota pitch-down
};
