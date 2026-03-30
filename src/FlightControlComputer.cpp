#include "FlightControlComputer.hpp"
#include "F16AeroData.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>

FlightControlComputer::FlightControlComputer() {}

void FlightControlComputer::set_initial_state(const FlightState &s) {
  std::lock_guard<std::mutex> lock(m_mtx);
  m_state = s;
}

FlightState FlightControlComputer::get_state() const {
  std::lock_guard<std::mutex> lock(m_mtx);
  return m_state;
}

ActuatorOut FlightControlComputer::get_actuator_state() const {
  std::lock_guard<std::mutex> lock(m_mtx);
  return {m_actuator.ele, m_actuator.ail, m_actuator.rud,
          m_actuator.lef, m_actuator.thr};
}

// =========================================================================
// Actuator Model — First-order lag con rate limit (trim_and_linearize.m)
//
//   δ_dot = clamp((δ_cmd - δ_actual) / τ, -rate_max, +rate_max)
//   δ_actual += δ_dot * dt
//   δ_actual = clamp(δ_actual, -pos_max, +pos_max)
// =========================================================================
static float actuator_step(float actual, float cmd,
                           float rate_max, float pos_min, float pos_max,
                           float tau, float dt) {
  float error = cmd - actual;
  float rate = error / tau;
  rate = std::clamp(rate, -rate_max, rate_max);
  actual += rate * dt;
  return std::clamp(actual, pos_min, pos_max);
}

void FlightControlComputer::update_actuators(const ControlSurfaces &cmd,
                                             float dt) {
  m_actuator.ele = actuator_step(m_actuator.ele, cmd.stabilator_deflection,
                                  ACT_RATE_ELE, -MAX_ELE, MAX_ELE, ACT_TAU, dt);
  m_actuator.ail = actuator_step(m_actuator.ail, cmd.flaperon_deflection,
                                  ACT_RATE_AIL, -MAX_AIL, MAX_AIL, ACT_TAU, dt);
  m_actuator.rud = actuator_step(m_actuator.rud, cmd.rudder_deflection,
                                  ACT_RATE_RUD, -MAX_RUD, MAX_RUD, ACT_TAU, dt);
  m_actuator.lef = actuator_step(m_actuator.lef, cmd.leading_edge_flap,
                                  ACT_RATE_LEF, 0.0f, MAX_LEF, ACT_TAU, dt);
  m_actuator.thr = actuator_step(m_actuator.thr, cmd.thrust_normalized,
                                  ACT_RATE_THR, 0.0f, 1.0f, ACT_TAU, dt);
}

