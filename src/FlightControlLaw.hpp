#pragma once
#include "FBW_Types.hpp"

// FlightControlLaw.hpp
// Flight Envelope Protection & Control Laws (Normal, Alternate, Direct)
class FlightControlLaw {
public:
  FlightControlLaw();

  // Calcola deflessioni superfici dall'input pilota e dallo stato di volo
  ControlSurfaces compute(const PilotInput &pilot, const FlightState &state,
                          float dt);

  FBWMode evaluate_mode(const FlightState &state) const;

  const ProtectionStatus &protection_status() const { return m_prot; }

private:
  ControlSurfaces normal_law(const PilotInput &p, const FlightState &s,
                             float dt);
  ControlSurfaces alternate_law(const PilotInput &p);
  ControlSurfaces direct_law(const PilotInput &p);
  void apply_protections(ControlSurfaces &surf, const FlightState &s,
                         const PilotInput &p);

  ProtectionStatus m_prot{};
  float m_pitch_integrator = 0.0f;

  static constexpr float MAX_ROLL_RATE = 1.5f;  // rad/s in Normal Law
  static constexpr float MAX_PITCH_RATE = 0.5f; // rad/s in Normal Law
  static constexpr float MAX_AIL_DEG = 25.0f;
  static constexpr float MAX_ELEV_DEG = 30.0f;
  static constexpr float MAX_RUD_DEG = 30.0f;
  static constexpr float BANK_LIMIT = 1.17f;      // 67° in rad
  static constexpr float ALPHA_MIN_SPEED = 60.0f; // KPH
  static constexpr float VMO = 290.0f;            // KPH
  static constexpr float ALT_MAX = 13000.0f;
  static constexpr float GPWS_ALT = 2000.0f;
};
