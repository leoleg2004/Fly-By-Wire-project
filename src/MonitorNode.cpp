#include "TelemetryPubSubTypes.hpp"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/statistics/dds/domain/DomainParticipant.hpp>
#include <fastdds/statistics/topic_names.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/statistics/dds/domain/DomainParticipant.hpp>
#include <fastdds/statistics/topic_names.hpp>
#include <fastdds/statistics/dds/publisher/qos/DataWriterQos.hpp>
#include "MonitorDisplay.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <string>
#include <vector>
#include <numeric>
#include <cmath>
#include <mutex>

using namespace eprosima::fastdds::dds;

// AGGIUNTA: Variabili globali condivise tra DDS (scrittura) e Raylib (lettura)
PlaneData shared_aereo;
float shared_jitter,shared_cycle_time;
std::mutex aereo_mutex;

// Funzione per disegnare barre di caricamento
std::string Barre_Caricamento(float value, float max, int width, std::string color) {
    int fill = (int)((value / max) * width);
    if (fill > width) fill = width;
    if (fill < 0) fill = 0;
    
    std::string bar = "[";
    bar += color;
    for (int i = 0; i < width; i++) {
        if (i < fill) bar += "|";
        else bar += " ";
    }
    bar += "\033[0m]";
    return bar;
}

class DashboardListener : public DataReaderListener {
    // Statistiche Rete
    long total_packets = 0;
    long missed_packets = 0;
    
    std::chrono::steady_clock::time_point last_pkt_time;
    bool first = true;
    std::vector<float> jitter_history; //vettore che mantiene la storia dei ritardi
    float max_jitter_seen = 0.0f;      //lo metto a zero in modo che il primo jitter diventi il massimo

public:
    void on_data_available(DataReader* reader) override {
        SystemStats telemetry; //importo telemetry.idl
        SampleInfo info;
        
        if (reader->take_next_sample(&telemetry, &info) == RETCODE_OK && info.valid_data) {
            auto now = std::chrono::steady_clock::now();
            
            //instauro la logica di controllo delle statistiche
            float cycle_time = 0.0f;
            float current_jitter = 0.0f;
            
            if (!first) {
                long diff_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last_pkt_time).count();
                cycle_time = diff_us / 1000.0f;
                current_jitter = std::abs(cycle_time - 50.0f); // Target 50ms

                if (current_jitter > max_jitter_seen) max_jitter_seen = current_jitter;
                
                jitter_history.push_back(current_jitter);
                if (jitter_history.size() > 20) jitter_history.erase(jitter_history.begin());
            }
            last_pkt_time = now;
            first = false;

            // Media Jitter il ritardo
            float avg_jitter = 0.0f;
            if (!jitter_history.empty()) {
                float sum = std::accumulate(jitter_history.begin(), jitter_history.end(), 0.0f);
                avg_jitter = sum / jitter_history.size();
            }

            // Statistiche Pacchetti
            total_packets++;
            if (telemetry.deadline_missed()) missed_packets++;
            float loss_perc = (total_packets > 0) ? ((float)missed_packets / total_packets) * 100.0f : 0.0f;


            {
                std::lock_guard<std::mutex> lock(aereo_mutex);
                shared_aereo.altitude = telemetry.altitude();
                shared_aereo.speed = telemetry.speed();
                shared_aereo.roll = telemetry.roll();
                shared_aereo.pitch = telemetry.pitch();
                shared_aereo.yaw = telemetry.yaw();

                        snprintf(shared_aereo.status_msg, sizeof(shared_aereo.status_msg), "%s", telemetry.status_msg().c_str());
            }

            std::string status = telemetry.status_msg().c_str();

            bool alarm_crit = (status.find("ALARM") != std::string::npos ||
                               status.find("PULL UP") != std::string::npos ||
                               status.find("QUOTA BASSA") != std::string::npos ||
                               status.find("PULL DOWN") != std::string::npos ||
                               status.find("HIGH ALTITUDE") != std::string::npos ||
                               telemetry.altitude() < 200.0f ||
                               telemetry.altitude() > 15000.0f);

            bool alarm_warn = (status.find("WARN") != std::string::npos || avg_jitter > 5.0f);

            std::cout << "\033[2J\033[1;1H";

            if (alarm_crit) std::cout << "\033[1;41m";
            else if (alarm_warn) std::cout << "\033[1;43m";
            else std::cout << "\033[1;44m";

            std::cout << "############################################################\n";
            std::cout << "           TORRE DI CONTROLLO - MONITORAGGIO REAL-TIME      \n";
            std::cout << "############################################################\n";
            std::cout << "\033[0m\n";

            std::cout << "\033[1;36m>>> TELEMETRIA DI VOLO <<<\033[0m            \033[1;35m>>> DIAGNOSTICA CORE & THREAD <<<\033[0m\n";

            std::cout << " ALTITUDINE : " << std::setw(5) << (int)telemetry.altitude() << " m ";
            if(telemetry.altitude() < 200.0f || telemetry.altitude() > 14000.0f) {
                std::cout << "\033[1;31m[CRIT]\033[0m";
            } else {
                std::cout << "      ";
            }

            std::cout << "   |   Cycle Time : " << std::fixed << std::setprecision(2) << cycle_time << " ms ";
            if(cycle_time > 55 || cycle_time < 45) {
                std::cout << "\033[1;33m[UNSTABLE]\033[0m";
            } else {
                std::cout << "\033[1;32m[OK]      \033[0m";
            }
            std::cout << "\n";

            std::cout << " ROLL (X)   : " << std::setw(6) << telemetry.roll() << " rad ";
            if(std::abs(telemetry.roll()) > 1.2) {
                std::cout << "\033[1;33m[WARN]\033[0m";
            } else {
                std::cout << "      ";
            }

            std::cout << "   |   Jitter Avg : " << std::setw(5) << avg_jitter << " ms ";
            if(avg_jitter > 2.0) {
                std::cout << "\033[1;31m[LAG]     \033[0m";
            } else {
                std::cout << "\033[1;32m[SMOOTH]  \033[0m";
            }
            std::cout << "\n";

            std::cout << " PITCH (Y)  : " << std::setw(6) << telemetry.pitch() << " rad       ";
            std::cout << "   |   RAM Access : " << std::setw(5) << (int)telemetry.latency_us() << " us \n";

            std::cout << " YAW (Z)    : " << std::setw(6) << telemetry.yaw() << " rad ";
            if(std::abs(telemetry.yaw()) > 1.2) {
                std::cout << "\033[1;33m[WARN]\033[0m";
            } else {
                std::cout << "      ";
            }

            std::cout << "   |   Packet Loss   : " << std::fixed << std::setprecision(1) << loss_perc << " %\n";

            std::cout << " SPEED      : " << std::setw(6) << (int)telemetry.speed()*2 << " Km/h \n\n";

            std::cout << " STATO CARICO CPU (Jitter): " << Barre_Caricamento(avg_jitter, 10.0f, 20, "\033[1;33m") << "\n";
            std::cout << " STATO ALTITUDINE (Quota) : " << Barre_Caricamento(telemetry.altitude(), 15000.0f, 20, "\033[1;32m") << "\n\n";

            std::cout << "------------------------------------------------------------\n";
            std::cout << "CONDIZIONE VOLO : ";

            if (alarm_crit) {
                std::cout << "\033[1;31m !!! IN PERICOLO: " << status << " (WARNING) !!!\033[0m\n";
            }
            else if (alarm_warn) {
                std::cout << "\033[1;33m " << status << "\033[0m\n";
            }
            else {
                std::cout << "\033[1;32m " << status << "\033[0m\n";
            }

            std::cout << " RETE DDS   : " << total_packets << " Rx | " << missed_packets << " Perse (" << loss_perc << "%)\n";
            std::cout << "------------------------------------------------------------\n";
            
            if (avg_jitter > 10.0f) {
                std::cout << "\033[1;31m [!] ALERT: IL SISTEMA SINGLE CORE E' SOVRACCARICO! \033[0m\n";
            } else {
                std::cout << "\033[1;32m [OK] Scheduling Thread Ottimale. \033[0m\n";
            }
        }
    }
};

