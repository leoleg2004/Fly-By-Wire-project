#!/bin/bash

# Se non passato come argomento, chiediamo l'eseguibile interattivamente
if [ "$#" -eq 0 ]; then
    read -p "Inserisci l'eseguibile da tracciare (es. app10): " EXECUTABLE
else
    EXECUTABLE=$1
fi

# TRUCCO 1: Se scrivi solo "app10", lo script aggiunge "./" da solo!
if [[ ! "$EXECUTABLE" == ./* ]] && [[ ! "$EXECUTABLE" == /* ]]; then
    EXECUTABLE="./$EXECUTABLE"
fi

# Verifica che l'eseguibile esista
if [ ! -f "$EXECUTABLE" ]; then
    echo "Errore: Eseguibile '$EXECUTABLE' non trovato!"
    exit 1
fi

APP_NAME=$(basename "$EXECUTABLE")
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="trace_results_${APP_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Inizio tracciamento per: $APP_NAME"
echo " Cartella di output: $OUTPUT_DIR"
echo "========================================================="

# Crea la cartella dedicata
mkdir -p "$OUTPUT_DIR"

# 1. Avvia la registrazione con trace-cmd e il simulatore
echo "[1/4] Avvio simulatore e registrazione trace-cmd..."
# TRUCCO 2: Rimosso "-e ftrace:print" che ti faceva crashare il sistema!
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup -o "$OUTPUT_DIR/trace.dat" "$EXECUTABLE"

# Verifica se il file è stato creato davvero prima di continuare
if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: Il file trace.dat non è stato creato. Registrazione fallita."
    exit 1
fi

# Aggiungi permessi al file generato da root
sudo chmod 666 "$OUTPUT_DIR/trace.dat"

# 2. Genera il report testuale
echo "[2/4] Generazione del report in testo..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

# 3. Estrae i thread e formatta in colonne
echo "[3/4] Estrazione dati thread (Activity_1 e Activity_2)..."
grep -E "Activity_1|Activity_2|print|START|END" "$OUTPUT_DIR/trace_output.txt" > "$OUTPUT_DIR/trace_THREAD.txt"

# Usa awk (come da TRACCIAMENTO DEI TASK.txt) per formattare a colonne
LC_ALL=C awk '{
    gsub(":", "", $3); 
    gsub(":", "", $4);
    printf "%-25s | %-5s | %-12s | %-15s | ", $1, $2, $3, $4;
    for(i=5; i<=NF; i++) printf "%s ", $i;
    print ""
}' "$OUTPUT_DIR/trace_THREAD.txt" > "$OUTPUT_DIR/trace_colonne.txt"

# 4. Compila il parser ed eseguilo per analizzare i dati
echo "[4/4] Avvio parser C++ per calcolo costi computazionali... "

# TRUCCO 3: Cerca il parser C++ dove si trova realmente (evita l'errore della cartella 'src' mancante)
if [ -f "parser_batch.cpp" ]; then
    g++ parser_batch.cpp -o parser_batch
    PARSER_EXE="./parser_batch"
elif [ -f "src/parser_batch.cpp" ]; then
    g++ src/parser_batch.cpp -o src/parser_batch
    PARSER_EXE="./src/parser_batch"
else
    echo "ATTENZIONE: File parser_batch non trovato! Salto il calcolo C++."
    PARSER_EXE=""
fi

echo ""
echo "================ RISULTATI SIMULAZIONE =================="
if [ -n "$PARSER_EXE" ]; then
    $PARSER_EXE "$OUTPUT_DIR/trace_colonne.txt" | tee "$OUTPUT_DIR/risultati_finali.txt"
else
    echo "Output C++ saltato."
fi
echo "========================================================="

echo ""
echo "Tracciamento completato."
echo "Tutti i file sono stati salvati nella cartella: $OUTPUT_DIR"
