✈️ Real-Time Flight Telemetry System: RM Scheduling con Fast DDS

Questo progetto analizza i paradigmi di computazione Hard Real-Time in ambiente Linux (ottimizzato con patch PREEMPT_RT), focalizzandosi sulla politica di scheduling Rate Monotonic (RM). Il sistema simula un ecosistema avionico distribuito che utilizza il middleware eProsima Fast DDS per lo scambio di telemetria complessa (dati testuali e immagini) in configurazioni single-core e multi-core.

L'intero progetto è strutturato per garantire l'automazione completa, dalla compilazione all'analisi dei tracciamenti del kernel, attraverso strumenti contenuti nelle cartelle Tesi e dds.
📡 Comunicazione DDS: Telemetria e Imaging

Il sistema supporta due tipologie principali di scambio dati, situate all'interno della directory dds/, con script dedicati per ogni scenario:

    Standard Message Telemetry: Scambio di pacchetti dati leggeri (altitudine, assetto, coordinate) per il controllo di volo in tempo reale.

    Photo/Image Transmission: Gestione di payload pesanti (immagini dai sensori ottici). Questa modalità testa la capacità dello scheduler RM di gestire task con tempi di esecuzione più lunghi (CPU burning elevato) e latenze di serializzazione DDS senza violare le scadenze critiche.

Entrambe le modalità sono testabili sia su singolo core (massima contesa) che su multi-core (isolamento tramite CPU affinity).
🛩️ Simulatore F-16 Non-Lineare (Cartella SimulatoreC++)

