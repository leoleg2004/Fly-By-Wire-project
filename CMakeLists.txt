cmake_minimum_required(VERSION 3.10)
project(FlightSensorSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_PREFIX_PATH "/home/leonardo/eprosima_install")

find_package(fastcdr REQUIRED)
find_package(fastdds REQUIRED)
find_package(raylib REQUIRED)

include_directories(src "/home/leonardo/eprosima_install/include")
link_directories("/home/leonardo/eprosima_install/lib")


set(DDS_SRCS 
    src/TelemetryPubSubTypes.cxx 
    src/TelemetryTypeObjectSupport.cxx
)

add_executable(FlightSim src/main.cpp src/FlightDisplay.cpp ${DDS_SRCS})
target_link_libraries(FlightSim fastdds fastcdr raylib pthread dl m)

add_executable(MonitorApp src/MonitorNode.cpp ${DDS_SRCS})
target_link_libraries(MonitorApp fastdds fastcdr pthread dl)
add_executable(DDSCORE rt_tests/DDSCORE.cpp ${DDS_SRCS})
target_link_libraries(DDSCORE fastdds fastcdr pthread)
add_executable(DDSMCORE rt_tests/DDSMCORE.cpp ${DDS_SRCS})
target_link_libraries(DDSMCORE fastdds fastcdr pthread)
add_executable(DDSEDFSCORE rt_tests/DDSMCORE.cpp ${DDS_SRCS})
target_link_libraries(DDSEDFSCORE fastdds fastcdr pthread)
add_executable(DDSEDFMCORE rt_tests/DDSMCORE.cpp ${DDS_SRCS})
target_link_libraries(DDSEDFMCORE fastdds fastcdr pthread)

add_executable(DDSMCORE rt_tests/DDSMCORE.cpp ${DDS_SRCS})
target_link_libraries(DDSMCORE fastdds fastcdr pthread)

# CORRETTI QUESTI DUE:
add_executable(DDSEDFSCORE rt_tests/DDSEDFSCORE.cpp ${DDS_SRCS})
target_link_libraries(DDSEDFSCORE fastdds fastcdr pthread)

add_executable(DDSEDFMCORE rt_tests/DDSEDFMCORE.cpp ${DDS_SRCS})
target_link_libraries(DDSEDFMCORE fastdds fastcdr pthread)
