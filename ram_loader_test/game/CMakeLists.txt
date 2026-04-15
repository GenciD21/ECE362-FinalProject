add_executable(demo_game
    tetris.c
    lcd.c
    support.c
)

# Link against pico-sdk libraries — games get full SDK access
target_link_libraries(demo_game
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_timer
    hardware_pio
    hardware_pwm
    hardware_adc
    hardware_clocks
)

# Build as no_flash (RAM-only binary)
pico_set_binary_type(demo_game no_flash)

# Use our custom linker script that places everything at 0x20040000
pico_set_linker_script(demo_game ${CMAKE_CURRENT_SOURCE_DIR}/memmap_game.ld)

# Generate .bin, .hex, .uf2 etc.
pico_add_extra_outputs(demo_game)

# Post-build: generate C header from the .bin for embedding in the loader
add_custom_command(TARGET demo_game POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:demo_game> ${CMAKE_CURRENT_BINARY_DIR}/demo_game.bin
    COMMAND ${CMAKE_COMMAND} -E echo "Generating demo_game_bin.h from demo_game.bin..."
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/../tools/bin2header.py
        ${CMAKE_CURRENT_BINARY_DIR}/demo_game.bin
        ${CMAKE_CURRENT_BINARY_DIR}/demo_game_bin.h
        demo_game_bin
    COMMENT "Converting demo_game.bin to C header"
    VERBATIM
)
