#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include "Telemetry.hpp"
#include "TelemetryPubSubTypes.hpp"

using namespace eprosima::fastdds::dds;

#define handle_error(en, msg) \
        if(en != 0) {errno = en; perror(msg); exit(EXIT_FAILURE);}

#define SCHED_DEADLINE  6
#define TYPE_PUBLISHER  0
#define TYPE_SUBSCRIBER 1

struct sched_attr {
	__u32 size;
	__u32 sched_policy;
	__u64 sched_flags;
	__s32 sched_nice;
	__u32 sched_priority;
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
};

// Syscall per abilitare SCHED_DEADLINE sul thread corrente
int sched_setattr(pid_t pid, const struct sched_attr *attr,
		unsigned int flags) {
	return syscall(__NR_sched_setattr, pid, attr, flags);
}

typedef struct arg {
	int id;
	long period_ms;
	long deadline_ms;
	long runtime_ms;
	int type;
	int final_jitter_violations;
	int final_deadline_misses;

	DataWriter *writer;
	DataReader *reader;
} t_arg;

void* Task(void *ptr);

void timespec_add_ms(struct timespec *t, long ms) {
	t->tv_sec += ms / 1000;
	t->tv_nsec += (ms % 1000) * 1000000;
	if (t->tv_nsec >= 1000000000) {
		t->tv_sec++;
		t->tv_nsec -= 1000000000;
	}
}

double time_diff_ms(struct timespec start, struct timespec end) {
	double s = end.tv_sec - start.tv_sec;
	double ns = end.tv_nsec - start.tv_nsec;
	return (s * 1000.0) + (ns / 1000000.0);
}

void burn_cpu(long ms) {
	struct timespec start, current;
	clock_gettime(CLOCK_MONOTONIC, &start);
	do {
		clock_gettime(CLOCK_MONOTONIC, &current);
	} while (time_diff_ms(start, current) < ms);
}

int main(int argc, char *argv[]) {

	if (argc < 5) {
		std::cerr << "USO: sudo ./DDSMCORE P_Pub D_Pub P_Sub D_Sub\n";
		exit(1);
	}

	std::cout << "--- Inizializzazione Rete DDS (EDF Mode) ---\n";

	// creo rete dds sul dominio 1 mentre per il simulatore e sul dominio 0
	DomainParticipantQos pqos;
	pqos.name("RT_EDF_Participant");
	DomainParticipant *participant =
			DomainParticipantFactory::get_instance()->create_participant(1,
					pqos);
	if (participant == nullptr) {
		std::cerr << "Errore DDS Participant\n";
		return 1;
	}

	TypeSupport type(new SystemStatsPubSubType());
	type.register_type(participant);
	Topic *topic = participant->create_topic("TelemetryTopic",
			type.get_type_name(), TOPIC_QOS_DEFAULT);

	Publisher *pub = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
	DataWriter *writer = pub->create_datawriter(topic, DATAWRITER_QOS_DEFAULT);

	Subscriber *sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
	DataReader *reader = sub->create_datareader(topic, DATAREADER_QOS_DEFAULT);

	if (writer == nullptr || reader == nullptr) {
		std::cerr << "Errore DDS Writer/Reader\n";
		return 1;
	}
	std::cout << "Rete DDS pronta. Avvio Thread SCHED_DEADLINE...\n\n";

	int NUM_THREADS = 2;
	pthread_attr_t attributes;
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE);



	t_arg *arg = (t_arg*) malloc(NUM_THREADS * sizeof(t_arg));
	pthread_t *thread = (pthread_t*) malloc(NUM_THREADS * sizeof(pthread_t));

	int arg_idx = 1;
	// Assegnazione su Core Uguali
					cpu_set_t cpuset;
					CPU_ZERO(&cpuset);
					CPU_SET(0*2, &cpuset);
					pthread_attr_setaffinity_np(&attributes, sizeof(cpu_set_t), &cpuset);
	for (unsigned int i = 0; i < NUM_THREADS; i++) {


		cpu_set_t cpuset;
				CPU_ZERO(&cpuset);
				CPU_SET(i* 2, &cpuset);
				pthread_attr_setaffinity_np(&attributes, sizeof(cpu_set_t), &cpuset);
		arg[i].id = i + 1;
		arg[i].period_ms = std::stol(argv[arg_idx++]);
		arg[i].deadline_ms = std::stol(argv[arg_idx++]);
		arg[i].type = i % 2; // 0 = Pub, 1 = Sub

		arg[i].writer = writer;
		arg[i].reader = reader;

		// in questa linea decido quanto stressare
		arg[i].runtime_ms = (long) (arg[i].deadline_ms * 0.9);
		if (arg[i].runtime_ms < 1)
			arg[i].runtime_ms = 1;


		std::string type_name =
				(arg[i].type == TYPE_PUBLISHER) ? "PUBLISHER" : "SUBSCRIBER";
		std::cout << "Creazione Thread " << arg[i].id << " [" << type_name
				<< "] -> Core: " << i << ", Period: " << arg[i].period_ms
				<< "ms, Runtime Allocato: " << arg[i].runtime_ms << "ms\n";

		int ret = pthread_create(&(thread[i]), &attributes, Task,
				(void*) &(arg[i]));
		handle_error(ret, "Thread Creation Failed");
	}

	for (unsigned int i = 0; i < NUM_THREADS; i++) {
		pthread_join(thread[i], NULL);
	}

	std::cout << "\n====================================================\n";
	std::cout << "           RISULTATI FINALI TEST EDF + DDS          \n";
	std::cout << "====================================================\n";
	for (unsigned int i = 0; i < NUM_THREADS; i++) {
		std::string type_name =
				(arg[i].type == TYPE_PUBLISHER) ? "PUB " : "SUB ";
		std::cout << "[" << type_name << "] -> Violazioni Jitter (>0.1ms): "
				<< std::setw(2) << arg[i].final_jitter_violations
				<< " | Deadline Missed: " << std::setw(2)
				<< arg[i].final_deadline_misses << "\n";
	}
	std::cout << "====================================================\n\n";

	free(arg);
	free(thread);
	pthread_attr_destroy(&attributes);
	exit(0);
}


