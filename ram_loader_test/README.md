# RP2350 RAM Game Loader

A two-part system for the Raspberry Pi Pico 2 (RP2350) that loads and executes
game programs on core 1 from SRAM.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     FLASH (XIP)                         │
│  Loader binary (pico-sdk, boots on core 0)              │
└───────────────────────┬─────────────────────────────────┘
                        │ Boot
┌───────────────────────▼─────────────────────────────────┐
│                     SRAM (520KB)                        │
│                                                         │
│  0x20000000 ┌──────────────────────┐                    │
│             │ Core 0: Loader       │ 256KB               │
│             │ (code + data + heap) │                    │
│  0x20040000 ├──────────────────────┤                    │
│             │ Core 1: Game Binary  │ 240KB               │
│             │ (loaded from binary) │                    │
│  0x2007C000 ├──────────────────────┤                    │
│             │ Core 1: Stack        │ 16KB                │
│  0x20080000 ├──────────────────────┤                    │
│             │ SCRATCH_X            │ 4KB                 │
│  0x20081000 ├──────────────────────┤                    │
│             │ SCRATCH_Y            │ 4KB                 │
│  0x20082000 └──────────────────────┘                    │
└─────────────────────────────────────────────────────────┘
```

## Building

### Prerequisites

- WSL with `arm-none-eabi-gcc` installed
- `cmake` (installed via pip in WSL)
- Pico SDK at `C:\pico-sdk` (accessible as `/mnt/c/pico-sdk` from WSL)

### Build Commands

```bash
# Build everything
wsl bash build.sh

# Clean and rebuild
wsl bash build.sh rebuild

# Clean only
wsl bash build.sh clean
```

### Output Files

| File | Description |
|------|-------------|
| `build/loader/loader.uf2` | Flash this to the Pico 2 |
| `build/game/demo_game.bin` | Raw game binary (240KB max) |
| `build/game/demo_game.elf` | Game ELF with debug symbols |

## Flashing

1. Hold BOOTSEL and plug in the Pico 2
2. Copy `build/loader/loader.uf2` to the `RPI-RP2` drive
3. The Pico will reboot, load the game into SRAM, and start it on core 1

## Creating New Games

1. Create a new `.c` file in the `game/` directory
2. Use pico-sdk functions normally (`gpio_init()`, `sleep_ms()`, etc.)
3. The `main()` function is your game entry point
4. Build - the game is automatically linked at `0x20040000` and embedded in the loader

### Example Game

```c
#include "pico/stdlib.h"
#include "hardware/gpio.h"

int main() {
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    while (true) {
        gpio_put(25, 1);
        sleep_ms(500);
        gpio_put(25, 0);
        sleep_ms(500);
    }
}
```

### Game Constraints

- **Max binary size**: 240KB (code + data + rodata)
- **Stack**: 16KB (at 0x2007C000 - 0x20080000)
- **No flash access**: Games run entirely from SRAM
- **Hardware init**: Already done by the loader (clocks, PLLs, etc.)
- **pico-sdk**: Full access to hardware libraries

## Future: SD Card Loading

The loader is designed to be extended with SD card support:

1. Add SPI + FatFS for SD card access
2. Read `.bin` files from the SD card instead of embedding them
3. Add a game selection menu on core 0
4. Support hot-swapping games (reset core 1, load new binary)

## Project Structure

```
ram_loader_test/
├── CMakeLists.txt          # Top-level CMake
├── pico_sdk_import.cmake   # Pico SDK integration
├── build.sh                # WSL build script
├── loader/
│   ├── CMakeLists.txt      # Loader build config
│   └── main.c              # Core 0: load + launch game
├── game/
│   ├── CMakeLists.txt      # Game build config
│   ├── memmap_game.ld      # Custom linker: RAM at 0x20040000
│   └── demo_game.c         # Demo game (LED blink)
└── tools/
    └── bin2header.py       # Converts .bin to C header
```
