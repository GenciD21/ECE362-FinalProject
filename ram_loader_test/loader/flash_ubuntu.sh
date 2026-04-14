#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

echo "=== 1. Compiling Project ==="
unset PICO_SDK_PATH

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DPICO_BOARD=pico2 ..
make -j4

if [ $? -ne 0 ]; then
    echo "========================================================="
    echo "ERROR: Compilation failed. Please check the errors above."
    echo "========================================================="
    exit 1
fi

echo ""
echo "=== 2. Flashing via USB (Requires BOOTSEL mode) ==="
# Since we are inside the build folder, the path is 'loader/loader.uf2'
picotool load -x loader/loader.uf2

if [ $? -ne 0 ]; then
    echo "========================================================="
    echo "ERROR: Picotool failed to find an RP-series device."
    echo "Please hold the BOOTSEL button and plug your Pico in,"
    echo "then run this script again!"
    echo "========================================================="
    exit 1
fi

echo ""
echo "=== 3. Code Uploaded Successfully! ==="

echo "Opening Serial port (Press Ctrl+C to exit)..."
sleep 2  # give it more time
PORT=$(ls /dev/ttyACM* 2>/dev/null | head -n 1)
if [ -z "$PORT" ]; then
    echo "No ttyACM found, trying ttyUSB..."
    PORT=$(ls /dev/ttyUSB* 2>/dev/null | head -n 1)
fi
echo "Using port: $PORT"
stty -F "$PORT" 115200 raw -echo
cat "$PORT"
