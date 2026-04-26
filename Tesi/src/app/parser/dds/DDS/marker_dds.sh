#!/bin/bash

# =============================================================================
#  marker_dds.sh — Traccia DDS con PID di processi GIÀ AVVIATI
#
#  Uso (da dds/DDS/ o da qualsiasi directory):
#    sudo bash marker_dds.sh                              # chiede interattivamente
#    sudo bash marker_dds.sh ImageSender ImageSubscriber  # entrambi i processi
#    sudo bash marker_dds.sh ImageSubscriber              # solo uno
#
#  Metodo del Prof: avvia prima i processi nei loro terminali, poi lancia questo.
#  Premi Ctrl+C per fermare la registrazione quando hai finito.
# =============================================================================

# --- 0. Setup paths ----------------------------------------------------------
DDS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARSER_ROOT="$(cd "$DDS_ROOT/../.." && pwd)"
BASE_DIR="$PWD"

# Carica SERL_HOME + LD_LIBRARY_PATH
source "$DDS_ROOT/environment.sh"

# --- 1. Scegli i processi da tracciare ---------------------------------------
if [ "$#" -eq 0 ]; then
    echo "Processi DDS disponibili in bin/components/:"
    ls "$DDS_ROOT/bin/components/" 2>/dev/null | grep -v '\.sh' || echo "  (nessuno)"
    echo ""
    read -p "Inserisci il PRIMO eseguibile (già avviato, es. ImageSender): " EXECUTABLE
    read -p "[Opzionale] SECONDO eseguibile (es. ImageSubscriber, lascia vuoto per skip): " SEC_EXECUTABLE
else
    EXECUTABLE=$1
    SEC_EXECUTABLE=${2:-}
fi

APP_NAME=$(basename "$EXECUTABLE")

# --- 2. Funzione: aggiungi PID + tutti i suoi thread -------------------------
PIDS_CMD=""

