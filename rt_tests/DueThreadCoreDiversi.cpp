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

void *Task(void *ptr);

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


int main(int argc, char* argv[]) {

    if(argc < 5) {
    	std::cerr << "USO: sudo ./rt_test Period_1 Dead_L1 Period_2 Dead_High2\n";
        exit(1);
    }

    int NUM_THREADS = 2;

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE);

    pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
    if(pthread_attr_setschedpolicy(&attributes, SCHED_FIFO) != 0) {
        std::cerr << "ERROR: Cannot set SCHED_FIFO. Run with sudo.\n";
        exit(1);
    }

    t_arg *arg = (t_arg*) malloc(NUM_THREADS * sizeof(t_arg));
    pthread_t *thread = (pthread_t*) malloc(NUM_THREADS * sizeof(pthread_t));
    struct sched_param param;

    int arg_idx = 1;

    for(unsigned int i=0; i < NUM_THREADS; i++) {
        arg[i].id = i + 1;
        arg[i].period_ms = std::stol(argv[arg_idx++]);
        arg[i].deadline_ms = std::stol(argv[arg_idx++]);
        arg[i].type = i % 2;


        arg[i].priority = 99 - (arg[i].period_ms / 10);
        if(arg[i].priority < 1) arg[i].priority = 1;

      //  watch -n 0.01 "ps -T -C DueThreadCoreDiversi -o pid,tid,psr,comm"
        //nel primo compare il thread che verrà assegnato a un core il secondo mostra quello usato effettivamente dai thread
        cpu_set_t cpuset;
                   CPU_ZERO(&cpuset);
                   CPU_SET(i, &cpuset);
                   pthread_attr_setaffinity_np(&attributes, sizeof(cpu_set_t), &cpuset);

        int s = pthread_attr_setaffinity_np(&attributes, sizeof(cpu_set_t), &cpuset);
        if (s != 0) handle_error(s, "pthread_attr_setaffinity_np");

        std::cout << "Configuring Thread " << i+1 << " on CORE " << i << "\n";


        param.sched_priority = arg[i].priority;
        pthread_attr_setschedparam(&attributes, &param);

        // Creo thread
        int ret = pthread_create(&(thread[i]), &attributes, Task, (void*) &(arg[i]));
        handle_error(ret, "Thread Creation Failed");

        std::string type_name = (arg[i].type == TYPE_LOW_RECOVERY) ? "LOW_REC" : "HIGH_REC";
        std::cout << "Thread " << arg[i].id << " [" << type_name << "] -> Period: "
                  << arg[i].period_ms << "ms, Prio: " << arg[i].priority << "\n";
    }


    for(unsigned int i=0; i < NUM_THREADS; i++) {
        pthread_join(thread[i], NULL);
    }


    std::cout << "\n====================================================\n";
    std::cout << "              RISULTATI FINALI TEST RM              \n";
    std::cout << "====================================================\n";
    for(unsigned int i=0; i < NUM_THREADS; i++) {
        std::string type_name = (arg[i].type == TYPE_LOW_RECOVERY) ? "LOW " : "HIGH";
        std::cout << "[" << type_name << "] -> Violazioni Jitter (>0.5ms): "
                  << std::setw(2) << arg[i].final_jitter_violations
                  << " | Deadline Missed: "
                  << std::setw(2) << arg[i].final_deadline_misses << "\n";
    }
    std::cout << "====================================================\n\n";

    printf("Test concluso.\n");
    free(arg);
    free(thread);
    pthread_attr_destroy(&attributes);
    exit(0);
}


void *Task(void *ptr) {
    t_arg *arg = (t_arg *) ptr;

    struct timespec next_activation, start_work, end_work;
    float simulated_altitude;
    bool recovery_active = false;
    std::string prefix;
    std::string status;
    int CountViolation=0;
    int CountDeadLineMiss=0;

    if (arg->type == TYPE_LOW_RECOVERY) {
        simulated_altitude = 4000.0f;
        prefix = "LOW ";
    } else {
        simulated_altitude = 12000.0f;
        prefix = "HIGH";
    }

    // Prendiamo il tempo iniziale
    clock_gettime(CLOCK_MONOTONIC, &next_activation);

    for(int i=0; i < 50; i++) {
         // 1. Calcolo prossima attivazione
         timespec_add_ms(&next_activation, arg->period_ms);

         // Sleep
         clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL);

         // Sveglia
         clock_gettime(CLOCK_MONOTONIC, &start_work);

         // Overhead sensori (Base Load)
         burn_cpu(1);

         if (arg->type == TYPE_LOW_RECOVERY) simulated_altitude -= 200.0f;
         else simulated_altitude += 200.0f;

         if (arg->type == TYPE_LOW_RECOVERY) {
             if (simulated_altitude < 2500.0f) recovery_active = true;
             if (simulated_altitude > 4500.0f) recovery_active = false;

             if (recovery_active) {
                 status = "\033[1;41m[PULL UP]\033[0m";
                 simulated_altitude += 500.0f;
                 burn_cpu(15);
             } else {
                 status = "\033[1;32m[STABLE] \033[0m";
                 burn_cpu(1);
             }
         }
         else {
             if (simulated_altitude > 15000.0f) recovery_active = true;
             if (simulated_altitude < 13000.0f) recovery_active = false;

             if (recovery_active) {
                 status = "\033[1;43m[PULL DN]\033[0m";
                 simulated_altitude -= 500.0f;
                 burn_cpu(15);
             } else {
                 status = "\033[1;36m[CLIMB]  \033[0m";
                 burn_cpu(1);
             }
         }

         clock_gettime(CLOCK_MONOTONIC, &end_work);
         double response_time = time_diff_ms(start_work, end_work);
         double jitter = std::abs(time_diff_ms(next_activation, start_work));

         if(jitter > 0.5){
             CountViolation++;
         }

         std::cout << "[" << prefix << "] Alt:" << std::setw(5) << (int)simulated_altitude
                   << " | " << status
                   << " | CPU:" << std::fixed << std::setprecision(2) << response_time << "ms"
                   << " | Jit:" << std::setprecision(3) << jitter << "ms";

         if(response_time <= arg->deadline_ms) {
             std::cout << " | DL: OK\n";
         } else {
             std::cout << " | \033[1;31mDeadLineMISSED\033[0m\n";
             CountDeadLineMiss++;
         }
    }

    arg->final_jitter_violations = CountViolation;
    arg->final_deadline_misses = CountDeadLineMiss;

    pthread_exit(NULL);
}
