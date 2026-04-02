#!/bin/bash

# Se non passato come argomento, chiediamo l'eseguibile interattivamente
if [ "$#" -eq 0 ]; then
    read -p "Inserisci l'eseguibile da tracciare (es. ./build/nomefile): " EXECUTABLE
else
    EXECUTABLE=$1
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
# cd nella directory del simulatore non è strettamente necessario se lo lanciamo da qui
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup -o "$OUTPUT_DIR/trace.dat" "$EXECUTABLE"

# Aggiungi permessi al file generato da root
sudo chmod 666 "$OUTPUT_DIR/trace.dat"

# 2. Genera il report testuale
echo "[2/4] Generazione del report in testo..."
trace-cmd report "$OUTPUT_DIR/trace.dat" > "$OUTPUT_DIR/trace_output.txt"

# 3. Estrae i thread e formatta in colonne
echo "[3/4] Estrazione dati thread (Activity_1 e Activity_2)..."
grep -E "Activity_1|Activity_2" "$OUTPUT_DIR/trace_output.txt" > "$OUTPUT_DIR/trace_THREAD.txt"

# Usa awk (come da TRACCIAMENTO DEI TASK.txt) per formattare a colonne
awk '{
    gsub(":", "", $3); 
    gsub(":", "", $4);
    printf "%-25s | %-5s | %-12s | %-15s | ", $1, $2, $3, $4;
    for(i=5; i<=NF; i++) printf "%s ", $i;
    print ""
}' "$OUTPUT_DIR/trace_THREAD.txt" > "$OUTPUT_DIR/trace_colonne.txt"

# 4. Compila il parser ed eseguilo per analizzare i dati
echo "[4/4] Avvio parser C++ per calcolo costi computazionali... "
(cd src && g++ parser_batch.cpp -o parser_batch)

echo ""
echo "================ RISULTATI SIMULAZIONE =================="
./src/parser_batch "$OUTPUT_DIR/trace_colonne.txt" | tee "$OUTPUT_DIR/risultati_finali.txt"
echo "========================================================="

echo ""
echo "Tracciamento completato."
echo "Tutti i file sono stati salvati nella cartella: $OUTPUT_DIR"
