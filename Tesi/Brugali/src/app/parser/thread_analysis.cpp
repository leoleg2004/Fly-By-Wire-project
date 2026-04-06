#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

struct Interval {
  double start;
  double end;
};
struct OutEvent {
  double time;
  char state;
};

struct TaskData {
  std::string name;
  double expected_period_sec = 0.0; // Letto dal file .c!
  std::vector<double> switch_in;
  std::vector<OutEvent> switch_out;
  std::vector<Interval> run;
  std::vector<Interval> sleep_int;
  std::vector<Interval> preempt_int;
  std::vector<std::pair<double, std::string>> markers;
};

std::string trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t");
  if (std::string::npos == first)
    return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Uso: " << argv[0] << " <trace.txt> <sorgente.c> [CPU_ID]\n";
    return 1;
  }

  std::string trace_file_path = argv[1];
  std::string source_file_path = argv[2];

  int target_cpu = -1;
  if (argc >= 4) {
    std::string cpu_arg = argv[3];
    if (cpu_arg != "ALL" && cpu_arg != "all")
      target_cpu = std::stoi(cpu_arg);
  }

  std::map<int, TaskData> tasks;
  std::map<std::string, double>
      expected_periods; // Mappa "Activity_1" -> 0.8 (sec)

  // =========================================================================
  // FASE 1: LETTURA DEL FILE SORGENTE C PER ESTRARRE I PERIODI
  // =========================================================================
  std::ifstream src_file(source_file_path);
  if (src_file.is_open()) {
    std::string src_line;

    // Regex aggiornate per accettare sia variabili semplici che array (es.
    // activities[0])
    std::regex name_re(
        R"DELIM(sprintf\s*\(\s*([a-zA-Z0-9_\[\]]+)\.name\s*,\s*"([^"]+)"\s*\))DELIM");
    std::regex period_re(
        R"DELIM(([a-zA-Z0-9_\[\]]+)\.period\s*=\s*(\d+))DELIM");

    std::map<std::string, std::string> var_to_name;
    std::map<std::string, double> var_to_period;

    while (std::getline(src_file, src_line)) {
      std::smatch m;
      if (std::regex_search(src_line, m, name_re)) {
        var_to_name[m[1]] = m[2];
      }
      if (std::regex_search(src_line, m, period_re)) {
        var_to_period[m[1]] =
            std::stod(m[2]) / 1000.0; // Converte ms in secondi
      }
    }

    // Uniamo le due mappe per associare direttamente il Nome al Periodo
    for (const auto &[var_name, act_name] : var_to_name) {
      if (var_to_period.count(var_name)) {
        expected_periods[act_name] = var_to_period[var_name];
      }
    }
    src_file.close();
  } else {
    std::cerr << "Attenzione: Impossibile aprire il file sorgente "
              << source_file_path << " per leggere i periodi.\n";
  }

  // =========================================================================
  // FASE 2: PARSING DEL FILE TRACE.TXT
  // =========================================================================
  std::ifstream file(trace_file_path);
  if (!file.is_open())
    return 1;

  std::regex line_re(
      R"DELIM(^\s*(.*?)-(\d+)\s+\[(\d+)\]\s+(\d+\.\d+):\s+(.*?):\s+(.*)$)DELIM");
  std::regex switch_re(
      R"DELIM(.*?:(\d+)\s+\[\d+\]\s+([A-Z]).*?==>\s+.*?:(\d+)\s+\[)DELIM");

  std::string line;

  // Troviamo i PID delle Activity e dei thread del kernel richiesti
  while (std::getline(file, line)) {
    std::smatch m;
    if (std::regex_search(line, m, line_re)) {
      std::string comm = m[1];
      int pid = std::stoi(m[2]);

      // Filtro dinamico per thread del sistema e custom
      bool is_tracked = false;
      std::string full_name = comm;

      if (comm.find("kworker/1") != std::string::npos ||
          comm.find("ksoftirqd/1") != std::string::npos ||
          comm.find("swapper/1") != std::string::npos) {
        is_tracked = true;
      } else {
        // Cerchiamo nei nomi custom (come Activity_X o Ciao). Gestiamo il
        // limite di 15 chars di Linux
        for (const auto &kv : expected_periods) {
          if (kv.first.find(comm) == 0) {
            is_tracked = true;
            full_name = kv.first; // ripristina il nome completo originale
            break;
          }
        }
        // Se non lo trova ma inizia per Activity_ lo forziamo (es. senza
        // periodo specificato)
        if (!is_tracked && comm.find("Activity_") == 0) {
          is_tracked = true;
        }
      }

      if (is_tracked) {
        if (tasks.count(pid) == 0) {
          tasks[pid].name = full_name;
          // Assegnamo il periodo estratto dal C!
          if (expected_periods.count(full_name)) {
            tasks[pid].expected_period_sec = expected_periods[full_name];
          }
        }
      }
    }
  }

  file.clear();
  file.seekg(0);

  // Estraiamo i tempi sched_switch
  while (std::getline(file, line)) {
    std::smatch m;
    if (std::regex_search(line, m, line_re)) {
      int pid = std::stoi(m[2]);
      int cpu = std::stoi(m[3]);
      double t = std::stod(m[4]);
      std::string event = trim(m[5]);
      std::string details = m[6];

      if (target_cpu != -1 && cpu != target_cpu)
        continue;

      if (event == "sched_switch") {
        std::smatch sm;
        if (std::regex_search(details, sm, switch_re)) {
          int prev_pid = std::stoi(sm[1]);
          char state = sm[2].str()[0];
          int next_pid = std::stoi(sm[3]);

          if (tasks.count(next_pid))
            tasks[next_pid].switch_in.push_back(t);
          if (tasks.count(prev_pid))
            tasks[prev_pid].switch_out.push_back({t, state});
        }
      } else if (event.find("print") != std::string::npos ||
                 event.find("tracing_mark_write") != std::string::npos) {
        std::string marker_text = trim(details);
        size_t colon_pos = marker_text.find(":");
        if (colon_pos != std::string::npos)
          marker_text = trim(marker_text.substr(colon_pos + 1));
        if (tasks.count(pid))
          tasks[pid].markers.push_back({t, marker_text});
      }
    }
  }

  // =========================================================================
  // FASE 3: ESPORTAZIONE CSV E CALCOLO STATISTICHE
  // =========================================================================
  std::ofstream csv("timeline.csv");
  csv << "task,type,start,end,duration_ms\n";

  std::cout << "\n================ RISULTATI MULTI-THREAD ================\n";
  if (target_cpu != -1)
    std::cout << " Filtro CPU attivo: CPU " << target_cpu << "\n";
  else
    std::cout << " Filtro CPU: TUTTE LE CPU\n";

  for (auto &pair : tasks) {
    int pid = pair.first;
    TaskData &tdata = pair.second;

    size_t i = 0, j = 0;
    while (i < tdata.switch_in.size() && j < tdata.switch_out.size()) {
      if (tdata.switch_out[j].time > tdata.switch_in[i]) {
        tdata.run.push_back({tdata.switch_in[i], tdata.switch_out[j].time});
        i++;
        j++;
      } else {
        j++;
      }
    }

    if (tdata.run.empty())
      continue;

    double wcet = 0, sum_rt = 0;
    for (auto &r : tdata.run) {
      double d = r.end - r.start;
      sum_rt += d;
      wcet = std::max(wcet, d);
      csv << tdata.name << ",RUN," << std::fixed << std::setprecision(6)
          << r.start << "," << r.end << "," << d * 1e3 << "\n";
    }
    double avg_rt = sum_rt / tdata.run.size();

    std::vector<double> periods;
    for (size_t k = 1; k < tdata.run.size(); k++)
      periods.push_back(tdata.run[k].start - tdata.run[k - 1].start);

    double avg_period = 0, jitter = 0;
    if (!periods.empty()) {
      for (auto p : periods)
        avg_period += p;
      avg_period /= periods.size();
      for (auto p : periods)
        jitter += (p - avg_period) * (p - avg_period);
      jitter = std::sqrt(jitter / periods.size());
    }

    // ===============================================================
    // AGGIUNTA LOGICA: Calcolo del Job (WCRT) e Miss Corretti
    // ===============================================================
    double wcrt = 0, sum_wcrt = 0;
    int jobs_completed = 0;
    int correct_misses = 0;

    if (!tdata.run.empty()) {
      double current_job_start = tdata.run.front().start;
      for (size_t k = 0; k < tdata.run.size(); k++) {
        char out_state =
            (k < tdata.switch_out.size()) ? tdata.switch_out[k].state : 'R';

        // Se il thread va in stato di riposo (ha finito il lavoro)
        if (out_state == 'S' || out_state == 'D' || out_state == 'Z' ||
            k == tdata.run.size() - 1) {
          double response_time = tdata.run[k].end - current_job_start;
          jobs_completed++;
          wcrt = std::max(wcrt, response_time);
          sum_wcrt += response_time;

          if (tdata.expected_period_sec > 0 &&
              response_time > tdata.expected_period_sec) {
            correct_misses +=
                std::floor(response_time / tdata.expected_period_sec);
          }

          if (k + 1 < tdata.run.size()) {
            current_job_start = tdata.run[k + 1].start;
          }
        }
      }
    }
    // ===============================================================

    for (size_t k = 0; k < tdata.run.size() - 1 && k < tdata.switch_out.size();
         k++) {
      double gap_start = tdata.run[k].end;
      double gap_end = tdata.run[k + 1].start;
      if (gap_end <= gap_start)
        continue;

      char state = tdata.switch_out[k].state;
      if (state == 'S' || state == 'D' || state == 'Z' || state == 'X') {
        tdata.sleep_int.push_back({gap_start, gap_end});
        csv << tdata.name << ",SLEEP," << gap_start << "," << gap_end << ","
            << (gap_end - gap_start) * 1e3 << "\n";
      } else if (state == 'R') {
        tdata.preempt_int.push_back({gap_start, gap_end});
        csv << tdata.name << ",PREEMPT," << gap_start << "," << gap_end << ","
            << (gap_end - gap_start) * 1e3 << "\n";
      }
    }

    for (const auto &m : tdata.markers) {
      csv << tdata.name << ",MARKER_" << m.second << "," << std::fixed
          << std::setprecision(6) << m.first << "," << m.first << ",0.0\n";
    }

    auto compute_stats = [](const std::vector<Interval> &v) {
      double max = 0, sum = 0;
      for (auto &i : v) {
        double d = i.end - i.start;
        sum += d;
        if (d > max)
          max = d;
      }
      return std::make_tuple(max, v.empty() ? 0 : sum / v.size());
    };

    auto [max_sleep, avg_sleep] = compute_stats(tdata.sleep_int);
    auto [max_preempt, avg_preempt] = compute_stats(tdata.preempt_int);

    std::cout
        << "\n----------------------------------------------------------\n";
    std::cout << " TASK: " << tdata.name << " (PID: " << pid << ")\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(3);

    std::cout << " [Esecuzione]WCET : " << std::setw(10) << wcet * 1e3
              << " ms  |  Avg: " << std::setw(10) << avg_rt * 1e3 << " ms\n";
    std::cout << " [Job Totale]WCRT : " << std::setw(10) << wcrt * 1e3
              << " ms  |  Avg: " << std::setw(10)
              << (jobs_completed ? (sum_wcrt / jobs_completed) * 1e3 : 0)
              << " ms\n";
    std::cout << " [Periodo]   Avg  : " << std::setw(10) << avg_period * 1e3
              << " ms  |  Jitter: " << std::setw(7) << jitter * 1e3 << " ms\n";

    // Mostra la riga Miss solo se il periodo è noto (Activity_).
    // Per i thread del kernel mostrerà N/A.
    if (tdata.expected_period_sec > 0) {
      std::cout << " [Deadline]  Miss : " << std::setw(10) << correct_misses
                << "      |  (Atteso dal .c: "
                << tdata.expected_period_sec * 1e3 << " ms)\n";
    } else {
      std::cout << " [Deadline]  Miss : " << std::setw(10) << "N/A"
                << "      |  (Periodo non trovato nel .c)\n";
    }

    std::cout << " [Sleep]     Count: " << std::setw(10)
              << tdata.sleep_int.size() << "  |  Avg: " << std::setw(10)
              << avg_sleep * 1e3 << " ms  |  Max: " << max_sleep * 1e3
              << " ms\n";
    std::cout << " [Preempt]   Count: " << std::setw(10)
              << tdata.preempt_int.size() << "  |  Avg: " << std::setw(10)
              << avg_preempt * 1e3 << " ms  |  Max: " << max_preempt * 1e3
              << " ms\n";
  }

  csv.close();
  std::cout << "\n==========================================================\n";
  std::cout << "[+] Dati esportati su timeline.csv\n";

  return 0;
}
