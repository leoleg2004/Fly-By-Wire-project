cmake_minimum_required(VERSION 3.10)
project(RMSenzaDDSapp09)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_library(time_library lib/time_library.c)
set_target_properties(time_library PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

add_executable(app09c src/app09c.cpp)
target_link_libraries(app09c time_library pthread m)

add_executable(app09b src/app09b.cpp)
target_link_libraries(app09b time_library pthread m)

add_custom_command(TARGET app09c POST_BUILD COMMAND chmod +x $<TARGET_FILE:app09c>)
add_custom_command(TARGET app09b POST_BUILD COMMAND chmod +x $<TARGET_FILE:app09b>)
