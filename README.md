# ✈️ Real-Time Flight Telemetry System: RM & EDF Scheduling with Fast DDS and Graphical Simulation

Il presente progetto esplora le prestazioni dei sistemi **Hard Real-Time** in ambiente Linux (su kernel con patch `PREEMPT_RT`), confrontando le logiche di scheduling **Rate Monotonic (RM)** ed **Earliest Deadline First (EDF)**. 

L'architettura del software modella un sistema avionico Fly-By-Wire: un modulo sensoriale di telemetria (Publisher) aggiorna l'altitudine del velivolo, mentre un nodo di controllo (Subscriber) analizza la serie temporale dei dati per comandare manovre correttive (PULL UP, PULL DOWN) o mantenere l'assetto (STABLE). Il transito delle informazioni avviene tramite il middleware di grado industriale **eProsima Fast DDS**, consentendo un'analisi meticolosa dell'overhead di comunicazione Inter-Process (IPC) in scenari ad alta criticità temporale. 

A corredo del motore computazionale logico, il progetto integra un **Simulatore Grafico basato su Raylib**, che fornisce una validazione visiva della telemetria senza violare l'isolamento e i vincoli temporali rigidi dei task di controllo.

Architettura Publish/Subscribe in Fast DDS
<img width="1024" height="559" alt="immagine" src="https://github.com/user-attachments/assets/fdd56291-ca06-49c0-9b32-92fccde148ed" />

---

## ⚙️ Fondamenti di Schedulazione e API POSIX

Per garantire un determinismo temporale assoluto, il progetto fa un uso estensivo delle API di sistema POSIX e delle chiamate di sistema (syscall) a basso livello del kernel Linux.

* **Gestione dei Thread e Isolamento (CPU Affinity):**
  L'ambiente di esecuzione è gestito tramite la libreria `<pthread.h>`. Al fine di mitigare l'OS Noise e il costo del context switch, il sistema implementa l'allocazione statica dei thread su core fisici isolati (es. Core 0 e Core 2). Tale partizionamento è garantito dalle macro `CPU_SET` e dalla funzione `pthread_attr_setaffinity_np()`.
* **Implementazione Rate Monotonic (RM):**
  Il protocollo RM è realizzato sfruttando l'intestazione `<sched.h>` e applicando la policy preemptive `SCHED_FIFO`. Le priorità statiche vengono assegnate a runtime in modo inversamente proporzionale al periodo del task, avvalendosi della struttura `sched_param` e della funzione `pthread_attr_setschedparam()`.
* **Implementazione Earliest Deadline First (EDF):**
  Poiché lo standard POSIX di base non espone nativamente un'interfaccia per lo scheduling su base deadline, il software interagisce direttamente con il kernel Linux tramite `<sys/syscall.h>`. Utilizzando la chiamata `syscall(__NR_sched_setattr)`, il progetto applica la policy `SCHED_DEADLINE`, configurando dinamicamente la banda di calcolo attraverso i parametri `sched_runtime`, `sched_deadline` e `sched_period`.
* **Controllo Deterministico del Tempo:**
  La periodicità rigorosa dei task è governata dalla libreria `<time.h>`. L'attivazione dei thread sfrutta la funzione `clock_nanosleep()` agganciata al clock hardware `CLOCK_MONOTONIC` in modalità assoluta (`TIMER_ABSTIME`). Questo approccio matematico previene il fenomeno di *drifting* (deriva temporale) causato dall'accumulo di ritardi iterativi.
* **Prevenzione del Page-Faulting (Memory Locking):**
  L'utilizzo di `<sys/mman.h>` e del comando `mlockall(MCL_CURRENT | MCL_FUTURE)` vincola l'intero spazio di indirizzamento del processo all'interno della memoria RAM fisica, eludendo i ritardi catastrofici intrinsechi nelle operazioni di swapping del sistema operativo.

---

## 🛠️ Architettura del Sistema e Separazione delle Criticità

Il sistema è compartimentato in tre moduli asincroni principali. I task avionici operano per una durata pre-calcolata (es. 20 secondi), sincronizzando matematicamente la propria terminazione per evitare inconsistenze dei dati residui (data starvation).

1. **Il Sensore di Volo (Publisher / LOW_RECOVERY):**
   * **Vincolo:** Hard Real-Time.
   * **Ruolo:** Modella la cinematica dell'aereo, calcolando variazioni di altitudine tra 1.000m e 15.000m.
   * **Azione:** Pubblica il dato aggiornato sul Topic DDS `TelemetryTopic` e inietta un carico computazionale deterministico (`burn_cpu`) per simulare il tempo di acquisizione hardware.

