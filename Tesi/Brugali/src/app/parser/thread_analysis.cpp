#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
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
  double expected_period_sec = 0.0;
  double expected_deadline_sec = 0.0;
  std::vector<double> switch_in;
  std::vector<OutEvent> switch_out;
  std::vector<Interval> run;
  std::vector<Interval> sleep_int;
  std::vector<Interval> preempt_int;
  std::vector<std::pair<double, std::string>> markers;
};

struct MissLogEntry {
  std::string task;
  int period_idx;
  double ps_abs;
  double ps_rel;
  double deadline_ms;
  double response_ms;
  double overshoot_ms;
};

std::string trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t");
  if (std::string::npos == first)
    return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

// Somma delle durate di intervalli che si sovrappongono a [lo, hi]
double state_time_in(const std::vector<Interval> &intervals, double lo,
                     double hi) {
  double total = 0;
  for (const auto &iv : intervals) {
    double overlap_start = std::max(iv.start, lo);
    double overlap_end = std::min(iv.end, hi);
    if (overlap_end > overlap_start)
      total += (overlap_end - overlap_start);
  }
  return total;
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
  std::map<std::string, double> expected_periods;
  std::map<std::string, double> expected_deadlines;

  // =========================================================================
  // FASE 1: LETTURA DEL FILE SORGENTE C PER ESTRARRE PERIODI E DEADLINE
  // =========================================================================
  std::ifstream src_file(source_file_path);
  if (src_file.is_open()) {
    std::string src_line;
    std::regex name_re(
        R"DELIM(sprintf\s*\(\s*([a-zA-Z0-9_\[\]]+)\.name\s*,\s*"([^"]+)"\s*\))DELIM");
    std::regex period_re(
        R"DELIM(([a-zA-Z0-9_\[\]]+)\.period\s*=\s*(\d+))DELIM");
    std::regex deadline_re(
        R"DELIM(([a-zA-Z0-9_\[\]]+)\.deadline\s*=\s*(\d+))DELIM");

    std::map<std::string, std::string> var_to_name;
    std::map<std::string, double> var_to_period;
    std::map<std::string, double> var_to_deadline;

    while (std::getline(src_file, src_line)) {
      std::smatch m;
      if (std::regex_search(src_line, m, name_re)) {
        var_to_name[m[1]] = m[2];
      }
      if (std::regex_search(src_line, m, period_re)) {
        var_to_period[m[1]] = std::stod(m[2]) / 1000.0;
      }
      if (std::regex_search(src_line, m, deadline_re)) {
        var_to_deadline[m[1]] = std::stod(m[2]) / 1000.0;
      }
    }

    for (const auto &[var_name, act_name] : var_to_name) {
      if (var_to_period.count(var_name))
        expected_periods[act_name] = var_to_period[var_name];
      if (var_to_deadline.count(var_name))
        expected_deadlines[act_name] = var_to_deadline[var_name];
    }
    src_file.close();
  } else {
    std::cerr << "Attenzione: Impossibile aprire " << source_file_path << "\n";
  }

  // =========================================================================
  // FASE 2: PARSING DEL FILE TRACE.TXT  (DISCOVERY DINAMICA)
  // =========================================================================
  std::ifstream file(trace_file_path);
  if (!file.is_open())
    return 1;

  std::regex line_re(
      R"DELIM(^\s*(.*?)-(\d+)\s+\[(\d+)\]\s+(\d+\.\d+):\s+(.*?):\s+(.*)$)DELIM");
  std::regex switch_re(
      R"DELIM(.*?:(\d+)\s+\[\d+\]\s+([A-Z]).*?==>\s+.*?:(\d+)\s+\[)DELIM");

  std::string line;

  // --- PASSO 1: scopriamo TUTTI i PID che scrivono marker (= thread utente) -
  std::map<int, std::string> marker_pids;

  while (std::getline(file, line)) {
    std::smatch m;
    if (std::regex_search(line, m, line_re)) {
      std::string comm = trim(m[1]);
      int pid = std::stoi(m[2]);
      std::string event = trim(m[5]);

      if (event.find("print") != std::string::npos ||
          event.find("tracing_mark_write") != std::string::npos) {
        if (marker_pids.count(pid) == 0)
          marker_pids[pid] = comm;
      }

      // Thread del kernel specifici della CPU 1
      if (comm.find("kworker/1") != std::string::npos ||
          comm.find("ksoftirqd/1") != std::string::npos ||
          comm.find("swapper/1") != std::string::npos) {
        if (tasks.count(pid) == 0)
          tasks[pid].name = comm;
      }
    }
  }

  // --- PASSO 2: registriamo i thread scoperti via marker ---
  for (const auto &[pid, comm] : marker_pids) {
    if (tasks.count(pid))
      continue;

    std::string full_name = comm;
    for (const auto &kv : expected_periods) {
      if (kv.first.find(comm) == 0) {
        full_name = kv.first;
        break;
      }
    }

    tasks[pid].name = full_name;
    if (expected_periods.count(full_name))
      tasks[pid].expected_period_sec = expected_periods[full_name];
    if (expected_deadlines.count(full_name))
      tasks[pid].expected_deadline_sec = expected_deadlines[full_name];
    else if (expected_periods.count(full_name))
      tasks[pid].expected_deadline_sec = expected_periods[full_name];
  }

  file.clear();
  file.seekg(0);

  // --- PASSO 3: estraiamo sched_switch e marker per i PID tracciati ---
  double global_min_time = std::numeric_limits<double>::infinity();
  double global_max_time = 0;

  while (std::getline(file, line)) {
    std::smatch m;
    if (std::regex_search(line, m, line_re)) {
      int pid = std::stoi(m[2]);
      int cpu = std::stoi(m[3]);
      double t = std::stod(m[4]);
      std::string event = trim(m[5]);
      std::string details = m[6];

      if (t < global_min_time)
        global_min_time = t;
      if (t > global_max_time)
        global_max_time = t;

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
  // FASE 3: COSTRUZIONE INTERVALLI + ESPORTAZIONE CSV + STATISTICHE AVANZATE
  // =========================================================================
  std::ofstream csv("timeline.csv");
  csv << "task,type,start,end,duration_ms\n";

  std::vector<MissLogEntry> all_misses;
  double csv_span_s = global_max_time - global_min_time;

  for (auto &pair : tasks) {
    TaskData &tdata = pair.second;

    // --- Costruiamo intervalli RUN ---
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

    // --- Scrivi RUN al CSV ---
    for (auto &r : tdata.run) {
      double d = r.end - r.start;
      csv << tdata.name << ",RUN," << std::fixed << std::setprecision(6)
          << r.start << "," << r.end << "," << d * 1e3 << "\n";
    }

    // --- Costruiamo SLEEP e PREEMPT e scriviamoli al CSV ---
    for (size_t k = 0; k < tdata.run.size() - 1 && k < tdata.switch_out.size();
         k++) {
      double gap_start = tdata.run[k].end;
      double gap_end = tdata.run[k + 1].start;
      if (gap_end <= gap_start)
        continue;

      char state = tdata.switch_out[k].state;
      if (state == 'S' || state == 'D' || state == 'Z' || state == 'X') {
        tdata.sleep_int.push_back({gap_start, gap_end});
        csv << tdata.name << ",SLEEP," << std::fixed << std::setprecision(6)
            << gap_start << "," << gap_end << "," << (gap_end - gap_start) * 1e3
            << "\n";
      } else if (state == 'R') {
        tdata.preempt_int.push_back({gap_start, gap_end});
        csv << tdata.name << ",PREEMPT," << std::fixed << std::setprecision(6)
            << gap_start << "," << gap_end << "," << (gap_end - gap_start) * 1e3
            << "\n";
      }
    }

    // --- Scrivi MARKER al CSV ---
    for (const auto &m : tdata.markers) {
      csv << tdata.name << ",MARKER_" << m.second << "," << std::fixed
          << std::setprecision(6) << m.first << "," << m.first << ",0.0\n";
    }

    // =================================================================
    // CALCOLO STATISTICHE AVANZATE (replica della logica Python)
    // =================================================================

    // Estraiamo PERIOD_START, PERIOD_END, FUNCTION_END dai marker
    std::vector<double> pstart, pend, func_ends;
    for (const auto &m : tdata.markers) {
      if (m.second.find("PERIOD_START") != std::string::npos) {
        pstart.push_back(m.first);
        // Fallback app10e-style inline params
        std::regex inline_p(R"(Period\s*=\s*(\d+))");
        std::regex inline_d(R"(Deadline\s*=\s*(\d+))");
        std::smatch sm;
        if (tdata.expected_period_sec == 0 &&
            std::regex_search(m.second, sm, inline_p))
          tdata.expected_period_sec = std::stod(sm[1]) / 1000.0;
        if (tdata.expected_deadline_sec == 0 &&
            std::regex_search(m.second, sm, inline_d))
          tdata.expected_deadline_sec = std::stod(sm[1]) / 1000.0;
      } else if (m.second.find("PERIOD_END") != std::string::npos) {
        pend.push_back(m.first);
      } else if (m.second.find("FUNCTION_END") != std::string::npos) {
        func_ends.push_back(m.first);
      }
    }

    std::sort(pstart.begin(), pstart.end());
    std::sort(pend.begin(), pend.end());
    std::sort(func_ends.begin(), func_ends.end());

    // --- Measured period (median of PERIOD_START gaps) ---
    double measured_ms = -1;
    double jitter_ms = -1;
    if (pstart.size() >= 2) {
      std::vector<double> gaps;
      for (size_t g = 1; g < pstart.size(); g++)
        gaps.push_back(pstart[g] - pstart[g - 1]);
      std::sort(gaps.begin(), gaps.end());
      measured_ms = gaps[gaps.size() / 2] * 1000.0; // median

      double mean_gap = 0;
      for (double g : gaps)
        mean_gap += g;
      mean_gap /= gaps.size();
      double var = 0;
      for (double g : gaps)
        var += (g - mean_gap) * (g - mean_gap);
      jitter_ms = std::sqrt(var / gaps.size()) * 1000.0;
    }

    double calc_period_sec = tdata.expected_period_sec > 0
                                 ? tdata.expected_period_sec
                                 : (measured_ms > 0 ? measured_ms / 1000.0 : 0);
    double calc_deadline_sec = tdata.expected_deadline_sec > 0
                                   ? tdata.expected_deadline_sec
                                   : calc_period_sec;

    // --- Per-period job instances ---
    double wcet_ms = 0, sum_run_ms = 0;
    int job_count = 0;
    double worst_slack_ms = 1e9;
    int n_misses = 0;

    for (size_t k = 0; k < pstart.size(); k++) {
      double ps = pstart[k];
      double pe = -1;

      // Match this PERIOD_START to the next PERIOD_END
      for (double cand : pend) {
        if (cand > ps) {
          pe = cand;
          break;
        }
      }
      if (pe == -1 && k + 1 < pstart.size())
        pe = pstart[k + 1];

      double pe_upper =
          (pe != -1) ? pe : std::numeric_limits<double>::infinity();

      double inst_run_ms = 0;
      if (pe != -1)
        inst_run_ms = state_time_in(tdata.run, ps, pe) * 1000.0;

      // Find last FUNCTION_END in this period
      double func_end_t = -1;
      for (double fe : func_ends) {
        if (fe > ps && fe < pe_upper)
          func_end_t = fe;
      }

      if (func_end_t != -1) {
        double response_ms = (func_end_t - ps) * 1000.0;
        if (calc_deadline_sec > 0) {
          double slack_ms = (calc_deadline_sec * 1000.0) - response_ms;
          worst_slack_ms = std::min(worst_slack_ms, slack_ms);
          if (slack_ms < 0) {
            n_misses++;
            MissLogEntry mle;
            mle.task = tdata.name;
            mle.period_idx = k + 1;
            mle.ps_abs = ps;
            mle.ps_rel = ps - global_min_time;
            mle.deadline_ms = calc_deadline_sec * 1000.0;
            mle.response_ms = response_ms;
            mle.overshoot_ms = -slack_ms;
            all_misses.push_back(mle);
          }
        }
      }

      if (inst_run_ms > 0) {
        wcet_ms = std::max(wcet_ms, inst_run_ms);
        sum_run_ms += inst_run_ms;
        job_count++;
      }
    }

    double acet_ms = job_count > 0 ? (sum_run_ms / job_count) : 0;

    // Fallback: se non ci sono marker PERIOD_START, usa i singoli RUN
    if (job_count == 0 && !tdata.run.empty()) {
      for (const auto &r : tdata.run) {
        double d = (r.end - r.start) * 1e3;
        wcet_ms = std::max(wcet_ms, d);
        sum_run_ms += d;
      }
      acet_ms = sum_run_ms / tdata.run.size();
    }

    // --- Totali globali ---
    double total_run_ms = 0;
    for (const auto &r : tdata.run)
      total_run_ms += (r.end - r.start) * 1000.0;
    double total_slp_ms = 0;
    for (const auto &r : tdata.sleep_int)
      total_slp_ms += (r.end - r.start) * 1000.0;
    double total_pre_ms = 0;
    for (const auto &r : tdata.preempt_int)
      total_pre_ms += (r.end - r.start) * 1000.0;

    // --- Utilization ---
    double util = 0;
    if (calc_period_sec > 0 && pstart.size() >= 2) {
      double denom = (pstart.size() - 1) * calc_period_sec;
      if (denom > 0)
        util = (total_run_ms / 1000.0) / denom;
    } else {
      if (csv_span_s > 0)
        util = (total_run_ms / 1000.0) / csv_span_s;
    }

    // --- Gaps ---
    bool has_gaps = false;
    double max_gap_ms = 0;
    int n_gaps = 0;
    if (tdata.run.size() > 1) {
      for (size_t g = 1; g < tdata.run.size(); g++) {
        double gap = tdata.run[g].start - tdata.run[g - 1].end;
        if (gap > 1e-6) {
          has_gaps = true;
          max_gap_ms = std::max(max_gap_ms, gap * 1000.0);
          n_gaps++;
        }
      }
    }

    double w_slack = (worst_slack_ms >= 1e9) ? -999999 : worst_slack_ms;

    // --- Emetti righe STAT_ nel CSV ---
    csv << std::fixed << std::setprecision(6);
    csv << tdata.name << ",STAT_PERIOD,0,0," << (calc_period_sec * 1000.0)
        << "\n";
    csv << tdata.name << ",STAT_MEASURED,0,0,"
        << (measured_ms > 0 ? measured_ms : -1) << "\n";
    csv << tdata.name << ",STAT_DEADLINE,0,0," << (calc_deadline_sec * 1000.0)
        << "\n";
    csv << tdata.name << ",STAT_WCET,0,0," << wcet_ms << "\n";
    csv << tdata.name << ",STAT_ACET,0,0," << acet_ms << "\n";
    csv << tdata.name << ",STAT_JITTER,0,0," << (jitter_ms > 0 ? jitter_ms : -1)
        << "\n";
    csv << tdata.name << ",STAT_UTIL,0,0," << util << "\n";
    csv << tdata.name << ",STAT_NRUN,0,0," << tdata.run.size() << "\n";
    csv << tdata.name << ",STAT_NPRE,0,0," << tdata.preempt_int.size() << "\n";
    csv << tdata.name << ",STAT_NSLP,0,0," << tdata.sleep_int.size() << "\n";
    csv << tdata.name << ",STAT_NMISS,0,0," << n_misses << "\n";
    csv << tdata.name << ",STAT_WSLACK,0,0," << w_slack << "\n";
    csv << tdata.name << ",STAT_HGAPS,0,0," << (has_gaps ? 1.0 : 0.0) << "\n";
    csv << tdata.name << ",STAT_NGAPS,0,0," << n_gaps << "\n";
    csv << tdata.name << ",STAT_MAXGAP,0,0," << max_gap_ms << "\n";
    csv << tdata.name << ",STAT_RUNTOT,0,0," << total_run_ms << "\n";
    csv << tdata.name << ",STAT_PRETOT,0,0," << total_pre_ms << "\n";
    csv << tdata.name << ",STAT_SLPTOT,0,0," << total_slp_ms << "\n";
  }

  // --- Emetti righe MISS_LOG ---
  for (const auto &mle : all_misses) {
    csv << mle.task << ",MISS_LOG," << std::fixed << std::setprecision(6)
        << mle.ps_abs << "," << mle.ps_rel << "," << mle.deadline_ms << "|"
        << mle.response_ms << "|" << mle.overshoot_ms << "|" << mle.period_idx
        << "\n";
  }

  csv.close();
  std::cout << "\n[+] Dati esportati su timeline.csv\n";
  return 0;
}
