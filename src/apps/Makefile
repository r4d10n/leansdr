default:	native

VERSION	:= leansdr-$(shell git describe)

# Portable flags for development.

CXXFLAGS	= -O3 -I.. -DVERSION=\"$(VERSION)\" \
		  -Wall \
		  -Wno-sign-compare -Wno-array-bounds -Wno-unused-variable

LDFLAGS		=

# Base applications have no external dependencies.
APPS	=
APPS	+= leandvbtx leandvb
APPS	+= leansdrscan leansdrserv
APPS	+= leantsgen leansdrcat leanchansim

# NEON-optimized applications (built on ARM platforms)
APPS_NEON	=
APPS_NEON	+= leandvb_neon


# leanmlmrx requires fftw3 and pthread.
ifneq "$(wildcard /usr/include/fftw3.h)" ""
ifneq "$(wildcard /usr/include/pthread.h)" ""
APPS		+= leanmlmrx
CXXFLAGS	+=
LDFLAGS		+= -lfftw3f -lpthread
else
$(info libpthread not found. Will not build leanmlmrx.)
endif
else
$(info libfftw3f not found. Will not build leanmlmrx.)
endif

# leaniio{rx,tx} require libiio.
# Expected in ./libiio/, including libiio.a.
# (Assume libiio dependencies are satisfied.)
ifneq "$(wildcard libiio/iio.h)" ""
APPS		+= leaniiorx leaniiotx
CXXFLAGS	+= -Ilibiio
LDFLAGS		+= -Llibiio -L. -liio -lpthread -lxml2 -lz -llzma
else
$(info libiio not found. Will not build leaniio{rx,tx}.)
endif

# Enable X11 GUI if easily found.
ifneq "$(wildcard /usr/include/X11/Xlib.h)" ""
CXXFLAGS_GUI	= -DGUI
LDFLAGS_GUI	= -lX11
else
$(info X11 not found. Will not compile GUI.)
endif

