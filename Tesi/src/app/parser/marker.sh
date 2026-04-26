#!/bin/bash

# Salviamo la cartella principale per non perderci durante i cd
BASE_DIR="$PWD"

# Chiediamo gli eseguibili (devono essere GIÀ AVVIATI negli altri terminali)
if [ "$#" -eq 0 ]; then
    read -p "Inserisci il PRIMO eseguibile (già avviato, es. GeometryBroadcaster o app10f): " EXECUTABLE
    read -p "[Opzionale] Inserisci il SECONDO eseguibile (già avviato, es. GeometryListener): " SEC_EXECUTABLE
else
    EXECUTABLE=$1
    SEC_EXECUTABLE=$2
fi

APP_NAME=$(basename "$EXECUTABLE")

# =========================================================================
# SOLUZIONE DEL PROFESSORE + ESTRAZIONE DEI THREAD DDS (TID)
# =========================================================================
PIDS_1=$(pidof "$APP_NAME")
if [ -z "$PIDS_1" ]; then
    echo "ERRORE: Il processo '$APP_NAME' non è in esecuzione!"
    echo "Segui il metodo del Prof: Avvia prima ./$APP_NAME nel Terminale 1, poi usa questo script nel Terminale 3."
    exit 1
fi

PIDS_CMD=""

# Funzione per estrarre tutti i thread (inclusi dds.enc, dds.async, ecc.)
aggiungi_pid_e_thread() {
    local process_name=$1
    local pids=$2
    for p in $pids; do
        PIDS_CMD="$PIDS_CMD -P $p"
        echo ">> Trovato processo principale: $process_name (PID: $p)"
        
        # Esploriamo /proc/$p/task/ per trovare tutti i thread interni (TID)
        if [ -d "/proc/$p/task" ]; then
            for tid_path in /proc/$p/task/*; do
                tid=$(basename "$tid_path")
                if [ "$tid" != "$p" ]; then
                    PIDS_CMD="$PIDS_CMD -P $tid"
                    # Leggiamo il nome del thread per conferma a video
                    thread_name=$(cat "$tid_path/comm" 2>/dev/null)
                    
                    # Se il nome contiene "dds", lo segnaliamo esplicitamente
                    if [[ "$thread_name" == *"dds"* ]]; then
                        echo "   -> Agganciato thread DDS: $thread_name (TID: $tid)"
                    else
                        echo "   -> Agganciato thread: $thread_name (TID: $tid)"
                    fi
                fi
            done
        fi
    done
}

# 1. Processiamo la prima app e i suoi thread
aggiungi_pid_e_thread "$APP_NAME" "$PIDS_1"

# 2. Se abbiamo inserito un secondo eseguibile, cerchiamo anche lui e i suoi thread
if [ -n "$SEC_EXECUTABLE" ]; then
    SEC_APP_NAME=$(basename "$SEC_EXECUTABLE")
    PIDS_2=$(pidof "$SEC_APP_NAME")
    if [ -n "$PIDS_2" ]; then
        echo "---------------------------------------------------------"
        aggiungi_pid_e_thread "$SEC_APP_NAME" "$PIDS_2"
        echo ">> Tracciamento congiunto pronto."
    else
        echo ">> ATTENZIONE: '$SEC_APP_NAME' non trovato in esecuzione. Traccio solo $APP_NAME."
    fi
else
    echo ">> Tracciamento singolo pronto."
fi
# =========================================================================

# Troviamo il file sorgente .c o .cpp da dare in pasto all'analizzatore!
SRC_FILE="sorgenti/${APP_NAME}.c"
if [ ! -f "$SRC_FILE" ]; then
    SRC_FILE="sorgenti/${APP_NAME}_main.c"
fi

# Fallback C++ standard
if [ ! -f "$SRC_FILE" ]; then
    SRC_FILE="sorgenti/${APP_NAME}.cpp"
fi
if [ ! -f "$SRC_FILE" ]; then
    SRC_FILE="sorgenti/${APP_NAME}_main.cpp"
fi

# FALLBACK DDS: ricerca nelle cartelle dds/app
if [ ! -f "$SRC_FILE" ]; then
    DDS_SRC=$(find dds/app -name "${APP_NAME}.cpp" -type f 2>/dev/null | head -n 1)
    if [ -n "$DDS_SRC" ]; then
        SRC_FILE="$DDS_SRC"
    fi
fi
if [ ! -f "$SRC_FILE" ]; then
    DDS_SRC=$(find dds/app -name "${APP_NAME}.c" -type f 2>/dev/null | head -n 1)
    if [ -n "$DDS_SRC" ]; then
        SRC_FILE="$DDS_SRC"
    fi
fi

if [ ! -f "$SRC_FILE" ]; then
    echo "ERRORE CRITICO: Non trovo il file sorgente ($APP_NAME.c o .cpp) per leggere i periodi!"
    exit 1
fi

# Creiamo il percorso assoluto al sorgente e allo script Python
SRC_FILE_ABS="$BASE_DIR/$SRC_FILE"
MONITOR_SCRIPT_ABS="$BASE_DIR/monitorRealTime.py"

CPU_FILTER="ALL"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="output/traces/trace_results_${APP_NAME}_${TIMESTAMP}"

echo "========================================================="
echo " Inizio tracciamento per: $APP_NAME"
echo " Sorgente C trovato: $SRC_FILE"
echo " Cartella di output: $OUTPUT_DIR"
echo "========================================================="

mkdir -p "$OUTPUT_DIR"

echo "[1/5] Registrazione trace-cmd sui PID e THREAD scelti..."
echo "--- PREMI Ctrl+C PER FERMARE LA REGISTRAZIONE QUANDO HAI FINITO! ---"
# L'eseguibile non viene più lanciato dallo script, passiamo direttamente i PID/TID a trace-cmd
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup $PIDS_CMD -o "$OUTPUT_DIR/trace.dat"

if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE CRITICO: trace.dat non creato."
    exit 1
fi

sudo chown -R "$(id -u):$(id -g)" "$OUTPUT_DIR"
chmod -R u+rwX "$OUTPUT_DIR"

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
        python3 "$MONITOR_SCRIPT_ABS"
    else
        echo "ATTENZIONE: Non trovo lo script $MONITOR_SCRIPT_ABS. Grafico non generato."
    fi
    
    # Torniamo alla cartella base in modo sicuro
    cd "$BASE_DIR" || exit
fi

echo "========================================================="
echo "Tracciamento completato! Dati in: $OUTPUT_DIR"

# =========================================================================
# FASE 6: AVVIO DI KERNELSHARK
# =========================================================================
echo ""
echo "[6/6] Avvio KernelShark..."
if command -v kernelshark >/dev/null 2>&1; then
    # Entriamo nella cartella appena creata e apriamo il file
    cd "$OUTPUT_DIR" || exit
    kernelshark &
    cd "$BASE_DIR" || exit
else
    echo "ATTENZIONE: KernelShark non è installato o non è nel PATH."
fi
