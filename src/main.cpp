#include "SharedMemory.hpp"
#include "TelemetryPubSubTypes.hpp" 
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include <thread>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

using namespace eprosima::fastdds::dds;

SharedMemoryBus bus;

// Input Non Bloccante
int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF) { ungetc(ch, stdin); return 1; }
    return 0;
}


void Grafica_Aereo(float roll, float pitch, float yaw, float alt, bool recovery, bool stability_kick) {
    std::cout << "\033[2J\033[1;1H"; // Pulisce schermo

    // COLORE BORDO
    std::string border_col = "\033[1;34m"; // Blu
    if (recovery) border_col = "\033[1;41m"; // Sfondo Rosso
    else if (stability_kick || std::abs(roll) > 1.2f) border_col = "\033[1;33m"; // Giallo

    std::cout << border_col << "==================================================" << "\033[0m\n";
    
    if (recovery)           std::cout << "\033[1;31m  !!! TERRAIN PULL UP(NOW) - AUTOMATIC RECOVERY !!!    \033[0m\n";
    else if (stability_kick)std::cout << "\033[1;33m  !!! STABILITY KICK - CORRECTING BANK ANGLE !!!  \033[0m\n";
    else                    std::cout << "\033[1;32m       SYSTEM NORMAL - MANUAL CONTROL             \033[0m\n";
    
    std::cout << border_col << "==================================================" << "\033[0m\n\n";

    // ORIZZONTE ARTIFICIALE (Grafica vettoriale simulata)
    // Il concetto: il mirino (+) è fisso, le ali (-) si muovono
    
    std::string w = "\033[1;37m"; // Bianco
    std::string g = "\033[1;32m"; // Verde (Terra)
    std::string s = "\033[1;36m"; // Ciano (Cielo)
    std::string r = "\033[0m";    // Reset

    std::cout << "            INTERFACCIA AEREO\n";
    std::cout << "      --------------------------------\n";
    
    // Logica di disegno basata sull'angolo
    if (std::abs(roll) < 0.2f) {
        // LIVELLATO
        if(pitch > 0.1f)      std::cout << "      |      " << s << "       ^        " << r << "      |\n"; // Cielo
        else if(pitch < -0.1f)std::cout << "      |      " << g << "       v        " << r << "      |\n"; // Terra
        else                  std::cout << "      |                                |\n";
        
        std::cout << "      |      " << w << "-----( + )-----" << r << "     |\n";
        std::cout << "      |                                |\n";
    }
    else if (roll >= 0.2f && roll < 1.4f) {
        // VIRATA DESTRA (Soft)
        std::cout << "      | " << s << "            /           " << r << "     |\n";
        std::cout << "      | " << s << "          /             " << r << "     |\n";
        std::cout << "      | " << w << "      ---( + )          " << r << "     |\n";
        std::cout << "      | " << g << "              \\         " << r << "     |\n";
        std::cout << "      | " << g << "               \\        " << r << "     |\n";
    }
    else if (roll <= -0.2f && roll > -1.4f) {
        // VIRATA SINISTRA (Soft)
        std::cout << "      | " << s << "     \\                  " << r << "     |\n";
        std::cout << "      | " << s << "      \\                 " << r << "     |\n";
        std::cout << "      | " << w << "          ( + )---      " << r << "     |\n";
        std::cout << "      | " << g << "         /              " << r << "     |\n";
        std::cout << "      | " << g << "        /               " << r << "     |\n";
    }
    else {
        // PERICOLO ESTREMO / RIBALTAMENTO
        std::cout << "      | " << "\033[1;31m" << "    ! ! ! DANGER ! ! !  " << r << "     |\n";
        std::cout << "      | " << "\033[1;31m" << "         CRITICAL       " << r << "     |\n";
        std::cout << "      | " << w << "         ( X )          " << r << "     |\n";
        std::cout << "      | " << "\033[1;31m" << "          BANK          " << r << "     |\n";
        std::cout << "      | " << "\033[1;31m" << "    ! ! ! DANGER ! ! !  " << r << "     |\n";
    }
    std::cout << "      --------------------------------\n\n";

    // DATI NUMERICI
    std::cout << " [8] SU [2] GIU | [4] SX [6] DX | [5] RESET\n";
    std::cout << " ------------------------------------------\n";
    
    std::cout << " ALTITUDE : " << std::setw(5) << (int)alt << " m ";
    if (alt < 2000) std::cout << "\033[1;31m[LOW]\033[0m";
    else if (recovery) std::cout << "\033[1;33m[CLIMB]\033[0m";
    else std::cout << "\033[1;32m[OK]\033[0m";
    std::cout << "\n";

    std::cout << " ROLL     : " << std::fixed << std::setprecision(2) << roll << " rad ";
    if(std::abs(roll) > 1.2f) std::cout << "\033[1;33m[LIMIT]\033[0m";
    std::cout << "\n";
    
    std::cout << " PITCH    : " << pitch << " rad\n";
    std::cout << " YAW      : " << yaw << " rad\n";
}

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// THREAD PILOTA
void pilot_task() {
    pin_to_core(0);
    unsigned long id = 0;
    
    float cmd_roll = 0.0f;
    float cmd_pitch = 0.0f;
    float cmd_yaw = 0.0f;
    float altitude = 10000.0f; 
    
    bool recovery_low = false;
    bool recovery_high=false;
    bool recovery_mode=false;
    bool stability_kick= false;

    while(true) {
        stability_kick= false;

        // 1. INPUT (Solo se non siamo in recovery totale)
        if ((!recovery_mode and !recovery_high) && kbhit()) {
            char c = getchar();
            if (c == '6') cmd_roll += 0.1f; if (c == '4') cmd_roll -= 0.1f;         
            if (c == '8') cmd_pitch += 0.1f; if (c == '2') cmd_pitch -= 0.1f;        
            if (c == '9') cmd_yaw += 0.1f; if (c == '7') cmd_yaw -= 0.1f;          
            if (c == '5') { cmd_roll = 0.0f; cmd_pitch = 0.0f; cmd_yaw = 0.0f; } 
        }

        // 2. SISTEMA DI STABILITA' (ANTI-RIBALTAMENTO)
        // 2. CONTROLLO INNESCO EMERGENZE (Trigger)
                if (altitude < 2000.0f) recovery_low = true;
                if (altitude > 15000.0f) recovery_high = true;

                // 3. GESTIONE STATI
                if (recovery_low) {
                    // RECUPERO BASSA QUOTA (PULL UP)
                    cmd_roll *= 0.9f; // Raddrizza ali
                    cmd_pitch = 0.5f; // Naso su deciso
                    if (altitude >= 4500.0f) { recovery_low = false; cmd_pitch = 0.0f; }
                }
                else if (recovery_high) {
                    // RECUPERO ALTA QUOTA (NOSE DOWN)
                    cmd_roll *= 0.9f;  // Raddrizza ali (segno positivo!)
                    cmd_pitch = -0.5f; // Naso giù deciso

                    // Esci solo quando torni a 11.000m
                    if (altitude <= 11000.0f) { recovery_high = false; cmd_pitch = 0.0f; }
                }
                else {
                    // VOLO NORMALE: Controllo Anti-Ribaltamento
                    if (std::abs(cmd_roll) > 1.4f) {
                        stability_kick = true;
                        if (cmd_roll > 0) cmd_roll -= 0.6f;
                        else cmd_roll += 0.6f;
                    }
                }
        float v_speed = (cmd_pitch * 60.0f);
        if (std::abs(cmd_roll) > 0.5f) v_speed -= 15.0f; // Perdita quota in virata
        altitude += v_speed; 

       //limitazioni di volo
        if(cmd_roll > 3.2f) cmd_roll = 3.2f; if(cmd_roll < -3.2f) cmd_roll = -3.2f;

        bus.write(id, cmd_roll, cmd_pitch, cmd_yaw, altitude, (recovery_low || recovery_high));
                 Grafica_Aereo(cmd_roll, cmd_pitch, cmd_yaw, altitude, recovery_low, stability_kick);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        id++;
    }
}

