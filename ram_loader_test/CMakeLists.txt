cmake_minimum_required(VERSION 3.13)
set(PICO_SDK_FETCH_FROM_GIT on)
include(pico_sdk_import.cmake)

# Pull in SDK (must be before project())
include(pico_sdk_import.cmake)

project(ram_loader_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Default to the Pico 2 / RP2350 SRAM map unless the caller overrides it.
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2 CACHE STRING "Target board")
endif()

# Initialize the Pico SDK
pico_sdk_init()

# Build the game first (produces the .bin we embed in the loader)
add_subdirectory(game)

# Build the loader (depends on game binary)
add_subdirectory(loader)
