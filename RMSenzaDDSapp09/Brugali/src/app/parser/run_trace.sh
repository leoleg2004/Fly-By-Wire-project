#!/bin/bash

# Chiediamo l'eseguibile
if [ "$#" -eq 0 ]; then
    read -p "Inserisci l'eseguibile da tracciare (es. app10c): " EXECUTABLE
else
    EXECUTABLE=$1
fi

if [[ ! "$EXECUTABLE" == ./* ]] && [[ ! "$EXECUTABLE" == /* ]]; then
    EXECUTABLE="./$EXECUTABLE"
fi

if [ ! -f "$EXECUTABLE" ]; then
    echo "Errore: Eseguibile '$EXECUTABLE' non trovato!"
    exit 1
fi

APP_NAME=$(basename "$EXECUTABLE")

# Troviamo il file sorgente .c da dare in pasto all'analizzatore!
SRC_FILE="${APP_NAME}.c"
if [ ! -f "$SRC_FILE" ]; then
    SRC_FILE="${APP_NAME}_main.c"
fi

if [ ! -f "$SRC_FILE" ]; then
    echo "ERRORE CRITICO: Non trovo il file sorgente ($APP_NAME.c) per leggere i periodi!"
    exit 1
fi

# Creiamo il percorso assoluto al sorgente
SRC_FILE_ABS="$PWD/$SRC_FILE"

echo ""
read -p "Vuoi filtrare per una CPU specifica? (Inserisci 0, 1... [Invio] per tutte): " CPU_FILTER
CPU_FILTER=${CPU_FILTER:-ALL}

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="trace_results_${APP_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Inizio tracciamento per: $APP_NAME"
echo " Sorgente C trovato: $SRC_FILE"
echo " Filtro CPU: $CPU_FILTER"
echo " Cartella di output: $OUTPUT_DIR"
echo "========================================================="

mkdir -p "$OUTPUT_DIR"

echo "[1/4] Avvio simulatore e registrazione trace-cmd..."
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup  -o "$OUTPUT_DIR/trace.dat" "$EXECUTABLE"

if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: trace.dat non creato."
    exit 1
fi

sudo chmod 666 "$OUTPUT_DIR/trace.dat"

echo "[2/4] Generazione del report testuale..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

echo "[3/4] Compilazione analizzatore C++..."
PARSER_SRC="thread_analysis.cpp" 
PARSER_EXE="$OUTPUT_DIR/thread_analysis"

if [ -f "$PARSER_SRC" ]; then
    g++ -O2 -std=c++17 "$PARSER_SRC" -o "$PARSER_EXE"
else
    echo "ATTENZIONE: File $PARSER_SRC non trovato!"
    PARSER_EXE=""
fi

echo "[4/4] Avvio Analisi (Lettura periodi dal codice C in corso)... "
if [ -n "$PARSER_EXE" ]; then
    echo ""
    cd "$OUTPUT_DIR" || exit
    
    # Passiamo al C++ il log testuale, il file .c originale e la CPU!
    ./thread_analysis "trace_output.txt" "$SRC_FILE_ABS" "$CPU_FILTER" | tee "risultati_finali.txt"
    
    cd ..
fi

echo "========================================================="
echo "Tracciamento completato! Dati in: $OUTPUT_DIR"