void flight_computer_task(DataWriter* writer) {
    pin_to_core(0);
    FlightControls state;
    SystemStats stats;
    
    while(true) {
        bool ok = bus.read_with_timeout(state, 100);
        if(ok) {
            stats.packet_id(state.packet_id);
            stats.roll(state.aileron);
            stats.pitch(state.elevator);
            stats.yaw(state.rudder);
            stats.altitude(state.altitude);

            if (state.autopilot_engaged) {
                            // Se l'autopilota è attivo, controlliamo perché
                            if (state.altitude < 4000.0f) {
                                stats.status_msg("ALARM: TERRAIN PULL UP"); // Allarme Rosso
                            } else if (state.altitude > 15000.0f) {
                                stats.status_msg("ALARM: HIGH ALTITUDE");   // Allarme Rosso
                            } else {
                                stats.status_msg("AUTOPILOT: STABILIZING"); // Transizione
                            }
                        }
                        else if (std::abs(state.aileron) > 1.2f) {
                            stats.status_msg("WARN: HIGH BANK ANGLE");      // Allarme Giallo
                        }
                        else {
                            stats.status_msg("NOMINAL FLIGHT");             // Verde
                        }
                    }
                    writer->write(&stats);
                }
            }
int main() {
    DomainParticipantQos pqos;
    pqos.name("Plane_Node");
    DomainParticipant* part = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    TypeSupport type(new SystemStatsPubSubType());
    type.register_type(part);
    Publisher* pub = part->create_publisher(PUBLISHER_QOS_DEFAULT);
    Topic* topic = part->create_topic("TelemetryTopic", "SystemStats", TOPIC_QOS_DEFAULT);
    DataWriter* writer = pub->create_datawriter(topic, DATAWRITER_QOS_DEFAULT);

    std::thread t1(pilot_task);
    std::thread t2(flight_computer_task, writer);
    t1.join(); 
    t2.join();
    return 0;
}
