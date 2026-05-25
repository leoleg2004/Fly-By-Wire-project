#!/bin/bash
# marker4.sh - Visualizzazione risultati (Monitor Python e KernelShark)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [ -f /tmp/last_marker_dir ]; then
    DEFAULT_DIR=$(cat /tmp/last_marker_dir)
fi

OUTPUT_DIR=${1:-$DEFAULT_DIR}

if [ -z "$OUTPUT_DIR" ] || [ ! -d "$OUTPUT_DIR" ]; then
    echo "ERRORE: Cartella di output non trovata."
    exit 1
fi

MONITOR_SCRIPT_ABS="$PROJECT_ROOT/src/tools/monitor.py"

echo "[5/6] Avvio monitor visivo (Python)..."
if [ -f "$MONITOR_SCRIPT_ABS" ]; then
    if [ -f "$OUTPUT_DIR/timeline.csv" ]; then
        cd "$OUTPUT_DIR" || exit
        PYTHON_BIN="python3"
        "$PYTHON_BIN" "$MONITOR_SCRIPT_ABS" "timeline.csv"
        cd "$PROJECT_ROOT" || exit
    else
        echo "ERRORE: timeline.csv non trovato in $OUTPUT_DIR. Esegui marker3.sh prima."
    fi
else
    echo "ATTENZIONE: Non trovo lo script $MONITOR_SCRIPT_ABS."
fi

echo "[6/6] Avvio KernelShark..."
if command -v kernelshark >/dev/null 2>&1; then
    if [ -f "$OUTPUT_DIR/trace.dat" ]; then
        (cd "$OUTPUT_DIR" && kernelshark trace.dat) &
    else
        echo "ERRORE: trace.dat non trovato in $OUTPUT_DIR."
    fi
else
    echo "ATTENZIONE: KernelShark non trovato."
fi

echo "Visualizzazione avviata."
