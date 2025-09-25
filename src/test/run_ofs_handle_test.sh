#!/bin/bash

# Simple test script to compile and run OFS file handle tests

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(dirname "$SCRIPT_DIR")"

echo "Testing OFS file handle encoding/decoding..."

# Create a temporary build directory
TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Copy necessary files
cp "$SCRIPT_DIR/test_ofs_file_handle.c" .
cp "$SRC_DIR/FSAL/OFS/fsal_ofs_internal.h" .

# Compile the test
gcc -std=c99 -Wall -Wextra -I. -DSTANDALONE_TEST \
    -o test_ofs_file_handle test_ofs_file_handle.c

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    cd - > /dev/null
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Run the test
echo "Running tests..."
./test_ofs_file_handle

TEST_RESULT=$?

# Clean up
cd - > /dev/null
rm -rf "$TEMP_DIR"

if [ $TEST_RESULT -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "Tests failed!"
    exit 1
fi