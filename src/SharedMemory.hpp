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
        float speed;//acceleratore da ggiungere al monitor
    std::chrono::steady_clock::time_point timestamp; // Per calcolare la latenza interna
    bool autopilot_engaged; // Dice al computer se siamo in "Recovery Mode"
    bool recovery_bank;
};

class SharedMemoryBus {
    FlightControls data;
    std::mutex mtx;
    std::condition_variable cv;
    bool new_data_available = false;

public:
    // Funzione di scrittura aggiornata con 7 parametri tutti messi nella struct FlightControls
    void write(long id, float roll, float pitch, float yaw, float alt, bool auto_on,float speed,float x,float z,bool recovery_bank) {
        std::unique_lock<std::mutex> lock(mtx);
        
        data.packet_id = id;
        data.aileron = roll;
        data.elevator = pitch;
        data.rudder = yaw;
        data.altitude = alt;            // Salviamo l'altitudine
        data.autopilot_engaged = auto_on; // Salviamo lo stato dell'autopilota
        data.recovery_bank= recovery_bank;//controlliamo lo stato di roll
        data.x=x;
        data.z=z;
        data.speed=speed;
        data.timestamp = std::chrono::steady_clock::now();
        
        new_data_available = true;
        cv.notify_one(); // Sveglia il thread del Computer di Volo
    }

    // Funzione di lettura copia tutta la struct usata
    bool read_with_timeout(FlightControls& final_data, int timeout_ms) {
        std::unique_lock<std::mutex> lock(mtx);
        // Aspetta finché non ci sono dati o scade il tempo
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
