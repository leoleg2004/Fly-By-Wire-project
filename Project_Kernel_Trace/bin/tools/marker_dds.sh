#!/bin/bash
# marker_dds.sh - Estrazione TID e tracciamento passivo DDS (bin/tools/ version)

# Salviamo la cartella principale del progetto
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Se siamo già root (es. dashboard), non serve sudo
if [ "$EUID" -eq 0 ]; then
    SUDO_CMD=""
else
    SUDO_CMD="sudo"
fi
BASE_DIR="$PROJECT_ROOT"

# Funzione per risolvere alias dei nomi
resolve_app_name() {
    case $1 in
        GeometryBroadcastner) echo "Broadcastner" ;;
        GeometryListener)    echo "Listener" ;;
        *)                    echo "$1" ;;
    esac
}

# 1. Scegli i processi da tracciare
if [ "$#" -eq 0 ]; then
    read -p "Inserisci il PRIMO eseguibile DDS (già avviato, es. Broadcastner): " RAW_NAME
    read -p "[Opzionale] SECONDO eseguibile DDS (es. Listener): " RAW_SEC_NAME
else
    RAW_NAME=$1
    RAW_SEC_NAME=$2
fi

EXECUTABLE_NAME=$(resolve_app_name "$RAW_NAME")
APP_NAME=$(basename "$EXECUTABLE_NAME")

#  2. Funzione: aggiungi PID + tutti i suoi thread -
PIDS_CMD=""

aggiungi_pid_e_thread() {
    local process_name=$1
    local pids
    pids=$(pidof "$process_name")

    if [ -z "$pids" ]; then
        echo "ERRORE: Il processo '$process_name' non è in esecuzione!"
        echo "  Avvialo prima con: sudo ./$process_name"
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

# Primo processo 
aggiungi_pid_e_thread "$APP_NAME" || exit 1

# Secondo processo (opzionale)
if [ -n "$RAW_SEC_NAME" ]; then
    SEC_EXECUTABLE_NAME=$(resolve_app_name "$RAW_SEC_NAME")
    SEC_APP_NAME=$(basename "$SEC_EXECUTABLE_NAME")
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
    # Priorità a src/app/
    local candidate=$(find "$PROJECT_ROOT/src/app" -name "${name}.cpp" -o -name "${name}.c" | head -n 1)
    if [ -n "$candidate" ]; then
        echo "$candidate"
    else
        # Cerca ovunque in src/
        find "$PROJECT_ROOT/src" -name "${name}.cpp" -o -name "${name}.c" | head -n 1
    fi
}

SRC_FILE=$(find_src "$APP_NAME")
if [ -z "$SRC_FILE" ] && [ -n "$SEC_APP_NAME" ]; then
    SRC_FILE=$(find_src "$SEC_APP_NAME")
fi

if [ -z "$SRC_FILE" ]; then
    echo "ATTENZIONE: Nessun sorgente trovato. thread_analysis salterà l'analisi dei periodi."
    SRC_FILE_ABS=""
else
    SRC_FILE_ABS="$SRC_FILE"
fi

MONITOR_SCRIPT_ABS="$PROJECT_ROOT/src/tools/monitor.py"
PARSER_SRC="$PROJECT_ROOT/src/tools/thread_analysis.cpp"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="$PROJECT_ROOT/bin/Test/trace_marker_dds_${TRACE_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Tracciamento DDS: $TRACE_NAME"
echo " Sorgente: $SRC_FILE_ABS"
echo " Output: $OUTPUT_DIR"
echo "========================================================="

mkdir -p "$OUTPUT_DIR"

# --- 4. Trace-cmd record (ferma con Ctrl+C) -----------------------------------
echo "[1/6] Registrazione trace-cmd... (Ctrl+C per fermare)"
# Lanciamo trace-cmd in background e catturiamo il suo PID per gestire il segnale
TRACE_CMD_PID=""
cleanup() {
    echo ""
    echo "--- Segnale ricevuto, arresto trace-cmd... ---"
    $SUDO_CMD pkill -INT trace-cmd 2>/dev/null
    if [ -n "$TRACE_CMD_PID" ]; then
        wait "$TRACE_CMD_PID" 2>/dev/null
    fi
    sleep 1
}
trap cleanup INT TERM

$SUDO_CMD trace-cmd record -e sched:sched_switch -e sched:sched_wakeup $PIDS_CMD -o "$OUTPUT_DIR/trace.dat" &
TRACE_CMD_PID=$!
wait $TRACE_CMD_PID 2>/dev/null
trap - INT TERM

if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: trace.dat non creato."
    exit 1
fi

$SUDO_CMD chown -R "${SUDO_UID:-$(id -u)}:${SUDO_GID:-$(id -g)}" "$OUTPUT_DIR"

# --- 5. Report testuale -------------------------------------------------------
echo "[2/6] Generazione report testuale..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

# --- 6. Compila thread_analysis -----------------------------------------------
echo "[3/6] Compilazione analizzatore C++..."
PARSER_EXE="$OUTPUT_DIR/thread_analysis"
if [ -f "$PARSER_SRC" ]; then
    g++ -O2 -std=c++17 "$PARSER_SRC" -o "$PARSER_EXE"
else
    echo "ATTENZIONE: $PARSER_SRC non trovato!"
    PARSER_EXE=""
fi

# --- 7. Analisi thread --------------------------------------------------------
echo "[4/6] Analisi thread..."
if [ -n "$PARSER_EXE" ]; then
    cd "$OUTPUT_DIR" || exit
    ./thread_analysis "trace_output.txt" "${SRC_FILE_ABS}" "ALL" > "risultati_finali.txt"
    echo ""
    cat "risultati_finali.txt"
    cd "$PROJECT_ROOT"
fi

# --- 8. Monitor Python --------------------------------------------------------
echo ""
echo "[5/6] Avvio monitor visivo Python..."
if [ -f "$MONITOR_SCRIPT_ABS" ]; then
    cd "$OUTPUT_DIR" || exit
    PYTHON_BIN="python3"
    "$PYTHON_BIN" "$MONITOR_SCRIPT_ABS" "timeline.csv"
    cd "$PROJECT_ROOT"
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
    echo "ATTENZIONE: KernelShark non trovato."
fi
