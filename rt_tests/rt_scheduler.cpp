/* g++ rt_scheduler.cpp -o rt_scheduler -pthread && g++ DueThreadCoreDiversi.cpp -o
 * DueThreadCoreDiversi -pthread && g++ EDFCoreUguali.cpp -o rt_edf_test -pthread
 * && g++ EDFCoreDIversi.cpp -o EDFCoreDiversi -pthread
 *
 *
 * comando per aggiornare le modifiche sul codcie da terminale
 */
#include <iostream>
#include <vector>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <time.h>
#include <cstring>
#include <cmath>
#include <iomanip>
#include <unistd.h>

#define handle_error(en, msg) \
        if(en != 0) {errno = en; perror(msg); exit(EXIT_FAILURE);}

#define TYPE_LOW_RECOVERY 0
#define TYPE_HIGH_RECOVERY 1

// Struttura dati aggiornata con i campi per il report finale
typedef struct arg {
	int id;
	long period_ms;
	long deadline_ms;
	int priority;
	int type;
	int final_jitter_violations;
	int final_deadline_misses;
} t_arg;

void* Task(void *ptr);

// Helper Time Functions
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
		std::cerr
				<< "USO: sudo ./rt_test Period_1 Dead_L1 Period_2 Dead_High2\n";
		exit(1);
	}

	int NUM_THREADS = 2;
	int TARGET_CORE = 0;
	pthread_attr_t attributes;
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE);

	pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
	if (pthread_attr_setschedpolicy(&attributes, SCHED_FIFO) != 0) {
		std::cerr << "ERROR: Cannot set SCHED_FIFO. Run with sudo.\n";
		exit(1);
	}

	t_arg *arg = (t_arg*) malloc(NUM_THREADS * sizeof(t_arg));
	pthread_t *thread = (pthread_t*) malloc(NUM_THREADS * sizeof(pthread_t));
	struct sched_param param;

	int arg_idx = 1;
	//watch -n 0.01 "ps -T -C rt_scheduler -o pid,tid,psr,comm"
	//nel primo compare il thread che verrà assegnato a un core il secondo mostra quello usato effettivamente dai thread

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(TARGET_CORE, &cpuset);
	pthread_attr_setaffinity_np(&attributes, sizeof(cpu_set_t), &cpuset);

	for (unsigned int i = 0; i < NUM_THREADS; i++) {
		arg[i].id = i + 1;
		arg[i].period_ms = std::stol(argv[arg_idx++]);
		arg[i].deadline_ms = std::stol(argv[arg_idx++]);
		arg[i].type = i % 2;

		//calcolo della priorità
		arg[i].priority = 99 - (arg[i].period_ms / 10);
		if (arg[i].priority < 1)
			arg[i].priority = 1;

		int s = pthread_attr_setaffinity_np(&attributes, sizeof(cpu_set_t),
				&cpuset);
		if (s != 0)
			handle_error(s, "pthread_attr_setaffinity_np");

		std::cout << "Configuring Thread " << i + 1 << " on CORE " << i << "\n";

		// applico la priorità alla schedulazione
		param.sched_priority = arg[i].priority;
		pthread_attr_setschedparam(&attributes, &param);

		// Creo thread
		int ret = pthread_create(&(thread[i]), &attributes, Task,
				(void*) &(arg[i]));
		handle_error(ret, "Thread Creation Failed");

		std::string type_name =
				(arg[i].type == TYPE_LOW_RECOVERY) ? "LOW_REC" : "HIGH_REC";
		std::cout << "Thread " << arg[i].id << " [" << type_name
				<< "] -> Period: " << arg[i].period_ms << "ms, Prio: "
				<< arg[i].priority << "\n";
	}

	for (unsigned int i = 0; i < NUM_THREADS; i++) {
		pthread_join(thread[i], NULL);
	}

	std::cout << "\n====================================================\n";
	std::cout << "              RISULTATI FINALI TEST RM              \n";
	std::cout << "====================================================\n";
	for (unsigned int i = 0; i < NUM_THREADS; i++) {
		std::string type_name =
				(arg[i].type == TYPE_LOW_RECOVERY) ? "PUB" : "SUB";
		std::cout << "[" << type_name << "] -> Violazioni Jitter (>0.1ms): "
				<< std::setw(2) << arg[i].final_jitter_violations
				<< " | Deadline Missed: " << std::setw(2)
				<< arg[i].final_deadline_misses << "\n";
	}
	std::cout << "====================================================\n\n";

	printf("Test concluso.\n");
	free(arg);
	free(thread);
	pthread_attr_destroy(&attributes);
	exit(0);
}

float shared_altitude = 15000.0f;
bool new_data_available = false;
void* Task(void *ptr) {
	t_arg *arg = (t_arg*) ptr;

	struct timespec next_activation, start_work, end_work;
	std::string type_str = (arg->type == TYPE_LOW_RECOVERY) ? "MANDO" : "AGISCO ";
	std::string status;
	int CountViolation = 0, CountDeadLineMiss = 0;

	double max_jitter = 0.0;

	float local_altitude = 15000.0f;
	bool descending = true;

	int tempo_totale_test_ms = 20000; // 20 secondi
	int iterazioni_da_fare = tempo_totale_test_ms / arg->period_ms;

	clock_gettime(CLOCK_MONOTONIC, &next_activation);

	for (int i = 0; i < iterazioni_da_fare; i++) {
		timespec_add_ms(&next_activation, arg->period_ms);
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL);
		clock_gettime(CLOCK_MONOTONIC, &start_work);

		double jitter = std::abs(time_diff_ms(next_activation, start_work));
		if (jitter > max_jitter) max_jitter = jitter;
		if (jitter > 0.1) CountViolation++;

		if (arg->type == TYPE_LOW_RECOVERY) {

			shared_altitude = local_altitude;
			new_data_available = true;

			status = "[Dati Inviati]";
			burn_cpu(2);

			if (descending) {
				local_altitude -= 200.0f;
				if (local_altitude <= 1000.0f) {
					descending = false;
				}
			} else {
				local_altitude += 200.0f;
				if (local_altitude >= 15000.0f) {
					descending = true;
				}
			}
		}

		else if (arg->type == TYPE_HIGH_RECOVERY) {
			bool got_new_data = false;

			if (new_data_available) {
				local_altitude = shared_altitude;
				new_data_available = false;
				got_new_data = true;
			}

			if (got_new_data) {
				if (local_altitude < 2500.0f) {
					status = "\033[1;41m[PULL UP ATTIVO]\033[0m";
					burn_cpu(15);
				} else if (local_altitude >= 13000.0f) {
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

		std::cout << "[" << type_str << "] Alt:" << std::setw(5)
				<< (int) local_altitude << " | " << std::left
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

	arg->final_jitter_violations = CountViolation;
	arg->final_deadline_misses = CountDeadLineMiss;

	std::cout << "\n====================================================";
	std::cout << "\n[" << type_str << "] FINE THREAD -> PICCO MAX JITTER: "
			  << std::fixed << std::setprecision(3) << max_jitter << " ms\n";
	std::cout << "====================================================\n\n";

	pthread_exit(NULL);
}
