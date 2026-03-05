cmake_minimum_required(VERSION 3.10)
project(FlightSensorSystem)

set(CMAKE_CXX_STANDARD 17)
set(EPROSIMA_INSTALL "/home/leonardo/eprosima_install")

add_subdirectory(raylib)

include_directories(src ${EPROSIMA_INSTALL}/include raylib/src)
link_directories(${EPROSIMA_INSTALL}/lib)

set(DDS_SRCS
    src/Telemetry.cxx
    src/TelemetryPubSubTypes.cxx
)

# Nuovi sorgenti del layer FBW (Flight Control Law + Computer)
set(FBW_SRCS
    src/FlightControlLaw.cpp
    src/FlightControlComputer.cpp
)

add_executable(FlightSim src/main.cpp src/FlightDisplay.cpp ${DDS_SRCS} ${FBW_SRCS})
target_link_libraries(FlightSim fastdds fastcdr raylib pthread dl m)

add_executable(MonitorApp src/MonitorNode.cpp src/MonitorDisplay.cpp ${DDS_SRCS})
target_link_libraries(MonitorApp fastdds fastcdr raylib pthread dl m)
