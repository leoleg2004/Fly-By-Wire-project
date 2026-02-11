cmake_minimum_required(VERSION 3.10)
project(FBW_System)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- 1. DEFINIAMO I PERCORSI SACRI ---
# Qui ci sono gli header (.hpp)
list(APPEND CMAKE_PREFIX_PATH "/home/leonardo/eprosima_install")

# --- 2. AGGIUNGIAMO I PERCORSI DELLE LIBRERIE (.so) ---
# QUESTO MANCAVA! Senza questo, il linker non trova i file binari.
link_directories("/home/leonardo/eprosima_install/lib")
# Aggiungo anche lib64 per sicurezza (alcune distro usano quello)
link_directories("/home/leonardo/eprosima_install/lib64")

# 3. Troviamo i pacchetti (solo per configurazione)
find_package(fastcdr REQUIRED)
find_package(fastdds REQUIRED)
find_package(Threads REQUIRED)

include_directories(src)

# 4. Raccogliamo tutti i file sorgente generati
file(GLOB TELEMETRY_SRCS "src/*.cxx")
# Rimuoviamo i main manuali dalla lista automatica
list(REMOVE_ITEM TELEMETRY_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
list(REMOVE_ITEM TELEMETRY_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/MonitorNode.cpp")

# --- ESEGUIBILE 1: FlightSim ---
add_executable(FlightSim src/main.cpp ${TELEMETRY_SRCS})

# FIX DEFINITIVO LINKER:
# Invece di usare variabili che potrebbero essere vuote, scriviamo
# i nomi espliciti delle librerie che il Linker cercherà nelle cartelle sopra.
target_link_libraries(FlightSim 
    fastdds   # Cerca libfastdds.so
    fastcdr   # Cerca libfastcdr.so
    Threads::Threads
    dl        # Serve spesso su Linux per i simboli dinamici
)

# --- ESEGUIBILE 2: MonitorApp ---
add_executable(MonitorApp src/MonitorNode.cpp ${TELEMETRY_SRCS})

target_link_libraries(MonitorApp 
    fastdds 
    fastcdr 
    Threads::Threads
    dl
)