void* Task(void *ptr) {
	t_arg *arg = (t_arg*) ptr;

	struct timespec next_activation, start_work, end_work;
	std::string type = (arg->type == TYPE_PUBLISHER) ? "MANDO " : "AGISCO";
	std::string status;
	int CountViolation = 0, CountDeadLineMiss = 0;

	// Aggiunto il tracciamento del Jitter massimo
	double max_jitter = 0.0;

	SystemStats stats;
	SampleInfo info;
	float simulated_altitude = 15000.0f;

	bool descending = true;


	int tempo_totale_test_ms = 20000; // 20 secondi
	int iterazioni_da_fare = tempo_totale_test_ms / arg->period_ms;

	clock_gettime(CLOCK_MONOTONIC, &next_activation);

	for (int i = 0; i < iterazioni_da_fare; i++) {
		timespec_add_ms(&next_activation, arg->period_ms);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL);
		clock_gettime(CLOCK_MONOTONIC, &start_work);

		// Calcolo Jitter
		double jitter = std::abs(time_diff_ms(next_activation, start_work));
		if (jitter > max_jitter) max_jitter = jitter;
		if (jitter > 0.1) CountViolation++;

		if (arg->type == TYPE_PUBLISHER) {
			// Il Publisher crea i dati e li spedisce
			stats.altitude(simulated_altitude);
			arg->writer->write(&stats);
			status = "[Dati Inviati]";
			burn_cpu(2);

			if (descending) {
				simulated_altitude -= 200.0f;
				if (simulated_altitude <= 1000.0f) {
					descending = false;
				}
			} else {
				simulated_altitude += 200.0f;
				if (simulated_altitude >= 15000.0f) {
					descending = true;
				}
			}
		}

		else if (arg->type == TYPE_SUBSCRIBER) {
			bool got_new_data = false;

			while (arg->reader->take_next_sample((void*) &stats, &info) == RETCODE_OK) {
				if (info.valid_data) {
					simulated_altitude = stats.altitude();
					got_new_data = true;
				}
			}

			if (got_new_data) {
				if (simulated_altitude < 2500.0f) {
					status = "\033[1;41m[PULL UP ATTIVO]\033[0m";
					burn_cpu(15);
				} else if (simulated_altitude >= 13000.0f) {
					status = "\033[1;43m[PULL DW ATTIVO]\033[0m";
					burn_cpu(15);
				} else {
					status = "\033[1;36m[CLIMB]  \033[0m";
					burn_cpu(1);
				}
			}  else {
				status = "\033[1;36m[STABLE]  \033[0m";
				burn_cpu(1);
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &end_work);
		double response_time = time_diff_ms(start_work, end_work);

		std::cout << "[" << type << "] Alt:" << std::setw(5)
				<< (int) simulated_altitude << " | " << std::left
				<< std::setw(20) << status << " | CPU:" << std::fixed
				<< std::setprecision(2) << response_time << "ms" << " | Jit:"
				<< std::setprecision(3) << jitter << "ms";

		if (response_time <= arg->deadline_ms)
			std::cout << " | DL: OK\n";
		else {
			std::cout << " | \033[1;31mDeadLineMISSED\033[0m\n";
			CountDeadLineMiss++;
		}
	}

	// Salvataggio statistiche e uscita
	arg->final_jitter_violations = CountViolation;
	arg->final_deadline_misses = CountDeadLineMiss;

	std::cout << "\n====================================================";
	std::cout << "\n[" << type << "] FINE THREAD -> PICCO MAX JITTER: "
			  << std::fixed << std::setprecision(3) << max_jitter << " ms\n";
	std::cout << "====================================================\n\n";

	pthread_exit(NULL);
}
