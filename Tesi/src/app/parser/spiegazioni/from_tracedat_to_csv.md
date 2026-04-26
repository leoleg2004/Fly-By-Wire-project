# Da `trace.dat` a `timeline.csv` — Guida Dettagliata

Questo documento spiega passo per passo come il file binario `trace.dat` viene trasformato nel CSV finale usato per la visualizzazione.

---

## 1. Registrazione: `trace.dat`

Lo script `run_trace.sh` utilizza `trace-cmd` per registrare gli eventi del kernel durante l'esecuzione dell'applicazione real-time:

```bash
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup \
     -o "$OUTPUT_DIR/trace.dat" "$EXECUTABLE"
```

| Evento | Cosa cattura |
|--------|-------------|
| `sched:sched_switch` | Ogni cambio di contesto: chi lascia la CPU, chi entra, e lo stato di uscita (R, S, D) |
| `sched:sched_wakeup` | Quando un thread viene risvegliato e messo in coda di esecuzione |
| `tracing_mark_write` | Marker scritti dal programma utente tramite `trace_marker()` |

---

## 2. Conversione Testuale: `trace_output.txt`

```bash
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"
```

Produce righe nel formato:

```
      Ciao-8052  [000]  1306.299646: print:  tracing_mark_write: PERIOD_START_Ciao
      Ciao-8052  [001]  1306.350123: sched_switch:  Ciao:8052 [120] R ==> ksoftirqd/1:24 [120]
```

Ogni riga contiene: **comm-PID [CPU] timestamp: evento: dettagli**

---

## 3. Il Parser C++: `thread_analysis.cpp`

### 3.1 FASE 1 — Lettura del sorgente C

Estrae periodo e deadline dal file `.c` dell'applicazione:

```cpp
// Trova: sprintf(activities[0].name, "Ciao")
std::regex name_re(
    R"(sprintf\s*\(\s*([a-zA-Z0-9_\[\]]+)\.name\s*,\s*"([^"]+)")");

// Trova: activities[0].period = 250
std::regex period_re(
    R"(([a-zA-Z0-9_\[\]]+)\.period\s*=\s*(\d+))");

// Trova: activities[0].deadline = 200
std::regex deadline_re(
    R"(([a-zA-Z0-9_\[\]]+)\.deadline\s*=\s*(\d+))");
```

Costruisce una mappa di associazioni in due passi:

```cpp
// Passo 1: lega la variabile al nome leggibile
//   var_to_name["activities[0]"] = "Ciao"
while (std::getline(src_file, src_line)) {
    if (std::regex_search(src_line, m, name_re))
        var_to_name[m[1]] = m[2];
    if (std::regex_search(src_line, m, period_re))
        var_to_period[m[1]] = std::stod(m[2]) / 1000.0;  // ms → sec
    if (std::regex_search(src_line, m, deadline_re))
        var_to_deadline[m[1]] = std::stod(m[2]) / 1000.0;
}

// Passo 2: unisce le mappe
//   expected_periods["Ciao"] = 0.250  (secondi)
for (const auto &[var_name, act_name] : var_to_name) {
    if (var_to_period.count(var_name))
        expected_periods[act_name] = var_to_period[var_name];
    if (var_to_deadline.count(var_name))
        expected_deadlines[act_name] = var_to_deadline[var_name];
}
```

---

### 3.2 FASE 2 — Discovery Dinamica dei Thread

Ogni riga di `trace_output.txt` viene analizzata con questa regex:

```cpp
std::regex line_re(
    R"(^\s*(.*?)-(\d+)\s+\[(\d+)\]\s+(\d+\.\d+):\s+(.*?):\s+(.*)$)");
//         comm  PID      CPU       timestamp       evento    dettagli
```

#### Passo 1 — Scoperta automatica dei thread utente

Qualsiasi PID che scrive un `tracing_mark_write` viene automaticamente tracciato:

