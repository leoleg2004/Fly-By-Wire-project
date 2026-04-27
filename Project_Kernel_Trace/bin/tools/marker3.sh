#!/bin/bash
# marker3.sh - Analisi dei thread con thread_analysis

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

APP_NAME=$(cat "$OUTPUT_DIR/app_name.txt" 2>/dev/null)

find_src() {
    local name=$1
    local candidate=$(find "$PROJECT_ROOT/src/app" -name "${name}.cpp" -o -name "${name}.c" | head -n 1)
    if [ -n "$candidate" ]; then
        echo "$candidate"
    else
        find "$PROJECT_ROOT/src" -name "${name}.cpp" -o -name "${name}.c" | head -n 1
    fi
}

SRC_FILE_ABS=$(find_src "$APP_NAME")
PARSER_SRC="$PROJECT_ROOT/src/tools/thread_analysis.cpp"
PARSER_EXE="$OUTPUT_DIR/thread_analysis"

echo "[3/4] Compilazione analizzatore C++..."
if [ -f "$PARSER_SRC" ]; then
    g++ -O2 -std=c++17 "$PARSER_SRC" -o "$PARSER_EXE"
else
    echo "ERRORE: $PARSER_SRC non trovato!"
    exit 1
fi

echo "[4/4] Avvio Analisi..."
if [ ! -f "$OUTPUT_DIR/trace_output.txt" ]; then
    echo "ERRORE: $OUTPUT_DIR/trace_output.txt non trovato. Esegui marker2.sh prima."
    exit 1
fi

cd "$OUTPUT_DIR" || exit
./thread_analysis "trace_output.txt" "$SRC_FILE_ABS" "ALL" > "risultati_finali.txt"
echo ""
cat "risultati_finali.txt"
cd "$PROJECT_ROOT" || exit

echo "Analisi completata. Usa marker4.sh per visualizzare i risultati."
