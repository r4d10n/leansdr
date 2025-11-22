# Makefile for NEON integration example

CXX = g++
CXXFLAGS = -std=c++11 -I../src -O3 -Wall

# Detect architecture and add NEON flags
ARCH := $(shell uname -m)

ifeq ($(ARCH),armv7l)
  CXXFLAGS += -march=armv7-a -mfpu=neon -ftree-vectorize
endif

ifeq ($(ARCH),aarch64)
  CXXFLAGS += -march=armv8-a -ftree-vectorize
endif

# Targets
EXAMPLES = neon_integration_example

all: $(EXAMPLES)

neon_integration_example: neon_integration_example.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(EXAMPLES) *.o

.PHONY: all clean
