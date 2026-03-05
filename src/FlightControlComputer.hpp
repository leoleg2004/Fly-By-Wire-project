#pragma once
#include "FBW_Types.hpp"
#include "FlightControlLaw.hpp"
#include <cstring>
#include <mutex>

// =========================================================================
// FlightControlComputer — FCC: cuore del sistema Fly-By-Wire
//
// Responsabilità:
//   1. Riceve PilotInput (stick, speed_delta, toggle comandi)
//   2. Richiama FlightControlLaw per ottenere le deflessioni superfici
//   3. Integra la fisica di volo con dt = frame time reale del renderer
//   4. Gestisce l'autopilota di recovery con hysteresis
//   5. Aggiorna FlightState in modo thread-safe
//   6. Determina il messaggio di stato EICAS
// =========================================================================
class FlightControlComputer {
public:
  FlightControlComputer();

  void step(const PilotInput &input, float dt);
  FlightState get_state() const;
  void set_initial_state(const FlightState &state);
  void debug_print() const;

private:
  FlightControlLaw m_law;
  FlightState m_state;
  mutable std::mutex m_mtx;

  // --- Hysteresis autopilota ---
  // Quando una protezione si attiva, l'autopilota resta ingaggiato
  // finché il target di recovery non viene raggiunto.
  // Senza hysteresis il flag oscilla on/off ogni frame → "blocco".
  bool m_terrain_recovery_active = false;  // Low Recovery ingaggiato
  bool m_high_alt_recovery_active = false; // High Recovery ingaggiato
  bool m_bank_recovery_active = false;     // Bank Recovery ingaggiato

  // Efficacia superfici
  static constexpr float EFF_AIL = 0.06f;
  static constexpr float EFF_ELEV = 0.04f;
};
