cmake_minimum_required(VERSION 3.10)
project(RMSenzaDDSapp09)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_library(time_library lib/time_library.c)
set_target_properties(time_library PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# === AUTOMAZIONE COMPLETAMENTE DINAMICA ===
# Il CMake cercherà automaticamente tutti i file .cpp dentro src/ e per ognuno creerà l'eseguibile!
# In questo modo, che tu aggiunga app10b, app10c, o modifichi i parser, basta fare `make -j4`
file(GLOB ALL_SOURCES "src/*.cpp")

foreach(SRC_FILE ${ALL_SOURCES})
    # Estrae il nome puro es: "app09b" o "parser_live"
    get_filename_component(TARGET_NAME ${SRC_FILE} NAME_WE)
    
    # Crea un eseguibile per ognuno di essi!
    add_executable(${TARGET_NAME} ${SRC_FILE})
    
    # Linka la libreria del tempo, il multithreading e la matematica a tutti quanti automaticamente
    target_link_libraries(${TARGET_NAME} time_library pthread m)
    
    # Li rende eseguibili in bash
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND chmod +x $<TARGET_FILE:${TARGET_NAME}>)
    
    message(STATUS "-> [AUTOMAZIONE CMake]: Trovato e generato eseguibile: ${TARGET_NAME}")
endforeach()
