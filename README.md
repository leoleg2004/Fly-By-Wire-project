✈️ Real-Time Flight Telemetry System: RM Scheduling con Fast DDS

Questo progetto analizza i paradigmi di computazione Hard Real-Time in ambiente Linux (ottimizzato con patch PREEMPT_RT), focalizzandosi sulla politica di scheduling Rate Monotonic (RM). Il sistema simula un ecosistema avionico distribuito che utilizza il middleware eProsima Fast DDS per lo scambio di telemetria complessa (dati testuali e immagini) in configurazioni single-core e multi-core.

L'intero progetto è strutturato per garantire l'automazione completa, dalla compilazione all'analisi dei tracciamenti del kernel, attraverso strumenti contenuti nelle cartelle Tesi e dds.
📡 Comunicazione DDS: Telemetria e Imaging

Il sistema supporta due tipologie principali di scambio dati, situate all'interno della directory dds/, con script dedicati per ogni scenario:

    Standard Message Telemetry: Scambio di pacchetti dati leggeri (altitudine, assetto, coordinate) per il controllo di volo in tempo reale.

    Photo/Image Transmission: Gestione di payload pesanti (immagini dai sensori ottici). Questa modalità testa la capacità dello scheduler RM di gestire task con tempi di esecuzione più lunghi (CPU burning elevato) e latenze di serializzazione DDS senza violare le scadenze critiche.

Entrambe le modalità sono testabili sia su singolo core (massima contesa) che su multi-core (isolamento tramite CPU affinity).
🛠️ Automazione e Build System

Il progetto implementa un sistema di gestione del codice avanzato per facilitare lo sviluppo e il testing:

    Makefile Intelligenti: Ogni modulo dispone di un Makefile configurato per gestire automaticamente le dipendenze, i file sorgente e la ricompilazione delle librerie collegate.

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

    Analisi: trace-cmd e kernelshark.

    Linguaggio: C++17.

🚀 Workflow di Analisi e Simulazione (run_trace.sh)

Per automatizzare il processo di valutazione base, è stato implementato lo script bash run_trace.sh. Questo strumento coordina la pipeline di lavoro:

    Inizializzazione: Setup dell'ambiente e isolamento dei core.

    Esecuzione Simulazione: Avvio dei nodi Publisher e Subscriber DDS.

    Trace Capture: Avvio automatico di trace-cmd per registrare gli eventi del kernel (context switch, preemption).

    Analisi e Visualizzazione: Interfacciamento con il Monitor Real-Time per mostrare i grafici di esecuzione e generare i report.

📂 Strumenti Avanzati di Tracciamento e Marker (dds/DDS/)

All'interno della directory dds/, è presente una specifica sottocartella DDS che contiene script avanzati per l'analisi concorrente e l'ispezione profonda dei thread:

    run_trace_dds.sh: Script dedicato all'avvio e al tracciamento specifico degli scenari di comunicazione DDS avanzati.

    marker_dds.sh: Questo script è fondamentale quando due processi (es. Publisher e Subscriber) vengono eseguiti in contemporanea per la comunicazione DDS complessa (come lo scambio di foto).

        Funzionamento: Utilizza il comando pidof per intercettare e "agganciare" in tempo reale i thread specifici che gestiscono la comunicazione DDS per ogni processo.

        Visualizzazione: Dopo aver tracciato l'esecuzione, lo script apre automaticamente il Monitor Real-Time affiancato a KernelShark. Questo permette di visualizzare visivamente i marker della simulazione direttamente sulla timeline del kernel, facilitando l'analisi delle latenze di rete e di IPC.

Nota sulle varianti di tracciamento: Lo stesso principio di tracciamento mirato è implementato anche tramite lo script marker.sh. Quest'ultimo viene utilizzato per profilare la comunicazione DDS con messaggi semplici e per valutare le esecuzioni Rate Monotonic (RM) standard, coprendo in modo esaustivo sia gli scenari single-core che multi-core.