DEPS	= ../leansdr/*.h

%:	%.cc $(DEPS)
	g++ $(CXXFLAGS) $(CXXFLAGS_GUI) $< $(LDFLAGS) $(LDFLAGS_GUI) -o $@

# Build NEON-optimized applications (always build, but only use NEON on ARM)
%_neon:	%_neon.cc $(DEPS)
	@if [ "$(IS_ARM)" = "yes" ]; then \
		echo "Building $@ with NEON optimizations..."; \
		g++ $(CXXFLAGS) $(CPU_FLAGS) $(CXXFLAGS_GUI) $< $(LDFLAGS) $(LDFLAGS_GUI) -o $@; \
	else \
		echo "Building $@ (NEON disabled on non-ARM platform)..."; \
		g++ $(CXXFLAGS) $(CXXFLAGS_GUI) $< $(LDFLAGS) $(LDFLAGS_GUI) -o $@; \
	fi

native::	$(APPS) $(APPS_NEON)
	@echo "Now 'make embedded' to also build static binaries."

clean::
	rm -f $(APPS) $(APPS_NEON)

##### CPU-specific flags for embedded platforms.

ARCH	:= $(shell uname -m)

# Detect if we're on an ARM architecture
IS_ARM	:= $(shell echo "$(ARCH)" | grep -E "^arm" > /dev/null && echo yes || echo no)
IS_ARMV7	:= $(shell [ "$(ARCH)" = "armv7l" ] || [ "$(ARCH)" = "armv7" ] && echo yes || echo no)
IS_ARMV6	:= $(shell [ "$(ARCH)" = "armv6l" ] || [ "$(ARCH)" = "armv6" ] && echo yes || echo no)
IS_AARCH64	:= $(shell [ "$(ARCH)" = "aarch64" ] && echo yes || echo no)

# NEON compilation flags for ARM architectures
NEON_FLAGS	= -march=armv7-a -mfpu=neon -ftree-vectorize -O3 \
		  -funsafe-math-optimizations -fsingle-precision-constant

# ARMv6 flags (without NEON support)
ARMV6_FLAGS	= -march=armv6 -O3 -funsafe-math-optimizations -fsingle-precision-constant

# AArch64 flags (ARMv8 with NEON)
AARCH64_FLAGS	= -march=armv8-a -O3 -funsafe-math-optimizations -fsingle-precision-constant

# x86/x64 scalar flags (fallback for non-ARM)
X86_FLAGS	= -O3 -march=native

# Select appropriate flags based on architecture
ifeq "$(IS_ARMV7)" "yes"
	CPU_FLAGS	= $(NEON_FLAGS)
	ARCH_TYPE	= armv7-neon
else ifeq "$(IS_AARCH64)" "yes"
	CPU_FLAGS	= $(AARCH64_FLAGS)
	ARCH_TYPE	= aarch64
else ifeq "$(IS_ARMV6)" "yes"
	CPU_FLAGS	= $(ARMV6_FLAGS)
	ARCH_TYPE	= armv6
else
	# Default to scalar x86/x64 compilation
	CPU_FLAGS	= $(X86_FLAGS)
	ARCH_TYPE	= x86-scalar
endif

# Embedded build flags (static linking, optimized)
CXXFLAGS_EMBEDDED	= $(CXXFLAGS) $(CPU_FLAGS)
LDFLAGS_EMBEDDED	= $(LDFLAGS) -static

# Debug build flags (NEON disabled for easier debugging)
CXXFLAGS_DEBUG		= $(CXXFLAGS) -g -O0 -DDEBUG
LDFLAGS_DEBUG		= $(LDFLAGS)

# NEON-specific debug flags (with NEON enabled and debug symbols)
ifeq "$(IS_ARM)" "yes"
	CXXFLAGS_DEBUG_NEON	= $(CXXFLAGS) -g -O2 $(NEON_FLAGS) -DDEBUG_NEON
	LDFLAGS_DEBUG_NEON	= $(LDFLAGS)
endif

embedded:	$(APPS:%=%.$(ARCH))

%.$(ARCH):	%.cc $(DEPS)
	g++ $(CXXFLAGS_EMBEDDED) $< $(LDFLAGS_EMBEDDED) -o $@

# Debug targets
debug:	$(APPS:%=%.debug)

%.debug:	%.cc $(DEPS)
	g++ $(CXXFLAGS_DEBUG) $< $(LDFLAGS_DEBUG) -o $@

# NEON-specific debug targets (ARM only)
ifneq "$(IS_ARM)" "no"
debug-neon:	$(APPS:%=%.debug-neon)

%.debug-neon:	%.cc $(DEPS)
	g++ $(CXXFLAGS_DEBUG_NEON) $< $(LDFLAGS_DEBUG_NEON) -o $@
endif

# Test targets for NEON-specific tests
test_neon:
	@if [ -f test_neon.cc ]; then \
		echo "Building test_neon..."; \
		g++ $(CXXFLAGS_EMBEDDED) test_neon.cc $(LDFLAGS) -o test_neon; \
		echo "Running test_neon..."; \
		./test_neon; \
	else \
		echo "test_neon.cc not found."; \
	fi

benchmark_neon:
	@if [ -f benchmark_neon.cc ]; then \
		echo "Building benchmark_neon..."; \
		g++ $(CXXFLAGS_EMBEDDED) benchmark_neon.cc $(LDFLAGS) -o benchmark_neon; \
		echo "Running benchmark_neon..."; \
		./benchmark_neon; \
	else \
		echo "benchmark_neon.cc not found."; \
	fi

# Enhanced clean targets
clean::
	rm -f $(APPS) $(APPS_NEON)
	rm -f $(APPS:%=%.debug)
	rm -f $(APPS:%=%.debug-neon)
	rm -f $(APPS_NEON:%=%.debug)
	rm -f $(APPS_NEON:%=%.debug-neon)
	rm -f test_neon benchmark_neon
	rm -f *.o *.a

clean-all: clean
	find . -name "*.$(ARCH)" -delete
	find . -name "*-neon" -type f -executable -delete

# Help target
help:
	@echo "Available targets for NEON/ARM compilation:"
	@echo "  native          - Build for native platform (x86/x64 scalar or ARM)"
	@echo "                    Includes: $(APPS) $(APPS_NEON)"
	@echo "  embedded        - Build static binaries for current ARM architecture"
	@echo "  debug           - Build with debug symbols (NEON disabled)"
	@echo "  debug-neon      - Build with debug symbols and NEON enabled (ARM only)"
	@echo "  test_neon       - Build and run NEON-specific tests"
	@echo "  benchmark_neon  - Build and run NEON benchmarks"
	@echo "  clean           - Remove built binaries"
	@echo "  clean-all       - Remove all built binaries and architecture-specific files"
	@echo ""
	@echo "NEON-optimized applications:"
	@echo "  leandvb_neon    - DVB-S demodulator with NEON SIMD optimizations"
	@echo "                    Use --neon flag to enable, --perf-report for stats"
	@echo ""
	@echo "Detected architecture: $(ARCH)"
	@echo "Architecture type: $(ARCH_TYPE)"
	@echo "Using CPU flags: $(CPU_FLAGS)"