// =========================================================================
// F16AeroFM — Replica esatta di F16AeroFM.m (MATLAB / NASA TP-1538)
//
// Input:  Vt (m/s), alpha,beta (rad), p,q,r (rad/s), qbar (Pa),
//         ele,ail,rud,lef (deg) — ACTUAL deflections post-attuatore
// Output: Fx,Fy,Fz (N), L,M,N (Nm)
// =========================================================================
static void F16AeroFM(float Vt, float alpha_rad, float beta_rad,
                      float p, float q, float r, float qbar,
                      float ele_deg, float ail_deg, float rud_deg,
                      float lef_deg,
                      float &Fx, float &Fy, float &Fz,
                      float &La, float &Ma, float &Na) {
  constexpr float R2D = 180.0f / M_PI;
  constexpr float ft2m = 0.3048f;
  constexpr float S    = 300.0f * ft2m * ft2m;
  constexpr float b    = 30.0f * ft2m;
  constexpr float cbar = 11.32f * ft2m;
  constexpr float xcgr = 0.35f;
  constexpr float xcg  = 0.30f;

  // Angoli in gradi per lookup table
  float alpha = alpha_rad * R2D;
  float beta  = beta_rad  * R2D;
  float ele   = ele_deg;  // Già in gradi (post-attuatore)
  float ail   = ail_deg;
  float rud   = rud_deg;
  float lef   = lef_deg;

  // Normalizzazioni superfici (F16AeroFM.m:34-36)
  float dail = ail / 21.5f;
  float drud = rud / 30.0f;
  float dlef = 1.0f - lef / 25.0f;

  float Vt_safe = std::max(Vt, 1.0f);

  // Coefficienti base (F16AeroFM.m:41-46)
  float Cx = aero_Cx(alpha, beta, ele);
  float Cy = aero_Cy(alpha, beta);
  float Cz = aero_Cz(alpha, beta, ele);
  float Cm = aero_Cm(alpha, beta, ele);
  float Cn = aero_Cn(alpha, beta, ele);
  float Cl = aero_Cl(alpha, beta, ele);

  // Delta LEF (F16AeroFM.m:48-54)
  float dCx_lef = aero_Cx_lef(alpha, beta) - aero_Cx(alpha, beta, 0.0f);
  float dCy_lef = aero_Cy_lef(alpha, beta) - aero_Cy(alpha, beta);
  float dCz_lef = aero_Cz_lef(alpha, beta) - aero_Cz(alpha, beta, 0.0f);
  float dCl_lef = aero_Cl_lef(alpha, beta) - aero_Cl(alpha, beta, 0.0f);
  float dCm_lef = aero_Cm_lef(alpha, beta) - aero_Cm(alpha, beta, 0.0f);
  float dCn_lef = aero_Cn_lef(alpha, beta) - aero_Cn(alpha, beta, 0.0f);

  // Derivate di stabilità 1D (F16AeroFM.m:56-64)
  float Cxq_v = aero_Cxq(alpha);
  float Cyr_v = aero_Cyr(alpha);
  float Cyp_v = aero_Cyp(alpha);
  float Czq_v = aero_Czq(alpha);
  float Clr_v = aero_Clr(alpha);
  float Clp_v = aero_Clp(alpha);
  float Cmq_v = aero_Cmq(alpha);
  float Cnr_v = aero_Cnr(alpha);
  float Cnp_v = aero_Cnp(alpha);

  // Delta derivate LEF (F16AeroFM.m:66-74)
  float dCxq_lef = aero_deltaCxq_lef(alpha);
  float dCyr_lef = aero_deltaCyr_lef(alpha);
  float dCyp_lef = aero_deltaCyp_lef(alpha);
  float dCzq_lef = aero_deltaCzq_lef(alpha);
  float dClr_lef = aero_deltaClr_lef(alpha);
  float dClp_lef = aero_deltaClp_lef(alpha);
  float dCmq_lef = aero_deltaCmq_lef(alpha);
  float dCnr_lef = aero_deltaCnr_lef(alpha);
  float dCnp_lef = aero_deltaCnp_lef(alpha);

  // Delta superfici di controllo (F16AeroFM.m:76-87)
  float dCy_r30 = aero_Cy_r30(alpha, beta) - aero_Cy(alpha, beta);
  float dCn_r30 = aero_Cn_r30(alpha, beta) - aero_Cn(alpha, beta, 0.0f);
  float dCl_r30 = aero_Cl_r30(alpha, beta) - aero_Cl(alpha, beta, 0.0f);

  float dCy_a20     = aero_Cy_a20(alpha, beta)     - aero_Cy(alpha, beta);
  float dCy_a20_lef = aero_Cy_a20_lef(alpha, beta) - aero_Cy_lef(alpha, beta) - dCy_a20;

  float dCn_a20     = aero_Cn_a20(alpha, beta)     - aero_Cn(alpha, beta, 0.0f);
  float dCn_a20_lef = aero_Cn_a20_lef(alpha, beta) - aero_Cn_lef(alpha, beta) - dCn_a20;

  float dCl_a20     = aero_Cl_a20(alpha, beta)     - aero_Cl(alpha, beta, 0.0f);
  float dCl_a20_lef = aero_Cl_a20_lef(alpha, beta) - aero_Cl_lef(alpha, beta) - dCl_a20;

  // Misc (F16AeroFM.m:89-93)
  float dCnbeta = aero_deltaCnbeta(alpha);
  float dClbeta = aero_deltaClbeta(alpha);
  float dCm_v   = aero_deltaCm(alpha);
  float eta_el  = aero_eta_el(ele);

  // ========== Coefficienti totali (F16AeroFM.m:97-125) ==========

  // Cx totale
  float dXdq   = (cbar / (2.0f * Vt_safe)) * (Cxq_v + dCxq_lef * dlef);
  float Cx_tot = Cx + dCx_lef * dlef + dXdq * q;

  // Cz totale
  float dZdq   = (cbar / (2.0f * Vt_safe)) * (Czq_v + dCzq_lef * dlef);
  float Cz_tot = Cz + dCz_lef * dlef + dZdq * q;

  // Cm totale
  float dMdq   = (cbar / (2.0f * Vt_safe)) * (Cmq_v + dCmq_lef * dlef);
  float Cm_tot = Cm * eta_el + Cz_tot * (xcgr - xcg)
               + dCm_lef * dlef + dMdq * q + dCm_v;

  // Cy totale
  float dYdail = dCy_a20 + dCy_a20_lef * dlef;
  float dYdr   = (b / (2.0f * Vt_safe)) * (Cyr_v + dCyr_lef * dlef);
  float dYdp   = (b / (2.0f * Vt_safe)) * (Cyp_v + dCyp_lef * dlef);
  float Cy_tot = Cy + dCy_lef * dlef + dYdail * dail
               + dCy_r30 * drud + dYdr * r + dYdp * p;

  // Cn totale
  float dNdail = dCn_a20 + dCn_a20_lef * dlef;
  float dNdr   = (b / (2.0f * Vt_safe)) * (Cnr_v + dCnr_lef * dlef);
  float dNdp   = (b / (2.0f * Vt_safe)) * (Cnp_v + dCnp_lef * dlef);
  float Cn_tot = Cn + dCn_lef * dlef
               - Cy_tot * (xcgr - xcg) * (cbar / b)
               + dNdail * dail + dCn_r30 * drud
               + dNdr * r + dNdp * p + dCnbeta * beta;

  // Cl totale
  float dLdail = dCl_a20 + dCl_a20_lef * dlef;
  float dLdr   = (b / (2.0f * Vt_safe)) * (Clr_v + dClr_lef * dlef);
  float dLdp   = (b / (2.0f * Vt_safe)) * (Clp_v + dClp_lef * dlef);
  float Cl_tot = Cl + dCl_lef * dlef + dLdail * dail
               + dCl_r30 * drud + dLdr * r + dLdp * p + dClbeta * beta;

  // Forze e momenti dimensionali (F16AeroFM.m:128-135)
  Fx = qbar * S * Cx_tot;
  Fy = qbar * S * Cy_tot;
  Fz = qbar * S * Cz_tot;

  La = Cl_tot * qbar * S * b;
  Ma = Cm_tot * qbar * S * cbar;
  Na = Cn_tot * qbar * S * b;
}

