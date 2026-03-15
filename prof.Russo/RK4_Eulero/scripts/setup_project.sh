#!/bin/bash
# Setup script for F-16 RK4_Eulero project — Eclipse/PyDev compatibility

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Project root: $PROJECT_ROOT"

# ── Check for venv ────────────────────────────────────────────────────────
if [ -d "$PROJECT_ROOT/venv" ]; then
    echo "Activating venv..."
    source "$PROJECT_ROOT/venv/bin/activate"
    echo "✅ venv activated"
elif [ -d "$PROJECT_ROOT/simulazione/py/venv" ]; then
    echo "Activating simulazione/py/venv..."
    source "$PROJECT_ROOT/simulazione/py/venv/bin/activate"
    echo "✅ venv activated"
else
    echo "ℹ️  No venv found. Using system Python."
fi

# ── Set PYTHONPATH for Eclipse ────────────────────────────────────────────
export PYTHONPATH="$PROJECT_ROOT:$PROJECT_ROOT/simulazione/py:$PYTHONPATH"
echo "✅ PYTHONPATH set: $PYTHONPATH"

# ── Validate environment ──────────────────────────────────────────────────
echo ""
echo "Validating environment..."
python3 "$PROJECT_ROOT/scripts/validate_env.py"

echo ""
echo "✅ Setup complete. Ready to run simulazione."
