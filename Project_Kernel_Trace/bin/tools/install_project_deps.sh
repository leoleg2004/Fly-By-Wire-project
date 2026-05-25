#!/bin/bash
# install_project_deps.sh - Install dependencies for Project Kernel Trace
#
# Uso: sudo bash install_project_deps.sh
#
# Installa:
#   - Java 11 (per fastddsgen)
#   - Librerie di sviluppo (OpenSSL, Asio, TinyXML2, Boost, Qt5, OpenCV)
#   - Fast-DDS v2.14.2 (foonathan_memory, Fast-CDR v2.1.3, Fast-DDS, Fast-DDS-Gen v3.3.0)
#   - Configura LD_LIBRARY_PATH e JAVA_HOME

set -e

echo "========================================================="
echo " Installazione dipendenze Kernel Trace Project (Incluso DDS)"
echo "========================================================="

if [ "$EUID" -ne 0 ]; then
    echo "Questo script richiede i permessi di root."
    echo "Uso: sudo bash $0"
    exit 1
fi

# Determina l'utente reale (chi ha lanciato sudo)
REAL_USER="${SUDO_USER:-$(logname 2>/dev/null || echo $USER)}"
REAL_HOME=$(eval echo "~$REAL_USER")

echo ""
echo "==> Utente rilevato: $REAL_USER (home: $REAL_HOME)"

# ─────────────────────────────────────────────────────────────
# 1. Java 11
# ─────────────────────────────────────────────────────────────
echo ""
echo "### [1/5] Installare Java 11 ###"
apt-get update -qq
apt-get install -y openjdk-11-jre-headless openjdk-11-jdk-headless

# Find JAVA_HOME dynamically (works on both x86_64 and arm64)
DYNAMIC_JAVA_HOME=$(dirname $(dirname $(readlink -f $(command -v javac))))

# Add to profile if not already there
if ! grep -q "JAVA_HOME=$DYNAMIC_JAVA_HOME" "$REAL_HOME/.profile" 2>/dev/null; then
    echo "export JAVA_HOME=$DYNAMIC_JAVA_HOME" >> "$REAL_HOME/.profile"
    echo 'export PATH=$JAVA_HOME/bin:$PATH' >> "$REAL_HOME/.profile"
    echo ">> Aggiunto JAVA_HOME in ~/.profile"
fi

export JAVA_HOME=$DYNAMIC_JAVA_HOME
export PATH=$JAVA_HOME/bin:$PATH

# ─────────────────────────────────────────────────────────────
# 2. Utilities e librerie di sviluppo
# ─────────────────────────────────────────────────────────────
echo ""
echo "### [2/5] Installare Utilities ###"
apt-get install -y \
    libssl-dev \
    libasio-dev \
    libtinyxml2-dev \
    git \
    build-essential \
    cmake \
    g++ \
    terminator \
    net-tools \
    libboost-all-dev \
    qtbase5-dev \
    qt5-qmake \
    qtcreator

# ─────────────────────────────────────────────────────────────
# 3. OpenCV
# ─────────────────────────────────────────────────────────────
echo ""
echo "### [3/5] Installare OpenCV (4.x) ###"
apt-get install -y \
    libopencv-dev \
    python3-opencv \
    pkg-config

# ─────────────────────────────────────────────────────────────
# 4. Fast-DDS v2.14.2 dai sorgenti
# ─────────────────────────────────────────────────────────────
echo ""
echo "### [4/5] Compilazione e Installazione Fast-DDS v2.14.2 dai sorgenti ###"
echo ""
echo "  Versioni selezionate:"
echo "    - foonathan_memory_vendor: latest"
echo "    - Fast-CDR:               v2.1.3"
echo "    - Fast-DDS:               v2.14.2"
echo "    - Fast-DDS-Gen:           v3.3.0"
echo ""

WORKSPACE="$REAL_HOME/TOOLS/eProsima/src"
mkdir -p "$WORKSPACE"

# --- 4a. foonathan_memory_vendor ---
echo "-> [4a/4d] Build foonathan_memory_vendor..."
cd "$WORKSPACE"
if [ ! -d foonathan_memory_vendor ]; then
    git clone https://github.com/eProsima/foonathan_memory_vendor.git
fi
cd foonathan_memory_vendor
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON
cmake --build . --target install -j$(nproc)

# --- 4b. Fast-CDR v2.2.2 ---
echo "-> [4b/4d] Build Fast-CDR v2.2.2..."
cd "$WORKSPACE"
if [ ! -d Fast-CDR ]; then
    git clone https://github.com/eProsima/Fast-CDR.git -b v2.2.2
else
    cd Fast-CDR && git fetch && git checkout v2.2.2 && cd ..
fi
cd Fast-CDR
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build . --target install -j$(nproc)

