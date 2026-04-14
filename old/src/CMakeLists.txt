add_executable(loader
    main.c
)

target_link_libraries(loader
    pico_stdlib
    pico_multicore
)

# Enable UART serial output for debug messages
# NOTE: Switch to USB if you initialize TinyUSB:
#   cd C:\pico-sdk && git submodule update --init lib/tinyusb
#   Then change the lines below to: usb=1, uart=0
pico_enable_stdio_usb(loader 0)
pico_enable_stdio_uart(loader 1)

# The loader needs to know where the game binary header is
target_include_directories(loader PRIVATE
    ${CMAKE_BINARY_DIR}/game
)

# Generate .uf2 for flashing
pico_add_extra_outputs(loader)

# Loader depends on the game being built first (for the embedded header)
add_dependencies(loader demo_game)
