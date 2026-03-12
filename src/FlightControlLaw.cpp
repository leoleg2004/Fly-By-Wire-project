#include "FlightControlLaw.hpp"
#include <algorithm>
#include <cmath>

FlightControlLaw::FlightControlLaw() {}

FBWMode FlightControlLaw::evaluate_mode(const FlightState &state) const {
  if (!state.system_active)
    return FBWMode::DIRECT_LAW;
  return FBWMode::NORMAL_LAW;
}

// =========================================================================
// compute() — Entry point della catena di controllo
// Flusso: Outer Loop (placeholder) → Inner Loop SAS → δ_cmd
// =========================================================================
ControlSurfaces FlightControlLaw::compute(const PilotInput &pilot,
                                          const FlightState &state, float dt) {
  ControlSurfaces surf{};

  // --- Outer Loop PID (placeholder prof.Russo) ---
  compute_outer_loop(state, dt);

  switch (evaluate_mode(state)) {
  case FBWMode::NORMAL_LAW:
    surf = normal_law(pilot, state, dt);
    break;
  case FBWMode::ALTERNATE_LAW:
    surf = alternate_law(pilot);
    break;
  case FBWMode::DIRECT_LAW:
    surf = direct_law(pilot);
    break;
  }
  apply_protections(surf, state, pilot);
  return surf;
}

// =========================================================================
// SAS Inner Loop — Stability Augmentation System
// Feedback proporzionale negativo sui body rates (p, q, r).
// L'F-16 con CG a 0.30 (avanti del punto neutro 0.35) ha margine
// statico negativo → senza SAS diverge in beccheggio.
// Ref: Stevens & Lewis, Cap. 5.5
// =========================================================================
FlightControlLaw::SASOutput
FlightControlLaw::compute_sas(const FlightState &s) const {
  SASOutput sas{};
  // δ_sas = K * (rate_cmd - rate_actual)
  // In modalità manuale pura, rate_cmd = 0 (smorzamento puro)
  // Se outer loop attivo, rate_cmd = m_outer.p_cmd / q_cmd
  float p_cmd = m_outer_loop_engaged ? m_outer.p_cmd : 0.0f;
  float q_cmd = m_outer_loop_engaged ? m_outer.q_cmd : 0.0f;

  sas.delta_ele = KQ_PITCH * (q_cmd - s.pitch_rate); // Pitch damper
  sas.delta_ail = KP_ROLL  * (p_cmd - s.roll_rate);  // Roll damper
  sas.delta_rud = KR_YAW   * (0.0f  - s.yaw_rate);   // Yaw damper (r_cmd=0)

  return sas;
}

// =========================================================================
// Outer Loop PID — PLACEHOLDER (Tesi prof.Russo)
//
// Struttura predisposta per PID su Φ e Θ.
// Con i gain a zero, l'output è nullo → non influenza il SAS.
//
// Per attivare: impostare m_outer_loop_engaged = true e assegnare
// i gain KP/KI/KD_PHI e KP/KI/KD_THETA dal documento del prof. Russo.
// =========================================================================
void FlightControlLaw::compute_outer_loop(const FlightState &s, float dt) {
  if (!m_outer_loop_engaged || dt <= 0.0f)
    return;

  // --- PID Roll (Φ) ---
  // TODO prof.Russo: phi_ref viene dal comando pilota o dal path planner
  m_outer.phi_error = m_outer.phi_ref - s.roll;
  m_outer.phi_integral += m_outer.phi_error * dt;
  m_outer.phi_integral = std::clamp(m_outer.phi_integral,
                                     -INTEGRAL_MAX, INTEGRAL_MAX);
  float phi_deriv = (m_outer.phi_error - m_outer.phi_error_prev) / dt;
  m_outer.phi_error_prev = m_outer.phi_error;

  // p_cmd = Kp*e + Ki*∫e + Kd*ė
  m_outer.p_cmd = KP_PHI   * m_outer.phi_error
                + KI_PHI   * m_outer.phi_integral
                + KD_PHI   * phi_deriv;

  // --- PID Pitch (Θ) ---
  // TODO prof.Russo: theta_ref viene dal comando pilota o dal path planner
  m_outer.theta_error = m_outer.theta_ref - s.pitch;
  m_outer.theta_integral += m_outer.theta_error * dt;
  m_outer.theta_integral = std::clamp(m_outer.theta_integral,
                                       -INTEGRAL_MAX, INTEGRAL_MAX);
  float theta_deriv = (m_outer.theta_error - m_outer.theta_error_prev) / dt;
  m_outer.theta_error_prev = m_outer.theta_error;

  // q_cmd = Kp*e + Ki*∫e + Kd*ė
  m_outer.q_cmd = KP_THETA * m_outer.theta_error
                + KI_THETA * m_outer.theta_integral
                + KD_THETA * theta_deriv;
}