Il progetto include una cartella indipendente denominata SimulatoreC++, che contiene l'implementazione di un simulatore di volo non-lineare basato sul modello dell'aereo F-16, con rendering grafico sviluppato in Raylib.

    Build System Dedicato: La cartella è provvista di un proprio Makefile e di un file CMakeLists.txt. Questo garantisce una ricompilazione rapida e isolata per qualsiasi tipo di modifica apportata al motore fisico o grafico.

    Origine MATLAB e Automazione: Questo codice in C++ è una derivazione diretta di un modello originariamente sviluppato in MATLAB. Tutto il lavoro di analisi, progettazione della teoria del controllo e automazione su questo specifico modello F-16 risiede in una repository Git separata. Puoi consultare il progetto originario qui:
    👉 [Repository MATLAB - Simulatore e Automazione F-16]([https://github.com/leoleg2004/AutomationF16])

    Scopo in questo Progetto: A differenza degli altri moduli, questo simulatore in C++ non utilizza gli script di tracciamento automatizzato illustrati di seguito, ma funge puramente da ambiente di implementazione e testing locale per la dinamica di volo.
🛠️ Automazione e Build System

Il progetto implementa un sistema di gestione del codice avanzato per facilitare lo sviluppo e il testing:

    Makefile Intelligenti: Ogni modulo (ad eccezione del simulatore F-16) dispone di un Makefile configurato per gestire automaticamente le dipendenze, i file sorgente e la ricompilazione delle librerie collegate.

    Compilazione Parallela: Supporto nativo per la compilazione accelerata tramite il comando make -j4, ottimizzando i tempi di build sui sistemi multi-processor.

    Gestione Eseguibili: Il sistema pulisce e rigenera automaticamente i binari garantendo che ogni simulazione utilizzi sempre l'ultima versione del codice e delle configurazioni delle librerie.

⚙️ Caratteristiche Tecniche e Requisiti
Gestione Thread e Scheduling

    Libreria POSIX: Creazione e gestione nativa dei thread tramite pthread_create.

    Politica RM: Implementazione con SCHED_FIFO e priorità statiche calcolate su base periodica.

    Memory Locking: Uso di mlockall per prevenire il paging virtuale e garantire il determinismo.

Requisiti di Sistema

    Kernel: Linux con patch PREEMPT_RT.

    Middleware: eProsima Fast DDS.

    Analisi: trace-cmd, kernelshark e Python 3 (per il monitor visuale).

    Grafica: Raylib.

    Linguaggio: C++17.

🚀 Workflow di Analisi e Simulazione Base (run_trace.sh e marker.sh)

Per automatizzare la valutazione degli algoritmi RM standard (singolo o multi-core) e la comunicazione DDS base (senza immagini), il sistema utilizza lo script principale run_trace.sh.

Lo script esegue una pipeline di analisi in 5 step sequenziali totalmente automatizzati:

    Rilevamento Dinamico: Lo script cerca automaticamente l'eseguibile e il relativo file sorgente. Il sorgente è fondamentale perché l'analizzatore andrà a estrarne i parametri temporali (i periodi dei task) necessari per la valutazione.

    Tracciamento Kernel (trace-cmd): L'eseguibile viene avviato sotto la supervisione di trace-cmd, che registra in tempo reale eventi critici come sched_switch e sched_wakeup, generando il file binario trace.dat.

    Estrazione Report: Il file binario viene convertito in un report testuale (trace_output.txt) leggibile dai successivi tool di analisi.

    Analisi C++ (thread_analysis): Viene compilato ed eseguito al volo lo strumento custom thread_analysis.cpp. Questo tool incrocia il log del kernel con i periodi letti dal file sorgente originale, calcolando latenze, Response Time, e Deadline Miss.

    Visualizzazione Python: Infine, lo script avvia il Monitor Real-Time in Python, un tool visivo che legge i dati generati per produrre grafici sull'andamento dei thread analizzati.

L'uso di marker.sh

Lo script marker.sh viene impiegato per profilare in tempo reale i task e agevolare la visualizzazione su KernelShark, intercettando i PID esatti e inserendo dei marker sulla timeline del kernel per evidenziare eventi specifici durante l'esecuzione.
📸 Esecuzione Avanzata: DDS con Immagini e Concorrenza (dds/DDS/)

All'interno della sottocartella dds/DDS/, troviamo la controparte avanzata degli script precedenti, progettata appositamente per le comunicazioni DDS con payload pesanti, come la trasmissione di immagini dai sensori.

    Stessa Pipeline, Carico Maggiore: Funzionano seguendo gli stessi concetti di tracciamento, ma simulano i colli di bottiglia causati dalla serializzazione DDS e da alti livelli di CPU burning.

    run_trace_dds.sh & marker_dds.sh: Quando Publisher e Subscriber vengono eseguiti in contemporanea, marker_dds.sh utilizza il comando pidof per intercettare e "agganciare" al volo i PID di entrambi i processi. Questo permette di tracciare le latenze inter-processo e visualizzarle chiaramente su KernelShark in un unico contesto visivo.

📊 Strumenti di Monitoraggio Avanzati (Cartella Tesi)

Il nucleo analitico del progetto è contenuto all'interno della cartella Tesi. Oltre agli script per la generazione dei report, questa cartella ospita tool diagnostici specifici per il middleware, fondamentali per analizzare a fondo la rete:

    eProsima Fast DDS Monitor: All'interno della cartella Tesi è presente la sottocartella DDS_MONITOR, che contiene lo strumento ufficiale di eProsima per tenere traccia in tempo reale dello stato della rete, del throughput e delle latenze di comunicazione.

    Procedura di Utilizzo del Monitor: Per avviare correttamente la tracciatura delle statistiche, seguire questi step:

        Avviare la Comunicazione: Eseguire in due terminali separati gli script del Publisher e del Subscriber DDS per instaurare e mantenere attiva la comunicazione.

        Avviare il Monitor: Entrare nella cartella Tesi/DDS_MONITOR e lanciare lo script del Fast DDS Monitor. Attenzione: Assicurarsi che lo script abbia i corretti privilegi di esecuzione (es. usando sudo o impostando preventivamente chmod +x sullo script) per evitare che il programma fallisca a causa di permessi negati nell'accesso ai socket di rete.

        Sintonizzazione del Dominio: Una volta aperta l'interfaccia grafica del monitor, è necessario sintonizzarlo sullo stesso Domain ID scelto e configurato all'interno del codice sorgente C++ dei nodi DDS. Fatto ciò, il monitor rileverà automaticamente i nodi e inizierà a tracciarne le statistiche.

    Attivazione via Codice: Si noti che i parametri profilati dal DDS Monitor non vengono esposti di default dai nodi. Vengono esplicitamente attivati via codice nel sorgente dei Publisher/Subscriber, modificando opportunamente le policy di QoS (Quality of Service) per consentire l'esportazione delle metriche senza sovraccaricare inutilmente l'IPC di base.
📂 Archiviazione Automatica degli Output (output/traces/)

Al termine di ogni singola simulazione, gli script creano automaticamente una cartella dedicata all'interno di output/traces/.

    Nomenclatura Timestamp: La cartella viene nominata dinamicamente unendo il nome dell'applicativo a un timestamp esatto (es. trace_results_app10c_20260426_190000).

    Contenuto: Tutti i file generati durante la run vengono isolati in questa directory, tra cui il dump grezzo del kernel (trace.dat), il log testuale (trace_output.txt), le metriche calcolate dall'analizzatore C++ (risultati_finali.txt) e i file .csv generati per alimentare i grafici del monitor Python.

Questo approccio garantisce che ogni test sia perfettamente documentato, riproducibile e pronto per l'estrazione delle metriche finali.
