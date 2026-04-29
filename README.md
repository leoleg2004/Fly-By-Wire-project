<<<<<<< HEAD

✈️ Real-Time Flight Telemetry System: RM Scheduling con Fast DDS
Questo progetto analizza i paradigmi di computazione Hard Real-Time in ambiente Linux (ottimizzato con patch PREEMPT_RT), focalizzandosi sulla politica di scheduling Rate Monotonic (RM). Il sistema simula un ecosistema avionico distribuito che utilizza il middleware eProsima Fast DDS per lo scambio di telemetria complessa (dati testuali e immagini) in configurazioni single-core e multi-core.

L'intero progetto è stato recentemente ristrutturato per garantire un'organizzazione industriale: separazione netta tra interfacce (header) e implementazioni (sorgenti), librerie autocontenute e automazione completa tramite un sistema di build gerarchico.

📂 Struttura del Progetto
La nuova architettura è organizzata per massimizzare la modularità e la facilità di manutenzione:

🔹 src/lib/ (Core Libraries)
Il cuore del sistema, dove risiedono tutte le librerie. Ogni modulo è diviso in include/ (header) e src/ (codice sorgente) con Makefile dedicati:

lib/: Librerie core (Activity, Math, Matrix, Time, Trace).
dds_lib/: Gestione della comunicazione DDS (Broadcastner, Listener).
image_lib/: Moduli avanzati per la trasmissione di immagini (Sender, Subscriber).
generated_msg/: Codice generato automaticamente dagli IDL.
🔹 src/app/ (Applications)
Contiene esclusivamente i file main.cpp delle diverse applicazioni (es. ImageSender, ImageSubscriber, Broadcastner, Listener, app10*). Ogni app ha il suo Makefile che punta alle librerie centralizzate.

🔹 src/msg/ (DDS Interfaces)
Contiene i file .idl che definiscono le interfacce dei messaggi. Il Makefile interno gestisce la generazione automatica del codice verso src/lib/generated_msg.

🔹 bin/ (Centralized Binaries)
Tutti gli output della compilazione sono centralizzati qui:

bin/app/: Eseguibili finali.
bin/lib/: Librerie statiche del core.
bin/msg/lib/: Librerie DDS e dei messaggi (statiche e dinamiche).
bin/msg/include/: Header pubblici necessari per il linking.
bin/tools/: Script di automazione per tracciamento e analisi.
🛠️ Automazione e Build System
Il progetto implementa un sistema di gestione del codice avanzato:

Makefile Gerarchici: È possibile compilare l'intero progetto dalla radice con un solo make, oppure entrare in ogni singolo modulo di libreria o applicazione per build granulari.
Separazione Include/Src: Ogni modulo di libreria gestisce indipendentemente le proprie interfacce, facilitando la modifica del codice senza rompere le dipendenze globali.
Compilazione Parallela: Supporto nativo per make -j4, ottimizzando i tempi di build grazie alla gestione sicura delle cartelle temporanee durante la generazione DDS.
Runtime Hardened (RPATH): Gli eseguibili sono compilati con RUNPATH configurato, garantendo che trovino sempre le librerie dinamiche in bin/msg/lib/ anche se lanciati con privilegi di root (sudo).
🛩️ Simulatore F-16 Non-Lineare (Cartella SimulatoreC++)
Il progetto include una cartella indipendente denominata SimulatoreC++, che contiene l'implementazione di un simulatore di volo non-lineare basato sul modello dell'aereo F-16, con rendering grafico sviluppato in Raylib.

Build System Dedicato: La cartella è provvista di un proprio Makefile e di un file CMakeLists.txt. Questo garantisce una ricompilazione rapida e isolata.
Origine MATLAB e Automazione: Questo codice in C++ è una derivazione diretta di un modello originariamente sviluppato in MATLAB. Tutto il lavoro di analisi e progettazione risiede in una repository separata: Repository MATLAB - AutomationF16.
Scopo: Funge da ambiente di testing locale per la dinamica di volo, indipendente dagli script di tracciamento RT.
🚀 Workflow di Analisi (bin/tools/)
Per automatizzare la valutazione degli algoritmi RM e della comunicazione DDS, il sistema utilizza gli script in bin/tools/:

1. run_trace.sh & run_trace_dds.sh
Eseguono una pipeline in 5 step:

Rilevamento Dinamico: Trova l'eseguibile in bin/app/ e il sorgente in src/app/ o src/lib/.
Tracciamento Kernel (trace-cmd): Registra gli eventi sched_switch e sched_wakeup.
Estrazione Report: Converte il trace binario in testo.
Analisi C++ (thread_analysis): Calcola Latenze, Response Time e Deadline Miss incrociando i log con i periodi letti dai sorgenti.
Visualizzazione Python: Produce grafici e timeline dell'andamento dei thread.
2. marker.sh & marker_dds.sh
Utilizzati per profilare i task su KernelShark, intercettando i PID e inserendo marker temporali sulla timeline del kernel durante l'esecuzione.

📊 Monitoraggio Avanzato (Tesi & DDS Monitor)
Il nucleo analitico risiede nella cartella Tesi, che ospita anche il eProsima Fast DDS Monitor:

Avvio: Lanciare Publisher e Subscriber.
Monitor: Avviare lo script in Tesi/DDS_MONITOR (con privilegi adeguati).
Sintonizzazione: Configurare lo stesso Domain ID del codice C++ per rilevare i nodi e tracciare statistiche su throughput e latenze.
📂 Archiviazione Output (bin/Test/)
Al termine di ogni simulazione, gli script creano una cartella dedicata in bin/Test/ (es. trace_results_ImageSubscriber_20260429_170000) contenente:

