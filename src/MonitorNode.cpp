#include "TelemetryPubSubTypes.hpp"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <iostream>
#include <iomanip>
#include <thread>
#include <string>
#include <vector>
#include <numeric>
#include <cmath>

using namespace eprosima::fastdds::dds;

// Funzione helper per disegnare barre di caricamento
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
    
    // Statistiche Thread (Single Core Analysis)
    std::chrono::steady_clock::time_point last_pkt_time;
    bool first = true;
    std::vector<float> jitter_history;
    float max_jitter_seen = 0.0f;

public:
    void on_data_available(DataReader* reader) override {
        SystemStats msg;//importo telemetry.idl
        SampleInfo info;
        
        if (reader->take_next_sample(&msg, &info) == RETCODE_OK && info.valid_data) {
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
            if (msg.deadline_missed()) missed_packets++;
            float loss_perc = (total_packets > 0) ? ((float)missed_packets / total_packets) * 100.0f : 0.0f;

//leggo cosa e stato scritto nel write del FlightSim e controllo dopo con che colore scriverlo
            std::string status = msg.status_msg().c_str();

            // Definiamo cosa è CRITICO (ROSSO)
            bool alarm_crit = (status.find("ALARM") != std::string::npos ||
                               status.find("PULL UP") != std::string::npos ||     // Bassa quota
                               status.find("PULL DOWN") != std::string::npos ||   // Alta quota
                               status.find("HIGH ALTITUDE") != std::string::npos); // Alta quota

            // Definiamo cosa è ATTENZIONE (GIALLO)
            bool alarm_warn = (status.find("WARN") != std::string::npos ||
                               avg_jitter > 5.0f);
            

            std::cout << "\033[2J\033[1;1H"; 

            // HEADER DINAMICO
            if (alarm_crit) std::cout << "\033[1;41m";      // Sfondo Rosso
            else if (alarm_warn) std::cout << "\033[1;43m"; // Sfondo Giallo
            else std::cout << "\033[1;44m";                 // Sfondo Blu
            
            std::cout << "############################################################\n";
            std::cout << "           TORRE DI CONTROLLO - MONITORAGGIO REAL-TIME      \n";
            std::cout << "############################################################\n";
            std::cout << "\033[0m\n";

            // --- CORPO INTERFACCIA (Doppia Colonna Allineata) ---
            
            std::cout << "\033[1;36m>>> TELEMETRIA DI VOLO <<<\033[0m            \033[1;35m>>> DIAGNOSTICA CORE & THREAD <<<\033[0m\n";
            
            // RIGA 1: Altitudine | Cycle Time
            std::cout << " ALTITUDINE : " << std::setw(5) << (int)msg.altitude() << " m ";
            if(msg.altitude() < 4000 || msg.altitude() > 12000) {

            	std::cout << "\033[1;31m[CRIT]\033[0m"; }else std::cout << "      ";

            
            std::cout << "   |   Cycle Time : " << std::fixed << std::setprecision(2) << cycle_time << " ms ";
            if(cycle_time > 55 || cycle_time < 45) {

            	std::cout << "\033[1;33m[UNSTABLE]\033[0m";

            }else std::cout << "\033[1;32m[OK]      \033[0m";

            std::cout << "\n";

            // RIGA 2: Roll | Jitter
            std::cout << " ROLL (X)   : " << std::setw(6) << msg.roll() << " rad ";
            if(std::abs(msg.roll())>1.2){

            	std::cout << "\033[1;33m[WARN]\033[0m";
            }
            else std::cout << "      ";


            std::cout << "   |   Jitter Avg : " << std::setw(5) << avg_jitter << " ms ";
            if(avg_jitter > 2.0) {

            	std::cout << "\033[1;31m[LAG]     \033[0m";

            }else std::cout << "\033[1;32m[SMOOTH]  \033[0m";

            std::cout << "\n";

            // RIGA 3: Pitch | RAM Access
            std::cout << " PITCH (Y)  : " << std::setw(6) << msg.pitch() << " rad       ";
            std::cout << "   |   RAM Access : " << std::setw(5) << (int)msg.latency_us() << " us ";
            std::cout << "\n";

            // RIGA 4: Yaw | Packet Loss
            std::cout << " YAW (Z)    : " << std::setw(6) << msg.yaw() << " rad ";
            if(std::abs(msg.yaw())>1.2){

            	std::cout << "\033[1;33m[WARN]\033[0m";}
            		else std::cout << "      ";



            std::cout << "   |   Packet Loss   : " << std::fixed << std::setprecision(1) << loss_perc << " %";
            std::cout << "\n\n";

            // --- BARRE GRAFICHE ---
            std::cout << " STATO CARICO CPU (Jitter): " << Barre_Caricamento(avg_jitter, 10.0f, 20, "\033[1;33m") << "\n";
            std::cout << " STATO ALTITUDINE (Quota) : " << Barre_Caricamento(msg.altitude(), 15000.0f, 20, "\033[1;32m") << "\n\n";


            std::cout << "------------------------------------------------------------\n";
            std::cout << "CONDIZIONE VOLO : ";


            if (status.find("ALARM") != std::string::npos ||
                status.find("PULL UP") != std::string::npos ||
                status.find("PULL DOWN") != std::string::npos ||
                status.find("HIGH ALTITUDE") != std::string::npos)
            {
                std::cout << "\033[1;31m" << status << "\033[0m\n"; // ROSSO
            }
            else if (alarm_warn)
            {
                std::cout << "\033[1;33m" << status << "\033[0m\n"; // GIALLO
            }
            else
            {
                std::cout << "\033[1;32m" << status << "\033[0m\n"; // VERDE
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
    pqos.name("Monitor_Node");
    DomainParticipant* part = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    TypeSupport type(new SystemStatsPubSubType());
    type.register_type(part);
    Subscriber* sub = part->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    Topic* topic = part->create_topic("TelemetryTopic", "SystemStats", TOPIC_QOS_DEFAULT);
    DashboardListener lst;
    DataReader* reader = sub->create_datareader(topic, DATAREADER_QOS_DEFAULT, &lst);

    while(true) std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
