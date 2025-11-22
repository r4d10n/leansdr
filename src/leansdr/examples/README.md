# LeanSDR NEON Optimization Examples

This directory contains example programs demonstrating the NEON-optimized implementations in LeanSDR.

## Reed-Solomon NEON Demo

**File:** `rs_neon_demo.cc`

Demonstrates the NEON-optimized Reed-Solomon RS(204, 188, 8) decoder for DVB-S.

### Features

The demo showcases:

1. **Basic Encoding** - How to encode a message with Reed-Solomon parity bytes
2. **Error Detection** - Detecting corruption using syndrome calculation
3. **Error Correction** - Correcting up to 8 byte errors
4. **Performance Testing** - Benchmarking syndrome calculation and error correction
5. **Maximum Correction** - Testing the limits (8 errors)

### Building

```bash
# From the examples directory
cd /home/user/leansdr/src/leansdr/examples

# For ARM with NEON
g++ -std=c++11 -O3 -march=armv7-a -mfpu=neon -I.. rs_neon_demo.cc -o rs_neon_demo

# For ARMv8/AArch64
g++ -std=c++11 -O3 -march=armv8-a -I.. rs_neon_demo.cc -o rs_neon_demo

# For x86 (scalar fallback)
g++ -std=c++11 -O3 -I.. rs_neon_demo.cc -o rs_neon_demo

# Using make (if Makefile is present)
make rs_neon_demo
```

### Running

```bash
./rs_neon_demo
```

### Expected Output

```
========================================
Reed-Solomon NEON Demo
RS(204, 188, 8) for DVB-S
========================================

NEON optimizations: ENABLED

=== Example 1: Basic Encoding ===
Original message: Hello, DVB-S! This is a Reed-Solomon encoded message.
Parity bytes: a3 5f 2c ...
Message after encoding: VALID

=== Example 2: Error Detection ===
Original: Test message for error detection
Corrupted: ... [ff] ... [0f] ...
Corruption detected: YES
Syndromes: 3a 5c 7f ...

=== Example 3: Error Correction ===
Original data: 07 14 21 2e ...
Introducing 4 errors at positions: 20 60 100 140
Corrupted data: 07 14 21 2e ...
Corrected data: 07 14 21 2e ...
Correction results:
  Bits corrected: 32
  Errors remaining: 0
  Status: SUCCESS

...
```

## Performance Characteristics

On typical ARM platforms (Cortex-A53, Cortex-A72):

- **Syndrome calculation:** 1.5-2x faster with NEON
- **Error correction:** 1.5-2x faster with NEON
- **Memory overhead:** ~68 KB for pre-computed tables

## More Information

See the comprehensive documentation at:
- `/home/user/leansdr/src/leansdr/RS_NEON_OPTIMIZATIONS.md`
- `/home/user/leansdr/src/leansdr/rs_neon.h` (implementation)
- `/home/user/leansdr/test/test_rs_neon.cc` (test suite)
