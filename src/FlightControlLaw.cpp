#include "FlightControlLaw.hpp"
#include <algorithm>
#include <cmath>

FlightControlLaw::FlightControlLaw() {}

FBWMode FlightControlLaw::evaluate_mode(const FlightState &state) const {
  if (!state.system_active)
    return FBWMode::DIRECT_LAW;
  return FBWMode::NORMAL_LAW;
}

ControlSurfaces FlightControlLaw::compute(const PilotInput &pilot,
                                          const FlightState &state, float dt) {
  ControlSurfaces surf{};
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

ControlSurfaces FlightControlLaw::normal_law(const PilotInput &p,
                                             const FlightState &s, float dt) {
  ControlSurfaces surf{};

  // --- CANALE LATERALE: Roll Rate Command ---
  float roll_rate_demand = p.stick_roll * MAX_ROLL_RATE;
  surf.aileron_deflection = (roll_rate_demand / MAX_ROLL_RATE) * MAX_AIL_DEG;

  // --- CANALE LONGITUDINALE: C* Law ---
  float pitch_rate_demand = p.stick_pitch * MAX_PITCH_RATE;
  if (std::abs(p.stick_pitch) < 0.02f) {
    m_pitch_integrator *= 0.97f;
    pitch_rate_demand = m_pitch_integrator;
  } else {
    m_pitch_integrator = pitch_rate_demand * 0.25f;
  }
  surf.elevator_deflection =
      (pitch_rate_demand / MAX_PITCH_RATE) * MAX_ELEV_DEG;

  // --- CANALE DIREZIONALE: Yaw Damper ---
  float yaw_damper = -s.yaw_rate * 0.5f;
  surf.rudder_deflection = std::clamp(p.rudder * MAX_RUD_DEG + yaw_damper,
                                      -MAX_RUD_DEG, MAX_RUD_DEG);
  return surf;
}

ControlSurfaces FlightControlLaw::alternate_law(const PilotInput &p) {
  return {p.stick_roll * MAX_AIL_DEG, p.stick_pitch * MAX_ELEV_DEG,
          p.rudder * MAX_RUD_DEG, 0.0f};
}

ControlSurfaces FlightControlLaw::direct_law(const PilotInput &p) {
  return {p.stick_roll * MAX_AIL_DEG, p.stick_pitch * MAX_ELEV_DEG,
          p.rudder * MAX_RUD_DEG, 0.0f};
}

void FlightControlLaw::apply_protections(ControlSurfaces &surf,
                                         const FlightState &s,
                                         const PilotInput &p) {
  // Stall (invariato)
  m_prot.alpha_floor = (s.speed < ALPHA_MIN_SPEED && s.altitude > 0.5f);

  // Overspeed (invariato)
  m_prot.overspeed = (s.speed > VMO);

  // =====================================================================
  // TERRAIN PULL UP (Low Recovery)
  // Audio (sndPullUp):  sotto 2000m      — gestito in FlightDisplay
  // Autopilota:         sotto 300m       — gestito in FlightControlComputer
  // Flag usato SOLO per messaggio EICAS: sotto 300m
  // =====================================================================
  m_prot.terrain_avoidance = (s.altitude < 300.0f && s.altitude > 0.5f &&
                              s.system_active && !p.landing_mode);

  // =====================================================================
  // OVERSHOOT PULL DOWN (High Recovery)
  // Audio (sndPullUp):  sopra 12500m     — gestito in FlightDisplay
  // Autopilota:         sopra 15000m     — gestito in FlightControlComputer
  // Flag usato SOLO per messaggio EICAS: sopra 15000m
  // =====================================================================
  m_prot.high_altitude =
      (s.altitude > 15000.0f && s.system_active && !p.landing_mode);

  // =====================================================================
  // CRITICAL BANK ANGLE
  // Audio (sndWarning): sopra 0.8 rad    — gestito in FlightDisplay
  // Autopilota:         sopra 1.0 rad    — gestito in FlightControlComputer
  // Flag usato SOLO per messaggio EICAS e per ingaggio autopilota: sopra 1.0
  // =====================================================================
  m_prot.bank_angle_limit =
      (std::abs(s.roll) > 1.0f && s.system_active && !p.landing_mode);

  // Clamp meccanico strutturale (limiti fisici massimi dell'aereo)
  surf.aileron_deflection =
      std::clamp(surf.aileron_deflection, -MAX_AIL_DEG, MAX_AIL_DEG);
  surf.elevator_deflection =
      std::clamp(surf.elevator_deflection, -MAX_ELEV_DEG, MAX_ELEV_DEG);
  surf.rudder_deflection =
      std::clamp(surf.rudder_deflection, -MAX_RUD_DEG, MAX_RUD_DEG);
}
