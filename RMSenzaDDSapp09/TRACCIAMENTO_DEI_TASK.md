# MANUALE COMPLETO AL TRACCIAMENTO DEI TASK E SCHEDULAZIONE

Questo documento illustra l'architettura completa di tracciamento (batch e live), progettata per intercettare l'esecuzione dei thread Real-Time (`Activity_1`, `Activity_2`, ecc.) e visualizzarne i costi computazionali. Tutto si basa sul motore interno del kernel Linux, **ftrace**, con interfaccia utente **trace-cmd** e integrato da un avanzato **Parser in C++** e script di automazione per lo streaming in tempo reale.

---

## 1. IL MOTORE DI BASE INTERNO: COSA ACCADE NEL KERNEL LINUX

Il tracciamento dei processi non si basa su software esterni intrusivi, ma ascolta direttamente "il cuore" del Kernel Linux.

- **Patching Dinamico del Codice (Tracepoints)**: Normalmente le "cimici" di tracciamento dentro il kernel sono istruzioni vuote (Nop) per non rallentare l'esecuzione. All'avvio del tracciamento, il kernel modifica fisicamente se stesso "a caldo" nella RAM attivando i sensori scelti.
- **Il Ring Buffer Real-Time**: Ogni volta che avviene un *Context Switch*, il kernel genera un minuscolo pacchetto dati compresso. Per non rallentare l'Esecuzione Real-Time (RM), non scrive su hard disk ma salva in un buffer ad anello (Ring Buffer) pre-allocato in RAM.
- **I Marcatori di Spazio Utente (`trace_marker`)**: Il Kernel fornisce ufficialmente la pipe speciale di sincronizzazione `/sys/kernel/debug/tracing/trace_marker`. I nostri simulatori (`app09b.cpp` / `c`) scrivono in questo canale (grazie alla libreria `lib/trace_marker.h`) le testuali indicazioni di *"Inizio Periodo"* o *"Fine Computazione"*. Ftrace ascolta e le posiziona cronologicamente fondendole con i cambi di contesto effettivi!

---

## 2. LETTURA LOGICA: GLI EVENTI INTERCETTATI

Gli eventi chiave per la programmazione Rate Monotonic sono:
1. `sched_wakeup`: "Ogni volta che un thread passa dallo stato 'addormentato' allo stato 'pronto' (Ready)". Indica ufficialmente **l'inizio biologico del Periodo**, ovvero il task è stato "Rilasciato" e richiede la CPU.
2. `sched_switch`: "Ogni volta che lo Scheduler fa un Context Switch, togliendo la CPU a un thread per darla a un altro". Questo traccia al microsecondo **quanto tempo il nostro aereo (thread) ha trattenuto la CPU in esecuzione**.
3. **Markers Manuali**: Strisce testuali custom come `=== PERIOD_START: ...` iniettate nel codice sorgente che mostrano in chiaro logica e deadline su KernelShark.

---

## 3. I DUE SCRIPT DI AUTOMAZIONE BASH

Per snellire le logiche prolisse di tracciamento manuale, sono stati creati due script esecutivi autonomi dedicati, ognuno capace di generare al volo cartelle `trace_results_APP_TIMESTAMP/` colme di risultati analitici e formattazioni per KernelShark.

### A) L'Estrazione Batch Classica: `run_trace.sh`
Destinato per ottenere simulazioni rapide, automatizza l'intero spettro operativo off-line:
1. Registra su file `trace.dat` tramite `trace-cmd record -e sched:sched_switch -e sched:sched_wakeup ./simulatore`.
2. Spacchetta il risultato in testo nudo con `trace-cmd report > trace_output.txt`.
3. Filtra con una pipe parallela via `grep -E "Activity_"` isolando esplicitamente solo i thread che ci interessano, tralasciando il rumore di sistema.
4. Usa una direttiva precalcolata di ordinamento `awk` per affettare il log caotico originario in stringhe allineate a `|` leggibili dalla macchina (genera il pulitissimo `trace_colonne.txt`).
5. Processa i dati sul collaudato Parser C++ (`src/parser.cpp`) mostrandoci il costo precalcolato su file log testuale permanente.

