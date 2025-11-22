# Makefile for LeanSDR NEON Examples
# Copyright (C) 2016-2025 <pabr@pabr.org>

CXX = g++
CXXFLAGS = -Wall -O3 -std=c++11 -I..
LDFLAGS = -lm

# Detect ARM architecture and enable NEON if available
ARCH := $(shell uname -m)

ifeq ($(ARCH),armv7l)
    CXXFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=hard
    HAVE_NEON = 1
endif

ifeq ($(ARCH),aarch64)
    CXXFLAGS += -march=armv8-a
    HAVE_NEON = 1
endif

# Targets
TARGET_RS = rs_neon_demo

.PHONY: all clean run help

all: $(TARGET_RS)
	@echo ""
	@echo "Build complete: $(TARGET_RS)"
	@if [ "$(HAVE_NEON)" = "1" ]; then \
		echo "NEON optimizations: ENABLED"; \
	else \
		echo "NEON optimizations: DISABLED (scalar fallback)"; \
	fi
	@echo ""
	@echo "Run './$(TARGET_RS)' to execute the demo"

$(TARGET_RS): rs_neon_demo.cc
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

run: $(TARGET_RS)
	@echo "Running Reed-Solomon NEON demo..."
	@echo ""
	./$(TARGET_RS)

clean:
	rm -f $(TARGET_RS) *.o

help:
	@echo "LeanSDR NEON Examples - Build Targets"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build all examples"
	@echo "  make run          - Build and run RS demo"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Show this help"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build demo"
	@echo "  make run          # Run demo"
