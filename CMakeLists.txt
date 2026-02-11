cmake_minimum_required(VERSION 3.10)
project(FBW_System)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 1. PERCORSI INSTALLAZIONE EPROSIMA
list(APPEND CMAKE_PREFIX_PATH "/home/leonardo/eprosima_install")
link_directories("/home/leonardo/eprosima_install/lib")
link_directories("/home/leonardo/eprosima_install/lib64")

# 2. TROVIAMO I PACCHETTI
find_package(fastcdr REQUIRED)
find_package(fastdds REQUIRED)
find_package(Threads REQUIRED)

# 3. GESTIONE SORGENTI
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)


file(GLOB TELEMETRY_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx")

# ESEGUIBILE 1: FlightSim 
add_executable(FlightSim 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp 
    ${TELEMETRY_SRCS}
)

target_link_libraries(FlightSim 
    fastdds 
    fastcdr 
    Threads::Threads
    dl
)

# --- ESEGUIBILE 2: MonitorApp ---
add_executable(MonitorApp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MonitorNode.cpp 
    ${TELEMETRY_SRCS}
)

target_link_libraries(MonitorApp 
    fastdds 
    fastcdr 
    Threads::Threads
    dl
)


