#!/bin/bash

# Build Script for App10 Family (Memory vs DDS, EDF vs RM, Single vs Multi Core)

cd /home/leonardo/eprosima_projects/flight_sensor/flight_sensor/rt_tests

echo "1. Compiling Shared RT Task Library (app09-style payload)..."
g++ -c -O2 -std=c++17 rt_task_library.cpp -pthread

echo "2. Building the 8 Executables..."
mkdir -p executables
\cp -f config.xml executables/

EXECUTABLES=(
    "mem_edf_single"
    "mem_edf_multi"
    "mem_rm_single"
    "mem_rm_multi"
    "dds_edf_single"
    "dds_edf_multi"
    "dds_rm_single"
    "dds_rm_multi"
)

for APP in "${EXECUTABLES[@]}"; do
    echo "   -> Compiling $APP"
    g++ -O2 -std=c++17 executables/${APP}.cpp rt_task_library.o -o executables/$APP \
        -I. -pthread
done

echo ""
echo "Build Sequence Complete! 8 Executables generated in executables/."
echo "You can now run any of them natively with sudo. Example:"
echo "cd executables && sudo ./dds_rm_multi"
