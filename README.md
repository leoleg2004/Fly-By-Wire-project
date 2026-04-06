✈️ Real-Time Flight Telemetry System: RM & EDF Scheduling with Fast DDS
Questo progetto analizza i paradigmi di computazione Hard Real-Time in ambiente Linux (ottimizzato con patch PREEMPT_RT), mettendo a confronto le prestazioni delle politiche di scheduling Rate Monotonic (RM) ed Earliest Deadline First (EDF).

L'architettura modella un ecosistema avionico distribuito dove il determinismo temporale e l'affidabilità della comunicazione IPC (Inter-Process Communication) sono requisiti critici. Il sistema è orchestrato tramite il middleware eProsima Fast DDS, che garantisce lo scambio di dati telemetrici ad alte prestazioni tra nodi computazionali isolati.

⚙️ Fondamenti di Schedulazione e Protocolli POSIX
Per garantire la prevedibilità del sistema (determinismo), il software interagisce con le interfacce a basso livello del kernel Linux e lo standard POSIX.1b.

1. Isolamento Computazionale (CPU Affinity)

Per minimizzare l'interferenza causata dallo scheduler generico (OS Noise) e massimizzare l'efficienza delle cache, il sistema implementa il partizionamento dei core. Utilizzando le macro CPU_SET e la funzione pthread_attr_setaffinity_np(), ogni thread critico viene vincolato a una CPU fisica specifica, riducendo drasticamente i cache miss e i context switch involontari.

2. Rate Monotonic Scheduling (RM)

La politica RM è implementata come schema a priorità fissa preventiva (fixed-priority preemptive).

Meccanismo: Viene utilizzata la classe di scheduling SCHED_FIFO tramite la struttura sched_param.

Assegnazione: Le priorità vengono calcolate secondo il teorema di Liu e Layland, assegnando priorità superiori ai task con periodi più brevi.

API: pthread_attr_setschedparam() e pthread_attr_setschedpolicy().

3. Earliest Deadline First (EDF)

Per scenari a priorità dinamica, il sistema sfrutta SCHED_DEADLINE, una politica basata sull'algoritmo Constant Bandwidth Server (CBS).

Meccanismo: Poiché non è esposto direttamente da pthread, si interagisce con il kernel tramite la syscall __NR_sched_setattr.

Parametri: Il task viene definito dalla terna (Runtime,Deadline,Period), permettendo al kernel di garantire una frazione di CPU dedicata ed eseguire sempre il task con la scadenza più prossima.

4. Gestione Temporale e Memory Locking

Precisione al Nanosecondo: Per evitare la deriva temporale (drifting), la periodicità è garantita da clock_nanosleep() con il clock CLOCK_MONOTONIC e flag TIMER_ABSTIME. Ciò assicura che il risveglio avvenga su un istante assoluto rispetto all'epoca di avvio.

Resilienza della Memoria: Il comando mlockall(MCL_CURRENT | MCL_FUTURE) disabilita il paging della memoria virtuale su disco (swapping). Questo previene latenze non deterministiche causate da page faults durante l'accesso a variabili critiche.

🏗️ Architettura del Sistema Distribuiti (DDS)
Il sistema è suddiviso in moduli funzionali che comunicano attraverso il protocollo Data Distribution Service (DDS), seguendo lo standard DCPS (Data Centric Publish-Subscribe).

1. Telemetry Provider (Publisher - Task Periodico)

Criticità: Hard Real-Time.

Funzione: Modella l'acquisizione dati dai sensori di bordo. Genera campioni telemetrici (altitudine, assetto) e li pubblica sul TelemetryTopic.

Simulazione Carico: Include una funzione di CPU burning deterministica per simulare il tempo di computazione necessario alla lettura fisica dei sensori.

2. Control & Analysis Node (Subscriber - Task Critico)

Criticità: Hard Real-Time.

Funzione: Riceve i dati in tempo reale e analizza la serie temporale. Monitora le soglie di sicurezza avionica e logga lo stato del sistema.

Determinismo IPC: L'overhead introdotto dalla serializzazione dei dati (Fast CDR) e dal trasporto DDS viene misurato per valutare l'impatto sulla latenza end-to-end del loop di controllo.

3. Flight Visualizer (DDS Listener - Soft Real-Time)

Criticità: Soft Real-Time / Best-Effort.

Tecnologia: Sviluppato con Raylib.

Isolamento: Questo modulo opera come un osservatore passivo. Grazie alle proprietà di QoS (Quality of Service) di DDS, il visualizzatore grafico non può rallentare o bloccare i nodi Hard Real-Time, garantendo l'integrità dei task di controllo anche in caso di sovraccarico della GPU.

📊 Analisi delle Prestazioni e Metriche RT
L'efficacia del sistema viene validata attraverso la raccolta di metriche ingegneristiche ad ogni ciclo di esecuzione:

Response Time (Tempo di Risposta): Misura l'intervallo tra l'istante di rilascio del task e il suo completamento. Fornisce indicazioni sulla capacità del sistema di soddisfare il carico computazionale.

Release Jitter (Variazione di Rilascio): Analizza la stabilità temporale del risveglio del thread. Un jitter elevato indica interferenze da parte degli interrupt di sistema o configurazioni errate della patch PREEMPT_RT.

Deadline Miss Rate: Monitora la frequenza con cui i task superano la propria scadenza temporale. In configurazione Hard Real-Time, questa metrica deve tendere a zero.

🛠️ Toolchain e Requisiti Tecnici
Requisiti

Kernel: Linux con patch RT-Preempt.

Middleware: eProsima Fast DDS (v2.x o superiore).

Grafica: Raylib (per il modulo di monitoraggio).

Linguaggio: C++17.
