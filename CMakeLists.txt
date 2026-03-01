cmake_minimum_required(VERSION 3.10)
project(FlightSensorSystem)

set(CMAKE_CXX_STANDARD 17)
set(EPROSIMA_INSTALL "/home/leonardo/eprosima_install")

# 1. Diciamo a CMake di compilare il TUO raylib locale!
add_subdirectory(raylib)

# 2. Includiamo le cartelle giuste
include_directories(src ${EPROSIMA_INSTALL}/include raylib/src)
link_directories(${EPROSIMA_INSTALL}/lib)

set(DDS_SRCS 
    src/Telemetry.cxx 
    src/TelemetryPubSubTypes.cxx 
)

add_executable(FlightSim src/main.cpp src/FlightDisplay.cpp ${DDS_SRCS})
# Ora 'raylib' punterà alla tua cartella, non a quella di sistema!
target_link_libraries(FlightSim fastdds fastcdr raylib pthread dl m)

add_executable(MonitorApp src/MonitorNode.cpp src/MonitorDisplay.cpp ${DDS_SRCS})
target_link_libraries(MonitorApp fastdds fastcdr raylib pthread dl m)
