# Confronto: Rate Monotonic (RM) vs Earliest Deadline First (EDF) su Linux Real-Time

Questo documento analizza le differenze architetturali e pratiche tra le due strategie di scheduling Real-Time che hai implementato nei tuoi test: **Rate Monotonic (RM)** ed **Earliest Deadline First (EDF)** all'interno del Kernel Linux.

---

## 1. Rate Monotonic (RM)
Il Rate Monotonic è un algoritmo a **priorità statica**. La regola d'oro del RM è: *più il periodo di un task è breve (maggiore frequenza), più alta sarà la sua priorità*.

### Come avviene in Linux (`DDSCORE`, `DDSMCORE`)
Linux implementa questo concetto tramite le policy POSIX `SCHED_FIFO` o `SCHED_RR`.
1.  **Assegnazione fissa:** Tramite il tuo codice, calcoli staticamente una priorità compresa tra `1` e `99`:
    ```cpp
    arg[i].priority = 99 - (arg[i].period_ms / 10);
    param.sched_priority = arg[i].priority;
    pthread_attr_setschedparam(&attributes, &param);
    ```
2.  **Preemption (Prelazione):** Quando due thread condividono la stessa CPU, se il thread a bassa priorità sta eseguendo e si risveglia il thread ad altissima priorità (es. 99), il Kernel **blocca istantaneamente** (pre-empts) il task inferiore e cede la CPU a quello superiore.
3.  **Vantaggi:** Molto stabile, facile da implementare tramite le librerie standard `pthread`. Basso overhead per il Kernel Linux.
4.  **Svantaggi:** L'utilizzo massimo teorico della CPU è matematicamente limitato a circa il **69%** (il famoso limite di Liu e Layland per un numero elevato di task). Oltre questa soglia, non puoi garantire matematicamente che tutte le deadline vengano rispettate. Se un task a priorità 99 entra in un loop infinito (o in deadlock), **congela interamente il sistema** (se non ci sono le dovute restrizioni come `kernel.sched_rt_runtime_us`).

---

## 2. Earliest Deadline First (EDF)
L'EDF è un algoritmo a **priorità dinamica**. La regola d'oro è: *il task la cui deadline è più imminente in termini di tempo assoluto, vince l'accesso alla CPU, indipendentemente dal suo periodo nominale*.

### Come avviene in Linux (`DDSEDFSCORE`, `DDSEDFMCORE`)
Linux implementa l'EDF in maniera nativa, rigida e "aggressiva" attraverso la policy Hard Real-Time `SCHED_DEADLINE`. Questa policy scavalca gerarchicamente tutte le altre (anche una FIFO priorità 99).
1.  **Syscall Diretta:** Non essendoci uno standard POSIX diffuso per `SCHED_DEADLINE`, si istruisce il Kernel tramite syscall compilando la struttura `sched_attr`:
    *   **Runtime (WCET):** Il tempo massimo di CPU riservato.
    *   **Deadline:** Il traguardo entro cui il runtime dev'essere concluso.
    *   **Period:** Il ciclo di riattivazione del task.
2.  **CBS (Constant Bandwidth Server):** Linux sposta fisicamente il thread su uno scompartimento garantito. Constant Bandwidth significa che il kernel assicura che il task riceva esattamente la porzione di CPU richiesta (`runtime / deadline`).
3.  **Preemption Dinamica:** Il Kernel analizza costantemente gli orologi assoluti di sistema. Se il task *B* si attiva improvvisamente e la sua reale deadline scade prima di quella del task *A* che sta attualmente elaborando, la CPU viene asportata da *A* per favorire *B*. È un calcolo continuo.
4.  **Throttling:** Onde evitare deadlock (loop infiniti), se un thread EDF consuma più millisecondi di quelli garantiti nel paramentro `runtime`, il Kernel lo stordisce (Throttling) e lo muta fino all'inizio del `period` successivo, proteggendo la vita del resto del sistema.
5.  **Vantaggi:** Sfruttamento ottimale della CPU. Si può arrivare teoricamente ad ammettere un set di task fino al **100% dell'utilizzo della CPU** garantendo i vincoli di tempo.
6.  **Svantaggi:** Molto difficile da tunare (assegnare il giusto parametro di `runtime` empirico è complesso). L'overhead per il sistema operativo per calcolare dinamicamente gli alberi delle scadenza (tramite i *Red-Black Trees* del Kernel) è maggiore rispetto alla priorità fissa del RM.

---

## 3. Riepilogo Matrice di Confronto

| Caratteristica | Rate Monotonic (Linux: `SCHED_FIFO`) | Earliest Deadline First (Linux: `SCHED_DEADLINE`) |
| :--- | :--- | :--- |
| **Tipo Priorità** | Statica (basata sul periodo) | Dinamica (basata sulla scadenza assoluta imminente) |
| **Pianificabilità Max (CPU%)** | ~ 69.3% | 100% |
| **Gestione Loop Infinito** | Il task monopolizza la CPU (morte del sistema se Single Core). | Il task viene ibernato (throttled) al superamento del `runtime`. |
| **Supporto Libreria C++** | Completo ed eccellente (`pthread`). | Assente, richiede Syscall dirette e strutture C custom. |
| **Dominanza Kernel** | Alta. Possono essere preemptati solo dagli interrupts (IRQ) o dall'EDF. | **Assoluta.** L'EDF in Linux scavalca anche e costantemente il RM/FIFO a priorità 99. |
| **Miglior Caso D'uso** | Sistemi critici, prevedibili, dove si conoscono a monte i tassi di attivazione e c'è molta CPU libera. | Sistemi stressati dove si ricerca la massima utilizzazione possibile delle magre risorse hardware. |
