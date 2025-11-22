#!/bin/bash
# Build and test script for LeanSDR NEON FFT optimization
# Copyright (C) 2016-2025 <pabr@pabr.org>

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Detect architecture
ARCH=$(uname -m)

echo -e "${BLUE}=====================================================================${NC}"
echo -e "${BLUE}LeanSDR NEON FFT - Build and Test Script${NC}"
echo -e "${BLUE}=====================================================================${NC}"
echo ""
echo "Architecture: $ARCH"
echo ""

# Check if we're on ARM
if [[ "$ARCH" == "armv7l" || "$ARCH" == "aarch64" ]]; then
    echo -e "${GREEN}✓ ARM architecture detected - NEON optimizations available${NC}"
    NEON_AVAILABLE=1
else
    echo -e "${YELLOW}⚠ Non-ARM architecture - will use scalar fallback${NC}"
    NEON_AVAILABLE=0
fi
echo ""

# Check for Ne10 library
echo -e "${BLUE}Checking for Ne10 library...${NC}"
if ldconfig -p | grep -q libNE10; then
    echo -e "${GREEN}✓ Ne10 library found${NC}"
    NE10_AVAILABLE=1
elif [ -f /usr/local/lib/libNE10.so ] || [ -f /usr/lib/libNE10.so ]; then
    echo -e "${GREEN}✓ Ne10 library found${NC}"
    NE10_AVAILABLE=1
else
    echo -e "${YELLOW}⚠ Ne10 library not found (optional, but provides best performance)${NC}"
    echo -e "  To install: sudo apt-get install libne10-dev"
    NE10_AVAILABLE=0
fi
echo ""

# Build options menu
echo -e "${BLUE}=====================================================================${NC}"
echo -e "${BLUE}Build Options:${NC}"
echo -e "${BLUE}=====================================================================${NC}"
echo "1. Quick test (NEON auto-detect, -O2)"
echo "2. Release build (NEON, -O3, optimized)"
echo "3. Ne10 build (best performance, requires libne10)"
echo "4. Scalar-only (no NEON, for comparison)"
echo "5. Debug build (with symbols, -g -O0)"
echo "6. Cross-compile for ARM (from x86)"
echo "7. Build all variants and compare"
echo "8. Clean build directory"
echo ""
read -p "Select option [1-8]: " choice

case $choice in
    1)
        echo -e "${BLUE}Building: Quick test${NC}"
        make -f Makefile.fft_neon clean
        make -f Makefile.fft_neon all
        BUILD_TYPE="quick"
        ;;
    2)
        echo -e "${BLUE}Building: Release (optimized)${NC}"
        make -f Makefile.fft_neon release
        BUILD_TYPE="release"
        ;;
    3)
        if [ $NE10_AVAILABLE -eq 0 ]; then
            echo -e "${RED}✗ Ne10 library not found!${NC}"
            exit 1
        fi
        echo -e "${BLUE}Building: Ne10 (best performance)${NC}"
        make -f Makefile.fft_neon ne10
        BUILD_TYPE="ne10"
        ;;
    4)
        echo -e "${BLUE}Building: Scalar-only${NC}"
        make -f Makefile.fft_neon scalar
        BUILD_TYPE="scalar"
        ;;
    5)
        echo -e "${BLUE}Building: Debug${NC}"
        make -f Makefile.fft_neon clean
        make -f Makefile.fft_neon DEBUG=1 all
        BUILD_TYPE="debug"
        ;;
    6)
        echo -e "${BLUE}Building: Cross-compile for ARM${NC}"
        if ! command -v arm-linux-gnueabihf-g++ &> /dev/null; then
            echo -e "${RED}✗ ARM cross-compiler not found!${NC}"
            echo "  Install: sudo apt-get install g++-arm-linux-gnueabihf"
            exit 1
        fi
        make -f Makefile.fft_neon cross-arm
        BUILD_TYPE="cross"
        ;;
    7)
        echo -e "${BLUE}Building: All variants${NC}"
        # This option will be handled separately
        BUILD_TYPE="compare"
        ;;
    8)
        echo -e "${BLUE}Cleaning build directory${NC}"
        make -f Makefile.fft_neon clean
        echo -e "${GREEN}✓ Clean complete${NC}"
        exit 0
        ;;
    *)
        echo -e "${RED}Invalid option${NC}"
        exit 1
        ;;
esac

echo ""

