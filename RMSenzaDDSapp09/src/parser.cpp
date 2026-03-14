#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

//struct richiesta dal prof
struct TraceRecord {
    std::string task_pid;
    std::string cpu;
    double timestamp;
    std::string event;
    std::string details;
};

// Funzione di utilità per rimuovere gli spazi vuoti all'inizio e alla fine delle stringhe
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

int main() {
    // vettore in cui contengo tutto quanto
    std::vector<TraceRecord> records;

    // Apertura del file
    std::ifstream file("../build/trace_colonne.txt");
    if (!file.is_open()) {
        std::cerr << "Errore: impossibile aprire il file trace_colonne.txt!" << std::endl;
        return 1;
    }

    std::string line;
    int line_count = 0;

    std::cout << "Lettura del file in corso..." << std::endl;

    // 3. Lettura riga per riga
    while (std::getline(file, line)) {
        // Salta le righe vuote
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        TraceRecord rec;

        // 4. Estrazione dei 5 campi separati dal simbolo '|'
        // messo in questo modo in base alla funzione awk messa nella libreria PTHREAD_lIBRARY
        if (std::getline(ss, token, '|')) rec.task_pid = trim(token);
        if (std::getline(ss, token, '|')) rec.cpu = trim(token);
        if (std::getline(ss, token, '|')) {
            try {
                rec.timestamp = std::stod(trim(token)); // Converte la stringa in numero
            } catch (const std::exception& e) {
                rec.timestamp = 0.0; // Valore di fallback in caso di errore di conversione
            }
        }
        if (std::getline(ss, token, '|')) rec.event = trim(token);
        if (std::getline(ss, token, '|')) rec.details = trim(token);

        // 5. Inserimento dell'istanza nel vettore
        records.push_back(rec);
        line_count++;
    }

    file.close();

    // --- Verifica del caricamento ---
    std::cout << "Lettura completata. Lette " << records.size() << " righe." << std::endl;


    for (size_t i = 0; i <records.size(); ++i) {
        std::cout << "Record " << i+1 << ":\n"
                  << "  Task/PID : " << records[i].task_pid << "\n"
                  << "  CPU      : " << records[i].cpu << "\n"
                  << "  Tempo    : " << std::fixed << records[i].timestamp << "\n"
                  << "  Evento   : " << records[i].event << "\n"
                  << "  Dettagli : " << records[i].details << "\n"
                  << "----------------------------------------" << std::endl;
    }

    return 0;
}
