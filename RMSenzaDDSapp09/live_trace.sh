#!/bin/bash

# Generiamo una cartella dedicata con il Timestamp per mantenere tutto pulito!
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_DIR="trace_results_live_${TIMESTAMP}"
mkdir -p "$OUTPUT_DIR"

# Assicuriamoci che non esistano "file orfani" nella root eliminandoli dal comando grep e ridirezionando
# tutto esclusivamente dentro a $OUTPUT_DIR.

# Abilita la pulizia sicura alla chiusura (CTRL+C)
cleanup() {
    echo -e "\n[!] Interruzione ricevuta (CTRL+C). Salvataggio finale in corso..."
    
    # 1. Spegni l'istanza live ftrace 
    sudo sh -c "echo 0 > $LIVE_INST/tracing_on" 2>/dev/null
    sudo rmdir "$LIVE_INST" 2>/dev/null
    
    # 2. Ferma il processo "trace-cmd record" in background, mandandogli un SIGINT per fargli scrivere il trace.dat corretto
    echo "Scrittura di trace.dat in corso per kernelshark..."
    sudo pkill -INT -x trace-cmd
    sleep 1
    
    # 3. Pulizia finali
    sudo chown $USER:$USER "$OUTPUT_DIR"/* 2>/dev/null
    sudo chmod 666 "$OUTPUT_DIR/trace.dat" 2>/dev/null
    
    echo "==============================================================="
    echo "Tracciamento completato."
    echo "Tutti i risultati, tracciati live e binari per KernelShark, si trovano nella cartella:"
    echo "   -> $OUTPUT_DIR"
    echo "Puoi analizzare in seguito con:"
    echo "    kernelshark $OUTPUT_DIR/trace.dat"
    echo "==============================================================="
    exit 0
}
trap cleanup SIGINT

echo "==============================================================="
echo " AVVIO LETTURA LIVE DA KERNEL E REGISTRAZIONE DUAL TRACE.DAT "
echo " Cartella di riferimento: $OUTPUT_DIR"
echo " ------------------------------------------------------------- "
echo " Mantieni questo terminale aperto. Dal secondo terminale, lancia:"
echo "     ./build/app09b   (o il simulatore che preferisci)"
echo " Premi CTRL+C per interrompere la visualizzazione Live e generare il .dat"
echo "==============================================================="

# Compila il parser C++ per sicurezza
(cd src && g++ parser.cpp -o parser)

echo "-> Avvio 'trace-cmd record' in background per KernelShark..."
# Avviamo trace-cmd in background per salvare il .dat per kernelshark SENZA visualizzarlo
sudo trace-cmd record -e sched:sched_switch -e sched:sched_wakeup -o "$OUTPUT_DIR/trace.dat" >/dev/null 2>&1 &
RECORD_PID=$!

echo "-> Creo una istanza ftrace separata per la lettura cronologica live..."
# Per non scontrarsi col trace-cmd di base, creiamo una istanza ftrace "copia" in esecuzione parallela per noi.
LIVE_INST="/sys/kernel/debug/tracing/instances/live_trace_inst_$$"
sudo mkdir -p "$LIVE_INST"
sudo sh -c "echo 1 > $LIVE_INST/events/sched/sched_switch/enable"
sudo sh -c "echo 1 > $LIVE_INST/events/sched/sched_wakeup/enable"
sudo sh -c "echo 1 > $LIVE_INST/tracing_on"

echo "-> Inizio ascolto degli eventi dello scheduler in Real-Time..."
echo "---------------------------------------------------------------"

# Leggi dalla pipe esclusiva della nostra istanza
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
}' | stdbuf -oL ./src/parser 2>&1 | tee "$OUTPUT_DIR/risultati_live.txt"

# Verrà chiamato solo se cadesse la cat in input.
cleanup
