#include <algorithm>
#include <csignal>
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

struct SimulationData {
  double timestamp;
  std::string task_name;
  std::string event_type;
  std::string message;
  double computation_cost;
};

struct TaskState {
  int current_period = 0;
  double period_start_time = 0.0;
  double period_computation_time = 0.0;
  double last_dispatch_time = 0.0;
  std::vector<TaskPeriodData> periods;
};

std::vector<SimulationData> global_sim_records;
std::map<std::string, TaskState> global_tasks;

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

void print_single_event(const SimulationData &sim) {
  if (sim.event_type == "PERIOD_START") {
    std::cout
        << "\n..............................................................\n";
    std::cout << std::fixed << std::setprecision(6) << std::setw(15)
              << sim.timestamp << " | [NUOVO PERIODO] Task " << sim.task_name
              << " " << sim.message << std::endl;
  } else if (sim.event_type == "COMPUTATION_END") {
    std::cout << std::fixed << std::setprecision(6) << std::setw(15)
              << sim.timestamp << " | [FINE LOGICA]   Task " << sim.task_name
              << " completa l'attivita' computazionale." << std::endl;
  } else if (sim.event_type == "DEADLINE_MISS") {
    std::cout
        << "\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
    std::cout << std::fixed << std::setprecision(6) << std::setw(15)
              << sim.timestamp << " | [DEADLINE MISS] Task " << sim.task_name
              << " !!! HA BUCATO LA SCADENZA !!!" << std::endl;
    std::cout
        << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n";
  } else if (sim.event_type == "WAKEUP") {
    std::cout << std::fixed << std::setprecision(6) << std::setw(15)
              << sim.timestamp << " | [WAKEUP SCED.]  Task " << sim.task_name
              << " viene svegliato dal kernel." << std::endl;
  } else if (sim.event_type == "DISPATCH") {
    std::cout << std::fixed << std::setprecision(6) << std::setw(15)
              << sim.timestamp << " | [CPU DISPATCH]  Task " << sim.task_name
              << " entra in esecuzione fisica." << std::endl;
  } else if (sim.event_type == "PREEMPT") {
    std::cout << std::fixed << std::setprecision(6) << std::setw(15)
              << sim.timestamp << " | [PREEMPT]       Task " << sim.task_name
              << " interrotto temporaneamente." << std::endl;
  } else if (sim.event_type == "SLEEP") {
    std::cout << std::fixed << std::setprecision(6) << std::setw(15)
              << sim.timestamp << " | [SLEEP SCED.]   Task " << sim.task_name
              << " va a riposo. Costo Reale: " << std::setprecision(6)
              << sim.computation_cost << " ms" << std::endl;
  }
}

void print_summary() {
  std::cout
      << "\n==============================================================\n\n";
  std::cout << "================ RISULTATI COMPUTAZIONALI PER "
            << global_tasks.size() << " THREAD ================\n";

  for (const auto &pair : global_tasks) {
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
}

void signal_handler(int signum) {
  if (signum == SIGINT) {
    std::cout << "\n\n[!] Chiusura Live del Parser intercettata... Generazione "
                 "tabelle...\n";
    print_summary();
    exit(0);
  }
}

int main(int argc, char *argv[]) {
  std::istream *input_stream = &std::cin;
  std::ifstream file_stream;

  if (argc > 1) {
    file_stream.open(argv[1]);
    if (!file_stream.is_open())
      return 1;
    input_stream = &file_stream;
  } else {
    std::cout << "\n[!] Stream Eventi Live in esecuzione...\n\n";
    std::cout
        << "================== SCHEDULATORE CRONOLOGICO ==================\n";
    std::cout << std::left << std::setw(15) << "TEMPO (s)" << " | "
              << "EVENTO\n";
    std::cout << "-------------------------------------------------------------"
                 "--------------\n";
  }

  signal(SIGINT, signal_handler);
  std::string line;

  while (std::getline(*input_stream, line)) {
    if (line.empty())
      continue;

    std::stringstream ss(line);
    std::string token;
    TraceRecord rec;

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

    TaskState &state = global_tasks[task_name];

    if (rec.details.find("=== PERIOD_START:") != std::string::npos) {
      size_t p_start = rec.details.find("(Period=");
      size_t p_end = rec.details.find(")", p_start);
      std::string params = "";
      if (p_start != std::string::npos && p_end != std::string::npos) {
        params = rec.details.substr(p_start + 1, p_end - p_start - 1);
      }
      global_sim_records.push_back({rec.timestamp, task_name, "PERIOD_START",
                                    "Inizia [" + params + "]", 0.0});
      print_single_event(global_sim_records.back());
      continue;
    } else if (rec.details.find("--- COMPUTATION_END:") != std::string::npos) {
      global_sim_records.push_back(
          {rec.timestamp, task_name, "COMPUTATION_END", "", 0.0});
      print_single_event(global_sim_records.back());
      continue;
    } else if (rec.details.find("!!! DEADLINE_MISS:") != std::string::npos) {
      global_sim_records.push_back(
          {rec.timestamp, task_name, "DEADLINE_MISS", "", 0.0});
      print_single_event(global_sim_records.back());
      continue;
    }

    if (rec.event.find("sched_wakeup") != std::string::npos) {
      if (state.period_start_time == 0.0) {
        state.period_start_time = rec.timestamp;
        state.current_period++;
      }
      global_sim_records.push_back(
          {rec.timestamp, task_name, "WAKEUP", "", 0.0});
      print_single_event(global_sim_records.back());
    } else if (rec.event.find("sched_switch") != std::string::npos) {
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
        global_sim_records.push_back(
            {rec.timestamp, task_name, "DISPATCH", "", 0.0});
        print_single_event(global_sim_records.back());
      } else if (is_leaving && state.last_dispatch_time > 0.0) {
        double elapsed = rec.timestamp - state.last_dispatch_time;
        if (elapsed > 0) {
          state.period_computation_time += elapsed;
        }
        state.last_dispatch_time = 0.0;

        if (goes_to_sleep(rec.details)) {
          double final_cost = state.period_computation_time * 1000.0;
          global_sim_records.push_back(
              {rec.timestamp, task_name, "SLEEP", "", final_cost});
          print_single_event(global_sim_records.back());

          state.periods.push_back({state.current_period, final_cost});
          state.period_computation_time = 0.0;
          state.period_start_time = 0.0;
        } else {
          global_sim_records.push_back(
              {rec.timestamp, task_name, "PREEMPT", "", 0.0});
          print_single_event(global_sim_records.back());
        }
      }
    }
  }

  if (file_stream.is_open())
    file_stream.close();

  print_summary();

  return 0;
}
