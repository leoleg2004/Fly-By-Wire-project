cmake_minimum_required(VERSION 3.16)

project(rt_tasks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Local Fast DDS install path (override with -DEPROSIMA_INSTALL=...)
set(EPROSIMA_INSTALL "/home/leonardo/eprosima_install" CACHE PATH "Fast DDS install prefix")
list(APPEND CMAKE_PREFIX_PATH "${EPROSIMA_INSTALL}")

find_package(Threads REQUIRED)
find_package(fastdds REQUIRED
    PATHS "${EPROSIMA_INSTALL}/share/fastdds/cmake"
    NO_DEFAULT_PATH
)
find_package(fastcdr REQUIRED
    PATHS "${EPROSIMA_INSTALL}/lib/cmake/fastcdr"
    NO_DEFAULT_PATH
)

add_library(rt_task_library STATIC
    rt_task_library.cpp
    TaskDataPubSubTypes.cpp
)

target_include_directories(rt_task_library PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rt_task_library PUBLIC
    fastdds
    fastcdr
    Threads::Threads
)



set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/executables)
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})


file(COPY 
    ${CMAKE_CURRENT_SOURCE_DIR}/executables/config.xml 
    ${CMAKE_CURRENT_SOURCE_DIR}/executables/configDDS.xml 
    DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
set(APP_LIST
    mem_edf_single
    mem_edf_multi
    mem_rm_single
    mem_rm_multi
    dds_edf_single
    dds_edf_multi
    dds_rm_single
    dds_rm_multi
)

foreach(app IN LISTS APP_LIST)
    add_executable(${app} ${CMAKE_CURRENT_SOURCE_DIR}/executables/${app}.cpp)
    target_link_libraries(${app} PRIVATE rt_task_library)
endforeach()