# Run tests if not comparing
if [ "$BUILD_TYPE" != "compare" ]; then
    echo -e "${BLUE}=====================================================================${NC}"
    echo -e "${BLUE}Running Tests${NC}"
    echo -e "${BLUE}=====================================================================${NC}"
    echo ""

    if [ -f test_fft_neon ]; then
        # Quick test
        echo -e "${BLUE}Quick correctness test (1024-point FFT, 1000 iterations)...${NC}"
        ./test_fft_neon 1024 1000

        echo ""
        echo -e "${BLUE}=====================================================================${NC}"
        read -p "Run full benchmark? [y/N]: " run_bench

        if [[ "$run_bench" == "y" || "$run_bench" == "Y" ]]; then
            echo -e "${BLUE}Full benchmark (10000 iterations)...${NC}"
            ./test_fft_neon 1024 10000
        fi

        # Check for NEON instructions
        if [ $NEON_AVAILABLE -eq 1 ] && [ "$BUILD_TYPE" != "scalar" ]; then
            echo ""
            echo -e "${BLUE}=====================================================================${NC}"
            echo -e "${BLUE}Verifying NEON Instructions${NC}"
            echo -e "${BLUE}=====================================================================${NC}"
            make -f Makefile.fft_neon check-neon
        fi

        echo ""
        echo -e "${GREEN}=====================================================================${NC}"
        echo -e "${GREEN}✓ Build and test complete!${NC}"
        echo -e "${GREEN}=====================================================================${NC}"
    else
        echo -e "${RED}✗ test_fft_neon not found after build${NC}"
        exit 1
    fi
else
    # Compare all variants
    echo -e "${BLUE}=====================================================================${NC}"
    echo -e "${BLUE}Building and Comparing All Variants${NC}"
    echo -e "${BLUE}=====================================================================${NC}"
    echo ""

    # 1. Scalar baseline
    echo -e "${BLUE}1. Building scalar baseline...${NC}"
    make -f Makefile.fft_neon clean > /dev/null 2>&1
    make -f Makefile.fft_neon scalar > /dev/null 2>&1
    mv test_fft_neon test_fft_scalar
    echo -e "${GREEN}✓ Scalar build complete${NC}"

    # 2. NEON optimized
    if [ $NEON_AVAILABLE -eq 1 ]; then
        echo -e "${BLUE}2. Building NEON optimized...${NC}"
        make -f Makefile.fft_neon clean > /dev/null 2>&1
        make -f Makefile.fft_neon release > /dev/null 2>&1
        mv test_fft_neon test_fft_neon_opt
        echo -e "${GREEN}✓ NEON build complete${NC}"
    fi

    # 3. Ne10 library
    if [ $NE10_AVAILABLE -eq 1 ]; then
        echo -e "${BLUE}3. Building with Ne10...${NC}"
        make -f Makefile.fft_neon clean > /dev/null 2>&1
        make -f Makefile.fft_neon ne10 > /dev/null 2>&1
        mv test_fft_neon test_fft_ne10
        echo -e "${GREEN}✓ Ne10 build complete${NC}"
    fi

    echo ""
    echo -e "${BLUE}=====================================================================${NC}"
    echo -e "${BLUE}Performance Comparison (1024-point FFT, 10000 iterations)${NC}"
    echo -e "${BLUE}=====================================================================${NC}"
    echo ""

    # Run scalar
    echo -e "${YELLOW}Scalar (baseline):${NC}"
    ./test_fft_scalar 1024 10000 | grep -A 3 "Benchmark:"
    echo ""

    # Run NEON if available
    if [ -f test_fft_neon_opt ]; then
        echo -e "${YELLOW}NEON optimized:${NC}"
        ./test_fft_neon_opt 1024 10000 | grep -A 3 "Benchmark:"
        echo ""
    fi

    # Run Ne10 if available
    if [ -f test_fft_ne10 ]; then
        echo -e "${YELLOW}Ne10 library:${NC}"
        ./test_fft_ne10 1024 10000 | grep -A 3 "Benchmark:"
        echo ""
    fi

    echo -e "${GREEN}=====================================================================${NC}"
    echo -e "${GREEN}✓ Comparison complete!${NC}"
    echo -e "${GREEN}=====================================================================${NC}"

    # Cleanup
    echo ""
    read -p "Clean up test binaries? [y/N]: " cleanup
    if [[ "$cleanup" == "y" || "$cleanup" == "Y" ]]; then
        rm -f test_fft_scalar test_fft_neon_opt test_fft_ne10
        echo -e "${GREEN}✓ Cleanup complete${NC}"
    fi
fi

echo ""
echo -e "${BLUE}For more information:${NC}"
echo "  - Implementation:    src/leansdr/fft_neon.h"
echo "  - Documentation:     NEON_FFT_OPTIMIZATION.md"
echo "  - Integration guide: INTEGRATION_PATCH.md"
echo "  - Examples:          src/leansdr/fft_neon_example.h"
echo ""
