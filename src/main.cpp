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
#include <vector>
#include "raylib.h"
#include "raymath.h"
#include "FlightDisplay.hpp"
#include <fastdds/rtps/attributes/PropertyPolicy.hpp>
#include <fastdds/statistics/dds/domain/DomainParticipant.hpp>
#include <fastdds/statistics/topic_names.hpp>
#include <fastdds/statistics/dds/publisher/qos/DataWriterQos.hpp>

using namespace eprosima::fastdds::dds;

SharedMemoryBus bus;
PlaneData Aereo;
std::mutex Aereo_mutex;

void flight_computer_task(DataWriter* writer) {
    FlightControls state;
    SystemStats stats;

    std::cout << "[DDS] Computer di bordo avviato. In attesa dati..." << std::endl;

    while(true) {
        bool active;
        std::string current_msg;
        {
            std::lock_guard<std::mutex> lock(Aereo_mutex);
            active = Aereo.system_active;
            current_msg = Aereo.status_msg;
        }

        bool ok = bus.read_with_timeout(state, 100);

        if(ok) {
            stats.packet_id(state.packet_id);
            stats.roll(state.aileron);
            stats.pitch(state.elevator);
            stats.yaw(state.rudder);
            stats.altitude(state.altitude);
            stats.speed(state.speed);
            stats.x(state.x);
            stats.z(state.z);
            stats.landing_mode(state.landing_mode);

            // ORA PASSA IL VERO STATO AL MONITOR (non più bloccato a true!)
            stats.system_active(active);
            stats.status_msg(current_msg);

            writer->write(&stats);
        }

        // Continua sempre a mandare dati (anche se spento), così il monitor lo sa!
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    DomainParticipantQos pqos;
    pqos.name("Pilot_Node_F35");

    // 1. Abilitazione QoS per le Statistiche
        pqos.properties().properties().emplace_back("fastdds.statistics",
            "HISTORY_LATENCY;"
            "NETWORK_LATENCY;"
            "PUBLICATION_THROUGHPUT;"
            "SUBSCRIPTION_THROUGHPUT;"
            "HEARTBEAT_COUNT;"
            "ACKNACK_COUNT;"
            "DISCOVERY_STATISTICS;"
            "PHYSICAL_DATA_STATISTICS");

        DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
        if (participant == nullptr) return 1;

        // 2. Attivazione dei DataWriter Statistici del kernel DDS
        auto* stat_participant = eprosima::fastdds::statistics::dds::DomainParticipant::narrow(participant);
        if (stat_participant != nullptr) {
            stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::PUBLICATION_THROUGHPUT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
            stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::NETWORK_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
            stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HISTORY_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
            stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HEARTBEAT_COUNT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
        }

        TypeSupport type(new SystemStatsPubSubType());
        type.register_type(participant);

        Publisher* pub = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
        Topic* topic = participant->create_topic("TelemetryTopic", type.get_type_name(), TOPIC_QOS_DEFAULT);

        DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
        wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;

        // 3. Assegnazione proprietà statistiche specifiche al DataWriter
        wqos.properties().properties().emplace_back("fastdds.statistics",
            "PUBLICATION_THROUGHPUT;"
            "HISTORY_LATENCY");

        DataWriter* writer = pub->create_datawriter(topic, wqos);
        if (writer == nullptr) return 1;

        std::thread Pilota_dds(flight_computer_task, writer);

    FlightDisplay display(1000, 900, "Leonardo Flight System - Manual Control");

    unsigned long packet_id = 0;
    bool recovery_low = false;
    bool recovery_high = false;
    bool recovery_bank = false;
    bool recovery_zero = false;

    Aereo.roll = 0.0f;
    Aereo.pitch = 0.0f;
    Aereo.yaw = 0.0f;
    Aereo.altitude = 0.0f;
    Aereo.x = 0.0f;
    Aereo.z = 0.0f;
    Aereo.speed = 0.0f;
    Aereo.system_active = true;

    while (display.IsActive()) {

        display.HandleInput(Aereo);

        if (Aereo.landing_mode) {
            recovery_low = false;
            recovery_zero = false;
            recovery_bank = false;
            recovery_high = false;
            std::lock_guard<std::mutex> lock(Aereo_mutex);
            strcpy(Aereo.status_msg, "ILS LANDING MODE ACTIVE");
        } else {
            if (Aereo.altitude < 2000.0f && Aereo.speed >= 0.0f) recovery_low = true;
            if (Aereo.speed < 10.0f && Aereo.altitude > 100.0f && Aereo.altitude <= 2500.0f) recovery_zero = true;
            if (std::abs(Aereo.roll) > 1.2f) recovery_bank = true;
            if (Aereo.altitude > 13000.0f) recovery_high = true;

            std::lock_guard<std::mutex> lock(Aereo_mutex);
            if (recovery_low) {
                strcpy(Aereo.status_msg, "TERRAIN PULL UP");
            } else if (recovery_high) {
                strcpy(Aereo.status_msg, "OVERSHOOT PULL DOWN");
            } else if (recovery_zero) {
                strcpy(Aereo.status_msg, "STALL WARNING - LOW SPEED");
            } else if (recovery_bank) {
                strcpy(Aereo.status_msg, "CRITICAL BANK ANGLE");
            } else {
                strcpy(Aereo.status_msg, "NORMAL FLIGHT");
            }
        }

        if (recovery_low) {
            Aereo.roll *= 0.95f;
            if (Aereo.pitch < 0.3f) Aereo.pitch += 0.005f;
            if (Aereo.speed < 150.0f) Aereo.speed += 0.5f;
            if (Aereo.altitude >= 2500.0f) recovery_low = false;
        }

        if (recovery_zero) {
            if (Aereo.speed < 100.0f) Aereo.speed += 1.5f;
            if (Aereo.pitch < 0.2f) Aereo.pitch += 0.01f;
            if (Aereo.altitude >= 2500.0f && Aereo.speed >= 100.0f) recovery_zero = false;
        } else if (recovery_high) {
            Aereo.roll *= 0.95f;
            if (Aereo.pitch > 0.0f) Aereo.pitch -= 0.05f;
            else if (Aereo.pitch > -0.2f) Aereo.pitch -= 0.005f;
            if (Aereo.altitude <= 12000.0f) recovery_high = false;
        }

        if (recovery_bank) {
                    // L'autopilota entra in azione con forza maggiore (0.08f invece di 0.03f)
                    if (Aereo.roll > 0.05f) {
                        Aereo.roll -= 0.08f;
                    } else if (Aereo.roll < -0.05f) {
                        Aereo.roll += 0.08f;
                    } else {
                        // Sgancio dell'autopilota
                        Aereo.roll = 0.0f;
                        recovery_bank = false;
                    }
                }
        // FISICA DI VOLO E VIRATA
        Aereo.yaw -= Aereo.roll * 0.015f;

        if (Aereo.pitch < -0.2f && Aereo.speed < 200.0f) {
            Aereo.speed += std::abs(Aereo.pitch) * 0.5f;
        }
        if (Aereo.pitch > 0.5f && Aereo.speed > 30.0f) {
            Aereo.speed -= Aereo.pitch * 0.3f;
        }

        float speed_orizzontale = Aereo.speed * std::cos(Aereo.pitch);
        float speed_verticale   = Aereo.speed * std::sin(Aereo.pitch);

        if (Aereo.speed < 60.0f && Aereo.altitude > 0.5f) {
            float mancanza_portanza = 60.0f - Aereo.speed;
            speed_verticale -= (mancanza_portanza * 0.8f);
        }

        if (std::abs(Aereo.roll) > 1.4f) {
            speed_verticale -= 1.5f;
        }

        float physics_scale = 1.0f;
        Aereo.x += std::sin(Aereo.yaw) * speed_orizzontale * physics_scale;
        Aereo.z += std::cos(Aereo.yaw) * speed_orizzontale * physics_scale;
        Aereo.altitude += speed_verticale * 0.1f;

        // ATTERRAGGIO E ATTRITO
        if (Aereo.altitude <= 0.1f) {
            Aereo.altitude = 0.0f;
            Aereo.pitch = Lerp(Aereo.pitch, 0.0f, 0.1f);
            Aereo.roll  = Lerp(Aereo.roll, 0.0f, 0.1f);
            if (Aereo.speed > 0.0f) {
                Aereo.speed -= 0.5f;
            }
        }

        // ==========================================
        // BLOCCHI E LIMITI DI SICUREZZA FISICI
        // ==========================================
        if(Aereo.roll > 3.2f)  Aereo.roll = 3.2f;
        if(Aereo.roll < -3.2f) Aereo.roll = -3.2f;
        if(Aereo.pitch > 1.5f) Aereo.pitch = 1.5f;
        if(Aereo.pitch < -1.5f) Aereo.pitch = -1.5f;

        // 1. Limite massimo di velocità a 300 KPH!
        if (Aereo.speed > 300.0f) Aereo.speed = 300.0f;
        if (Aereo.speed < 0.0f) Aereo.speed = 0.0f;

        // 2. Se l'aereo è a terra ed è SPENTO, velocità forzata a 0
        if (!Aereo.system_active && Aereo.altitude <= 0.1f) {
            Aereo.speed = 0.0f;
        }

        bus.write(packet_id++, Aereo.roll, Aereo.pitch, Aereo.yaw, Aereo.altitude, (recovery_low || recovery_high), Aereo.speed, Aereo.x, Aereo.z, recovery_bank, Aereo.landing_mode);

        display.Draw(Aereo);
    }

    // Chiusura sicura
    {
        std::lock_guard<std::mutex> lock(Aereo_mutex);
        Aereo.system_active = false;
    }

    exit(0); // Forza l'uscita rapida senza bloccare i thread
    return 0;
}