// =========================================================================
// FCC step — Catena completa ogni frame
//
// 1. FlightControlLaw::compute() → δ_cmd  (Outer PID + Inner SAS + pilot)
// 2. update_actuators(δ_cmd)     → δ_actual (rate-limited first-order)
// 3. F16AeroFM(δ_actual)         → Forze/Momenti aerodinamici
// 4. EOM 6-DOF (RK4)             → nuovo stato
// =========================================================================
void FlightControlComputer::step(const PilotInput &input, float dt) {
  dt = std::clamp(dt, 0.001f, 0.05f);

  FlightState snap;
  {
    std::lock_guard<std::mutex> lock(m_mtx);
    snap = m_state;
  }

  snap.system_active = input.engines_on;
  snap.landing_mode = input.landing_mode;

  // ===== STEP 1: Control Law (Outer PID placeholder + Inner SAS) =====
  ControlSurfaces cmd = m_law.compute(input, snap, dt);
  snap.mode = m_law.evaluate_mode(snap);
  snap.protections = m_law.protection_status();

  // ===== STEP 2: Actuator Dynamics =====
  update_actuators(cmd, dt);

  // ===== Atmosfera Standard ISA =====
  float alt_m = snap.altitude;
  float T_kelvin = 288.15f - 0.0065f * alt_m;
  T_kelvin = std::max(T_kelvin, 216.65f);
  float rho = RHO_SEA_LEVEL * std::pow(T_kelvin / 288.15f, 4.2561f);

  // ===== Velocità, alpha, beta =====
  float V_T = std::sqrt(snap.u * snap.u + snap.v * snap.v + snap.w * snap.w);
  if (V_T > 1.0f) {
    snap.alpha = std::atan2(snap.w, snap.u);
    snap.beta  = std::asin(std::clamp(snap.v / V_T, -1.0f, 1.0f));
  } else {
    snap.alpha = 0.0f;
    snap.beta  = 0.0f;
  }

  float q_bar = 0.5f * rho * V_T * V_T;

  // ===== Propulsione: F100-PW-200 (mil power + parziale afterburner) =====
  // T_MIN = idle (1000 lbf), T_MAX = 23000 lbf ≈ mil power + ~20% AB
  // Rapporto spinta/peso a massa piena: 102270 N / (9298 kg * 9.806) ≈ 1.12
  // Questo garantisce T/W > 1 per decollo e climb accettabili.
  constexpr float LBF2N = 4.44822f;
  constexpr float T_MIN = 1000.0f  * LBF2N;  // [N] idle
  constexpr float T_MAX = 23000.0f * LBF2N;  // [N] mil power (era 19000)
  float thrust_force = 0.0f;
  if (snap.system_active && input.engine_ready) {
    thrust_force = T_MIN + m_actuator.thr * (T_MAX - T_MIN);
  }

  // ===== Drag carrello (gear drag) =====
  // Quando il carrello è esteso e l'aereo è in moto aggiunge resistenza parassite.
  // CD_gear_equiv ~ 0.05 → Forza = 0.5 * rho * V² * S_wing * CD_gear
  // Applicato come forza contraria a u (direzione avanzamento body frame).
  float gear_drag = 0.0f;
  if (input.gear_deploy) {
    constexpr float CD_GEAR = 0.05f; // coefficiente di resistenza aggiuntivo
    gear_drag = 0.5f * rho * (snap.u * snap.u) * (27.87f) * CD_GEAR; // [N]
  }

  // ===== STEP 3: Aerodinamica (δ_actual post-attuatore → F16AeroFM) =====
  float Fx_aero, Fy_aero, Fz_aero, L_aero, M_aero, N_aero;
  F16AeroFM(V_T, snap.alpha, snap.beta,
            snap.roll_rate, snap.pitch_rate, snap.yaw_rate, q_bar,
            m_actuator.ele, m_actuator.ail, m_actuator.rud, m_actuator.lef,
            Fx_aero, Fy_aero, Fz_aero,
            L_aero, M_aero, N_aero);

  // ===== Gravità nel Body Frame =====
  float sin_th = std::sin(snap.pitch);
  float cos_th = std::cos(snap.pitch);
  float sin_ph = std::sin(snap.roll);
  float cos_ph = std::cos(snap.roll);

  float W_x = -MASS_KG * GRAVITY * sin_th;
  float W_y =  MASS_KG * GRAVITY * cos_th * sin_ph;
  float W_z =  MASS_KG * GRAVITY * cos_th * cos_ph;

  // ===== STEP 4: EOM 6-DOF =====
  float denom = I_XX * I_ZZ - I_XZ * I_XZ;
  float L_tot = L_aero;
  float M_tot = M_aero;
  float N_tot = N_aero;

  // Lambda per RK4: ricalcola accelerazioni con stato perturbato
  // Forze/momenti aerodinamici congelati al valore corrente (semi-implicit)
  auto compute_accel = [&](float su, float sv, float sw,
                           float sp, float sq, float sr)
      -> std::array<float, 6> {
    float au = (Fx_aero + thrust_force - gear_drag + W_x) / MASS_KG + sr*sv - sq*sw;
    float av = (Fy_aero + W_y) / MASS_KG + sp*sw - sr*su;
    float aw = (Fz_aero + W_z) / MASS_KG + sq*su - sp*sv;

    // Stevens & Lewis Eq. 1.4-4
    float ap = (I_ZZ * L_tot + I_XZ * N_tot
              - (I_XZ*(I_YY - I_XX - I_ZZ)*sp
               + (I_XZ*I_XZ + I_ZZ*(I_ZZ - I_YY))) * sq*sr) / denom;
    float aq = (M_tot - (I_XX - I_ZZ)*sp*sr
              - I_XZ*(sp*sp - sr*sr)) / I_YY;
    float ar = (I_XZ * L_tot + I_XX * N_tot
              + (I_XZ*(I_YY - I_XX - I_ZZ)*sr
               + (I_XZ*I_XZ + I_XX*(I_XX - I_YY))) * sp*sq) / denom;

    return {au, av, aw, ap, aq, ar};
  };

  // RK4 su [u, v, w, p, q, r]
  float su = snap.u, sv = snap.v, sw = snap.w;
  float sp = snap.roll_rate, sq = snap.pitch_rate, sr = snap.yaw_rate;

  auto k1 = compute_accel(su, sv, sw, sp, sq, sr);
  float h2 = dt * 0.5f;
  auto k2 = compute_accel(su+k1[0]*h2, sv+k1[1]*h2, sw+k1[2]*h2,
                           sp+k1[3]*h2, sq+k1[4]*h2, sr+k1[5]*h2);
  auto k3 = compute_accel(su+k2[0]*h2, sv+k2[1]*h2, sw+k2[2]*h2,
                           sp+k2[3]*h2, sq+k2[4]*h2, sr+k2[5]*h2);
  auto k4 = compute_accel(su+k3[0]*dt, sv+k3[1]*dt, sw+k3[2]*dt,
                           sp+k3[3]*dt, sq+k3[4]*dt, sr+k3[5]*dt);

  constexpr float S6 = 1.0f / 6.0f;
  snap.u          += dt * S6 * (k1[0] + 2*k2[0] + 2*k3[0] + k4[0]);
  snap.v          += dt * S6 * (k1[1] + 2*k2[1] + 2*k3[1] + k4[1]);
  snap.w          += dt * S6 * (k1[2] + 2*k2[2] + 2*k3[2] + k4[2]);
  snap.roll_rate  += dt * S6 * (k1[3] + 2*k2[3] + 2*k3[3] + k4[3]);
  snap.pitch_rate += dt * S6 * (k1[4] + 2*k2[4] + 2*k3[4] + k4[4]);
  snap.yaw_rate   += dt * S6 * (k1[5] + 2*k2[5] + 2*k3[5] + k4[5]);

  // ===== Cinematica: body rates → Euler angles =====
  sin_ph = std::sin(snap.roll);
  cos_ph = std::cos(snap.roll);
  sin_th = std::sin(snap.pitch);
  cos_th = std::cos(snap.pitch);

  float Phi_dot   = snap.roll_rate
                  + sin_ph * std::tan(snap.pitch) * snap.pitch_rate
                  + cos_ph * std::tan(snap.pitch) * snap.yaw_rate;
  float Theta_dot = cos_ph * snap.pitch_rate - sin_ph * snap.yaw_rate;
  float cos_th_safe = std::max(std::abs(cos_th), 0.001f);
  float Psi_dot   = (sin_ph * snap.pitch_rate + cos_ph * snap.yaw_rate)
                  / cos_th_safe * (cos_th >= 0 ? 1.0f : -1.0f);

  snap.roll  += Phi_dot * dt;
  snap.pitch += Theta_dot * dt;
  snap.yaw   += Psi_dot * dt;

  // Wrap angoli
  if (snap.roll > M_PI)       snap.roll -= 2.0f * M_PI;
  else if (snap.roll < -M_PI) snap.roll += 2.0f * M_PI;
  if (snap.yaw > M_PI)        snap.yaw -= 2.0f * M_PI;
  else if (snap.yaw < -M_PI)  snap.yaw += 2.0f * M_PI;
  snap.pitch = std::clamp(snap.pitch, -1.57f, 1.57f);

  // ===== Cinematica: body velocity → earth frame position =====
  float cos_ps = std::cos(snap.yaw);
  float sin_ps = std::sin(snap.yaw);
  cos_th = std::cos(snap.pitch);
  sin_th = std::sin(snap.pitch);
  cos_ph = std::cos(snap.roll);
  sin_ph = std::sin(snap.roll);

  float x_dot_e = snap.u * cos_th * cos_ps
                + snap.v * (sin_ph*sin_th*cos_ps - cos_ph*sin_ps)
                + snap.w * (cos_ph*sin_th*cos_ps + sin_ph*sin_ps);

  float y_dot_e = snap.u * cos_th * sin_ps
                + snap.v * (sin_ph*sin_th*sin_ps + cos_ph*cos_ps)
                + snap.w * (cos_ph*sin_th*sin_ps - sin_ph*cos_ps);

  float z_dot_e = snap.u * (-sin_th)
                + snap.v * sin_ph * cos_th
                + snap.w * cos_ph * cos_th;

  snap.z += x_dot_e * dt;        // Nord
  snap.x += y_dot_e * dt;        // Est
  snap.altitude -= z_dot_e * dt; // -Z_E (NED)

  // Ground clamp
  if (snap.altitude <= 0.0f) {
    snap.altitude = 0.0f;
    if (snap.w > 0.0f) snap.w = 0.0f;
    snap.roll  *= 0.9f;
    snap.pitch *= 0.9f;
  }

  // ===== Sicurezza numerica =====
  if (!std::isfinite(snap.u))          snap.u = 0.0f;
  if (!std::isfinite(snap.v))          snap.v = 0.0f;
  if (!std::isfinite(snap.w))          snap.w = 0.0f;
  if (!std::isfinite(snap.roll_rate))  snap.roll_rate = 0.0f;
  if (!std::isfinite(snap.pitch_rate)) snap.pitch_rate = 0.0f;
  if (!std::isfinite(snap.yaw_rate))   snap.yaw_rate = 0.0f;

  // ===== EICAS — messaggi di stato operativo =====
  snap.packet_id++;
  if (snap.landing_mode) {
    strncpy(snap.status_msg, "ILS LANDING MODE  [L per uscire]", 63);
  } else if (!snap.system_active) {
    strncpy(snap.status_msg, "ENGINES SHUT DOWN", 63);
  } else if (snap.protections.alpha_floor) {
    strncpy(snap.status_msg, "!! STALL WARNING — HIGH ALPHA !!", 63);
  } else if (snap.altitude < 1000.0f && snap.altitude > 50.0f) {
    // Autopilota quota bassa attivo
    strncpy(snap.status_msg, "AP QUOTA: PITCH UP ATTIVO  (<1000m)", 63);
  } else if (snap.altitude > 15000.0f) {
    // Autopilota quota alta attivo
    strncpy(snap.status_msg, "AP QUOTA: PITCH DOWN ATTIVO (>15km)", 63);
  } else if (snap.altitude < 3000.0f) {
    // Zona allarme bassa quota (1000m..3000m): solo warning, no AP
    strncpy(snap.status_msg, "CAUTION: BASSA QUOTA (< 3000m)", 63);
  } else if (snap.altitude > 12000.0f) {
    // Zona allarme alta quota (12000m..15000m): solo warning, no AP
    strncpy(snap.status_msg, "CAUTION: ALTA QUOTA (> 12000m)", 63);
  } else {
    strncpy(snap.status_msg, "VOLO NORMALE — 6-DOF F-16", 63);
  }
  snap.status_msg[63] = '\0';

  {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_state = snap;
  }
}

void FlightControlComputer::debug_print() const {
  FlightState s = get_state();
  const ProtectionStatus &p = s.protections;

  float Vt = std::sqrt(s.u*s.u + s.v*s.v + s.w*s.w);
  std::cout << "\033[1;36m[FCC 6-DOF]\033[0m "
            << "Vt=" << std::fixed << std::setprecision(1) << Vt
            << " a=" << s.alpha * (180.0f / M_PI)
            << " b=" << s.beta * (180.0f / M_PI)
            << " | R=" << std::setprecision(2) << s.roll * (180.0f / M_PI)
            << " P=" << s.pitch * (180.0f / M_PI)
            << " Y=" << s.yaw * (180.0f / M_PI)
            << " | alt=" << (int)s.altitude
            << " | act[e=" << std::setprecision(1) << m_actuator.ele
            << " a=" << m_actuator.ail
            << " r=" << m_actuator.rud
            << " l=" << m_actuator.lef
            << " T=" << std::setprecision(2) << m_actuator.thr << "]"
            << " | " << s.status_msg
            << std::endl;
}
