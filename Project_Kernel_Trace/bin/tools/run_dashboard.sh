#!/bin/bash
# run_dashboard.sh - Launcher for the Kernel Trace Unified Dashboard
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

python3 "$PROJECT_ROOT/src/tools/dashboard.py" "$@"
