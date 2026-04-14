#!/bin/bash
# build.sh - Build the RP2350 RAM loader project using WSL
#
# Usage:
#   wsl bash build.sh          # Build everything
#   wsl bash build.sh clean    # Clean build directory
#   wsl bash build.sh rebuild  # Clean + rebuild

set -e

# Resolve paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
PICO_SDK_PATH="~/pico-sdk"

# Add cmake from pip install location to PATH
export PATH="$HOME/.local/bin:$PATH"

# Verify tools
echo "=== Checking build tools ==="
echo "CMake:  $(cmake --version | head -1)"
echo "GCC:    $(arm-none-eabi-gcc --version | head -1)"
echo "SDK:    ${PICO_SDK_PATH}"
echo ""

# Handle arguments
case "${1}" in
    clean)
        echo "=== Cleaning build directory ==="
        rm -rf "${BUILD_DIR}"
        echo "Done."
        exit 0
        ;;
    rebuild)
        echo "=== Rebuilding from scratch ==="
        rm -rf "${BUILD_DIR}"
        ;;
esac

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake (only if not already configured)
if [ ! -f "Makefile" ] && [ ! -f "build.ninja" ]; then
    echo "=== Configuring with CMake ==="
    cmake \
        -DPICO_SDK_PATH="${PICO_SDK_PATH}" \
        -DPICO_BOARD=pico2 \
        -DCMAKE_BUILD_TYPE=Release \
        "${SCRIPT_DIR}"
fi

# Build
echo ""
echo "=== Building ==="
cmake --build . --parallel $(nproc)

echo ""
echo "=== Build complete ==="
echo ""

# Show output files
if [ -f "loader/loader.uf2" ]; then
    echo "Loader UF2:      $(ls -lh loader/loader.uf2 | awk '{print $5}')"
fi
if [ -f "game/demo_game.bin" ]; then
    echo "Game binary:     $(ls -lh game/demo_game.bin | awk '{print $5}')"
fi
if [ -f "game/demo_game_bin.h" ]; then
    echo "Game C header:   game/demo_game_bin.h"
fi

echo ""
echo "To flash: copy build/loader/loader.uf2 to the Pico 2 in BOOTSEL mode"
