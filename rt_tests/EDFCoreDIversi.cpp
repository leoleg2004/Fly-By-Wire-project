/* g++ rt_scheduler.cpp -o rt_scheduler -pthread && g++ DueThreadCoreDiversi.cpp -o
 * DueThreadCoreDiversi -pthread && g++ EDFCoreUguali.cpp -o rt_edf_test -pthread
 * && g++ EDFCoreDIversi.cpp -o EDFCoreDiversi -pthread
 *
 *
 * comando per aggiornare le modifiche sul codcie da terminale
*/
//#define _GNU_SOURCE
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

#define handle_error(en, msg) \
        if(en != 0) {errno = en; perror(msg); exit(EXIT_FAILURE);}

#define SCHED_DEADLINE  6
#define TYPE_LOW_RECOVERY 0
#define TYPE_HIGH_RECOVERY 1

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

//devo fare la chiamata da sistema per poter utilizzare edf devo fornire periodo deadlne a cnhe cpu usata
int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
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
} t_arg;

void *Task(void *ptr);

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
    int TARGET_CORE = 0;

    std::cout << "--- RT Dual Test | SCHED_DEADLINE (EDF) | Core " << TARGET_CORE << " ---\n";

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE);

    t_arg *arg = (t_arg*) malloc(NUM_THREADS * sizeof(t_arg));
    pthread_t *thread = (pthread_t*) malloc(NUM_THREADS * sizeof(pthread_t));

    int arg_idx = 1;

    for(unsigned int i=0; i < NUM_THREADS; i++) {
        arg[i].id = i + 1;
        arg[i].period_ms = std::stol(argv[arg_idx++]);
        arg[i].deadline_ms = std::stol(argv[arg_idx++]);
        arg[i].type = i % 2;

//watch -n 0.01 "ps -T -C EDFCoreDiversi -o pid,tid,psr,comm"
        //nel primo compare il thread che verrà assegnato a un core il secondo mostra quello usato effettivamente dai thread
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        pthread_attr_setaffinity_np(&attributes, sizeof(cpu_set_t), &cpuset);

        // Chiediamo il 100% della deadline come Runtime garantito decido qua quanto stressare il sistema
        arg[i].runtime_ms = (long)(arg[i].deadline_ms * 1);
        if(arg[i].runtime_ms < 1) arg[i].runtime_ms = 1;

        int ret = pthread_create(&(thread[i]), &attributes, Task, (void*) &(arg[i]));
        handle_error(ret, "Thread Creation Failed");

        std::string type_name = (arg[i].type == TYPE_LOW_RECOVERY) ? "LOW_REC" : "HIGH_REC";
        std::cout << "Thread " << arg[i].id << " [" << type_name << "] -> Req Runtime: "
                  << arg[i].runtime_ms << "ms (Deadline: " << arg[i].deadline_ms << "ms)\n";
    }


    for(unsigned int i=0; i < NUM_THREADS; i++) {
        pthread_join(thread[i], NULL);
    }


    std::cout << "\n====================================================\n";
    std::cout << "              RISULTATI FINALI TEST EDF             \n";
    std::cout << "====================================================\n";
    for(unsigned int i=0; i < NUM_THREADS; i++) {
        std::string type_name = (arg[i].type == TYPE_LOW_RECOVERY) ? "LOW " : "HIGH";
        std::cout << "[" << type_name << "] -> Violazioni Jitter (>0.5ms): "
                  << std::setw(2) << arg[i].final_jitter_violations
                  << " | Deadline Missed: "
                  << std::setw(2) << arg[i].final_deadline_misses << "\n";
    }
    std::cout << "====================================================\n\n";

    printf("Test EDF concluso.\n");
    free(arg);
    free(thread);
    pthread_attr_destroy(&attributes);
    exit(0);
}


void *Task(void *ptr) {
    t_arg *arg = (t_arg *) ptr;

    struct sched_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.sched_policy = SCHED_DEADLINE;

    attr.sched_runtime  = arg->runtime_ms * 1000 * 1000;
    attr.sched_deadline = arg->deadline_ms * 1000 * 1000;
    attr.sched_period   = arg->period_ms * 1000 * 1000;

    int ret = sched_setattr(0, &attr, 0);
    if (ret < 0) {
        perror("sched_setattr failed");
        pthread_exit(NULL);
    }

    struct timespec start_work, end_work, expected_arrival;
    float simulated_altitude;
    bool recovery_active = false;
    std::string prefix;
    std::string status;
    int CountViolation = 0;
    int CountDeadLineMiss = 0;

    if (arg->type == TYPE_LOW_RECOVERY) {
        simulated_altitude = 4000.0f;
        prefix = "LOW ";
    } else {
        simulated_altitude = 12000.0f;
        prefix = "HIGH";
    }

    clock_gettime(CLOCK_MONOTONIC, &expected_arrival);

    for(int i=0; i < 50; i++) {

         sched_yield();

         clock_gettime(CLOCK_MONOTONIC, &start_work);

         if (i > 0) timespec_add_ms(&expected_arrival, arg->period_ms);
         else expected_arrival = start_work;

         double jitter = std::abs(time_diff_ms(expected_arrival, start_work));

         if(jitter > 0.5 && i > 0) {
             CountViolation++;
         }

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

         double response_time = time_diff_ms(expected_arrival, end_work);

         std::cout << "[" << prefix << "] Alt:" << std::setw(5) << (int)simulated_altitude
                   << " | " << status
                   << " | CPU:" << std::fixed << std::setprecision(1) << response_time << "ms"
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
