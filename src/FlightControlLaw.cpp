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

  // =========================================================================
  // Gestione transizioni landing_mode
  // =========================================================================
  bool in_flight = pilot.engines_on && !pilot.landing_mode;

  if (in_flight && m_prev_landing_mode) {
    // ── Uscita da landing mode ────────────────────────────────────────
    // NON engaggiare l'outer loop (guadagni = 0, produce solo q_cmd=0
    // che unito al SAS fa azzerare il pitch rate → muso a terra).
    m_outer_loop_engaged = false;
    reset_outer_loop();

    // Inizializza pitch integrator con un piccolo bias nose-up (trim).
    // Senza questo, elevatore = 0 → instabilità naturale F-16 → picchiata.
    m_pitch_integrator = 0.12f; // ~4° di elevatore trim (decadrà lentamente)

    // Attiva safety pitch hold: mantiene almeno SAFETY_THETA_MIN di cabrata.
    // La quota di riferimento è la MASSIMA tra la posizione attuale
    // e il minimo sicuro (così non si "inarca" se già in cabrata).
    m_safety.theta_ref = std::max(state.pitch, SAFETY_THETA_MIN);
    m_safety.active    = true;

  } else if (!in_flight && !m_prev_landing_mode) {
    // ── Entrata in landing mode (o spegnimento motori) ────────────────
    m_outer_loop_engaged = false;
    reset_outer_loop();
    m_safety.active = false;
  }
  m_prev_landing_mode = pilot.landing_mode;

  // ── Disattiva safety hold quando il pilota riprende il controllo ────
  // o quando è raggiunta la quota sicura (3000m)
  if (m_safety.active) {
    bool pilot_override  = (std::abs(pilot.stick_pitch) > 0.05f);
    bool altitude_safe   = (state.altitude > 3000.0f);
    if (pilot_override || altitude_safe) {
      m_safety.active = false;
    }
  }

  // --- Outer Loop PID (placeholder prof.Russo — bypass finché gains=0) ---
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
  float p_cmd = m_outer_loop_engaged ? m_outer.p_cmd : 0.0f;
  float q_cmd = m_outer_loop_engaged ? m_outer.q_cmd : 0.0f;

  sas.delta_ele = KQ_PITCH * (q_cmd - s.pitch_rate); // Pitch damper

  sas.delta_ail = KP_ROLL  * (p_cmd - s.roll_rate);  // Roll damper

  // Yaw damper coordinato: invece di forzare r=0, comanda il rateo di imbardata
  // necessario per una virata coordinata (r_coord = g*sin(phi)/V).
  // Questo impedisce che il SAS cancelli il yaw naturale della virata,
  // facendo sì che l'aereo si sposti fisicamente nello spazio.
  float V_body = std::sqrt(s.u * s.u + s.v * s.v + s.w * s.w);
  float V_safe = std::max(V_body, 50.0f); // evita divisione per zero
  float r_coord = 9.806f * std::sin(s.roll) / V_safe; // [rad/s] virata livellata
  sas.delta_rud = KR_YAW * (r_coord - s.yaw_rate);   // Yaw damper coordinato

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

  // --- CANALE LONGITUDINALE: C* law pitch rate command ---
  // Quando lo stick è neutro, l'integratore mantiene il pitch rate demand
  // con decadimento lentissimo (0.998/frame ≈ 0.89/s) per hold assetto.
  // Quando lo stick è attivo, viene catturato il demand corrente × 0.70
  // per persistenza quando il pilota rilascia lo stick.
  float pitch_rate_demand = p.stick_pitch * MAX_PITCH_RATE;
  if (std::abs(p.stick_pitch) < 0.02f) {
    // Stick neutro: mantieni assetto (hold attitude) — decadimento lento
    m_pitch_integrator *= 0.998f;  // era 0.97 → troppo veloce, ora ~0.89/s
    pitch_rate_demand = m_pitch_integrator;
  } else {
    // Stick attivo: cattura demand con buona persistenza
    m_pitch_integrator = pitch_rate_demand * 0.70f; // era 0.25 → troppo basso
  }
  float delta_ele_pilot = (pitch_rate_demand / MAX_PITCH_RATE) * MAX_ELEV_DEG;
  surf.stabilator_deflection = delta_ele_pilot + sas.delta_ele;

  // --- SAFETY PITCH HOLD (attivo dopo uscita da landing mode) ---
  // Controller proporzionale su angolo di pitch:
  //   delta_safe = K_SAFETY * (theta_ref - theta_actual)
  // Se l'aereo è sotto theta_ref (muso basso), aggiunge elevatore nose-up.
  // Si sovrappone al comando del pilota senza annullarlo:
  // il pilota può sempre vincere applicando stick input (> 0.05 → safety OFF).
  if (m_safety.active) {
    float theta_err   = m_safety.theta_ref - s.pitch;   // [rad]
    float safety_cmd  = K_SAFETY_PITCH * theta_err;     // [deg]
    surf.stabilator_deflection += safety_cmd;
  }

  // --- CANALE LATERALE: Pilot + SAS roll damper ---
  float delta_ail_pilot = p.stick_roll * MAX_AIL_DEG;
  surf.flaperon_deflection = delta_ail_pilot + sas.delta_ail;

  // --- CANALE DIREZIONALE: Pilot + SAS yaw damper coordinato ---
  float delta_rud_pilot = p.rudder * MAX_RUD_DEG;
  // Auto-rudder: accoppiamento roll→yaw per virata coordinata.
  // Quando il pilota applica stick_roll, si aggiunge automaticamente timone
  // proporzionale per mantenere il vettore portanza allineato con l'asse verticale.
  float auto_rud = p.stick_roll * K_COORD_RUD * MAX_RUD_DEG;
  surf.rudder_deflection = delta_rud_pilot + sas.delta_rud + auto_rud;

  // --- COMPENSAZIONE PITCH IN VIRATA ---
  // In una virata banked, la componente verticale della portanza diminuisce,
  // richiedendo pitch-up per mantenere la quota.
  // ΔElevatore ∝ phi² (approssimazione lifting-line per piccoli angoli).
  float bank_pitch_comp = s.roll * s.roll * K_TURN_PITCH;
  surf.stabilator_deflection += bank_pitch_comp;

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

  // GPWS: ground proximity < 300m
  m_prot.terrain_avoidance = (s.altitude < GPWS_ALT && s.altitude > 0.5f &&
                              s.system_active && !p.landing_mode);

  // Alta quota: allarme a 12000m, autopilota a 15000m
  m_prot.high_altitude =
      (s.altitude > ALT_WARN_HIGH && s.system_active && !p.landing_mode);

  // Bank angle: allarme superati i 60°, limite assoluto a 75° (F-16 Normal Law)
  m_prot.bank_angle_limit =
      (std::abs(s.roll) > BANK_LIMIT && s.system_active && !p.landing_mode);

  // =========================================================================
  // AUTOPILOTA QUOTA — attivo solo fuori da landing_mode e dopo il decollo
  //
  // PITCH-UP  sotto ALT_AP_PULLUP  (1000m): spinge il naso in su progressivamente.
  // PITCH-DOWN sopra ALT_AP_PUSHDOWN (15000m): spinge il naso in giù.
  //
  // Guardia airborne: s.altitude > 50m (evita interferenza durante corsa di decollo).
  // Usa std::max/min → interviene solo se il pilota non sta già correggendo
  // in modo più aggressivo (il pilota ha sempre la priorità).
  //
  // Guadagno conservativo: max 8° per pull-up, max 6° per push-down.
  // Questo lascia margine di manovra al pilota senza strappi bruschi.
  // =========================================================================
  if (s.system_active && !p.landing_mode && s.altitude > 50.0f) {
    if (s.altitude < ALT_AP_PULLUP) {
      // [50m .. 1000m]: t da 0 a 1 → comando AP da 0° a 8° nose-up
      float t      = (ALT_AP_PULLUP - s.altitude) / (ALT_AP_PULLUP - 50.0f);
      float ap_cmd = t * 8.0f;  // max 8° stabilatore (nose-up)
      surf.stabilator_deflection = std::max(surf.stabilator_deflection, ap_cmd);
    } else if (s.altitude > ALT_AP_PUSHDOWN) {
      // [15000m .. 17000m]: t da 0 a 1 → comando AP da 0° a -6° (nose-down)
      float t      = std::min((s.altitude - ALT_AP_PUSHDOWN) / 2000.0f, 1.0f);
      float ap_cmd = -t * 6.0f; // max -6° stabilatore (nose-down)
      surf.stabilator_deflection = std::min(surf.stabilator_deflection, ap_cmd);
    }
  }

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
