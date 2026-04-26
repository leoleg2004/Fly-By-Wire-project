cmake_minimum_required(VERSION 3.10)
project(FlightSensorSystem)

set(CMAKE_CXX_STANDARD 17)
set(EPROSIMA_INSTALL "/home/leonardo/eprosima_install")

add_subdirectory(raylib)

include_directories(src ${EPROSIMA_INSTALL}/include raylib/src)
link_directories(${EPROSIMA_INSTALL}/lib)

# =========================================================================
# IDL Regeneration target (optional — requires fastddsgen).
# =========================================================================
find_program(FASTDDSGEN_EXE fastddsgen
    HINTS "${EPROSIMA_INSTALL}/bin" /usr/local/bin /opt/fastdds/bin)

if(FASTDDSGEN_EXE)
    message(STATUS "fastddsgen: ${FASTDDSGEN_EXE}")
    add_custom_target(regenerate_idl
        COMMAND ${FASTDDSGEN_EXE} -replace -d ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/F16Kinematics.idl
        COMMAND ${FASTDDSGEN_EXE} -replace -d ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/F16AeroState.idl
        COMMAND ${FASTDDSGEN_EXE} -replace -d ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/F16Actuators.idl
        COMMENT "Regenerating DDS type stubs — PubSubTypes NOT overwritten"
        VERBATIM
    )
else()
    message(STATUS "fastddsgen not found — using pre-written DDS type files")
    message(STATUS "  Expected: ${EPROSIMA_INSTALL}/bin/fastddsgen")
    message(STATUS "  After install: cmake --build . --target regenerate_idl")
endif()

# =========================================================================
# DDS serialization sources — 3 topics, manual CDR (no fastddsgen required)
# Replaces legacy Telemetry.cxx + TelemetryPubSubTypes.cxx
# =========================================================================
set(DDS_SRCS
    src/F16KinematicsPubSubTypes.cxx
    src/F16AeroStatePubSubTypes.cxx
    src/F16ActuatorsPubSubTypes.cxx
)

# FBW layer
set(FBW_SRCS
    src/FlightControlLaw.cpp
    src/FlightControlComputer.cpp
)

# ── FlightSim: simulator + Raylib renderer + DDS publisher + FBW ─────────
add_executable(FlightSim
    src/main.cpp
    src/FlightDisplay.cpp
    ${DDS_SRCS}
    ${FBW_SRCS}
)
target_link_libraries(FlightSim fastdds fastcdr raylib pthread dl m)

# ── MonitorApp: ground station + Raylib display + 3 DDS subscribers ──────
add_executable(MonitorApp
    src/MonitorNode.cpp
    src/MonitorDisplay.cpp
    ${DDS_SRCS}
)
target_link_libraries(MonitorApp fastdds fastcdr raylib pthread dl m)

# =========================================================================
# Resource Management — Copy assets to build directory
# =========================================================================
set(RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")
add_custom_target(copy_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCES_DIR}/sky ${CMAKE_BINARY_DIR}/sky
    COMMAND ${CMAKE_COMMAND} -E copy ${RESOURCES_DIR}/aerodrome.glb ${RESOURCES_DIR}/f35.glb ${CMAKE_BINARY_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${RESOURCES_DIR}/air.wav ${RESOURCES_DIR}/caution.wav ${RESOURCES_DIR}/engine_down.wav ${RESOURCES_DIR}/engine_loop.wav ${RESOURCES_DIR}/engine_start.wav ${RESOURCES_DIR}/gear.wav ${RESOURCES_DIR}/landing.wav ${RESOURCES_DIR}/pull_up.wav ${RESOURCES_DIR}/warning.wav ${CMAKE_BINARY_DIR}/
    COMMENT "Copying flight simulator resources to build directory..."
)
add_dependencies(FlightSim copy_resources)
add_dependencies(MonitorApp copy_resources)
