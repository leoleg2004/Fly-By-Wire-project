#!/bin/bash

# Chiediamo l'eseguibile
if [ "$#" -eq 0 ]; then
    read -p "Inserisci l'eseguibile da tracciare (es. app10c): " EXECUTABLE
else
    EXECUTABLE=$1
fi

if [[ ! "$EXECUTABLE" == ./* ]] && [[ ! "$EXECUTABLE" == /* ]]; then
    EXECUTABLE="eseguibili/$EXECUTABLE"
fi

if [ ! -f "$EXECUTABLE" ]; then
    echo "Errore: Eseguibile '$EXECUTABLE' non trovato!"
    exit 1
fi

APP_NAME=$(basename "$EXECUTABLE")

# Troviamo il file sorgente .c o .cpp da dare in pasto all'analizzatore!
SRC_FILE="sorgenti/${APP_NAME}.c"
if [ ! -f "$SRC_FILE" ]; then
    SRC_FILE="sorgenti/${APP_NAME}_main.c"
fi

# Fallback C++
if [ ! -f "$SRC_FILE" ]; then
    SRC_FILE="sorgenti/${APP_NAME}.cpp"
fi

if [ ! -f "$SRC_FILE" ]; then
    SRC_FILE="sorgenti/${APP_NAME}_main.cpp"
fi

if [ ! -f "$SRC_FILE" ]; then
    echo "ERRORE CRITICO: Non trovo il file sorgente ($APP_NAME.c o .cpp) per leggere i periodi!"
    exit 1
fi

# Creiamo il percorso assoluto al sorgente e allo script Python
SRC_FILE_ABS="$PWD/$SRC_FILE"
# AGGIORNATO CON IL NOME CORRETTO DEL TUO FILE PYTHON
MONITOR_SCRIPT_ABS="$PWD/monitorRealTime.py"

CPU_FILTER="ALL"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="output/traces/trace_results_${APP_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Inizio tracciamento per: $APP_NAME"
echo " Sorgente C trovato: $SRC_FILE"
echo " Cartella di output: $OUTPUT_DIR"
echo "========================================================="

mkdir -p "$OUTPUT_DIR"

echo "[1/5] Avvio simulatore e registrazione trace-cmd..."
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup  -o "$OUTPUT_DIR/trace.dat" "$EXECUTABLE"

if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: trace.dat non creato."
    exit 1
fi

sudo chmod 666 "$OUTPUT_DIR/trace.dat"

echo "[2/5] Generazione del report testuale..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

echo "[3/5] Compilazione analizzatore C++..."
PARSER_SRC="thread_analysis.cpp" 
PARSER_EXE="$OUTPUT_DIR/thread_analysis"

if [ -f "$PARSER_SRC" ]; then
    g++ -O2 -std=c++17 "$PARSER_SRC" -o "$PARSER_EXE"
else
    echo "ATTENZIONE: File $PARSER_SRC non trovato!"
    PARSER_EXE=""
fi

echo "[4/5] Avvio Analisi (Lettura periodi dal codice C in corso)... "
if [ -n "$PARSER_EXE" ]; then
    echo ""
    cd "$OUTPUT_DIR" || exit
    
    # Passiamo al C++ il log testuale, il file originale e la CPU. Salviamo il testo in background!
    ./thread_analysis "trace_output.txt" "$SRC_FILE_ABS" "$CPU_FILTER" > "risultati_finali.txt"
    
    # =========================================================================
    # NUOVA FASE: AVVIO DEL MONITOR PYTHON
    # =========================================================================
    echo ""
    echo "[5/5] Avvio monitor visivo (Python)..."
    if [ -f "$MONITOR_SCRIPT_ABS" ]; then
        # Eseguiamo lo script Python. Poiché siamo nella cartella OUTPUT_DIR, 
        # Python leggerà automaticamente il timeline.csv appena generato qui dentro.
        python3 "$MONITOR_SCRIPT_ABS"
    else
        echo "ATTENZIONE: Non trovo lo script $MONITOR_SCRIPT_ABS. Grafico non generato."
    fi
    
    cd ..
fi

echo "========================================================="
echo "Tracciamento completato! Dati in: $OUTPUT_DIR"