```cpp
while (std::getline(file, line)) {
    if (std::regex_search(line, m, line_re)) {
        std::string comm = trim(m[1]);
        int pid = std::stoi(m[2]);
        std::string event = trim(m[5]);

        // Thread utente = chiunque scriva un marker
        if (event.find("print") != std::string::npos ||
            event.find("tracing_mark_write") != std::string::npos) {
            marker_pids[pid] = comm;
        }

        // Thread kernel fissi (CPU 1)
        if (comm.find("kworker/1") != std::string::npos ||
            comm.find("ksoftirqd/1") != std::string::npos)
            tasks[pid].name = comm;
    }
}
```

#### Passo 2 — Ricostruzione nome completo e assegnazione parametri

Linux tronca i nomi dei thread a 15 caratteri. Il C++ li ripristina:

```cpp
for (const auto &[pid, comm] : marker_pids) {
    std::string full_name = comm;
    // Se comm="Activity_12345" (troncato), cerca in expected_periods
    // un nome che inizia con "Activity_12345..."
    for (const auto &kv : expected_periods) {
        if (kv.first.find(comm) == 0) {
            full_name = kv.first;  // nome completo
            break;
        }
    }
    tasks[pid].name = full_name;
    tasks[pid].expected_period_sec = expected_periods[full_name];
}
```

#### Passo 3 — Raccolta eventi per ogni PID tracciato

```cpp
while (std::getline(file, line)) {
    // ... parsing della riga ...

    if (event == "sched_switch") {
        // Estrae: prev_pid, stato (R/S/D), next_pid
        std::regex switch_re(
            R"(.*?:(\d+)\s+\[\d+\]\s+([A-Z]).*?==>\s+.*?:(\d+)\s+\[)");

        if (tasks.count(next_pid))
            tasks[next_pid].switch_in.push_back(t);   // entra in CPU
        if (tasks.count(prev_pid))
            tasks[prev_pid].switch_out.push_back({t, state}); // esce
    }
    else if (event.find("tracing_mark_write") != std::string::npos) {
        // Estrae il testo dopo "tracing_mark_write: "
        std::string marker_text = trim(details);
        size_t colon_pos = marker_text.find(":");
        if (colon_pos != std::string::npos)
            marker_text = trim(marker_text.substr(colon_pos + 1));
        // → "PERIOD_START_Ciao"
        tasks[pid].markers.push_back({t, marker_text});
    }
}
```

---

### 3.3 FASE 3 — Costruzione Intervalli

#### Intervalli RUN

Si accoppiano gli `switch_in` con il successivo `switch_out`:

```cpp
size_t i = 0, j = 0;
while (i < tdata.switch_in.size() && j < tdata.switch_out.size()) {
    if (tdata.switch_out[j].time > tdata.switch_in[i]) {
        tdata.run.push_back({tdata.switch_in[i], tdata.switch_out[j].time});
        //                    ↑ entra in CPU        ↑ esce dalla CPU
        i++; j++;
    } else {
        j++;  // switch_out orfano, scartato
    }
}
```

#### Intervalli SLEEP e PREEMPT

I gap tra RUN consecutivi vengono classificati in base allo **stato di uscita**:

```cpp
for (size_t k = 0; k < tdata.run.size() - 1; k++) {
    double gap_start = tdata.run[k].end;
    double gap_end   = tdata.run[k + 1].start;

    char state = tdata.switch_out[k].state;
    if (state == 'S' || state == 'D')  // Sleeping / Blocked
        → SLEEP
    else if (state == 'R')             // Ready (preempted dal kernel)
        → PREEMPT
}
```

| Stato | Significato | Tipo |
|-------|------------|------|
| `S` | Il thread ha volontariamente dormito (`nanosleep`, `clock_nanosleep`) | SLEEP |
| `D` | Bloccato su I/O (disco, mutex kernel) | SLEEP |
| `R` | Pronto ma espulso dalla CPU da un thread a priorità più alta | PREEMPT |

---

### 3.4 Estrazione Marker dal Testo

I marker vengono cercati nei testi salvati:

