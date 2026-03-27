#include <iostream>
#include <fstream>
#include <regex>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

struct Interval {
    double start;
    double end;
};

struct OutEvent {
    double time;
    char state;
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <trace.txt> <PID> <period_us>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    int target_pid = std::stoi(argv[2]);
    double expected_period = std::stod(argv[3]) / 1e6; // us → sec

    // LA REGEX AGGIORNATA PER IL TUO FORMATO TRACE-CMD
    std::regex re(R"((\d+\.\d+):\s+sched_switch:.*?:(\d+)\s+\[\d+\].*?([A-Z]).*?==>.*?:(\d+)\s+\[)");

    std::string line;

    std::vector<double> switch_in;
    std::vector<OutEvent> switch_out;

    while (std::getline(file, line)) {
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            double t = std::stod(m[1]);
            int prev_pid = std::stoi(m[2]);
            char state = m[3].str()[0];
            int next_pid = std::stoi(m[4]);

            if (next_pid == target_pid)
                switch_in.push_back(t);

            if (prev_pid == target_pid)
                switch_out.push_back({t, state});
        }
    }

    // Pair execution intervals
    std::vector<Interval> run;
    size_t i = 0, j = 0;

    while (i < switch_in.size() && j < switch_out.size()) {
        if (switch_out[j].time > switch_in[i]) {
            run.push_back({switch_in[i], switch_out[j].time});
            i++; j++;
        } else j++;
    }

    if (run.empty()) {
        std::cerr << "No execution intervals found\n";
        return 1;
    }

    // --- Runtime stats ---
    double wcet = 0, sum_rt = 0;
    for (auto &r : run) {
        double d = r.end - r.start;
        sum_rt += d;
        wcet = std::max(wcet, d);
    }
    double avg_rt = sum_rt / run.size();

    // --- Period stats ---
    std::vector<double> periods;
    for (size_t k = 1; k < run.size(); k++)
        periods.push_back(run[k].start - run[k-1].start);

    double avg_period = 0, jitter = 0;
    if (!periods.empty()) {
        for (auto p : periods) avg_period += p;
        avg_period /= periods.size();

        for (auto p : periods)
            jitter += (p - avg_period) * (p - avg_period);

        jitter = std::sqrt(jitter / periods.size());
    }

    // --- Deadline misses ---
    int misses = 0;
    for (size_t k = 0; k + 1 < run.size(); k++) {
        double deadline = run[k].start + expected_period;
        if (run[k].end > deadline)
            misses++;
    }

    // --- Classify non-running intervals ---
    std::vector<Interval> sleep_int, preempt_int;

    for (size_t k = 0; k < run.size() - 1 && k < switch_out.size(); k++) {
        double gap_start = run[k].end;
        double gap_end   = run[k+1].start;

        if (gap_end <= gap_start) continue;

        char state = switch_out[k].state;

        if (state == 'S' || state == 'D' || state == 'Z' || state == 'X')
            sleep_int.push_back({gap_start, gap_end});
        else if (state == 'R')
            preempt_int.push_back({gap_start, gap_end});
    }

    auto compute_stats = [](const std::vector<Interval>& v) {
        double max = 0, sum = 0;
        for (auto &i : v) {
            double d = i.end - i.start;
            sum += d;
            if (d > max) max = d;
        }
        return std::make_tuple(max, v.empty()?0:sum/v.size());
    };

    auto [max_sleep, avg_sleep] = compute_stats(sleep_int);
    auto [max_preempt, avg_preempt] = compute_stats(preempt_int);

    // --- CSV export ---
    std::ofstream csv("timeline.csv");
    csv << "type,start,end,duration_us\n";

    for (auto &r : run)
        csv << "RUN," << std::fixed << std::setprecision(6) << r.start << "," << r.end << ","
            << (r.end - r.start)*1e6 << "\n";

    for (auto &s : sleep_int)
        csv << "SLEEP," << std::fixed << std::setprecision(6) << s.start << "," << s.end << ","
            << (s.end - s.start)*1e6 << "\n";

    for (auto &p : preempt_int)
        csv << "PREEMPT," << std::fixed << std::setprecision(6) << p.start << "," << p.end << ","
            << (p.end - p.start)*1e6 << "\n";

    csv.close();

    // --- Output ---
    std::cout << std::fixed << std::setprecision(3);

    std::cout << "\n=== Analisi per PID " << target_pid << " ===\n";
    std::cout << "=== Runtime ===\n";
    std::cout << "WCET: " << wcet*1e6 << " us\n";
    std::cout << "Avg : " << avg_rt*1e6 << " us\n";

    std::cout << "\n=== Period ===\n";
    std::cout << "Avg period: " << avg_period*1e6 << " us\n";
    std::cout << "Jitter    : " << jitter*1e6 << " us\n";

    std::cout << "\n=== Deadline ===\n";
    std::cout << "Misses: " << misses << "\n";

    std::cout << "\n=== Sleep ===\n";
    std::cout << "Count: " << sleep_int.size()
              << " Avg: " << avg_sleep*1e6
              << " us Max: " << max_sleep*1e6 << " us\n";

    std::cout << "\n=== Preemption ===\n";
    std::cout << "Count: " << preempt_int.size()
              << " Avg: " << avg_preempt*1e6
              << " us Max: " << max_preempt*1e6 << " us\n";

    std::cout << "\nTimeline exported to timeline.csv\n";

    return 0;
}
