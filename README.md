

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
