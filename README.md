✈️ Real-Time Flight Telemetry System: RM Scheduling con Fast DDS

Questo progetto analizza i paradigmi di computazione Hard Real-Time in ambiente Linux (ottimizzato con patch PREEMPT_RT), focalizzandosi sulla politica di scheduling Rate Monotonic (RM). Il sistema simula un ecosistema avionico distribuito che utilizza il middleware eProsima Fast DDS per lo scambio di telemetria complessa (dati testuali e immagini) in configurazioni single-core e multi-core.

L'intero progetto è strutturato per garantire l'automazione completa, dalla compilazione all'analisi dei tracciamenti del kernel, attraverso strumenti contenuti nelle cartelle Tesi e dds.
🛠️ Sistema di Build e Makefile

Il progetto implementa un sistema di compilazione altamente automatizzato basato su Makefile intelligenti, strutturati per gestire in modo trasparente la complessità delle librerie POSIX e Fast DDS:

    Gestione Automatica dei Target: I Makefile sono configurati per individuare automaticamente i file sorgente (.c, .cpp) e collegare le corrette librerie (-lpthread, eProsima Fast DDS, Raylib).

    Posizionamento Dinamico: Una volta compilati, gli eseguibili vengono automaticamente spostati nelle corrette directory di destinazione (es. cartella eseguibili/ o dds/app/), separando nettamente i binari dai file sorgente.

    Compilazione Parallela: Supportano nativamente il multithreading per la fase di build. Eseguendo il comando make -j4, il sistema sfrutta più core per compilare moduli complessi (specialmente per DDS) in una frazione del tempo.

    Clean & Rebuild: Regole dedicate (make clean) assicurano la rimozione di file oggetto e binari obsoleti, garantendo che le nuove simulazioni girino sempre sull'ultima versione del codice.

🚀 Workflow di Analisi e Simulazione Base (run_trace.sh e marker.sh)

Per automatizzare la valutazione degli algoritmi RM standard (singolo o multi-core) e la comunicazione DDS base (senza immagini), il sistema utilizza lo script principale run_trace.sh.

Lo script esegue una pipeline di analisi in 5 step sequenziali totalmente automatizzati:

    Rilevamento Dinamico: Lo script cerca automaticamente l'eseguibile e il relativo file sorgente (nelle cartelle eseguibili/, sorgenti/ o dds/app/). Il sorgente è fondamentale perché l'analizzatore andrà a estrarne i parametri temporali (i periodi dei task) necessari per la valutazione.

    Tracciamento Kernel (trace-cmd): L'eseguibile viene avviato sotto la supervisione di trace-cmd, che registra in tempo reale eventi critici come sched_switch e sched_wakeup, generando il file binario trace.dat.

    Estrazione Report: Il file binario viene convertito in un report testuale (trace_output.txt) leggibile dai successivi tool di analisi.

    Analisi C++ (thread_analysis): Viene compilato ed eseguito al volo lo strumento custom thread_analysis.cpp. Questo tool incrocia il log del kernel con i periodi letti dal file sorgente originale, calcolando latenze, Response Time, e Deadline Miss, salvando il tutto in risultati_finali.txt.

    Visualizzazione Python: Infine, lo script avvia monitorRealTime.py, un monitor visivo che legge i file CSV appena generati per produrre grafici sull'andamento dei thread analizzati.

L'uso di marker.sh

Al pari della sua controparte per configurazioni DDS più complesse, marker.sh viene impiegato per profilare in tempo reale i task e agevolare la visualizzazione su KernelShark, intercettando i PID esatti e inserendo dei marker sulla timeline del kernel per evidenziare eventi specifici durante l'esecuzione del codice RM.
📸 Esecuzione Avanzata: DDS con Immagini e Concorrenza (dds/DDS/)

All'interno della sottocartella dds/DDS/, troviamo la controparte avanzata degli script precedenti, progettata appositamente per le comunicazioni DDS con payload pesanti, come la trasmissione di immagini dai sensori.

    Stessa Pipeline, Carico Maggiore: Gli eseguibili per la trasmissione immagini funzionano seguendo gli stessi identici concetti di tracciamento di run_trace.sh, ma sono ingegnerizzati per simulare colli di bottiglia causati dalla serializzazione DDS e da alti livelli di CPU burning.

    run_trace_dds.sh & marker_dds.sh: Quando Publisher (es. fotocamera) e Subscriber (es. display) vengono eseguiti in contemporanea, marker_dds.sh utilizza il comando pidof per intercettare e "agganciare" al volo i PID di entrambi i processi. Questo permette di tracciare le latenze di comunicazione inter-processo e i conflitti di scheduling direttamente nel visualizzatore KernelShark, unendo i tracciati in un unico contesto visivo.

📂 Archiviazione Automatica degli Output (output/traces/)

Una delle caratteristiche chiave del framework di simulazione è la tracciabilità e riproducibilità dei test.

Al termine di ogni singola simulazione (che sia RM standard, DDS testuale o DDS con immagini), gli script creano automaticamente una cartella dedicata all'interno di output/traces/.

    Nomenclatura Timestamp: La cartella viene nominata dinamicamente unendo il nome dell'applicativo a un timestamp esatto (es. trace_results_app10c_20260426_190000).

    Contenuto: Tutti i file generati durante la run vengono isolati in questa directory, tra cui:

        trace.dat (Il dump grezzo del kernel)

        trace_output.txt (Il dump testuale decodificato)

        risultati_finali.txt (Le metriche calcolate dall'analizzatore C++)

        File .csv (Generati per alimentare il monitor grafico Python)

Questo approccio garantisce che ogni test sia perfettamente documentato, confrontabile e pronto per essere inserito nella valutazione prestazionale finale.