Dump del kernel (trace.dat).
Report testuale (trace_output.txt).
Metriche calcolate (risultati_finali.txt).
File .csv per i grafici Python.
=======
# ✈️ Real-Time Flight Telemetry System: RM Scheduling con Fast DDS

Questo progetto analizza i paradigmi di computazione **Hard Real-Time** in ambiente Linux (ottimizzato con patch **PREEMPT_RT**), focalizzandosi sulla politica di scheduling **Rate Monotonic (RM)**. Il sistema simula un ecosistema avionico distribuito che utilizza il middleware **eProsima Fast DDS** per lo scambio di telemetria complessa (dati testuali e immagini) in configurazioni single-core e multi-core.

L'intero progetto è strutturato per garantire l'automazione completa: dalla generazione delle interfacce DDS alla compilazione parallela, fino all'analisi dei tracciamenti del kernel.

---

## 📂 Architettura e Struttura del Progetto

Il codice è organizzato seguendo criteri di modularità industriale, separando nettamente interfacce, implementazioni e applicazioni:

### 🔹 `src/lib/` (Librerie Centralizzate)
Tutte le funzionalità condivise sono raggruppate qui. Ogni libreria è suddivisa in `include/` (header pubblici) e `src/` (codice sorgente e Makefile privato):
- **`lib/`**: Moduli core per la gestione di tempo, attività, matrici e tracciamento.
- **`dds_lib/`**: Strati di astrazione per la comunicazione DDS (Broadcastner/Listener).
- **`image_lib/`**: Moduli specializzati per il trattamento e la trasmissione di immagini.
- **`generated_msg/`**: Codice C++ generato automaticamente dai file IDL.

### 🔹 `src/app/` (Entry Points)
Contiene esclusivamente i file `main.cpp` delle applicazioni finali (es. `ImageSender`, `ImageSubscriber`, `Broadcastner`, `Listener`). Ogni applicazione dispone di un proprio Makefile per compilazione e linking rapidi.

### 🔹 `src/msg/` (Interfacce DDS)
Definizioni IDL dei messaggi. Il Makefile integrato gestisce la generazione del codice verso la cartella `generated_msg` in modo sicuro per il build parallelo.

### 🔹 `bin/` (Binari e Risultati)
- **`app/`**: Eseguibili pronti per la simulazione.
- **`lib/` & `msg/lib/`**: Librerie statiche (`.a`) e dinamiche (`.so`).
- **`tools/`**: Script di automazione (`run_trace.sh`, `run_trace_dds.sh`, `marker.sh`).
- **`Test/`**: Archiviazione automatica dei risultati dei test con timestamp.

---

## 🛠️ Build System & Automazione

- **Compilazione Totale**: Eseguendo `make` dalla radice si orchestra l'intero processo di build.
- **Supporto Parallelo**: Ottimizzato per `make -j4`, riducendo drasticamente i tempi di compilazione.
- **Determinismo a Runtime (RPATH)**: Gli eseguibili includono i percorsi delle librerie dinamiche, garantendo il corretto caricamento anche durante i tracciamenti del kernel con `sudo`.
- **Pulizia Automatica**: Il sistema gestisce in autonomia i file temporanei e i vecchi binari per evitare conflitti tra run diverse.

---

## 🛩️ Simulatore F-16 Non-Lineare (`SimulatoreC++`)

Il progetto include un simulatore di volo indipendente basato sul modello dell'aereo F-16:
- **Rendering**: Sviluppato con la libreria **Raylib**.
- **Origine**: Derivato direttamente da modelli fisici sviluppati in MATLAB.
- **Repository Dedicata**: Per la teoria del controllo e l'automazione originale, consultare: [AutomationF16](https://github.com/leoleg2004/AutomationF16).
- **Nota**: Questo modulo è autocontenuto e non è collegato direttamente agli script di tracciamento RT.

---

## 🚀 Workflow di Analisi e Simulazione

Il sistema automatizza la valutazione delle performance Real-Time in 5 step:

1.  **Rilevamento**: Lo script identifica l'eseguibile e analizza il sorgente per estrarre i periodi dei task.
2.  **Tracciamento**: Registrazione degli eventi del kernel (`sched_switch`, `sched_wakeup`) tramite `trace-cmd`.
3.  **Reportistica**: Generazione di log testuali dettagliati del comportamento del kernel.
4.  **Analisi Analitica**: Lo strumento `thread_analysis` calcola Latenze, Response Time e Deadline Miss.
5.  **Monitor Visuale**: Visualizzazione grafica dei dati tramite lo script Python in tempo reale.

### Utilizzo degli Script
- **`run_trace.sh`**: Per simulazioni standard di telemetria.
- **`run_trace_dds.sh`**: Per test avanzati di trasmissione immagini con DDS.
- **`marker.sh`**: Per inserire marker personalizzati visibili su KernelShark.

---

## 📊 Monitoraggio Avanzato e Statistiche

All'interno della cartella `Tesi` è disponibile il **eProsima Fast DDS Monitor**. Questo strumento permette di analizzare in tempo reale il throughput della rete, le latenze di comunicazione e lo stato dei nodi DDS, sintonizzandosi sul Domain ID configurato nel codice C++.

---

## ⚙️ Requisiti Tecnici
- **OS**: Linux con patch **PREEMPT_RT**.
- **Middleware**: eProsima Fast DDS.
- **Strumenti**: `trace-cmd`, `kernelshark`, `Python 3`.
- **Linguaggio**: C++17 / C.
- **Librerie Grafiche**: Raylib (per il simulatore F-16) e OpenCV (per il modulo immagini).
>>>>>>> 4093428 (readme)