```cpp
std::vector<double> pstart, pend, func_ends;

for (const auto &m : tdata.markers) {
    if (m.second.find("PERIOD_START") != std::string::npos)
        pstart.push_back(m.first);
    else if (m.second.find("PERIOD_END") != std::string::npos)
        pend.push_back(m.first);
    else if (m.second.find("FUNCTION_END") != std::string::npos)
        func_ends.push_back(m.first);
}
```

In caso di marker inline (stile `app10e`), i parametri vengono estratti dal testo:

```cpp
// Marker: "PERIOD_START_Activity_1 | Period = 800 | Deadline = 800"
std::regex inline_p(R"(Period\s*=\s*(\d+))");
if (tdata.expected_period_sec == 0 &&
    std::regex_search(m.second, sm, inline_p))
    tdata.expected_period_sec = std::stod(sm[1]) / 1000.0;
```

---

### 3.5 Calcolo Statistiche

#### Periodo Misurato (mediana)

```cpp
std::vector<double> gaps;
for (size_t g = 1; g < pstart.size(); g++)
    gaps.push_back(pstart[g] - pstart[g - 1]);
std::sort(gaps.begin(), gaps.end());
measured_ms = gaps[gaps.size() / 2] * 1000.0;
```

#### Jitter (deviazione standard)

```cpp
double mean_gap = sum(gaps) / gaps.size();
double var = 0;
for (double g : gaps)
    var += (g - mean_gap) * (g - mean_gap);
jitter_ms = sqrt(var / gaps.size()) * 1000.0;
```

#### WCET / ACET (per job, non per singolo segmento)

```cpp
// Per ogni periodo [PERIOD_START, PERIOD_END]:
inst_run_ms = state_time_in(tdata.run, ps, pe) * 1000.0;
//            ↑ somma le durate RUN che si sovrappongono al periodo

// state_time_in calcola:
for (const auto &iv : intervals) {
    double overlap = min(iv.end, hi) - max(iv.start, lo);
    if (overlap > 0) total += overlap;
}

wcet_ms = max(wcet_ms, inst_run_ms);  // Worst Case
acet_ms = sum_run_ms / job_count;      // Average Case
```

#### Utilization

```cpp
// U = tempo_totale_RUN / (N_periodi × T_periodo_atteso)
if (calc_period_sec > 0 && pstart.size() >= 2) {
    double denom = (pstart.size() - 1) * calc_period_sec;
    util = (total_run_ms / 1000.0) / denom;
}
```

#### Deadline Miss Detection

```cpp
// Per ogni periodo, trova l'ultimo FUNCTION_END
double response_ms = (func_end_t - ps) * 1000.0;
double slack_ms = deadline_ms - response_ms;

if (slack_ms < 0) {
    n_misses++;
    // Registra: task, periodo, timestamp, deadline, response, overshoot
    mle.overshoot_ms = -slack_ms;
    all_misses.push_back(mle);
}
worst_slack_ms = min(worst_slack_ms, slack_ms);
```

---

### 3.6 Scrittura CSV

Il CSV contiene 4 tipi di riga:

```csv
task,type,start,end,duration_ms
```

**1. Righe di stato** — usate per il Gantt chart:
```csv
Ciao,RUN,1306.299646,1306.350123,50.477000
Ciao,SLEEP,1306.350123,1306.549680,199.557000
Ciao,PREEMPT,1306.120000,1306.195000,75.000000
```

**2. Righe marker** — usate per linee periodo/deadline:
```csv
Ciao,MARKER_PERIOD_START_Ciao,1306.299646,1306.299646,0.0
Ciao,MARKER_FUNCTION_END_Ciao,1306.350123,1306.350123,0.0
```

**3. Righe statistiche** — lette direttamente nella tabella:
```csv
Ciao,STAT_PERIOD,0,0,2000.000000
Ciao,STAT_WCET,0,0,101.256000
Ciao,STAT_NMISS,0,0,0
```

**4. Righe miss log** — per il pannello miss:
```csv
Task_A,MISS_LOG,100.200000,0.200000,200.000000|280.000000|80.000000|2
```

Formato MISS_LOG: `task,MISS_LOG,ps_abs,ps_rel,deadline|response|overshoot|period_idx`
