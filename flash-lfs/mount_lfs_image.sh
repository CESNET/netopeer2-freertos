#!/bin/bash

# Check if input file argument is provided
if [ $# -eq 0 ]; then
    echo "Error: Input file argument is required"
    echo "Usage: $0 <input_file>"
    exit 1
fi

INPUT_FILE="$1"

# Configuration
BLOCK_SIZE=4096
BLOCK_COUNT=1024
OUTPUT_DIR="output"

if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: File '$INPUT_FILE' not found"
    exit 1
fi

if ! command -v littlefs_extract &> /dev/null; then
    echo "Error: littlefs_extract not found in PATH"
    echo "Please ensure littlefs_extract is installed and available in your PATH"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Extract littlefs filesystem contents to output directory
echo "Extracting littlefs filesystem from '$INPUT_FILE' to '$OUTPUT_DIR'..."
littlefs_extract -i "$INPUT_FILE" -d "$OUTPUT_DIR" -b $BLOCK_SIZE -c $BLOCK_COUNT -f

echo "Done! Filesystem contents extracted to $OUTPUT_DIR"