### B) Il Live Streaming Sincrono: `live_trace.sh`
Questo script è stato costruito per bypassare il limite di ftrace, che solitamente blocca e incapsula i buffer chiusi. Con `live_trace.sh` **è possibile guardare lo schedulatore decidere in TEMPO REALE loggando in background il file binario, mentre il simulatore lavora nell'altro monitor**.
Spiegazione passaggi logici:
- **Istanze Multiple (Ftrace Cloning)**: Il kernel possiede un sistema di "clonazione dei contenitori di traccia". Allo script non basta accendere `trace-cmd record`: questo preverrebbe la lettura "a schermo". Perciò, avvia prima il demone `trace-cmd record -o trace.dat` per immagazzinare per KernelShark, e in parallelo crea una folder fittizia dentro `/sys/kernel/debug/tracing/instances/live_trace_inst/`.
- Sfruttando questa istanza "fantasma", re-iniettiamo le variabili di `sched_switch`. Otteniamo perciò `trace_pipe`, il rubinetto diretto non ostacolato che sputa senza pause tutti gli scambi OS Real-Time. 
- Il comando inietta la live stream testuale dentro `awk`, il quale riformatta e alimenta la `std::cin` del **Parser C++ in "Live Mode"**.

(*Per uscire in sicurezza senza spezzare file di traccia delicati, `pkill -INT -x trace-cmd` imiterà la siglatura di interruzione umana sul background, generando un `trace.dat` sano e privo del difetto "kvm_combo failed".*)

---

## 4. IL PARSER C++ (`src/parser.cpp`) : CRONOLOGIA DINAMICA

Il vecchio format visualizzatore Tabellare di tipo awk aveva limiti di interpretazione massiva. Ora, `parser.cpp` compie l'analisi del 100% dell'architettura in tempo reale con precisione di calcolo millimetrica in base decimale (`double IEEE 754`).

1. **Gestione N Threads**: Indipendentemente se crei due thread (`Activity_1`, `Activity_2`) o una ventina, il parser C++ cerca e decripta istantaneamente i nomi al volo dai PID e dai dettagli stringa, istanziando dinamicamente `std::map<std::string, TaskState>` contenitori traccianti. Null'altro va programmato manuale.
2. **Costo su base Rilascio**: 
   - Non appena compare `sched_wakeup` -> Contrassegna Periodo Avviato e Resetta Start Time.
   - Match `sched_switch` a favore del task (Task -> CPU) -> Start Elapsed Timer.
   - Match `sched_switch` a discapito del task (CPU -> Preempt) -> **Computing Cost += Elapsed Timer**. 
3. **Chiusura Periodo**: Se la stringa di estromissione recita specificatamente `S ==>`, `D ==>` oppure `X ==>`, l'Applicativo ha coscienziosamente richiamato le syscall termiche di riposo (come Nanosleep). Il parser lo intercetta, chiude logicamente le statistiche e sigla il termine dell'analisi iterativa stampando nanosecondi e sommari di Tabella.
4. **Resa "Paper Tracking"**: La pipeline in vivo produce stampe grafiche sequenziali identiche alle griglie redatte con "Carta e Penna": un task viene `[WAKEUP]` (Svegliato), quindi ottiene il `[DISPATCH]` (Controllo CPU), poi accorre in `[PREEMPT]` (Rimozione forza per compiti altolocati) e infine culmina in `[SLEEP]` (Computazione ultimata).

---

## 5. REVISIONE E VISUALIZZAZIONE SU KERNELSHARK
Una volta generato ed esaurito lo scopo di verifica in testuale puro, la cartella `trace_results_*` accoglierà il `.dat` intatto in totale integrità assieme alle timeline storiche .txt. 
L'apertura mediante: `kernelshark trace_results_*/trace.dat`
Dimostra come la visibilità degli storici marcati manualmente dalle librerie C++ `trace_marker` e incastonati assieme agli sleep di esecuzione si sposano in un'unica visuale sincronizzata, permettendoci debug completi degli intrecci applicativo-Kernel all'istante esatto di accensione dei cicli Rate Monotonic.
