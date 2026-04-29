
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
