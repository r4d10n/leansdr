# NEON Integration Examples

This directory contains example programs demonstrating how to use the NEON integration layer in LeanSDR.

## Building the Examples

### On ARM platforms (with NEON support):

```bash
cd /home/user/leansdr/examples
make clean
make
```

### On x86 platforms (without NEON):

```bash
cd /home/user/leansdr/examples
make clean
make
# Examples will still compile and run, using scalar fallback
```

## Running the Examples

### Basic Integration Example

```bash
./neon_integration_example
```

Expected output:
```
=== LeanSDR NEON Integration Example ===

Step 1: Initializing NEON...
=== LeanSDR CPU Capabilities ===
Architecture: ARMv8/AArch64
NEON SIMD:    Available
NEON Compile: Enabled
...

Step 2: Checking NEON availability...
NEON available: YES
Status: Active
Expected speedup: 4.0x
Expected CPU reduction: 30.0%
...
```

## Example Programs

### neon_integration_example.cc

Demonstrates:
- NEON initialization and cleanup
- CPU capability detection
- Creating optimized components (AGC)
- Runtime configuration
- Performance estimation

## See Also

- Main integration guide: `/home/user/leansdr/docs/INTEGRATION-GUIDE.md`
- NEON implementation details: `/home/user/leansdr/docs/NEON-IMPLEMENTATION.md`
- Integration header: `/home/user/leansdr/src/leansdr/neon_integration.h`
