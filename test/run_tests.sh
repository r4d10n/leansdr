#!/bin/bash
# Quick test runner script for LeanSDR NEON optimization tests

set -e

echo "========================================"
echo "LeanSDR NEON Test Runner"
echo "========================================"

# Check if we're on ARM
ARCH=$(uname -m)
echo "Architecture: $ARCH"

if [[ "$ARCH" == "armv7l" || "$ARCH" == "aarch64" ]]; then
    echo "ARM detected - NEON optimizations available"
    NEON_AVAILABLE=1
else
    echo "Non-ARM platform - testing scalar fallback only"
    NEON_AVAILABLE=0
fi

# Build and run tests
echo ""
echo "Building tests..."
make clean > /dev/null 2>&1
make

echo ""
echo "Running tests..."
./test_neon

EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "✓ All tests passed!"
    if [ $NEON_AVAILABLE -eq 1 ]; then
        echo ""
        echo "Checking for NEON instructions in binary..."
        make check-neon
    fi
else
    echo "✗ Some tests failed (exit code: $EXIT_CODE)"
fi

exit $EXIT_CODE
