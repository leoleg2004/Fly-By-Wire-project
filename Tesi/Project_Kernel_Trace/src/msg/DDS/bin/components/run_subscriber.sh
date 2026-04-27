#!/bin/bash
#fatto per poter lanciare il subacriber senza avere problemi di path
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../../environment.sh"
"$SCRIPT_DIR/ImageSubscriber" "$@"
