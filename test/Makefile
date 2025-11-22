# Makefile for LeanSDR NEON Optimization Tests
# Copyright (C) 2016-2025 <pabr@pabr.org>

CXX = g++
CXXFLAGS = -Wall -O2 -std=c++11
LDFLAGS = -lm

# Detect ARM architecture and enable NEON if available
ARCH := $(shell uname -m)

ifeq ($(ARCH),armv7l)
    # ARMv7 with NEON
    CXXFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=hard
    HAVE_NEON = 1
endif

ifeq ($(ARCH),aarch64)
    # ARMv8 (64-bit) - NEON is standard
    CXXFLAGS += -march=armv8-a
    HAVE_NEON = 1
endif

# Allow manual override
ifdef ENABLE_NEON
    CXXFLAGS += -mfpu=neon
    HAVE_NEON = 1
endif

# Disable NEON (for testing scalar fallback)
ifdef DISABLE_NEON
    CXXFLAGS += -UDISABLE_NEON
    HAVE_NEON = 0
endif

# Enable auto-vectorization
CXXFLAGS += -ftree-vectorize

# Verbose build
ifdef VERBOSE
    CXXFLAGS += -v
endif

# Debug build
ifdef DEBUG
    CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
    CXXFLAGS += -g -O0 -DDEBUG
endif

# Optimization flags for release
ifdef RELEASE
    CXXFLAGS := $(filter-out -O2,$(CXXFLAGS))
    CXXFLAGS += -O3 -ffast-math -funroll-loops
endif

# Targets
TARGET = test_neon
SOURCES = test_neon.cc
OBJECTS = $(SOURCES:.cc=.o)

# Benchmark targets
BENCH_SCALAR = benchmark_neon_scalar
BENCH_NEON = benchmark_neon_neon
BENCH_SOURCE = benchmark_neon.cc

.PHONY: all clean test run benchmark bench-build bench-run bench-all help

all: $(TARGET)
	@echo ""
	@echo "Build complete: $(TARGET)"
	@if [ "$(HAVE_NEON)" = "1" ]; then \
		echo "NEON optimizations: ENABLED"; \
	else \
		echo "NEON optimizations: DISABLED (scalar only)"; \
	fi
	@echo ""
	@echo "Run 'make test' to execute tests"
	@echo "Run 'make benchmark' for performance tests only"

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test: $(TARGET)
	@echo "========================================="
	@echo "Running NEON optimization tests..."
	@echo "========================================="
	./$(TARGET)

run: test

# Benchmark mode - run tests multiple times for timing
benchmark: $(TARGET)
	@echo "Running benchmark (may take a while)..."
	@for i in 1 2 3; do \
		echo ""; \
		echo "=== Run $$i ==="; \
		./$(TARGET); \
	done

# Cross-compilation for ARM (from x86)
cross-arm:
	$(MAKE) clean
	$(MAKE) CXX=arm-linux-gnueabihf-g++ \
	        CXXFLAGS="-Wall -O3 -std=c++11 -march=armv7-a -mfpu=neon -mfloat-abi=hard -ftree-vectorize" \
	        all

# Build scalar-only version (no NEON)
scalar:
	$(MAKE) clean
	$(MAKE) DISABLE_NEON=1 all

# Build optimized release version
release:
	$(MAKE) clean
	$(MAKE) RELEASE=1 all

# Check assembly output
asm: test_neon.cc
	$(CXX) $(CXXFLAGS) -S -fverbose-asm -o test_neon.s $<
	@echo "Assembly output written to test_neon.s"
	@echo "Look for NEON instructions like: vmla, vadd, vmul, vld1, vst1"

# Check if NEON instructions are actually used
check-neon: $(TARGET)
	@echo "Checking for NEON instructions in binary..."
	@if command -v objdump >/dev/null 2>&1; then \
		objdump -d $(TARGET) | grep -E 'vmla|vadd|vmul|vld|vst' | head -20; \
		count=$$(objdump -d $(TARGET) | grep -E 'vmla|vadd|vmul|vld|vst' | wc -l); \
		echo ""; \
		echo "Found $$count NEON instructions"; \
		if [ $$count -gt 0 ]; then \
			echo "✓ NEON optimizations are present"; \
		else \
			echo "✗ No NEON instructions found"; \
		fi \
	else \
		echo "objdump not available, cannot check for NEON instructions"; \
	fi

# Build benchmarks (scalar version, always available)
$(BENCH_SCALAR): $(BENCH_SOURCE)
	@echo "Building scalar benchmark..."
	$(CXX) -O3 -I../src -Wall -Wno-sign-compare $< -o $@
	@echo "✓ Scalar benchmark built: $@"

# Build NEON benchmark (ARM only)
ifeq ($(HAVE_NEON),1)
$(BENCH_NEON): $(BENCH_SOURCE)
	@echo "Building NEON benchmark..."
	@echo "Architecture: $(ARCH)"
	$(CXX) $(CXXFLAGS) -O3 -I../src -Wall -Wno-sign-compare $< -o $@
	@echo "✓ NEON benchmark built: $@"

bench-build: $(BENCH_SCALAR) $(BENCH_NEON)
else
bench-build: $(BENCH_SCALAR)
	@echo "Note: NEON benchmark not built (non-ARM architecture or NEON disabled)"
endif

# Run benchmarks using the helper script
bench-run: bench-build
	@if [ -x ./run_benchmark_neon.sh ]; then \
		./run_benchmark_neon.sh; \
	else \
		echo "Error: run_benchmark_neon.sh not found or not executable"; \
		echo "Running scalar benchmark directly..."; \
		./$(BENCH_SCALAR); \
	fi

# Build and run all benchmarks
bench-all: bench-run

clean:
	rm -f $(OBJECTS) $(TARGET) test_neon.s *.o
	rm -f $(BENCH_SCALAR) $(BENCH_NEON)

help:
	@echo "LeanSDR NEON Test Suite - Build Targets"
	@echo ""
	@echo "Unit Tests:"
	@echo "  make              - Build test suite with default settings"
	@echo "  make test         - Build and run all tests"
	@echo "  make benchmark    - Run performance benchmarks"
	@echo "  make release      - Build optimized release version (-O3)"
	@echo "  make scalar       - Build without NEON (scalar fallback only)"
	@echo "  make cross-arm    - Cross-compile for ARM from x86"
	@echo "  make asm          - Generate assembly output"
	@echo "  make check-neon   - Check if binary contains NEON instructions"
	@echo ""
	@echo "Micro-Benchmarks:"
	@echo "  make bench-build  - Build benchmark_neon (scalar and NEON versions)"
	@echo "  make bench-run    - Build and run comprehensive NEON benchmarks"
	@echo "  make bench-all    - Same as bench-run"
	@echo ""
	@echo "General:"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Environment variables:"
	@echo "  DEBUG=1           - Build with debug symbols (-g -O0)"
	@echo "  ENABLE_NEON=1     - Force enable NEON"
	@echo "  DISABLE_NEON=1    - Force disable NEON"
	@echo "  VERBOSE=1         - Verbose compiler output"
	@echo ""
	@echo "Examples:"
	@echo "  make test                    # Build and run unit tests"
	@echo "  make bench-run               # Run micro-benchmarks"
	@echo "  make DEBUG=1 test            # Debug build"
	@echo "  make DISABLE_NEON=1 test     # Test scalar fallback"
	@echo "  make release benchmark       # Optimized benchmark"
