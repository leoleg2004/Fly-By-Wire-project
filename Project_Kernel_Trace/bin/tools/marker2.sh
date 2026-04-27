#!/bin/bash
# marker2.sh - Generazione report testuale

if [ -f /tmp/last_marker_dir ]; then
    DEFAULT_DIR=$(cat /tmp/last_marker_dir)
fi

OUTPUT_DIR=${1:-$DEFAULT_DIR}

if [ -z "$OUTPUT_DIR" ] || [ ! -d "$OUTPUT_DIR" ]; then
    echo "ERRORE: Cartella di output non trovata. Esegui prima marker1.sh o specifica la cartella come argomento."
    exit 1
fi

echo "[2/4] Generazione del report testuale in $OUTPUT_DIR..."
if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE: $OUTPUT_DIR/trace.dat non trovato."
    exit 1
fi

trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"
echo "Report generato: $OUTPUT_DIR/trace_output.txt"
echo "Usa marker3.sh per l'analisi dei thread."