void FlightControlLaw::reset_outer_loop() {
  m_outer = OuterLoopState{};
}

// =========================================================================
// Normal Law — Pilot input + SAS damping
// Il pilota comanda deflessioni proporzionali (stick → δ_pilot).
// Il SAS aggiunge il termine di smorzamento (δ_sas).
// δ_cmd = δ_pilot + δ_sas
// =========================================================================
ControlSurfaces FlightControlLaw::normal_law(const PilotInput &p,
                                             const FlightState &s, float dt) {
  ControlSurfaces surf{};

  // --- Inner Loop SAS ---
  SASOutput sas = compute_sas(s);

  // --- CANALE LONGITUDINALE: Pilot + SAS pitch damper ---
  float pitch_rate_demand = p.stick_pitch * MAX_PITCH_RATE;
  if (std::abs(p.stick_pitch) < 0.02f) {
    m_pitch_integrator *= 0.97f;
    pitch_rate_demand = m_pitch_integrator;
  } else {
    m_pitch_integrator = pitch_rate_demand * 0.25f;
  }
  float delta_ele_pilot = (pitch_rate_demand / MAX_PITCH_RATE) * MAX_ELEV_DEG;
  surf.stabilator_deflection = delta_ele_pilot + sas.delta_ele;

  // --- CANALE LATERALE: Pilot + SAS roll damper ---
  float delta_ail_pilot = p.stick_roll * MAX_AIL_DEG;
  surf.flaperon_deflection = delta_ail_pilot + sas.delta_ail;

  // --- CANALE DIREZIONALE: Pilot + SAS yaw damper ---
  float delta_rud_pilot = p.rudder * MAX_RUD_DEG;
  surf.rudder_deflection = delta_rud_pilot + sas.delta_rud;

  // --- LEF (Automatici in funzione di α) ---
  float alpha_deg = s.alpha * (180.0f / M_PI);
  surf.leading_edge_flap = std::clamp(alpha_deg * 1.38f, 0.0f, MAX_LEF_DEG);

  // --- PROPULSIONE ---
  surf.thrust_normalized = p.throttle_input;

  return surf;
}

ControlSurfaces FlightControlLaw::alternate_law(const PilotInput &p) {
  return {p.stick_pitch * MAX_ELEV_DEG,
          p.stick_roll * MAX_AIL_DEG,
          p.rudder * MAX_RUD_DEG,
          0.0f,
          p.throttle_input};
}

ControlSurfaces FlightControlLaw::direct_law(const PilotInput &p) {
  return {p.stick_pitch * MAX_ELEV_DEG,
          p.stick_roll * MAX_AIL_DEG,
          p.rudder * MAX_RUD_DEG,
          0.0f,
          p.throttle_input};
}

void FlightControlLaw::apply_protections(ControlSurfaces &surf,
                                         const FlightState &s,
                                         const PilotInput &p) {
  float alpha_deg = s.alpha * (180.0f / M_PI);
  m_prot.alpha_floor = (alpha_deg > 25.0f && s.altitude > 0.5f);

  float V_T_sq = s.u * s.u + s.v * s.v + s.w * s.w;
  m_prot.overspeed = (V_T_sq > (VMO * VMO));

  m_prot.terrain_avoidance = (s.altitude < 300.0f && s.altitude > 0.5f &&
                              s.system_active && !p.landing_mode);

  m_prot.high_altitude =
      (s.altitude > 15000.0f && s.system_active && !p.landing_mode);

  m_prot.bank_angle_limit =
      (std::abs(s.roll) > 1.0f && s.system_active && !p.landing_mode);

  // Clamp strutturale (limiti fisici F-16)
  surf.stabilator_deflection =
      std::clamp(surf.stabilator_deflection, -MAX_ELEV_DEG, MAX_ELEV_DEG);
  surf.flaperon_deflection =
      std::clamp(surf.flaperon_deflection, -MAX_AIL_DEG, MAX_AIL_DEG);
  surf.rudder_deflection =
      std::clamp(surf.rudder_deflection, -MAX_RUD_DEG, MAX_RUD_DEG);
  surf.leading_edge_flap =
      std::clamp(surf.leading_edge_flap, 0.0f, MAX_LEF_DEG);
}
