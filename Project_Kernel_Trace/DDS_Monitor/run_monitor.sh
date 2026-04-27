#!/bin/bash
# Wrapper per Fast DDS Monitor
# Forza il caricamento delle librerie dalla cartella del monitor
# (la libfastcdr di sistema e' incompatibile con questa versione)
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
exec "$DIR/fastdds_monitor" "$@"
