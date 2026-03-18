#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct TraceRecord {
  std::string task_pid;
  std::string cpu;
  double timestamp;
  std::string event;
  std::string details;
};

struct TaskPeriodData {
  int period_id;
  double computation_time_ms;
};

struct TaskState {
  int current_period = 0;
  double period_start_time = 0.0;
  double period_computation_time = 0.0;
  double last_dispatch_time = 0.0;
  std::vector<TaskPeriodData> periods;
};

std::string trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t");
  if (std::string::npos == first)
    return "";
  size_t last = str.find_last_not_of(" \t");
  return str.substr(first, (last - first + 1));
}

std::string extract_task_name(const std::string &task_pid,
                              const std::string &details) {
  std::string combined = task_pid + " " + details;
  size_t pos = combined.find("Activity_");
  if (pos == std::string::npos)
    return "";
  size_t end = pos + 9;
  while (end < combined.length() && isdigit(combined[end])) {
    end++;
  }
  return combined.substr(pos, end - pos);
}

bool goes_to_sleep(const std::string &details) {
  return (details.find(" S ==>") != std::string::npos ||
          details.find(" D ==>") != std::string::npos ||
          details.find(" X ==>") != std::string::npos ||
          details.find(" [S] ==>") != std::string::npos ||
          details.find(" [D] ==>") != std::string::npos);
}

int main(int argc, char *argv[]) {
  // Leggi da std::cin per default (o pipe live), o da file se passato come
  // argomento
  std::istream *input_stream = &std::cin;
  std::ifstream file_stream;

  if (argc > 1) {
    file_stream.open(argv[1]);
    if (!file_stream.is_open()) {
      std::cerr << "Errore: impossibile aprire il file " << argv[1] << "!"
                << std::endl;
      return 1;
    }
    input_stream = &file_stream;
    std::cout << "\n[!] Lettura batch da file: " << argv[1] << "\n\n";
  } else {
    std::cout << "\n[!] In attesa dello stream "
                 "/sys/kernel/debug/tracing/trace_pipe...\n\n";
  }

  std::map<std::string, TaskState> tasks;
  std::string line;

  std::cout
      << "================== SCHEDULATORE CRONOLOGICO ==================\n";
  std::cout << std::left << std::setw(15) << "TEMPO (s)" << " | " << "EVENTO"
            << "\n";
  std::cout
      << "--------------------------------------------------------------\n";

  while (std::getline(*input_stream, line)) {
    if (line.empty())
      continue;

    std::stringstream ss(line);
    std::string token;
    TraceRecord rec;

    // Formato basato su awk trace_colonne.txt
    // $1 | $2 | $3 | $4 | $5...
    if (!std::getline(ss, token, '|'))
      continue;
    rec.task_pid = trim(token);

    if (!std::getline(ss, token, '|'))
      continue;
    rec.cpu = trim(token);

    if (!std::getline(ss, token, '|'))
      continue;
    try {
      rec.timestamp = std::stod(trim(token));
    } catch (...) {
      rec.timestamp = 0.0;
    }

    if (!std::getline(ss, token, '|'))
      continue;
    rec.event = trim(token);

    if (!std::getline(ss, token, '|'))
      continue;
    rec.details = trim(token);

    std::string task_name = extract_task_name(rec.task_pid, rec.details);
    if (task_name.empty())
      continue;

    TaskState &state = tasks[task_name];

    if (rec.event.find("sched_wakeup") != std::string::npos) {
      // WAKEUP EVENT: Il task viene "rilasciato" dal timer/OS
      if (state.period_start_time == 0.0) {
        state.period_start_time = rec.timestamp;
        state.current_period++;
        std::cout << std::fixed << std::setprecision(6) << std::setw(15)
                  << rec.timestamp << " | [WAKEUP]   Task " << task_name
                  << " Rilasciato (Inizio Percorso " << state.current_period
                  << ")" << std::endl;
      }
    } else if (rec.event.find("sched_switch") != std::string::npos) {
      // Se task_name è nella destinazione (a destra della freccia `==>`)
      // In una stringa come `prev_comm=... ==> next_comm=Activity_1`
      size_t pos_name = rec.details.find(task_name);
      size_t pos_arrow = rec.details.find("==>");

      bool is_entering =
          (pos_name != std::string::npos && pos_arrow != std::string::npos &&
           pos_name > pos_arrow);
      bool is_leaving =
          (rec.task_pid.find(task_name) != std::string::npos) ||
          (pos_name != std::string::npos && pos_arrow != std::string::npos &&
           pos_name < pos_arrow);

      if (is_entering) {
        state.last_dispatch_time = rec.timestamp;
        if (state.period_start_time == 0.0) {
          state.period_start_time = rec.timestamp;
          state.current_period++;
        }
        std::cout << std::fixed << std::setprecision(6) << std::setw(15)
                  << rec.timestamp << " | [DISPATCH] Task " << task_name
                  << " prende la CPU" << std::endl;
      } else if (is_leaving && state.last_dispatch_time > 0.0) {
        double elapsed = rec.timestamp - state.last_dispatch_time;
        if (elapsed > 0) {
          state.period_computation_time += elapsed;
        }
        state.last_dispatch_time = 0.0;

        if (goes_to_sleep(rec.details)) {
          std::cout << std::fixed << std::setprecision(6) << std::setw(15)
                    << rec.timestamp << " | [SLEEP]    Task " << task_name
                    << " ha TERMINATO l'esecuzione! Costo: "
                    << std::setprecision(6)
                    << (state.period_computation_time * 1000.0) << " ms"
                    << std::endl;

          state.periods.push_back(
              {state.current_period, state.period_computation_time * 1000.0});
          state.period_computation_time = 0.0;
          state.period_start_time = 0.0;
        } else {
          std::cout << std::fixed << std::setprecision(6) << std::setw(15)
                    << rec.timestamp << " | [PREEMPT]  Task " << task_name
                    << " interrotto temporaneamente (Costo accumulato: "
                    << std::setprecision(6)
                    << (state.period_computation_time * 1000.0) << " ms)"
                    << std::endl;
        }
      }
    }
  }

  if (file_stream.is_open())
    file_stream.close();

  std::cout
      << "==============================================================\n\n";

  // Stampa in stile tabella riassuntiva dinamicamente per N task processati
  std::cout << "================ RISULTATI COMPUTAZIONALI PER " << tasks.size()
            << " THREAD ================\n";
  for (const auto &pair : tasks) {
    std::cout << "\n--- SCHEDA RIASSUNTIVA: " << pair.first << " ---\n";
    std::cout << std::left << std::setw(10) << "PERIODO" << " | "
              << "COSTO COMPUTAZIONALE" << "\n";
    std::cout << "--------------------------------------------\n";
    for (const auto &p : pair.second.periods) {
      std::cout << std::left << std::setw(10) << p.period_id << " | "
                << std::right << std::setw(12) << std::fixed
                << std::setprecision(6) << p.computation_time_ms << " ms\n";
    }
  }
  std::cout << "\n============================================================="
               "===========\n\n";

  return 0;
}
