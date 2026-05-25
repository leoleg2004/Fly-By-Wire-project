#!/bin/bash
# install_kernelshark.sh - Installa KernelShark e tutte le dipendenze del progetto
# Uso: sudo bash bin/tools/install_kernelshark.sh

set -e

echo "========================================================="
echo " Installazione dipendenze Kernel Trace Project"
echo "========================================================="

if [ "$EUID" -ne 0 ]; then
    echo "Questo script richiede i permessi di root."
    echo "Uso: sudo bash $0"
    exit 1
fi

echo ""
echo "[1/3] Aggiornamento repository..."
apt-get update -qq

echo ""
echo "[2/3] Installazione pacchetti Python (numpy, pandas, matplotlib, Pillow)..."
apt-get install -y \
    python3-numpy \
    python3-pandas \
    python3-matplotlib \
    python3-pil \
    python3-pil.imagetk \
    python3-tk \
    pkg-config \
    libopencv-dev

echo ""
echo "[3/3] Installazione KernelShark e trace-cmd..."
apt-get install -y \
    kernelshark \
    trace-cmd

echo ""
echo "========================================================="
echo " Installazione completata!"
echo ""
echo " Verifica:"

# Verify
if command -v kernelshark >/dev/null 2>&1; then
    echo "   ✓ KernelShark installato"
else
    echo "   ✗ KernelShark NON trovato"
fi

if command -v trace-cmd >/dev/null 2>&1; then
    echo "   ✓ trace-cmd installato"
else
    echo "   ✗ trace-cmd NON trovato"
fi

python3 -c "import numpy, pandas, matplotlib, PIL" 2>/dev/null && \
    echo "   ✓ Librerie Python OK (numpy, pandas, matplotlib, Pillow)" || \
    echo "   ✗ Alcune librerie Python mancano"

if command -v pkg-config >/dev/null 2>&1; then
    echo "   ✓ pkg-config e OpenCV installati"
else
    echo "   ✗ pkg-config NON trovato (OpenCV potrebbe mancare)"
fi

echo "========================================================="
