#include "TelemetryPubSubTypes.hpp"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/attributes/PropertyPolicy.hpp>
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

PlaneData shared_aereo;
float shared_jitter, shared_cycle_time;
std::mutex aereo_mutex;

class DashboardListener : public DataReaderListener {
    long total_packets = 0;
    long missed_packets = 0;
    
    std::chrono::steady_clock::time_point last_pkt_time;
    bool first = true;
    std::vector<float> jitter_history;
    float max_jitter_seen = 0.0f;

public:
    void on_data_available(DataReader* reader) override {
        SystemStats telemetry;
        SampleInfo info;
        
        if (reader->take_next_sample(&telemetry, &info) == RETCODE_OK && info.valid_data) {
            auto now = std::chrono::steady_clock::now();
            float cycle_time = 0.0f;
            float current_jitter = 0.0f;
            
            if (!first) {
                long diff_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last_pkt_time).count();
                cycle_time = diff_us / 1000.0f;
                current_jitter = std::abs(cycle_time - 50.0f);

                if (current_jitter > max_jitter_seen) max_jitter_seen = current_jitter;
                
                jitter_history.push_back(current_jitter);
                if (jitter_history.size() > 20) jitter_history.erase(jitter_history.begin());
            }
            last_pkt_time = now;
            first = false;

            float avg_jitter = 0.0f;
            if (!jitter_history.empty()) {
                float sum = std::accumulate(jitter_history.begin(), jitter_history.end(), 0.0f);
                avg_jitter = sum / jitter_history.size();
            }

            total_packets++;
            float loss_perc = (total_packets > 0) ? ((float)missed_packets / total_packets) * 100.0f : 0.0f;

            // Assegnazione in mutuo esclusione
            {
                std::lock_guard<std::mutex> lock(aereo_mutex);
                shared_aereo.altitude = telemetry.altitude();
                shared_aereo.speed    = telemetry.speed();
                shared_aereo.roll     = telemetry.roll();
                shared_aereo.pitch    = telemetry.pitch();
                shared_aereo.yaw      = telemetry.yaw();
                shared_aereo.x        = telemetry.x();
                shared_aereo.z        = telemetry.z();
                shared_aereo.landing_mode  = telemetry.landing_mode();
                shared_aereo.system_active = telemetry.system_active(); // Sincronizzazione stato operativo

                snprintf(shared_aereo.status_msg, sizeof(shared_aereo.status_msg), "%s", telemetry.status_msg().c_str());
            }

            std::string status = telemetry.status_msg().c_str();
            bool alarm_crit = (status.find("ALARM") != std::string::npos || status.find("PULL UP") != std::string::npos || status.find("HIGH ALTITUDE") != std::string::npos);
            bool alarm_warn = (status.find("WARN") != std::string::npos || avg_jitter > 5.0f);

            // Aggiornamento console per diagnostica
            std::cout << "\033[2J\033[1;1H";
            if (alarm_crit) std::cout << "\033[1;41m";
            else if (alarm_warn) std::cout << "\033[1;43m";
            else if (telemetry.landing_mode()) std::cout << "\033[1;45m";
            else std::cout << "\033[1;44m";

            std::cout << "############################################################\n";
            std::cout << "           TORRE DI CONTROLLO - MONITORAGGIO REAL-TIME      \n";
            std::cout << "############################################################\n\033[0m\n";

            std::cout << "\033[1;36m>>> TELEMETRIA DI VOLO <<<\033[0m            \033[1;35m>>> DIAGNOSTICA CORE & THREAD <<<\033[0m\n";
            std::cout << " ALTITUDINE : " << std::setw(5) << (int)telemetry.altitude() << " m ";
            std::cout << "   |   Cycle Time : " << std::fixed << std::setprecision(2) << cycle_time << " ms \n";
            std::cout << " ROLL (X)   : " << std::setw(6) << telemetry.roll() << " rad ";
            std::cout << "   |   Jitter Avg : " << std::setw(5) << avg_jitter << " ms \n";
            std::cout << " PITCH (Y)  : " << std::setw(6) << telemetry.pitch() << " rad       ";
            std::cout << "   |   RAM Access : " << std::setw(5) << (int)0 << " us \n";
            std::cout << " YAW (Z)    : " << std::setw(6) << telemetry.yaw() << " rad ";
            std::cout << "   |   Packet Loss: " << std::fixed << std::setprecision(1) << loss_perc << " %\n";
            std::cout << " SPEED      : " << std::setw(6) << (int)telemetry.speed() << " KPH \n\n";

            std::cout << "------------------------------------------------------------\nCONDIZIONE VOLO : ";
            if (alarm_crit) std::cout << "\033[1;31m !!! " << status << " !!!\033[0m\n";
            else if (alarm_warn) std::cout << "\033[1;33m " << status << "\033[0m\n";
            else if (telemetry.landing_mode()) std::cout << "\033[1;35m " << status << "\033[0m\n";
            else std::cout << "\033[1;32m " << status << "\033[0m\n";
            std::cout << "------------------------------------------------------------\n";
        }
    }
};

int main() {
	DomainParticipantQos pqos;
	    pqos.name("Monitor_Node_Leonardo");

	    // 1. Abilitazione QoS Globale per le Statistiche
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

	    // 2. Attivazione dei DataWriter Statistici (lato Subscriber)
	    auto* stat_participant = eprosima::fastdds::statistics::dds::DomainParticipant::narrow(participant);
	    if (stat_participant != nullptr) {
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::SUBSCRIPTION_THROUGHPUT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::ACKNACK_COUNT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HEARTBEAT_COUNT_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::HISTORY_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
	        stat_participant->enable_statistics_datawriter(eprosima::fastdds::statistics::NETWORK_LATENCY_TOPIC, eprosima::fastdds::statistics::dds::STATISTICS_DATAWRITER_QOS);
	    }

	    TypeSupport type(new SystemStatsPubSubType());
	    type.register_type(participant);

	    Subscriber* sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
	    Topic* topic = participant->create_topic("TelemetryTopic", type.get_type_name(), TOPIC_QOS_DEFAULT);

	    DataReaderQos dr_qos = DATAREADER_QOS_DEFAULT;
	    dr_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
	    dr_qos.durability().kind = VOLATILE_DURABILITY_QOS;

	    // 3. Assegnazione proprietà statistiche specifiche al DataReader
	    dr_qos.properties().properties().emplace_back("fastdds.statistics",
	        "SUBSCRIPTION_THROUGHPUT;"
	        "HISTORY_LATENCY;"
	        "ACKNACK_COUNT");

	    DashboardListener listener;
	    DataReader* reader = sub->create_datareader(topic, dr_qos, &listener);

    MonitorDisplay display(850, 700, "Torre di Controllo - Telemetria F-35");

    while (display.IsActive()) {
        PlaneData local_aereo;
        {
            std::lock_guard<std::mutex> lock(aereo_mutex);
            local_aereo = shared_aereo;
        }

        // Disegno interfaccia basato interamente su telemetry DDS
        // Il flag system_active non blocca più il ciclo grafico

        display.Draw(local_aereo);
    }

    sub->delete_datareader(reader);
    participant->delete_subscriber(sub);
    participant->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(participant);

    return 0;
}
