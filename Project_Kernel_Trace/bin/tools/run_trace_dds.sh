#!/bin/bash
# run_trace_dds.sh - Tracciamento per Project_Kernel_Trace (DDS launch mode)

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

# Chiediamo l'eseguibile
if [ "$#" -eq 0 ]; then
    read -p "Inserisci l'eseguibile DDS da tracciare (es. Broadcastner): " RAW_NAME
else
    RAW_NAME=$1
fi

EXECUTABLE_NAME=$(resolve_app_name "$RAW_NAME")

# Cerca l'eseguibile in bin/app/
EXECUTABLE=$(find "$PROJECT_ROOT/bin/app" -type f -executable -name "$EXECUTABLE_NAME" | head -n 1)

if [ -z "$EXECUTABLE" ]; then
    echo "ERRORE: Eseguibile '$EXECUTABLE_NAME' non trovato in bin/app/!"
    exit 1
fi

APP_NAME=$(basename "$EXECUTABLE")

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
OUTPUT_DIR="$PROJECT_ROOT/bin/Test/trace_results_dds_${APP_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Inizio tracciamento DDS per: $APP_NAME"
echo " Sorgente trovato: $SRC_FILE_ABS"
echo " Cartella di output: $OUTPUT_DIR"
echo "========================================================="

mkdir -p "$OUTPUT_DIR"

echo "[1/5] Avvio simulatore DDS e registrazione trace-cmd..."
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup -o "$OUTPUT_DIR/trace.dat" "$EXECUTABLE"

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
