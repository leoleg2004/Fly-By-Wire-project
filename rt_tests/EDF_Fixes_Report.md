# Report di Fix: Schedulazione EDF in Real-Time Linux

## Contesto
I test real-time che implementano lo scheduling **Earliest Deadline First (EDF)** configuravano accuratamente runtime, deadline e periodi. Inoltre dichiaravano sia la struttura `sched_attr` sia il wrapper POSIX per invocare il kernel (`sched_setattr`). 

Tuttavia, all'interno della routine dei thread (`Task(void *ptr)`), mancava l'effettiva invocazione del sistema operativo per applicare la policy al kernel.

## Intervento Effettuato
Ho modificato i 4 file contenenti le routine EDF per iniettare l'assegnazione **attiva** delle policy real-time `SCHED_DEADLINE`.
I file aggiornati sono:
1. `EDFCoreUguali.cpp`
2. `EDFCoreDIversi.cpp`
3. `DDSEDFSCORE.cpp`
4. `DDSEDFMCORE.cpp`

### Dettaglio del Codice
Per ciascuno di questi file, all'inizio della funzione thread `void* Task(void *ptr)`, ho aggiunto il seguente blocco di codice fondamentale per agganciare il kernel:

```cpp
	struct sched_attr attr;
	memset(&attr, 0, sizeof(attr));
	attr.size = sizeof(attr);
	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime = arg->runtime_ms * 1000 * 1000;  // in nanosecondi
	attr.sched_deadline = arg->deadline_ms * 1000 * 1000; // in nanosecondi
	attr.sched_period = arg->period_ms * 1000 * 1000;    // in nanosecondi

	if (sched_setattr(0, &attr, 0) != 0) {
		perror("sched_setattr failed");
		pthread_exit(NULL);
	}
```

## Risultato
*   **0 come PID:** Invoca il kernel e associa gli attributi al thread corrente (*Caller Thread*).
*   **Gestione Errore:** Nel caso in cui non sussistano i requisiti di sistema per Hard Real-Time, il thread avvisa ed esce dolcemente stampando un errore `sched_setattr failed` (potenzialmente visibile se non avvii i binari con `sudo`).
*   **Conversione unità:** Sono stati convertiti i millisecondi estratti all'avvio nel main in **nanosecondi** (moltiplicati *1.000.000*), rispettando lo standard Kernel che la struct Linux per il `SCHED_DEADLINE` impone a basso livello.

Ora eseguendo i tuoi script ed eseguibili come `sudo`, il Kernel Linux prenderà controllo dei task con politica PREEMPT_RT Hard Real-Time e visualizzerai gli scheduling EDF reali sulla tua CPU.
