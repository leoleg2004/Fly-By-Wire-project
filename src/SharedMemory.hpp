// Bus di memoria condivisa per comunicazione inter-thread (Pilota/Monitor)
#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

#include <mutex>
#include <condition_variable>
#include <chrono>

// Struttura dati
struct FlightControls {
    long packet_id;
    float aileron;      // Roll (X)
    float elevator;     // Pitch (Y)
    float rudder;       // Yaw (Z)
    float altitude;     // altitudine
    float x;        // Posizione sulla mappa Est/Ovest
        float z;
    float speed;// Spinta propulsiva (Throttle)
    std::chrono::steady_clock::time_point timestamp; // Timestamp metriche di latenza
    bool autopilot_engaged; // Stato sistema di volo autonomo (Recovery)
    bool recovery_bank;
    bool landing_mode;
};

class SharedMemoryBus {
    FlightControls data;
    std::mutex mtx;
    std::condition_variable cv;
    bool new_data_available = false;

public:
    // Scrittura atomica dei dati telemetrici nella struttura FlightControls
    void write(long id, float roll, float pitch, float yaw, float alt, bool auto_on,float speed,float x,float z,bool recovery_bank,bool landing_mode) {
        std::unique_lock<std::mutex> lock(mtx);
        
        data.packet_id = id;
        data.aileron = roll;
        data.elevator = pitch;
        data.rudder = yaw;
        data.altitude = alt;            // Quota
        data.autopilot_engaged = auto_on; // Flag autopilota
        data.recovery_bank= recovery_bank;// Flag correzione assetto
        data.x=x;
        data.z=z;
        data.speed=speed;
        data.landing_mode=landing_mode;
        data.timestamp = std::chrono::steady_clock::now();
        
        new_data_available = true;
        cv.notify_one(); // Segnale di nuovi dati disponibili
    }

    // Lettura bloccante con timeout dei dati di volo
    bool read_with_timeout(FlightControls& final_data, int timeout_ms) {
        std::unique_lock<std::mutex> lock(mtx);
        // Attesa condizionale sui nuovi dati
        if(cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]{
        	return new_data_available;
        })) {
            final_data = data;
            new_data_available = false;
            return true;
        }
        return false;
    }
};

#endif
