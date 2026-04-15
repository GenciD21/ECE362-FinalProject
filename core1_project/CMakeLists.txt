cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(core1_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(core1_project main.c)

target_link_libraries(core1_project
    pico_stdlib
    pico_multicore
)

# No USB/UART output needed on core1 side — saves space
pico_enable_stdio_usb(core1_project 0)
pico_enable_stdio_uart(core1_project 0)

# Strip to raw binary after build
add_custom_command(TARGET core1_project POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:core1_project>
            ${CMAKE_BINARY_DIR}/core1.bin
    COMMENT "Stripping core1 ELF to core1.bin"
)