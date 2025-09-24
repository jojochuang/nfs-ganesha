#!/bin/bash
# Test script for OFS configuration

set -e

echo "Building OFS configuration test..."

# Create a temporary directory for test compilation
TEST_DIR="/tmp/ofs_test_$$"
mkdir -p "$TEST_DIR"

# Copy necessary files
cp src/FSAL/OFS/fsal_ofs_conf.c "$TEST_DIR/"
cp src/FSAL/OFS/fsal_ofs_internal.h "$TEST_DIR/"
cp src/test/test_ofs_config.c "$TEST_DIR/"

# Create minimal header stubs for compilation
cat > "$TEST_DIR/config.h" << 'EOF'
/* Minimal config.h for testing */
#ifndef _CONFIG_H
#define _CONFIG_H
#endif
EOF

cat > "$TEST_DIR/fsal.h" << 'EOF'
#ifndef _FSAL_H
#define _FSAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Minimal stubs for compilation
typedef struct fsal_export fsal_export;
typedef struct fsal_obj_handle fsal_obj_handle;

#endif
EOF

# Modify the test to include local headers
cd "$TEST_DIR"
sed -i 's|../FSAL/OFS/fsal_ofs_internal.h|fsal_ofs_internal.h|' test_ofs_config.c

echo "Compiling test..."
gcc -I. -DTEST_BUILD -o test_ofs_config test_ofs_config.c fsal_ofs_conf.c

echo "Running test..."
./test_ofs_config

echo "Test output complete."

# Clean up
cd /
rm -rf "$TEST_DIR"

echo "OFS configuration test completed successfully!"