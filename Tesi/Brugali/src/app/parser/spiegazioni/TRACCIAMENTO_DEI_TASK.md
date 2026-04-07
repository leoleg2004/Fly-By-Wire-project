# MANUALE COMPLETO AL TRACCIAMENTO DEI TASK E SCHEDULAZIONE

Questo documento illustra l'architettura completa di tracciamento (batch e live), progettata per intercettare l'esecuzione dei thread Real-Time (`Activity_1`, `Activity_2`, ecc.) e visualizzarne i costi computazionali. Tutto si basa sul motore interno del kernel Linux, **ftrace**, con interfaccia utente **trace-cmd** e integrato da un avanzato **Parser in C++** e script di automazione per lo streaming in tempo reale.

---

## 1. IL MOTORE DI BASE INTERNO: COSA ACCADE NEL KERNEL LINUX

Il tracciamento dei processi non si basa su software esterni intrusivi, ma ascolta direttamente "il cuore" del Kernel Linux.

- **Patching Dinamico del Codice (Tracepoints)**: Normalmente le "cimici" di tracciamento dentro il kernel sono istruzioni vuote (Nop) per non rallentare l'esecuzione. All'avvio del tracciamento, il kernel modifica fisicamente se stesso "a caldo" nella RAM attivando i sensori scelti.
- **Il Ring Buffer Real-Time**: Ogni volta che avviene un *Context Switch*, il kernel genera un minuscolo pacchetto dati compresso. Per non rallentare l'Esecuzione Real-Time (RM), non scrive su hard disk ma salva in un buffer ad anello (Ring Buffer) pre-allocato in RAM.
- **I Marcatori di Spazio Utente (`trace_marker`)**: Il Kernel fornisce ufficialmente la pipe speciale di sincronizzazione `/sys/kernel/debug/tracing/trace_marker`. I nostri simulatori (`app09b.cpp` / `c`) scrivono in questo canale (grazie alla libreria `lib/trace_marker.h`) scritte personalizzate come *"Inizio Periodo"* o *"Fine Computazione"*.
  **Nota Tecnica**: Per permettere agli script di elaborare in *Real-Time*, il nostro codice C++ ingegneristicamente **duplica** la scrittura inviando i log sia sulla pipe globale madre (per KernelShark), sia sulla pipe istanziata speciale `live_trace_inst` utile allo streaming di terminale. In questo modo le cronologie restano separate ma impeccabili.
---

## 2. LETTURA LOGICA: GLI EVENTI INTERCETTATI

Gli eventi chiave per la programmazione Rate Monotonic sono:
1. `sched_wakeup`: "Ogni volta che un thread passa dallo stato 'addormentato' allo stato 'pronto' (Ready)". Indica ufficialmente **l'inizio biologico del Periodo**, ovvero il task è stato "Rilasciato" e richiede la CPU.
2. `sched_switch`: "Ogni volta che lo Scheduler fa un Context Switch, togliendo la CPU a un thread per darla a un altro". Questo traccia al microsecondo **quanto tempo il nostro aereo (thread) ha trattenuto la CPU in esecuzione**.
3. **Markers Manuali**: Strisce testuali custom come `=== PERIOD_START: ...` iniettate nel codice sorgente che mostrano in chiaro logica e deadline su KernelShark.

---

## 3. L'AUTOMAZIONE A TERMINALE SINGOLO: GLI SCRIPT BASH

Per snellire le logiche prolisse di tracciamento, sono stati creati due script esecutivi autonomi. La novità assoluta è che **non richiedono mai un doppio terminale**. All'avvio, gli script ti chiederanno interattivamente quale applicativo tracciare (es: `./build/app09b`) e si prenderanno cura di eseguirlo al tuo posto in loop nascosto generandoti poi le cartelle colme di risultati analitici e formattazioni storiche `.dat` per KernelShark.

### A) L'Elaborazione Completa Finale (Offline): `run_trace.sh`
Destinato per ottenere simulazioni rapide ma pulite, lavora solo alla fine della simulazione:
1. Ti chiede l'eseguibile via testuale e lo lancia automaticamente.
2. Registra le azioni su file binario tramite `trace-cmd record`.
3. Attende pazientemente e senza alcun log a video la chiusura autonoma del processo.
4. Spacchetta tutto, filtra su `Activity_`, formatta tabellare tramite `awk` e passa l'intero monolite al parser offline **`parser_batch`**.
5. Lo schermo si accende con una cascata ordinatissima dell'intera traccia temporale e del tracciato computazionale finale.

