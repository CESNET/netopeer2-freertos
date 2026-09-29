#!/bin/bash

# Check if output folder argument is provided
if [ $# -eq 0 ]; then
    echo "Error: Output folder argument is required"
    echo "Usage: $0 <output_folder>"
    exit 1
fi

OUTPUT_FOLDER="$1"

# Run sysrepo initialization before creating the image
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Initializing sysrepo..."
if ! "$SCRIPT_DIR/init_sysrepo.sh"; then
    echo "Error: init_sysrepo.sh failed"
    exit 1
fi

# Configuration
BLOCK_SIZE=4096
BLOCK_COUNT=1024
OUTPUT_FILE="lfs.bin"
INPUT_DIR="input"

if ! command -v littlefs_create &> /dev/null; then
    echo "Error: littlefs_create not found in PATH"
    echo "Please ensure littlefs_create is installed and available in your PATH"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_FOLDER"

# Create a 2MB bin file with dd (block size 4096 bytes)
echo "Creating ${BLOCK_SIZE} x ${BLOCK_COUNT} = $((BLOCK_SIZE * BLOCK_COUNT / 1024 / 1024))MB binary file..."
dd if=/dev/zero of="$OUTPUT_FOLDER/$OUTPUT_FILE" bs=$BLOCK_SIZE count=$BLOCK_COUNT

# Use littlefs_create to populate the filesystem with input folder contents
echo "Populating littlefs filesystem with $INPUT_DIR folder contents..."
littlefs_create -s $INPUT_DIR -i "$OUTPUT_FOLDER/$OUTPUT_FILE" -b $BLOCK_SIZE -c $BLOCK_COUNT

echo "Done! Filesystem image created at $OUTPUT_FOLDER/$OUTPUT_FILE"
