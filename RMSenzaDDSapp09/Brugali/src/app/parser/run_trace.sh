#!/bin/bash

# Se non passato come argomento, chiediamo l'eseguibile interattivamente
if [ "$#" -eq 0 ]; then
    read -p "Inserisci l'eseguibile da tracciare (es. parser/app10): " EXECUTABLE
else
    EXECUTABLE=$1
fi

# Aggiunge "./" se l'utente non lo ha messo e se non è un percorso assoluto
if [[ ! "$EXECUTABLE" == ./* ]] && [[ ! "$EXECUTABLE" == /* ]]; then
    EXECUTABLE="./$EXECUTABLE"
fi

# Verifica che l'eseguibile esista
if [ ! -f "$EXECUTABLE" ]; then
    echo "Errore: Eseguibile '$EXECUTABLE' non trovato!"
    exit 1
fi

APP_NAME=$(basename "$EXECUTABLE")

# Chiediamo i parametri per l'analisi C++
echo ""
read -p "Inserisci il NOME del thread/activity da analizzare (es. Activity_1) [Invio per usare $APP_NAME]: " THREAD_NAME
THREAD_NAME=${THREAD_NAME:-$APP_NAME}

# Il codice del prof si aspetta il periodo. Glielo chiediamo.
read -p "Inserisci il periodo atteso in microsecondi (es. 120000 per 120ms) [Invio per default 120000]: " PERIOD_US
PERIOD_US=${PERIOD_US:-120000}

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="trace_results_${APP_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Inizio tracciamento per: $APP_NAME"
echo " Analisi target: $THREAD_NAME (Periodo: $PERIOD_US us)"
echo " Cartella di output: $OUTPUT_DIR"
echo "========================================================="

# Crea la cartella dedicata
mkdir -p "$OUTPUT_DIR"

# 1. Avvia la registrazione con trace-cmd e il simulatore
echo "[1/4] Avvio simulatore e registrazione trace-cmd..."
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup -o "$OUTPUT_DIR/trace.dat" "$EXECUTABLE"

# Verifica se il file è stato creato davvero prima di continuare
if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: Il file trace.dat non è stato creato. Registrazione fallita."
    exit 1
fi

# Aggiungi permessi al file generato da root
sudo chmod 666 "$OUTPUT_DIR/trace.dat"

# 2. Genera il report testuale GREZZO
echo "[2/4] Generazione del report in testo..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

# 3. Compilazione del parser C++ del professore
echo "[3/4] Compilazione thread_analysis nella cartella di esecuzione..."

PARSER_SRC="parser/thread_analysis.cpp" # Metti qui il percorso del file del prof
PARSER_EXE="$OUTPUT_DIR/thread_analysis"

if [ -f "$PARSER_SRC" ]; then
    echo "Trovato $PARSER_SRC, compilo..."
    g++ -O2 -std=c++17 "$PARSER_SRC" -o "$PARSER_EXE"
else
    if [ -f "thread_analysis.cpp" ]; then
        echo "Trovato thread_analysis.cpp nella cartella corrente, compilo..."
        g++ -O2 -std=c++17 "thread_analysis.cpp" -o "$PARSER_EXE"
    else
        echo "ATTENZIONE: File thread_analysis.cpp non trovato! Salto l'analisi."
        PARSER_EXE=""
    fi
fi

# 4. Ricerca del PID e Avvio Analisi
echo "[4/4] Avvio analisi C++ del professore... "

if [ -n "$PARSER_EXE" ]; then
    # Cerchiamo il PID del thread specifico 
   # Cerchiamo il PID del thread specifico prendendo solo i numeri DOPO il trattino o i due punti
    TARGET_PID=$(grep -m 1 -o -E "\b${THREAD_NAME}[-:][0-9]+" "$OUTPUT_DIR/trace_output.txt" | head -n 1 | awk -F'[-:]' '{print $NF}')

    if [ -z "$TARGET_PID" ]; then
        echo "ATTENZIONE: Non sono riuscito a trovare il PID di '$THREAD_NAME' nel trace."
        read -p "Inserisci manualmente il PID da analizzare: " TARGET_PID
    else
        echo "Trovato PID per '$THREAD_NAME': $TARGET_PID"
    fi

    echo ""
    echo "================ RISULTATI SIMULAZIONE =================="
    # Ci spostiamo nella cartella di output così timeline.csv viene salvato lì!
    cd "$OUTPUT_DIR" || exit
    
    # Eseguiamo il parser del prof passando <trace.txt> <PID> <period_us>
    ./thread_analysis "trace_output.txt" "$TARGET_PID" "$PERIOD_US" | tee "risultati_finali.txt"
    
    cd ..
else
    echo "Output C++ saltato."
fi
echo "========================================================="

echo ""
echo "Tracciamento completato."
echo "Tutti i file sono stati salvati nella cartella: $OUTPUT_DIR"
