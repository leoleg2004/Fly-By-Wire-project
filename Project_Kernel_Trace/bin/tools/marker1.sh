#!/bin/bash
# marker1.sh - Estrazione TID e tracciamento passivo (Registrazione)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Funzione per risolvere alias dei nomi
resolve_app_name() {
    case $1 in
        GeometryBroadcastner) echo "Broadcastner" ;;
        GeometryListener)    echo "Listener" ;;
        *)                    echo "$1" ;;
    esac
}

if [ "$#" -eq 0 ]; then
    read -p "Inserisci il PRIMO eseguibile (già avviato, es. Broadcastner o app10f): " RAW_NAME
    read -p "[Opzionale] Inserisci il SECONDO eseguibile: " RAW_SEC_NAME
else
    RAW_NAME=$1
    RAW_SEC_NAME=$2
fi

EXECUTABLE_NAME=$(resolve_app_name "$RAW_NAME")
APP_NAME=$(basename "$EXECUTABLE_NAME")

PIDS_1=$(pidof "$APP_NAME")
if [ -z "$PIDS_1" ]; then
    echo "ERRORE: Il processo '$APP_NAME' non è in esecuzione!"
    exit 1
fi

PIDS_CMD=""

aggiungi_pid_e_thread() {
    local process_name=$1
    local pids=$2
    for p in $pids; do
        PIDS_CMD="$PIDS_CMD -P $p"
        echo ">> Trovato processo principale: $process_name (PID: $p)"
        if [ -d "/proc/$p/task" ]; then
            for tid_path in /proc/$p/task/*; do
                tid=$(basename "$tid_path")
                if [ "$tid" != "$p" ]; then
                    PIDS_CMD="$PIDS_CMD -P $tid"
                    thread_name=$(cat "$tid_path/comm" 2>/dev/null)
                    echo "   -> Agganciato thread: $thread_name (TID: $tid)"
                fi
            done
        fi
    done
}

aggiungi_pid_e_thread "$APP_NAME" "$PIDS_1"

if [ -n "$RAW_SEC_NAME" ]; then
    SEC_EXECUTABLE_NAME=$(resolve_app_name "$RAW_SEC_NAME")
    SEC_APP_NAME=$(basename "$SEC_EXECUTABLE_NAME")
    PIDS_2=$(pidof "$SEC_APP_NAME")
    if [ -n "$PIDS_2" ]; then
        aggiungi_pid_e_thread "$SEC_APP_NAME" "$PIDS_2"
    fi
fi

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="$PROJECT_ROOT/bin/Test/trace_marker_${APP_NAME}_${TIMESTAMP}"
mkdir -p "$OUTPUT_DIR"

# Salviamo la cartella di output per i prossimi script
echo "$OUTPUT_DIR" > /tmp/last_marker_dir
# Salviamo anche l'app name per trovare il sorgente in marker3
echo "$APP_NAME" > "$OUTPUT_DIR/app_name.txt"

echo "========================================================="
echo " Inizio tracciamento per: $APP_NAME"
echo " Cartella di output: $OUTPUT_DIR"
echo "========================================================="

echo "[1/4] Registrazione trace-cmd..."
echo "--- PREMI Ctrl+C PER FERMARE LA REGISTRAZIONE ---"
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup $PIDS_CMD -o "$OUTPUT_DIR/trace.dat"

if [ ! -f "$OUTPUT_DIR/trace.dat" ]; then
    echo "ERRORE: trace.dat non creato."
    exit 1
fi

sudo chown -R "$(id -u):$(id -g)" "$OUTPUT_DIR"
echo "Registrazione completata. Usa marker2.sh per generare il report."
