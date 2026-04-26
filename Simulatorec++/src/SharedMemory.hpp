// Bus di memoria condivisa per comunicazione inter-thread (Pilota/Monitor)
#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

#include "FBW_Types.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>

// Struttura dati originale (mantenuta per retrocompatibilità interna)
struct FlightControls {
  long packet_id;
  float aileron;  // Roll (X)
  float elevator; // Pitch (Y)
  float rudder;   // Yaw (Z)
  float altitude; // altitudine
  float x;        // Posizione sulla mappa Est/Ovest
  float z;
  float speed; // Spinta propulsiva (Throttle)
  std::chrono::steady_clock::time_point timestamp;
  bool autopilot_engaged;
  bool recovery_bank;
  bool landing_mode;
};

class SharedMemoryBus {
  FlightControls data;
  std::mutex mtx;
  std::condition_variable cv;
  bool new_data_available = false;

public:
  void write(long id, float roll, float pitch, float yaw, float alt,
             bool auto_on, float speed, float x, float z, bool recovery_bank,
             bool landing_mode) {
    std::unique_lock<std::mutex> lock(mtx);
    data.packet_id = id;
    data.aileron = roll;
    data.elevator = pitch;
    data.rudder = yaw;
    data.altitude = alt;
    data.autopilot_engaged = auto_on;
    data.recovery_bank = recovery_bank;
    data.x = x;
    data.z = z;
    data.speed = speed;
    data.landing_mode = landing_mode;
    data.timestamp = std::chrono::steady_clock::now();
    new_data_available = true;
    cv.notify_one();
  }

  bool read_with_timeout(FlightControls &final_data, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mtx);
    if (cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                    [this] { return new_data_available; })) {
      final_data = data;
      new_data_available = false;
      return true;
    }
    return false;
  }
};

// =========================================================================
// PilotInputBus — canale FBW dal thread di rendering al FCC
//
// Il thread grafico scrive i comandi pilota (stick, throttle, toggle).
// Il thread FCC legge a ogni ciclo da 100 Hz.
// Usa un semplice mutex: nessun timeout necessario perché il FCC usa
// l'ultimo input disponibile anche se non ci sono nuovi dati.
// =========================================================================
class PilotInputBus {
  PilotInput m_data{};
  mutable std::mutex m_mtx;

public:
  void write(const PilotInput &input) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_data = input;
  }

  PilotInput read() const {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_data;
  }
};

#endif