aggiungi_pid_e_thread() {
    local process_name=$1
    local pids
    pids=$(pidof "$process_name")

    if [ -z "$pids" ]; then
        echo "ERRORE: Il processo '$process_name' non è in esecuzione!"
        echo "  → Avvialo prima in un altro terminale:"
        echo "      source $DDS_ROOT/environment.sh"
        echo "      $DDS_ROOT/bin/components/$process_name"
        return 1
    fi

    for p in $pids; do
        PIDS_CMD="$PIDS_CMD -P $p"
        echo ">> Trovato processo: $process_name (PID: $p)"

        # Aggiungi tutti i thread interni (dds.enc, dds.async, ecc.)
        if [ -d "/proc/$p/task" ]; then
            for tid_path in /proc/$p/task/*; do
                tid=$(basename "$tid_path")
                if [ "$tid" != "$p" ]; then
                    PIDS_CMD="$PIDS_CMD -P $tid"
                    thread_name=$(cat "$tid_path/comm" 2>/dev/null)
                    if [[ "$thread_name" == *"dds"* ]]; then
                        echo "   -> Thread DDS: $thread_name (TID: $tid)"
                    else
                        echo "   -> Thread: $thread_name (TID: $tid)"
                    fi
                fi
            done
        fi
    done
    return 0
}

# Primo processo (obbligatorio)
aggiungi_pid_e_thread "$APP_NAME" || exit 1

# Secondo processo (opzionale)
if [ -n "$SEC_EXECUTABLE" ]; then
    SEC_APP_NAME=$(basename "$SEC_EXECUTABLE")
    echo "---------------------------------------------------------"
    if aggiungi_pid_e_thread "$SEC_APP_NAME"; then
        echo ">> Tracciamento congiunto $APP_NAME + $SEC_APP_NAME pronto."
        TRACE_NAME="${APP_NAME}_${SEC_APP_NAME}"
    else
        echo ">> Traccio solo $APP_NAME."
        TRACE_NAME="$APP_NAME"
    fi
else
    echo ">> Tracciamento singolo: $APP_NAME"
    TRACE_NAME="$APP_NAME"
fi

# --- 3. Cerca il sorgente .cpp per thread_analysis ---------------------------
find_src() {
    local name=$1
    for candidate in \
        "$DDS_ROOT/src/components/SendImage/${name}.cpp" \
        "$DDS_ROOT/src/components/SendImage/${name}.c" \
        "$DDS_ROOT/src/runtime/${name}.cpp"; do
        [ -f "$candidate" ] && echo "$candidate" && return
    done
    # Fallback ricorsivo
    find "$DDS_ROOT/src" -type f \( -name "${name}.cpp" -o -name "${name}.c" \) \
        2>/dev/null | head -n 1
}

SRC_FILE=$(find_src "$APP_NAME")
if [ -z "$SRC_FILE" ] && [ -n "$SEC_EXECUTABLE" ]; then
    SRC_FILE=$(find_src "$SEC_APP_NAME")
fi

if [ -z "$SRC_FILE" ]; then
    echo "ATTENZIONE: Nessun sorgente .cpp trovato. thread_analysis salterà l'analisi dei periodi."
    SRC_FILE_ABS=""
else
    SRC_FILE_ABS="$SRC_FILE"
    echo " Sorgente: $SRC_FILE_ABS"
fi

MONITOR_SCRIPT_ABS="$PARSER_ROOT/monitorRealTime.py"
THREAD_ANALYSIS_CPP="$PARSER_ROOT/thread_analysis.cpp"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="$DDS_ROOT/output/traces/trace_results_${TRACE_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Tracciamento DDS: $TRACE_NAME"
echo " Output: $OUTPUT_DIR"
echo "========================================================="

mkdir -p "$OUTPUT_DIR"

# --- 4. Trace-cmd record (ferma con Ctrl+C) -----------------------------------
echo "[1/6] Registrazione trace-cmd... (Ctrl+C per fermare)"
sudo trace-cmd record \
    -e sched:sched_switch \
    -e sched:sched_wakeup \
    $PIDS_CMD \
    -o "$OUTPUT_DIR/trace.dat"

if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: trace.dat non creato."
    exit 1
fi

sudo chown -R "$(id -u):$(id -g)" "$OUTPUT_DIR"
chmod -R u+rwX "$OUTPUT_DIR"

# --- 5. Report testuale -------------------------------------------------------
echo "[2/6] Generazione report testuale..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

# --- 6. Compila thread_analysis -----------------------------------------------
echo "[3/6] Compilazione analizzatore C++..."
PARSER_EXE="$OUTPUT_DIR/thread_analysis"
if [ -f "$THREAD_ANALYSIS_CPP" ]; then
    g++ -O2 -std=c++17 "$THREAD_ANALYSIS_CPP" -o "$PARSER_EXE"
else
    echo "ATTENZIONE: $THREAD_ANALYSIS_CPP non trovato!"
    PARSER_EXE=""
fi

# --- 7. Analisi thread --------------------------------------------------------
echo "[4/6] Analisi thread..."
if [ -n "$PARSER_EXE" ]; then
    cd "$OUTPUT_DIR" || exit
    ./thread_analysis "trace_output.txt" "${SRC_FILE_ABS}" "ALL" > "risultati_finali.txt"
    echo ""
    cat "risultati_finali.txt"
    cd "$DDS_ROOT"
fi

# --- 8. Monitor Python --------------------------------------------------------
echo ""
echo "[5/6] Avvio monitor visivo Python..."
if [ -f "$MONITOR_SCRIPT_ABS" ]; then
    cd "$OUTPUT_DIR" || exit
    python3 "$MONITOR_SCRIPT_ABS"
    cd "$DDS_ROOT"
else
    echo "ATTENZIONE: $MONITOR_SCRIPT_ABS non trovato."
fi

echo "========================================================="
echo " Tracciamento completato! Dati in: $OUTPUT_DIR"
echo "========================================================="

# --- 9. KernelShark ----------------------------------------------------------
echo "[6/6] Avvio KernelShark..."
if command -v kernelshark >/dev/null 2>&1; then
    (cd "$OUTPUT_DIR" && kernelshark trace.dat) &
else
    echo "ATTENZIONE: KernelShark non trovato nel PATH."
fi