2. **Il Sistema di Controllo (Subscriber / HIGH_RECOVERY):**
   * **Vincolo:** Hard Real-Time.
   * **Ruolo:** Esegue il polling deterministico dei dati telemetrici.
   * **Azioni Logiche:** * Altitudine < 2500m ➔ Comando `[PULL UP ATTIVO]` (Carico CPU massivo per simulare la recovery)
     * Altitudine > 13000m ➔ Comando `[PULL DW ATTIVO]` (Carico CPU massivo per simulare la recovery)
     * Volo Standard ➔ Comando `[CLIMB]` o `[STABLE]` (Carico CPU nominale)

3. **Il Simulatore Visivo (MonitorApp / FlightSim):**
   * **Vincolo:** Soft Real-Time (Best-Effort).
   * **Ruolo:** Sviluppato in **Raylib**, opera come un nodo DDS Listener passivo. Intercetta i campioni sul `TelemetryTopic` e aggiorna un *Primary Flight Display* (PFD). L'architettura software garantisce che le fluttuazioni del framerate (es. colli di bottiglia della GPU) non generino *backpressure* o latenza sui nodi di controllo critici.

Primary Flight Display Simulato in Raylib
<img width="1024" height="559" alt="immagine" src="https://github.com/user-attachments/assets/087f69e2-57ec-4894-b5d8-f08cf77b3a09" />

---

## 🛩️ Dinamica di Volo e Controllo Longitudinale (F-16)

Parallelamente all'infrastruttura informatica, il progetto integra la modellazione matematica e la sintesi delle leggi di controllo di un velivolo F-16, sviluppate in ambiente MATLAB per la futura esecuzione nei nodi di controllo.

### 1. Estrazione del Modello e Analisi di Stabilità
A partire da un modello non lineare a 6 gradi di libertà (6-DOF), è stata isolata la dinamica longitudinale del velivolo. Il partizionamento delle matrici in spazio di stato ha permesso di estrarre le quattro variabili di stato fondamentali ($\theta$, $q$, $U$, $W$) e i comandi attivi (Manetta, Equilibratore, Flap al bordo d'attacco), trattando le raffiche di vento come disturbi esogeni. L'analisi agli autovalori sulla realizzazione minima del sistema ha confermato l'intrinseca instabilità aperiodica del caccia (polo a parte reale positiva), rendendo imperativo l'uso di un sistema *Fly-By-Wire* in retroazione.

### 2. Analisi Multivariabile (RGA) e Disaccoppiamento MIMO
L'analisi della matrice RGA (*Relative Gain Array*) sull'impianto ad anello aperto ha rivelato un profondo accoppiamento cinematico e aerodinamico (es. l'equilibratore degrada drasticamente la velocità $V_t$). Essendo il sistema sottoattuato (5 ingressi fisici per 6 uscite monitorate), si è resa necessaria la sintesi di un pre-compensatore statico in avanti ($W$). 
Sfruttando la **pseudo-inversa di Moore-Penrose**, è stata calcolata la matrice ottima ai minimi quadrati:

$$W = K_{tot}^\dagger$$

L'applicazione di questa rete di disaccoppiamento ha permesso di convertire gli attuatori fisici in **Comandi Virtuali** indipendenti. L'RGA post-disaccoppiamento certifica una diagonalizzazione perfetta ($1.000$ sulla diagonale principale) per i canali di navigazione primari (Velocità, Incidenza, Beccheggio).

### 3. Sintesi del Flight Control System
L'avvenuto isolamento dei canali ha frammentato il sistema multivariabile in loop SISO (*Single-Input Single-Output*) indipendenti. Ciò costituisce la base matematica per la sintesi decentralizzata dei controllori PID, demandando il controllo della velocità a un *Auto-Throttle* e la stabilizzazione dell'assetto a un *Pitch Stability Augmentation System (SAS)*.

---

## 📊 Metriche Analizzate

Durante l'esecuzione, il core Hard Real-Time stampa in standard output i log di volo per ogni ciclo di attivazione, tracciando tre parametri ingegneristici fondamentali:

* **Tempo di Risposta (Response Time / CPU time):** Il delta temporale effettivo richiesto dal thread per completare l'intera computazione (dal risveglio al completamento della logica applicativa).
* **Jitter (Latenza di Attivazione):** La discrepanza temporale assoluta tra il momento teorico di schedulazione del task e la reale acquisizione della CPU, fortemente influenzata dall'overhead del middleware DDS e dalle preemption del kernel.
* **Deadline Miss:** Conteggio cumulativo delle violazioni temporali assolute (istanti in cui il response time eccede la deadline di progettazione).

---

## 💻 Configurazione, Compilazione ed Esecuzione

Il progetto si avvale di **CMake** per la generazione automatizzata dei Makefile e il linking delle librerie dipendenti.

### 1. Requisiti di Sistema
* Sistema Operativo: Linux Ubuntu (kernel raccomandato: `PREEMPT_RT`).
* Toolchain: `g++` (C++17), `CMake`.
* Dipendenze: `Fast DDS`, `Fast CDR`, `Raylib`, `pthread`.

### 2. Build del Progetto
```bash
mkdir build && cd build
cmake ..
make -j4
