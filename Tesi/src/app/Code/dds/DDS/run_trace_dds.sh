#!/bin/bash

# =============================================================================
#  run_trace_dds.sh — Traccia un eseguibile DDS (ImageSender/ImageSubscriber)
#  Usabile da qualsiasi directory; va bene anche come: ./run_trace_dds.sh
# =============================================================================

# --- 0. Trova la root di DDS (la directory che contiene questo script) -------
DDS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARSER_ROOT="$(cd "$DDS_ROOT/../.." && pwd)"

# Carica SERL_HOME e LD_LIBRARY_PATH
source "$DDS_ROOT/environment.sh"

# --- 1. Scegli l'eseguibile --------------------------------------------------
if [ "$#" -eq 0 ]; then
    echo "Eseguibili DDS disponibili:"
    ls "$DDS_ROOT/bin/components/" 2>/dev/null | grep -v '\.sh' || echo "  (nessuno trovato)"
    echo ""
    read -p "Inserisci l'eseguibile da tracciare (es. ImageSubscriber): " EXECUTABLE_INPUT
else
    EXECUTABLE_INPUT=$1
fi

# Risolvi il path completo
if [[ "$EXECUTABLE_INPUT" == /* ]]; then
    # Path assoluto già fornito
    EXECUTABLE="$EXECUTABLE_INPUT"
elif [ -f "$EXECUTABLE_INPUT" ]; then
    # Path relativo a cwd
    EXECUTABLE="$(realpath "$EXECUTABLE_INPUT")"
elif [ -f "$DDS_ROOT/bin/components/$EXECUTABLE_INPUT" ]; then
    # Solo nome dell'eseguibile → cerca in bin/components/
    EXECUTABLE="$DDS_ROOT/bin/components/$EXECUTABLE_INPUT"
else
    echo "Errore: eseguibile '$EXECUTABLE_INPUT' non trovato in bin/components/!"
    exit 1
fi

APP_NAME=$(basename "$EXECUTABLE")

# --- 2. Cerca il sorgente .cpp -----------------------------------------------
SRC_FILE=""
# Prima cerca nella cartella specifica di SendImage (case-sensitive)
for candidate in \
    "$DDS_ROOT/src/components/SendImage/${APP_NAME}.cpp" \
    "$DDS_ROOT/src/components/SendImage/${APP_NAME}.c" \
    "$DDS_ROOT/src/runtime/${APP_NAME}.cpp" \
    "$DDS_ROOT/src/runtime/${APP_NAME}.c"; do
    if [ -f "$candidate" ]; then
        SRC_FILE="$candidate"
        break
    fi
done
# Fallback: ricerca ricorsiva in tutto src/
if [ -z "$SRC_FILE" ]; then
    SRC_FILE=$(find "$DDS_ROOT/src" -type f \
        \( -name "${APP_NAME}.cpp" -o -name "${APP_NAME}.c" \) 2>/dev/null | head -n 1)
fi

if [ -z "$SRC_FILE" ]; then
    echo "ATTENZIONE: Sorgente '${APP_NAME}.cpp/.c' non trovato in src/."
    echo "L'analisi del periodo non sarà disponibile, ma il trace verrà eseguito comunque."
    SRC_FILE_ABS=""
else
    SRC_FILE_ABS="$SRC_FILE"
    echo " Sorgente trovato: $SRC_FILE_ABS"
fi
MONITOR_SCRIPT_ABS="$PARSER_ROOT/monitorRealTime.py"
THREAD_ANALYSIS_CPP="$PARSER_ROOT/thread_analysis.cpp"

CPU_FILTER="ALL"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="$DDS_ROOT/output/traces/trace_results_${APP_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Inizio tracciamento DDS per: $APP_NAME"
echo " Sorgente: ${SRC_FILE_ABS:-N/A}"
echo " Output:   $OUTPUT_DIR"
echo " SERL_HOME: $SERL_HOME"
echo "========================================================="

mkdir -p "$OUTPUT_DIR"

# --- 3. Trace-cmd record -----------------------------------------------------
echo "[1/5] Avvio trace-cmd per $APP_NAME..."
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup \
    -o "$OUTPUT_DIR/trace.dat" \
    env LD_LIBRARY_PATH="$SERL_HOME/DDS/bin/runtime/lib:$LD_LIBRARY_PATH" \
    "$EXECUTABLE"

if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: trace.dat non creato."
    exit 1
fi

sudo chown -R "$(id -u):$(id -g)" "$OUTPUT_DIR"
chmod -R u+rwX "$OUTPUT_DIR"

# --- 4. Report testuale -------------------------------------------------------
echo "[2/5] Generazione report testuale..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

# --- 5. Compila thread_analysis ----------------------------------------------
echo "[3/5] Compilazione analizzatore C++..."
PARSER_EXE="$OUTPUT_DIR/thread_analysis"
if [ -f "$THREAD_ANALYSIS_CPP" ]; then
    g++ -O2 -std=c++17 "$THREAD_ANALYSIS_CPP" -o "$PARSER_EXE"
else
    echo "ATTENZIONE: $THREAD_ANALYSIS_CPP non trovato!"
    PARSER_EXE=""
fi

# --- 6. Analisi --------------------------------------------------------------
echo "[4/5] Avvio analisi thread..."
if [ -n "$PARSER_EXE" ]; then
    cd "$OUTPUT_DIR" || exit
    ./thread_analysis "trace_output.txt" "$SRC_FILE_ABS" "$CPU_FILTER" > "risultati_finali.txt"
    echo ""
    cat "risultati_finali.txt"

    # --- 7. Monitor Python ---------------------------------------------------
    echo ""
    echo "[5/5] Avvio monitor visivo Python..."
    if [ -f "$MONITOR_SCRIPT_ABS" ]; then
        python3 "$MONITOR_SCRIPT_ABS"
    else
        echo "ATTENZIONE: $MONITOR_SCRIPT_ABS non trovato. Grafico non generato."
    fi

    cd "$DDS_ROOT"
fi

echo "========================================================="
echo " Tracciamento completato! Dati in: $OUTPUT_DIR"
echo "========================================================="