# --- 4c. Fast-DDS v2.14.2 ---
echo "-> [4c/4d] Build Fast-DDS v2.14.2..."
cd "$WORKSPACE"
if [ ! -d Fast-DDS ]; then
    git clone https://github.com/eProsima/Fast-DDS.git -b v2.14.2
fi
cd Fast-DDS

# ── Patch per compatibilità GCC 15 ──
# GCC 15 richiede #include <cstdint> esplicito in molti file.
# Questa patch aggiunge l'include dove manca, senza modificare
# file che lo hanno già.
echo "   Applicazione patch GCC 15 (aggiunta #include <cstdint>)..."
find src/cpp include -type f \( -name "*.hpp" -o -name "*.h" -o -name "*.cpp" \) \
    -exec grep -lE 'uint[0-9]|int[0-9]|size_t' {} \; | while read f; do
    if ! grep -q '#include <cstdint>' "$f" && ! grep -q '#include <cstddef>' "$f"; then
        sed -i '0,/^#include/{s/^#include/#include <cstdint>\n#include/}' "$f"
    fi
done

mkdir -p build && cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_CXX_STANDARD=14 \
    -DTHIRDPARTY=ON \
    -DSECURITY=OFF
cmake --build . --target install -j$(nproc)

# --- 4d. Fast-DDS-Gen v3.3.0 ---
echo "-> [4d/4d] Build Fast-DDS-Gen v3.3.0..."
cd "$WORKSPACE"
if [ ! -d Fast-DDS-Gen ]; then
    git clone --recursive https://github.com/eProsima/Fast-DDS-Gen.git -b v3.3.0
fi
cd Fast-DDS-Gen

# Gradle 7.6 (usato da Fast-DDS-Gen v3.3.0) non supporta Java > 19.
# Forziamo JAVA_HOME su Java 11 installato in precedenza.
JAVA11_HOME=$(dirname $(dirname $(readlink -f $(command -v javac))))
# Cerca Java 11 specificamente se il default è più recente
if [ -d /usr/lib/jvm/java-11-openjdk-$(dpkg --print-architecture) ]; then
    JAVA11_HOME="/usr/lib/jvm/java-11-openjdk-$(dpkg --print-architecture)"
fi
echo "   Usando Java 11: $JAVA11_HOME"
JAVA_HOME="$JAVA11_HOME" ./gradlew assemble
cp scripts/fastddsgen /usr/local/bin/
mkdir -p /usr/local/share/fastddsgen/java
cp share/fastddsgen/java/fastddsgen.jar /usr/local/share/fastddsgen/java/

# Aggiorna la cache delle librerie
ldconfig

# Fix permessi
chown -R "$REAL_USER:$REAL_USER" "$REAL_HOME/TOOLS"

# ─────────────────────────────────────────────────────────────
# 5. Variabili d'ambiente
# ─────────────────────────────────────────────────────────────
echo ""
echo "### [5/5] Configurazione variabili d'ambiente ###"

if ! grep -q "LD_LIBRARY_PATH=/usr/local/lib" "$REAL_HOME/.profile" 2>/dev/null; then
    echo "export LD_LIBRARY_PATH=/usr/local/lib/" >> "$REAL_HOME/.profile"
    echo ">> Aggiunto LD_LIBRARY_PATH in ~/.profile"
fi

# ─────────────────────────────────────────────────────────────
# Verifica finale
# ─────────────────────────────────────────────────────────────
echo ""
echo "========================================================="
echo " Verifica installazione"
echo "========================================================="
echo ""

# Verifica che libfastrtps esista
if [ -f /usr/local/lib/libfastrtps.so ]; then
    echo " ✓ libfastrtps.so installata"
else
    echo " ✗ ATTENZIONE: libfastrtps.so NON trovata!"
fi

# Verifica che libfastcdr esista
if [ -f /usr/local/lib/libfastcdr.so ]; then
    echo " ✓ libfastcdr.so installata"
else
    echo " ✗ ATTENZIONE: libfastcdr.so NON trovata!"
fi

# Verifica fastddsgen
if command -v fastddsgen &>/dev/null; then
    echo " ✓ fastddsgen disponibile"
else
    echo " ✗ ATTENZIONE: fastddsgen NON trovato in PATH!"
fi

# Verifica header
if [ -d /usr/local/include/fastrtps ]; then
    echo " ✓ Header fastrtps/ presenti"
else
    echo " ✗ ATTENZIONE: Header fastrtps/ NON trovati!"
fi

if [ -d /usr/local/include/fastdds ]; then
    echo " ✓ Header fastdds/ presenti"
else
    echo " ✗ ATTENZIONE: Header fastdds/ NON trovati!"
fi

echo ""
echo "========================================================="
echo " Installazione completata!"
echo " Esegui 'source ~/.profile' o riavvia il terminale"
echo " per applicare le variabili d'ambiente."
echo ""
echo " Ora puoi compilare le app DDS del progetto:"
echo "   cd src/app && make"
echo "========================================================="
