/*
 * Real-Time Thread Test in C++
 * Features: Deadline Check, Jitter Calculation, Absolute Timing
 * PLUS: Rate Monotonic Scheduling (RM) & CPU Affinity
 */

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <pthread.h> // Libreria POSIX Threads
#include <sched.h>   // Libreria POSIX Scheduling
#include <time.h>
#include <unistd.h>
#include <iomanip>

// Macro per gestione errori POSIX
#define handle_error(en, msg) \
        if(en != 0) { errno = en; perror(msg); exit(EXIT_FAILURE); }

// Struttura dati per gli argomenti del thread
struct ThreadArgs {
    int id;
    long period_ms;    // Ogni quanto deve girare (T)
    long deadline_ms;  // Entro quanto deve finire (D)
    int priority;      // Priorità assegnata (RM)
};

// Funzioni Helper per il tempo
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

// Funzione eseguita dai thread
void *Task(void *ptr) {
    ThreadArgs *arg = (ThreadArgs *) ptr;
    
    struct timespec next_activation;
    struct timespec now, start_work, end_work;
    
    // Prendiamo il tempo attuale
    clock_gettime(CLOCK_MONOTONIC, &next_activation);

    for(int i = 0; i < 10; i++) {
        // 1. Calcoliamo la PROSSIMA attivazione (Periodo)
        timespec_add_ms(&next_activation, arg->period_ms);

        // 2. Sleep Assoluta (Fondamentale per ridurre il drift)
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_activation, NULL);

        // --- INIZIO CICLO DI LAVORO ---
        clock_gettime(CLOCK_MONOTONIC, &start_work); 

        // 3. Calcolo Jitter
        double jitter = time_diff_ms(next_activation, start_work);

        // 4. Stampa Info (inclusa Priorità per verifica)
        std::cout << "[Thread " << arg->id << " | Prio " << arg->priority << "] "
                  << "Iter: " << i+1 
                  << " | Jitter: " << std::fixed << std::setprecision(3) << jitter << " ms";

        // 5. Controllo Deadline
        clock_gettime(CLOCK_MONOTONIC, &end_work);
        double response_time = time_diff_ms(start_work, end_work);
        
        // Simuliamo carico CPU per vedere se la priorità funziona (Opzionale)
        // for(int k=0; k<1000000; k++); 

        if (response_time <= arg->deadline_ms) {
            std::cout << " | DL: OK (" << response_time << "ms)\n";
        } else {
            std::cout << " | \033[1;31mDL: MISSED\033[0m (" << response_time << " > " << arg->deadline_ms << ")\n";
        }
    }
    
    pthread_exit(NULL);
}

// Funzione Helper per calcolare priorità Rate Monotonic
int calculate_rm_priority(long period_ms) {
    // RM: Periodo più piccolo = Priorità più alta
    // FIFO va da 1 (min) a 99 (max). 
    // Facciamo una formula semplice: 99 - (periodo / 10)
    int prio = 99 - (period_ms / 10);
    if (prio < 1) prio = 1; // Minimo sindacale
    if (prio > 99) prio = 99;
    return prio;
}

int main(int argc, char* argv[]) {
    
    if(argc < 3 || (argc - 1) % 2 != 0) {
        std::cerr << "USO: sudo ./rt_deadline_test <Periodo1> <Deadline1> ...\n";
        exit(1);
    }

    int NUM_THREADS = (argc - 1) / 2;
    int CORE_ID = 0; // [AFFINITY] Inchiodiamo tutto sul Core 0

    std::cout << "--- Avvio RM Scheduling su Core " << CORE_ID << " ---\n";

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    
    // [SCHEDULING 1] Diciamo al thread di usare la policy che settiamo noi, non quella della shell
    pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);

    // [SCHEDULING 2] Impostiamo la policy POSIX FIFO (Real-Time)
    // Questo è il comando chiave che chiedevi
    int ret = pthread_attr_setschedpolicy(&attributes, SCHED_FIFO);
    if(ret != 0) {
        std::cerr << "ERRORE: Impossibile settare SCHED_FIFO. Hai usato sudo?\n";
        exit(1);
    }

    // [AFFINITY] Creiamo la maschera per il Core 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(CORE_ID, &cpuset);
    // Applichiamo l'affinity agli attributi
    pthread_attr_setaffinity_np(&attributes, sizeof(cpu_set_t), &cpuset);

    std::vector<ThreadArgs> args(NUM_THREADS);
    std::vector<pthread_t> threads(NUM_THREADS);

    int arg_idx = 1;
    for(int i = 0; i < NUM_THREADS; i++) {
        args[i].id = i + 1;
        args[i].period_ms = std::stol(argv[arg_idx++]);
        args[i].deadline_ms = std::stol(argv[arg_idx++]);
        
        // [SCHEDULING 3] Calcoliamo la priorità RM
        args[i].priority = calculate_rm_priority(args[i].period_ms);

        // [SCHEDULING 4] Applichiamo la priorità specifica a questo thread
        struct sched_param param;
        param.sched_priority = args[i].priority;
        pthread_attr_setschedparam(&attributes, &param);

        // Creazione Thread
        ret = pthread_create(&threads[i], &attributes, Task, (void*) &args[i]);
        char error_msg[50];
        sprintf(error_msg, "Errore creazione Thread %d", i+1);
        handle_error(ret, error_msg);
        
        std::cout << "Thread " << i+1 << " -> Periodo: " << args[i].period_ms 
                  << "ms | Priority FIFO: " << args[i].priority << "\n";
    }

    for(int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    std::cout << "\nTest Concluso.\n";
    pthread_attr_destroy(&attributes);
    return 0;
}
