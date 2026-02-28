#include "SharedMemory.hpp"
#include "TelemetryPubSubTypes.hpp"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/attributes/PropertyPolicy.hpp>
#include <fastdds/statistics/dds/domain/DomainParticipant.hpp>
#include <fastdds/statistics/topic_names.hpp>
#include <fastdds/statistics/dds/publisher/qos/DataWriterQos.hpp>
#include <thread>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include "raylib.h"
#include "raymath.h"
#include "FlightDisplay.hpp"

using namespace eprosima::fastdds::dds;
//metto in numeri in questa scrittura 0f per trattarli come float

//metto bus per usare la sharedMemoryBus.hpp
SharedMemoryBus bus;

// Variabili Globali
PlaneData Aereo;       // Stato attuale dell'aereo messo nel FlightDispaly.hpp
std::mutex Aereo_mutex;      // Semaforo per thread safety


void flight_computer_task(DataWriter* writer) {
    FlightControls state;
    SystemStats stats;
    int count = 0;

    std::cout << "[DDS] Computer di bordo avviato. In attesa dati..." << std::endl;

    while(true) {
        // Controllo se dobbiamo chiudere
    	bool active;
        {
            std::lock_guard<std::mutex> lock(Aereo_mutex);
            active = Aereo.system_active;
        }
        if (!active) break;

        //e qua che faccio la lettura dopo la scrittura del pilota con un timeout di 100 milisecondi perui se pri,a non arriva nulla leggo
        bool ok = bus.read_with_timeout(state, 100);

        if(ok) {
            // prendo dalla strucin telemtry idl che nel monitor node usero per la stampa
            stats.packet_id(state.packet_id);
            stats.roll(state.aileron);
            stats.pitch(state.elevator);
            stats.yaw(state.rudder);
            stats.altitude(state.altitude);
            stats.speed(state.speed);//aggiunto a posteriori ho dovuto aggiornare file con .idl

            // Logica autopilota
            if (state.autopilot_engaged) {
            	// Usiamo questo flag per indicare RECOVERY
                if (state.altitude < 1000.0f) stats.status_msg("ALARM: TERRAIN PULL UP");

                else if (state.altitude > 12000.0f) stats.status_msg("ALARM: HIGH ALTITUDE");

                else stats.status_msg("AUTOPILOT: RECOVERY");
            }
            else if (std::abs(state.aileron) > 1.2f) stats.status_msg("WARN: HIGH BANK ANGLE");

            else stats.status_msg("NOMINAL FLIGHT");

            // scrivo all'interno della struct
            writer->write(&stats);
            count++;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}


int main() {

	DomainParticipantQos pqos;
	    pqos.name("Pilot_Node_F35");
//messo per poter usare le statistiche
	    pqos.properties().properties().emplace_back("fastdds.statistics",
	        "HISTORY_LATENCY;"
	        "NETWORK_LATENCY;"
	        "PUBLICATION_THROUGHPUT;"
	        "SUBSCRIPTION_THROUGHPUT;"
	        "HEARTBEAT_COUNT;"
	        "ACKNACK_COUNT;"
	        "PHYSICAL_DATA_STATISTICS");
	    DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
	    if (participant == nullptr) return 1;

	    // statistiche uso narrow per attivarle
	    auto* stat_participant = eprosima::fastdds::statistics::dds::DomainParticipant::narrow(participant);

	    if (stat_participant != nullptr) {
	        // Traffico Publisher e Subscriber
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::PUBLICATION_THROUGHPUT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::SUBSCRIPTION_THROUGHPUT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);

	        // Latenza della rete DDS
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HISTORY_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::NETWORK_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);

	        // Affidabilità della comunicazione fra datawriter e datareader
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HEARTBEAT_COUNT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::ACKNACK_COUNT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);

	        // utilizzo della cpu
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::PHYSICAL_DATA_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);


	    }
    TypeSupport type(new SystemStatsPubSubType());
    type.register_type(participant);

    Publisher* pub = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
    Topic* topic = participant->create_topic("TelemetryTopic", type.get_type_name(), TOPIC_QOS_DEFAULT);



    //DATAWRITER
    DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;//data writer quality of service

    //setto a reliable
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    //cerco di stampare gli heartbeat ogni 100 ms
    wqos.reliable_writer_qos().times.heartbeat_period.seconds = 0;
    wqos.reliable_writer_qos().times.heartbeat_period.seconds =0.100; // 100ms

     DataWriter* writer = pub->create_datawriter(topic, wqos);
    if (writer == nullptr) return 1;//controllo che e stat creato correttamente


    std::thread Pilota_dds(flight_computer_task, writer);


        FlightDisplay display(1000, 800, "Leonardo Flight System - Manual Control");

        unsigned long packet_id = 0;


        bool recovery_low = false;
        bool recovery_high = false;
        bool recovery_bank = false;
        bool recovery_zero=false;

        Aereo.roll = 0.0f;
        Aereo.pitch = 0.0f;
        Aereo.yaw = 0.0f;
        Aereo.altitude = 4000.0f;
        Aereo.x = 0.0f; // Partenza al centro della mappa
        Aereo.z = 0.0f;
        Aereo.speed=0.0f;
        Aereo.system_active = true;



        while (display.IsActive()) {

                    // 1. LEGGE I COMANDI (Nessuna fisica qui, solo input utente)
                    display.HandleInput(Aereo);


                    // Rischio schianto a terra (Sotto i 2000m)
                    if (Aereo.altitude < 2000.0f && Aereo.speed > 0.0f) recovery_low = true;

                    // Rischio caduta a motori spenti
                    if (Aereo.speed < 10.0f && Aereo.altitude <= 2500.0f) recovery_zero = true;

                    // Rischio stratosfera / stallo (Sopra i 13000m)
                    if (Aereo.altitude > 13000.0f) recovery_high = true;

                    // Inclinazione critica (Superiore a 1.2 radianti, come nell'HUD)
                    if (std::abs(Aereo.roll) > 1.2f) recovery_bank = true;


                    if (recovery_low) {
                        Aereo.roll *= 0.95f; // Raddrizza le ali
                        if (Aereo.pitch < 0.3f) Aereo.pitch += 0.005f; // Tira su il muso dolcemente
                        if (Aereo.speed < 150.0f) Aereo.speed += 0.5f; // Dà gas per salire

                        // Si spegne quando raggiungi quota di sicurezza (2500m)
                        if (Aereo.altitude >= 2500.0f) recovery_low = false;
                    }

                    // Recupero a Motori Spenti (Riaccensione d'emergenza)
                    if (recovery_zero) {
                        if (Aereo.speed < 100.0f) Aereo.speed += 1.5f; // Booster ai motori
                        if (Aereo.pitch < 0.2f) Aereo.pitch += 0.01f;  // Alza il muso per non cadere a picco

                        if (Aereo.altitude >= 2500.0f && Aereo.speed >= 100.0f) recovery_zero = false;
                    }
                    // Recupero da Alta Quota (OVERSHOOT PULL DOWN)
                    else if (recovery_high) {
                        Aereo.roll *= 0.95f; // Raddrizza
                        if (Aereo.pitch > 0.0f) Aereo.pitch -= 0.05f; // Abbassa il muso velocemente se punti in alto
                        else if (Aereo.pitch > -0.2f) Aereo.pitch -= 0.005f; // Lo tiene inclinato verso il basso

                        // Si spegne quando torni sotto i 12000m
                        if (Aereo.altitude <= 12000.0f) recovery_high = false;
                    }


                    if (recovery_bank) {
                        // Raddrizza il rollio usando la funzione fluida
                        if (Aereo.roll > 0.15f) Aereo.roll -= 0.015f;
                        else if (Aereo.roll < -0.15f) Aereo.roll += 0.015f;
                        else recovery_bank = false; // Disinnesca l'autopilota quando sei dritto
                    }


                    float speed_orizzontale = Aereo.speed * std::cos(Aereo.pitch);
                    float speed_verticale   = Aereo.speed * std::sin(Aereo.pitch);

                    // Perdita di portanza se l'aereo è troppo inclinato (Virata stretta)
                    if (std::abs(Aereo.roll) > 0.8f) {
                        speed_verticale -= 1.5f;
                    }

                    // Effetto gravità se l'aereo va troppo piano
                    if (Aereo.speed < 50.0f) {
                        speed_verticale -= (50.0f - Aereo.speed) * 0.05f;
                    }

                    // Variabile MAGICA per regolare lo spostamento nello spazio senza toccare la velocità
                    float physics_scale = 0.015f;

                    Aereo.x += std::sin(Aereo.yaw) * speed_orizzontale * physics_scale;
                    Aereo.z += std::cos(Aereo.yaw) * speed_orizzontale * physics_scale;

                    // Altitudine gestita dal moltiplicatore che hai scelto (0.2f o modificalo a piacimento)
                    Aereo.altitude += speed_verticale * 0.2f;

                    if (Aereo.altitude < 0) Aereo.altitude = 0.0f; // Pavimento assoluto

                    if(Aereo.roll > 3.2f)  Aereo.roll = 3.2f;
                    if(Aereo.roll < -3.2f) Aereo.roll = -3.2f;
                    if(Aereo.pitch > 1.5f) Aereo.pitch = 1.5f;
                    if(Aereo.pitch < -1.5f) Aereo.pitch = -1.5f;


            bus.write(packet_id++, Aereo.roll, Aereo.pitch, Aereo.yaw, Aereo.altitude, (recovery_low || recovery_high),Aereo.speed,Aereo.x,Aereo.z,recovery_bank);

            display.Draw(Aereo);
        }

        // chiudo il programma
        {
            std::lock_guard<std::mutex> lock(Aereo_mutex);
            Aereo.system_active = false;
        }

        if (Pilota_dds.joinable()) Pilota_dds.join();

        return 0;
}