### B) Il Live Streaming (In-Vivo): `live_trace.sh`
Questo potente script incapsula **l'esecuzione simultanea interattiva**, sfidando i limiti di blocco di `trace-cmd`.
1. Ti chiede l'eseguibile via testuale.
2. Crea sul Kernel una cartella "fantasma": `/sys/kernel/debug/tracing/instances/live_trace_inst/`. Attivando qui lo switch del Kernel, si ricava un rubinetto (`trace_pipe`) esclusivo senza derubare le metriche al collaudato `trace-cmd record` che in foreground genera il .dat!
3. Indirizza la valvola infinita dentro `awk` per formattazione ed inoltra il torrente al nostro parser ad alte prestazioni in modalità real-time **`parser_live`**.
4. Dispone di una routine intelligente `cleanup` automatica: appena il tuo `./build/app09b` smette di lavorare, il terminale spegne la valvola pipe, comanda al C++ di esportare i totalizzatori matriciali di periodo, e chiude tutto il software impeccabilmente. Nessun `[CTRL+C]` stressante è richiesto all'utente.

---

## 4. IL PARSER C++ Sdoppiato: `parser_live` e `parser_batch`

Abbiamo ricalcato la geniale architettura di design nativa orientata agli Object Arrays: I parser (prima unici, ora fusi in due applicativi unificati per scopo) elaborano N infiniti `Thread Activity_X` instradandoli alla cieca tramite estrazione Regex stringa. Soprattutto, **immagazzinano ogni singola azione direttamente e strettamente all'interno di Vettori Dinamici `std::vector<SimulationData>` in pancia alla RAM del compilato**.

La differenza comportamentale tra i due Parser cugini è radicale e focalizzata:
1. **`parser_batch.cpp` (Il Muto Contabile)**: Legge il file di storicizzazione in pochi secondi alla conclusione dello script `run_trace.sh`. Butta passivamente tutte e 100.000 le righe del Kernel formattato dentro la matrice numerica `SimulationData`. Nessuna riga disturba la console. Alla fine, invoca `print_summary()` e *vomita* l'intero albero di tracciamento e le tabelle di millisecondi estrapolando i dati esclusivamente dai vettori logici ordinati.
2. **`parser_live.cpp` (Il Graphic Novelist)**: Collegato alla pipe in continuo via `live_trace.sh`. Costruisce il vettore `SimulationData` esattamente come il fratello Batch, ma per ogni cella salvata invoca dinamicamente `print_single_event()`, illuminandoti la console di stringhe mozzafiato che descrivono ogni respiro fisico e logico del thread mentre vive (Dal `PERIOD_START` di trace_marker al `DEADLINE_MISS` fino all'abbandono della CPU in `PREEMPT`). Quando lo script principale capisce che l'applicazione ha chiuso, innesca a valle il Segnale di Morte pulita e stampa le canoniche schede riassuntive matematiche.

**Cos'è il Costo Computazionale Reale?**
Tutti i parser fanno affidamento sull'intersezione pura: 
- Un periodo nasce su una estrazione Regex dalla pipe testuale generata testualmente in `trace_marker.h` (es: `(Period=1800 ms)`). 
- Da quel momento, ogni match `sched_switch` a suo favore attiva un elapsed timer su base nanosecondo (`double 6 decimale`).
- Ogni switch a suo sfavore (`preempt`) freeza la conta in attesa del suo ritorno.
- E non appena lo scheduling riporta una stringa recitante uno state logico addormentato come `S ==>`, l'elapsed time definitivo viene estratto in log terminale, salvato nel vettore di Periodo associato e resettato all'istante di wakeup successivo per la iterazione N+1.
- Eventuali messaggi `DEADLINE_MISS` del `trace_marker` emessi volontariamente dall'applicativo accenderanno un allarme grafico di rottura tracciando l'overflow su KernelShark e terminale!

## 5. REVISIONE E VISUALIZZAZIONE SU KERNELSHARK
Una volta generato ed esaurito lo scopo di verifica in testuale puro, la cartella `trace_results_*` accoglierà il `.dat` intatto in totale integrità assieme alle timeline storiche .txt. 
L'apertura mediante: `kernelshark trace_results_*/trace.dat`
Dimostra come la visibilità degli storici marcati manualmente dalle librerie C++ `trace_marker` e incastonati assieme agli sleep di esecuzione si sposano in un'unica visuale sincronizzata, permettendoci debug completi degli intrecci applicativo-Kernel all'istante esatto di accensione dei cicli Rate Monotonic.
