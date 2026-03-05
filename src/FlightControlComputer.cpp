#include "FlightControlComputer.hpp"
#include <algorithm>
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

// Main FCC step integration
// Bound dt to prevent physics instability during frame spikes
// Ensures NaN/Inf validation
void FlightControlComputer::step(const PilotInput &input, float dt) {
  dt = std::clamp(dt, 0.001f, 0.05f);

  FlightState snap;
  {
    std::lock_guard<std::mutex> lock(m_mtx);
    snap = m_state;
  }

  snap.system_active = input.engines_on;
  snap.landing_mode = input.landing_mode;

  // 1. Control Law Evaluation
  ControlSurfaces surf = m_law.compute(input, snap, dt);
  snap.mode = m_law.evaluate_mode(snap);
  snap.protections = m_law.protection_status();

  // 2. Base Attitude Integration
  snap.roll_rate = surf.aileron_deflection * EFF_AIL;
  snap.pitch_rate = surf.elevator_deflection * EFF_ELEV;

  // --- Hysteresis Autopilot ---
  // A. Terrain Avoidance (Engagement: < 300m, Disengagement: >= 2000m)
  if (snap.protections.terrain_avoidance && !m_terrain_recovery_active) {
    m_terrain_recovery_active = true;
    std::cout << "[AUTOPILOT] TERRAIN PULL UP ENGAGED (Alt < 300m)\n";
  } else if (m_terrain_recovery_active && snap.altitude >= 2000.0f) {
    m_terrain_recovery_active = false;
    std::cout << "[AUTOPILOT] TERRAIN PULL UP DISENGAGED (Safe Alt: 2000m)\n";
  }

  // B. High Recovery (Overshoot)
  // Scatta a 15000m e si disinserisce a 12500m
  if (snap.protections.high_altitude && !m_high_alt_recovery_active) {
    m_high_alt_recovery_active = true;
    std::cout << "[AUTOPILOT] OVERSHOOT PULL DOWN ENGAGED (Alt > 15000m)\n";
  } else if (m_high_alt_recovery_active && snap.altitude <= 12500.0f) {
    m_high_alt_recovery_active = false;
    std::cout
        << "[AUTOPILOT] OVERSHOOT PULL DOWN DISENGAGED (Safe Alt: 12500m)\n";
  }

  // C. Bank Recovery
  // Scatta a 1.0 rad (~57 gradi) e si disinserisce a 0.05 rad (ali quasi
  // dritte)
  if (snap.protections.bank_angle_limit && !m_bank_recovery_active) {
    m_bank_recovery_active = true;
    std::cout << "[AUTOPILOT] CRITICAL BANK ENGAGED (|Roll| > 1.0 rad)\n";
  } else if (m_bank_recovery_active && std::abs(snap.roll) <= 0.05f) {
    m_bank_recovery_active = false;
    std::cout << "[AUTOPILOT] CRITICAL BANK DISENGAGED (Roll stabilized)\n";
  }

  // System/Landing mode disengages recoveries
  if (!snap.system_active || snap.landing_mode) {
    m_terrain_recovery_active = false;
    m_high_alt_recovery_active = false;
    m_bank_recovery_active = false;
  }

  // --- Apply Autopilot Overrides ---
  if (m_terrain_recovery_active) {
    // TERRAIN PULL UP (Pitch Up, Level Wings)
    if (snap.pitch < 0.2f) {
      snap.pitch_rate = 3.0f;
    } else {
      snap.pitch_rate = 0.0f;
    }
    // Emergency altitude boost
    if (snap.altitude < 1000.0f) {
      snap.altitude += 5.0f * (dt * 60.0f);
    }
    // Level wings
    snap.roll_rate = -snap.roll * 2.0f;
  } else if (m_high_alt_recovery_active) {
    // OVERSHOOT PULL DOWN (Pitch Down, Level Wings)
    if (snap.pitch > -0.2f) {
      snap.pitch_rate = -3.0f;
    } else {
      snap.pitch_rate = 0.0f;
    }
    snap.roll_rate = -snap.roll * 2.0f;
  } else if (m_bank_recovery_active) {
    // CRITICAL BANK (Level Wings, Stabilize Pitch)
    snap.roll_rate = -snap.roll * 3.0f;

    snap.pitch_rate = -snap.pitch * 1.5f;
  }

  // Apply final rates (Pilot OR Autopilot)
  snap.roll = std::clamp(snap.roll + snap.roll_rate * dt, -3.2f, 3.2f);
  snap.pitch = std::clamp(snap.pitch + snap.pitch_rate * dt, -1.5f, 1.5f);

  // 3. Virata coordinata
  snap.yaw_rate = -snap.roll * 0.015f;
  snap.yaw -= snap.roll * 0.015f * dt * 60.0f;

  // 4. Propulsione
  if (snap.system_active && input.engine_ready) {
    if (snap.pitch < -0.2f && snap.speed < 200.0f)
      snap.speed += std::abs(snap.pitch) * 0.5f;
    if (snap.pitch > 0.5f && snap.speed > 30.0f)
      snap.speed -= snap.pitch * 0.3f;

    snap.speed += input.speed_delta;
  } else if (!input.engine_ready) {
    snap.speed = 0.0f;
  }
  snap.speed = std::clamp(snap.speed, 0.0f, 300.0f);

  // 5. Fisica traslazionale
  float speed_h = snap.speed * std::cos(snap.pitch);
  float speed_v = snap.speed * std::sin(snap.pitch);

  if (snap.speed < 60.0f && snap.altitude > 0.5f)
    speed_v -= (60.0f - snap.speed) * 0.8f;
  if (std::abs(snap.roll) > 1.4f)
    speed_v -= 1.5f;

  snap.x += std::sin(snap.yaw) * speed_h * dt * 60.0f;
  snap.z += std::cos(snap.yaw) * speed_h * dt * 60.0f;
  snap.altitude += speed_v * dt * 6.0f;

  // 6. Suolo
  if (snap.altitude <= 0.1f) {
    snap.altitude = 0.0f;
    snap.pitch *= 0.9f;
    snap.roll *= 0.9f;
    if (snap.speed > 0.0f)
      snap.speed -= 0.5f;
  }
  if (!snap.system_active && snap.altitude <= 0.1f)
    snap.speed = 0.0f;

  // SICUREZZA NUMERICA
  if (!std::isfinite(snap.roll)) {
    snap.roll = 0.0f;
  }
  if (!std::isfinite(snap.pitch)) {
    snap.pitch = 0.0f;
  }
  if (!std::isfinite(snap.yaw)) {
    snap.yaw = 0.0f;
  }
  if (!std::isfinite(snap.speed)) {
    snap.speed = 0.0f;
  }
  if (!std::isfinite(snap.altitude)) {
    snap.altitude = 0.0f;
  }

  // 7. Messaggio EICAS
  // Mostra il messaggio corretto a seconda se il sistema è ingaggiato
  snap.packet_id++;
  if (snap.landing_mode) {
    strncpy(snap.status_msg, "ILS LANDING MODE ACTIVE", 63);
  } else if (!snap.system_active) {
    strncpy(snap.status_msg, "ENGINES SHUT DOWN", 63);
  } else if (m_terrain_recovery_active) {
    strncpy(snap.status_msg, "TERRAIN PULL UP (AUTOPILOT ENGAGED)", 63);
  } else if (m_high_alt_recovery_active) {
    strncpy(snap.status_msg, "OVERSHOOT PULL DOWN (AUTOPILOT ENGAGED)", 63);
  } else if (m_bank_recovery_active) {
    strncpy(snap.status_msg, "CRITICAL BANK (AUTOPILOT ENGAGED)", 63);
  } else if (snap.protections.alpha_floor) {
    strncpy(snap.status_msg, "STALL WARNING - LOW SPEED", 63);
  } else if (snap.protections.overspeed) {
    strncpy(snap.status_msg, "OVERSPEED WARNING", 63);
  } else {
    strncpy(snap.status_msg, "NORMAL FLIGHT", 63);
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

  std::cout << "\033[1;36m[FBW DEBUG]\033[0m "
            << "roll=" << std::fixed << std::setprecision(3) << s.roll
            << " pitch=" << s.pitch << " yaw=" << s.yaw
            << " spd=" << (int)s.speed << " alt=" << (int)s.altitude
            << " mode=" << (int)s.mode
            << " | PROT:" << (p.alpha_floor ? " ALPHA" : "")
            << (p.bank_angle_limit ? " b-lim" : "")
            << (p.terrain_avoidance ? " t-avd" : "")
            << (p.high_altitude ? " h-alt" : "") << (p.overspeed ? " VMO" : "")
            << " | AUTO:" << (m_terrain_recovery_active ? " TERRAIN" : "")
            << (m_high_alt_recovery_active ? " HIGH_ALT" : "")
            << (m_bank_recovery_active ? " BANK" : "") << " | " << s.status_msg
            << std::endl;
}
