#!/bin/bash

# Find the first USB serial device
DEVICE=$(ls /dev/tty.usbserial-* 2>/dev/null | head -1)

if [ -z "$DEVICE" ]; then
    echo "Error: No USB serial device found"
    exit 1
fi

echo "Found USB device: $DEVICE"

# Check if esphome is installed natively
if ! command -v esphome &> /dev/null; then
    echo "Error: esphome not found. Install with: pip install esphome"
    exit 1
fi

# Build in Docker
echo "Building in Docker..."
docker run --rm -it \
    -v "$(pwd):$(pwd)" \
    -v "$(pwd)/.platformio:/config/.esphome/platformio" \
    -w "$(pwd)" \
    ghcr.io/esphome/esphome compile components/pcm5122/yaml/idf-media-player.yaml

if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

# Flash natively
echo "Flashing device at $DEVICE..."
esphome upload components/pcm5122/yaml/idf-media-player.yaml --device "$DEVICE"
