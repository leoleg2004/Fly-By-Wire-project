#!/bin/bash
# marker.sh - Estrazione TID e tracciamento passivo (standard app version)

# Salviamo la cartella principale del progetto
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BASE_DIR="$PROJECT_ROOT"

# Funzione per risolvere alias dei nomi
resolve_app_name() {
    case $1 in
        GeometryBroadcastner) echo "Broadcastner" ;;
        GeometryListener)    echo "Listener" ;;
        *)                    echo "$1" ;;
    esac
}

# Chiediamo gli eseguibili (devono essere GIÀ AVVIATI negli altri terminali)
if [ "$#" -eq 0 ]; then
    read -p "Inserisci il PRIMO eseguibile (già avviato, es. Broadcastner o app10f): " RAW_NAME
    read -p "[Opzionale] Inserisci il SECONDO eseguibile: " RAW_SEC_NAME
else
    RAW_NAME=$1
    RAW_SEC_NAME=$2
fi

EXECUTABLE_NAME=$(resolve_app_name "$RAW_NAME")
APP_NAME=$(basename "$EXECUTABLE_NAME")

# =========================================================================
# ESTRAZIONE DEI THREAD (TID)
# =========================================================================
PIDS_1=$(pidof "$APP_NAME")
if [ -z "$PIDS_1" ]; then
    echo "ERRORE: Il processo '$APP_NAME' non è in esecuzione!"
    echo "Segui il metodo del Prof: Avvia prima ./$APP_NAME in un terminale, poi usa questo script."
    exit 1
fi

PIDS_CMD=""

# Funzione per estrarre tutti i thread
aggiungi_pid_e_thread() {
    local process_name=$1
    local pids=$2
    for p in $pids; do
        PIDS_CMD="$PIDS_CMD -P $p"
        echo ">> Trovato processo principale: $process_name (PID: $p)"
        
        # Esploriamo /proc/$p/task/ per trovare tutti i thread interni (TID)
        if [ -d "/proc/$p/task" ]; then
            for tid_path in /proc/$p/task/*; do
                tid=$(basename "$tid_path")
                if [ "$tid" != "$p" ]; then
                    PIDS_CMD="$PIDS_CMD -P $tid"
                    # Leggiamo il nome del thread per conferma a video
                    thread_name=$(cat "$tid_path/comm" 2>/dev/null)
                    echo "   -> Agganciato thread: $thread_name (TID: $tid)"
                fi
            done
        fi
    done
}

# 1. Processiamo la prima app e i suoi thread
aggiungi_pid_e_thread "$APP_NAME" "$PIDS_1"

# 2. Se abbiamo inserito un secondo eseguibile, cerchiamo anche lui e i suoi thread
if [ -n "$RAW_SEC_NAME" ]; then
    SEC_EXECUTABLE_NAME=$(resolve_app_name "$RAW_SEC_NAME")
    SEC_APP_NAME=$(basename "$SEC_EXECUTABLE_NAME")
    PIDS_2=$(pidof "$SEC_APP_NAME")
    if [ -n "$PIDS_2" ]; then
        echo "---------------------------------------------------------"
        aggiungi_pid_e_thread "$SEC_APP_NAME" "$PIDS_2"
        echo ">> Tracciamento congiunto pronto."
    else
        echo ">> ATTENZIONE: '$SEC_APP_NAME' non trovato in esecuzione. Traccio solo $APP_NAME."
    fi
else
    echo ">> Tracciamento singolo pronto."
fi
# =========================================================================

# Troviamo il file sorgente per l'analisi
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

if [ ! -f "$SRC_FILE" ]; then
    echo "ERRORE CRITICO: Non trovo il file sorgente per $APP_NAME!"
    exit 1
fi

SRC_FILE_ABS="$SRC_FILE"
MONITOR_SCRIPT_ABS="$PROJECT_ROOT/src/tools/monitor.py"
PARSER_SRC="$PROJECT_ROOT/src/tools/thread_analysis.cpp"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="$PROJECT_ROOT/bin/Test/trace_marker_${APP_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Inizio tracciamento per: $APP_NAME"
echo " Sorgente trovato: $SRC_FILE_ABS"
echo " Cartella di output: $OUTPUT_DIR"
echo "========================================================="

mkdir -p "$OUTPUT_DIR"

echo "[1/5] Registrazione trace-cmd sui PID e THREAD scelti..."
echo "--- PREMI Ctrl+C PER FERMARE LA REGISTRAZIONE QUANDO HAI FINITO! ---"
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup $PIDS_CMD -o "$OUTPUT_DIR/trace.dat"

if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: trace.dat non creato."
    exit 1
fi

sudo chown -R "$(id -u):$(id -g)" "$OUTPUT_DIR"

echo "[2/5] Generazione del report testuale..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

echo "[3/5] Compilazione analizzatore C++..."
PARSER_EXE="$OUTPUT_DIR/thread_analysis"
if [ -f "$PARSER_SRC" ]; then
    g++ -O2 -std=c++17 "$PARSER_SRC" -o "$PARSER_EXE"
else
    echo "ATTENZIONE: File $PARSER_SRC non trovato!"
    PARSER_EXE=""
fi

echo "[4/5] Avvio Analisi (Lettura periodi dal codice)... "
if [ -n "$PARSER_EXE" ]; then
    cd "$OUTPUT_DIR" || exit
    ./thread_analysis "trace_output.txt" "$SRC_FILE_ABS" "ALL" > "risultati_finali.txt"
    echo ""
    cat "risultati_finali.txt"
    
    echo "[5/5] Avvio monitor visivo (Python)..."
    if [ -f "$MONITOR_SCRIPT_ABS" ]; then
        python3 "$MONITOR_SCRIPT_ABS" "timeline.csv"
    else
        echo "ATTENZIONE: Non trovo lo script $MONITOR_SCRIPT_ABS."
    fi
    cd "$PROJECT_ROOT" || exit
fi

echo "========================================================="
echo "Tracciamento completato! Dati in: $OUTPUT_DIR"

echo "[6/6] Avvio KernelShark..."
if command -v kernelshark >/dev/null 2>&1; then
    (cd "$OUTPUT_DIR" && kernelshark trace.dat) &
else
    echo "ATTENZIONE: KernelShark non trovato."
fi
