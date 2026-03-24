#!/bin/bash

# Se non passato come argomento, chiediamo l'eseguibile interattivamente
if [ "$#" -eq 0 ]; then
    read -p "Inserisci l'eseguibile da tracciare (es. ./build/app09b): " EXECUTABLE
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
OUTPUT_DIR="trace_results_live_${APP_NAME}_${TIMESTAMP}"
mkdir -p "$OUTPUT_DIR"

cleanup() {
    echo -e "\n\n[!] Chiusura ambiente di tracciamento in corso..."
    
    # 1. Spegni l'istanza live ftrace 
    sudo sh -c "echo 0 > $LIVE_INST/tracing_on" 2>/dev/null
    sudo rmdir "$LIVE_INST" 2>/dev/null
    
    # 2. Se l'utente preme CTRL+C prima della fine del simulatore, chiudiamo trace-cmd in modo pulito
    if [ ! -z "$RECORD_PID" ] && kill -0 $RECORD_PID 2>/dev/null; then
        echo "Interruzione manuale: Scrittura di trace.dat in corso per kernelshark..."
        sudo pkill -INT -x trace-cmd
        sleep 1
    fi
    
    # 3. Uccidi il processo in background della pipeline di visualizzazione per stampare la Summary finale
    if [ ! -z "$LIVE_PID" ]; then
        kill -INT $LIVE_PID 2>/dev/null
        wait $LIVE_PID 2>/dev/null
    fi
    
    # Pulizia finali permessi
    sudo chown $USER:$USER "$OUTPUT_DIR"/* 2>/dev/null
    sudo chmod 666 "$OUTPUT_DIR/trace.dat" 2>/dev/null
    
    echo "==============================================================="
    echo "Tracciamento completato."
    echo "Tutti i risultati, sia grafici che binari per KernelShark, si trovano in:"
    echo "   -> $OUTPUT_DIR"
    echo "==============================================================="
    exit 0
}
trap cleanup SIGINT

echo "==============================================================="
echo " AVVIO LETTURA LIVE DA KERNEL E REGISTRAZIONE DUAL TRACE.DAT "
echo " Simulatore scelto: $EXECUTABLE"
echo " Cartella di salvataggio: $OUTPUT_DIR"
echo " ------------------------------------------------------------- "
echo " >> AUTOMAZIONE COMPLETA ATTIVA <<"
echo " Il simulatore verra' eseguito nel sistema, e tracciato QUI LOGICAMENTE."
echo " Non hai bisogno di un secondo terminale aperto!"
echo "==============================================================="

# Compila il parser C++ per sicurezza
(cd src && g++ parser_live.cpp -o parser_live)

echo "-> Creo una istanza ftrace separata per la lettura live..."
# Per non scontrarsi col trace-cmd di base, creiamo una istanza ftrace fissa per la visualizzazione a schermo.
LIVE_INST="/sys/kernel/debug/tracing/instances/live_trace_inst"
sudo mkdir -p "$LIVE_INST"
sudo sh -c "echo 1 > $LIVE_INST/events/sched/sched_switch/enable"
sudo sh -c "echo 1 > $LIVE_INST/events/sched/sched_wakeup/enable"
sudo sh -c "echo 1 > $LIVE_INST/tracing_on"

echo "-> Inizio ascolto degli eventi dello scheduler in Real-Time..."
echo "---------------------------------------------------------------"

# Avviamo la logica live in BACKGROUND. Continuerà a sputare dati a schermo fluidamente!
sudo cat "$LIVE_INST/trace_pipe" | grep --line-buffered -E "Activity_" | \
awk '{
    task_pid = $1; cpu = $2;
    timestamp = 0; event = ""; starting_detail = 5;
    for (i=3; i<=NF; i++) {
        if ($i ~ /[0-9]+\.[0-9]+:/) {
            timestamp = $i; gsub(":", "", timestamp);
            event = $(i+1); gsub(":", "", event);
            starting_detail = i+2; break;
        }
    }
    printf "%-25s | %-5s | %-12s | %-15s | ", task_pid, cpu, timestamp, event;
    for(i=starting_detail; i<=NF; i++) printf "%s ", $i; print ""
}' | stdbuf -oL ./src/parser_live 2>&1 | tee "$OUTPUT_DIR/risultati_live.txt" &
LIVE_PID=$!

# Piccola tregua per permettere al live stream di mettersi in ascolto e stampare a vuoto le prime righe
sleep 1

# Eseguiamo trace-cmd in FOREGROUND bloccante. 
# Si chiuderà automaticamente quando $EXECUTABLE avrà concluso il suo normale ciclo vitale.
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup -o "$OUTPUT_DIR/trace.dat" "$EXECUTABLE" >/dev/null 2>&1

# Appena `trace-cmd` si chiude, triggero la funzione manuale interna di spegnimento
RECORD_PID=""
cleanup