int main() {

    DomainParticipantQos pqos;
    pqos.name("Monitor_Node_Leonardo");

    pqos.properties().properties().emplace_back("fastdds.statistics",
        "HISTORY_LATENCY;"
        "NETWORK_LATENCY;"
        "PUBLICATION_THROUGHPUT;"
        "SUBSCRIPTION_THROUGHPUT;"
        "HEARTBEAT_COUNT;"
        "ACKNACK_COUNT;"
        "DISCOVERY_STATISTICS;"
        "PHYSICAL_DATA_STATISTICS");

    //creo il participant
    DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    if (participant == nullptr) return 1;

    auto* stat_participant = eprosima::fastdds::statistics::dds::DomainParticipant::narrow(participant);
    if (stat_participant != nullptr) {
        // Traffico e Affidabilità
        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::SUBSCRIPTION_THROUGHPUT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::ACKNACK_COUNT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HEARTBEAT_COUNT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
        // Latenza della rete DDS
        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HISTORY_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::NETWORK_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
        // Latenza della rete DDS
        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HISTORY_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
    }

    TypeSupport type(new SystemStatsPubSubType());
    type.register_type(participant);

    Subscriber* sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    Topic* topic = participant->create_topic("TelemetryTopic", type.get_type_name(), TOPIC_QOS_DEFAULT);

    //creo il reader
    DataReaderQos dr_qos = DATAREADER_QOS_DEFAULT;

    // Reliability del quality of service
    dr_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    dr_qos.durability().kind = VOLATILE_DURABILITY_QOS;

    // Abilitiamo le statistiche lato ricezione
    dr_qos.properties().properties().emplace_back("fastdds.statistics",
        "SUBSCRIPTION_THROUGHPUT;"
        "HISTORY_LATENCY;"
        "ACKNACK_COUNT");

    DashboardListener listener;
    DataReader* reader = sub->create_datareader(topic, dr_qos, &listener);


    if (reader == nullptr) {
        return 1;
    }
    std::cout << "=== DASHBOARD MONITOR IN ASCOLTO (RELIABLE) ===" << std::endl;



    MonitorDisplay display(1000, 800, "Torre di Controllo - Telemetria F-35");
    PlaneData local_aereo;


    while (display.IsActive()) {
        PlaneData local_aereo;

        {
            // Lettura sicura solo dei dati aereo
            std::lock_guard<std::mutex> lock(aereo_mutex);
            local_aereo = shared_aereo;
        }

        // Chiamata pulita con 1 solo argomento
        display.Draw(local_aereo);
    }

    sub->delete_datareader(reader);
    participant->delete_subscriber(sub);
    participant->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(participant);

    return 0;
}
