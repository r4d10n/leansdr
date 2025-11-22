# LeanSDR NEON Infrastructure

Complete ARM NEON configuration, detection, and helper infrastructure for LeanSDR.

## Overview

This infrastructure provides comprehensive support for ARM NEON SIMD optimizations in LeanSDR, with automatic detection, runtime dispatching, and graceful fallback to scalar code.

## Files Created

### 1. `/home/user/leansdr/src/leansdr/config.h` (477 lines)

Central configuration and feature detection header providing:

#### Platform Detection
- **Architecture detection**: ARM (32/64-bit), x86 (32/64-bit)
- **Operating system detection**: Linux, Android
- **SIMD support**: NEON, SSE, SSE2, AVX, AVX2

#### NEON Detection
- **Compile-time detection**: `LEANSDR_HAS_NEON_COMPILETIME`
- **Runtime detection**: `has_neon_runtime()` - checks `/proc/cpuinfo` on ARM32, always true on ARM64
- **Combined check**: `use_neon()` - combines compile-time and runtime checks

#### SIMD Configuration
- **Vector widths**: `LEANSDR_SIMD_WIDTH` (128 for NEON/SSE, 256 for AVX)
- **Alignment requirements**: `LEANSDR_SIMD_ALIGN` (16 for NEON)
- **Elements per vector**: `LEANSDR_FLOAT32_PER_SIMD`, `LEANSDR_INT32_PER_SIMD`, etc.

#### Compiler Attributes
- `LEANSDR_FORCE_INLINE` - Force function inlining
- `LEANSDR_NOINLINE` - Prevent inlining
- `LEANSDR_ALIGNED(x)` - Memory alignment
- `LEANSDR_RESTRICT` - Pointer aliasing hints
- `LEANSDR_LIKELY(x)`, `LEANSDR_UNLIKELY(x)` - Branch prediction
- `LEANSDR_PURE`, `LEANSDR_CONST` - Function attributes for optimization

#### Debug and Profiling
- `LEANSDR_ASSERT(cond)` - Runtime assertions (debug builds only)
- `LEANSDR_STATIC_ASSERT(cond, msg)` - Compile-time assertions
- `LEANSDR_LOG_DEBUG/INFO/WARN/ERROR(fmt, ...)` - Logging macros
- `LEANSDR_PROFILE_START/END(name)` - Performance profiling (when enabled)
- `LEANSDR_PREFETCH_READ/WRITE(addr)` - Cache prefetching hints

#### Configuration Flags
- `LEANSDR_ENABLE_AUTO_FALLBACK` - Enable automatic scalar fallback
- `LEANSDR_ENABLE_RUNTIME_DISPATCH` - Enable runtime SIMD detection
- `LEANSDR_ENABLE_PROFILING` - Enable profiling macros

### 2. `/home/user/leansdr/src/leansdr/neon_helpers.h` (644 lines)

Comprehensive NEON helper functions and optimized operations:

#### Type Definitions
- `cf32x2_t` - Two complex<float> in interleaved format
- `cf32x4_sep_t` - Four complex<float> with separated real/imaginary
- `ci16x4_t` - Four complex<int16_t> in interleaved format

#### Vector Initialization
- `vzero_f32()`, `vzero_s32()`, `vzero_s16()` - Zero vectors
- `vset1_f32(val)` - Set all elements to same value
- `vset_f32(v0, v1, v2, v3)` - Set elements individually

#### Memory Operations
- `vload_f32(ptr)`, `vloadu_f32(ptr)` - Aligned/unaligned loads
- `vstore_f32(ptr, v)`, `vstoreu_f32(ptr, v)` - Aligned/unaligned stores
- `vload_cf32_interleaved(ptr)` - Load complex numbers (interleaved)
- `vload_cf32_separated(ptr)` - Load complex numbers (separated)
- Corresponding store operations

#### Basic Arithmetic
- `vadd_f32(a, b)`, `vsub_f32(a, b)`, `vmul_f32(a, b)` - Element-wise operations
- `vmadd_f32(a, b, c)` - Multiply-add: a + b * c
- `vmsub_f32(a, b, c)` - Multiply-subtract: a - b * c
- `vneg_f32(a)` - Negate

#### Complex Number Operations (Interleaved)
- `vcadd_f32(a, b)` - Complex addition
- `vcsub_f32(a, b)` - Complex subtraction
- `vcmul_f32(a, b)` - Complex multiplication
- `vcconj_f32(a)` - Complex conjugate
- `vcmagsq_f32(a)` - Complex magnitude squared

#### Complex Number Operations (Separated)
- `vcmul_f32_sep(a, b)` - Complex multiplication
- `vcmadd_f32_sep(a, b, c)` - Complex multiply-add
- `vcconj_f32_sep(a)` - Complex conjugate
- `vcmagsq_f32_sep(a)` - Magnitude squared

#### Horizontal Reductions
- `vhsum_f32(v)` - Sum all elements
- `vhsum_s32(v)` - Sum all int32 elements
- `vhmax_f32(v)` - Maximum element
- `vhmin_f32(v)` - Minimum element

#### Type Conversions
- `vcvt_f32_s32(v)` - int32 to float32
- `vcvt_s32_f32(v)` - float32 to int32 (truncate)
- `vcvt_s32_f32_round(v)` - float32 to int32 (round)
- `vcvt_s32_s16_low/high(v)` - Widen int16 to int32
- `vcvt_s16_s32_sat(v)` - Narrow int32 to int16 (saturating)

#### Bitwise Operations
- `vand_f32(a, b)`, `vor_f32(a, b)`, `vxor_f32(a, b)` - Bitwise ops
- `vandnot_f32(a, b)` - AND NOT

#### Comparison and Selection
- `vcmpeq_f32(a, b)`, `vcmpgt_f32(a, b)`, `vcmpge_f32(a, b)` - Comparisons
- `vselect_f32(mask, a, b)` - Select based on mask
- `vmin_f32(a, b)`, `vmax_f32(a, b)` - Min/max

#### Advanced Operations
- `vrecip_f32(v)` - Reciprocal (with Newton-Raphson refinement)
- `vrsqrt_f32(v)` - Reciprocal square root
- `vsqrt_f32(v)` - Square root
- `vabs_f32(v)` - Absolute value

#### Shuffles and Permutations
- `vreverse_f32(v)` - Reverse elements
- `vget_low/high_f32x2(v)` - Extract halves
- `vcombine_f32x4(low, high)` - Combine halves
- `vdup_low/high_f32(v)` - Duplicate half

#### Optimization Hints
- `LEANSDR_NEON_HOT_PATH` - Mark hot code paths
- `LEANSDR_NEON_ASSUME_ALIGNED(ptr, align)` - Alignment hints
- `LEANSDR_NEON_IVDEP` - Loop independence hint
- `LEANSDR_NEON_UNROLL(n)` - Loop unrolling hint

### 3. `/home/user/leansdr/src/leansdr/neon_example.h` (258 lines)

Practical examples demonstrating NEON optimization patterns:

#### Example Functions
1. **complex_macc()** - Complex multiply-accumulate with SIMD and scalar versions
2. **complex_dot()** - Complex dot product with horizontal reduction
3. **magsq()** - Vector magnitude squared computation
4. **fir_filter** - FIR filter class template

Each example includes:
- Scalar reference implementation
- NEON-optimized version
- Runtime dispatch wrapper
- Proper remainder handling for non-multiple-of-4 sizes

## Usage Examples

### Basic Usage - Check NEON Support

```cpp
#include "leansdr/config.h"
#include <stdio.h>

int main() {
    // Print build configuration
    printf("%s\n", leansdr::get_build_config());

    // Check if NEON is available
    if (leansdr::use_neon()) {
        printf("NEON acceleration enabled\n");
    } else {
        printf("Using scalar fallback\n");
    }

    return 0;
}
```

### NEON Vector Operations

```cpp
#include "leansdr/config.h"
#include "leansdr/neon_helpers.h"

void process_samples(float* output, const float* input, size_t count) {
#if LEANSDR_USE_NEON_INTRINSICS
    using namespace leansdr::neon;

    size_t i = 0;
    const size_t simd_count = (count / 4) * 4;

    // Process 4 elements at a time
    for (; i < simd_count; i += 4) {
        float32x4_t v = vload_f32(input + i);
        v = vmul_f32(v, vset1_f32(2.0f));  // Multiply by 2
        v = vadd_f32(v, vset1_f32(1.0f));  // Add 1
        vstore_f32(output + i, v);
    }

    // Handle remainder
    for (; i < count; ++i) {
        output[i] = input[i] * 2.0f + 1.0f;
    }
#else
    // Scalar fallback
    for (size_t i = 0; i < count; ++i) {
        output[i] = input[i] * 2.0f + 1.0f;
    }
#endif
}
```

### Complex Number Processing

```cpp
#include "leansdr/config.h"
#include "leansdr/math.h"
#include "leansdr/neon_helpers.h"

void multiply_complex_arrays(leansdr::complex<float>* result,
                              const leansdr::complex<float>* a,
                              const leansdr::complex<float>* b,
                              size_t count) {
#if LEANSDR_USE_NEON_INTRINSICS
    using namespace leansdr::neon;

    size_t i = 0;
    const size_t simd_count = (count / 4) * 4;

    // SIMD: Process 4 complex numbers at a time
    for (; i < simd_count; i += 4) {
        cf32x4_sep_t va = vload_cf32_separated(a + i);
        cf32x4_sep_t vb = vload_cf32_separated(b + i);
        cf32x4_sep_t vr = vcmul_f32_sep(va, vb);
        vstore_cf32_separated(result + i, vr);
    }

    // Scalar remainder
    for (; i < count; ++i) {
        result[i] = a[i] * b[i];
    }
#else
    for (size_t i = 0; i < count; ++i) {
        result[i] = a[i] * b[i];
    }
#endif
}
```

### Runtime Dispatch Pattern

```cpp
#include "leansdr/config.h"
#include "leansdr/neon_helpers.h"

// Function with automatic runtime dispatch
void optimized_function(float* data, size_t count) {
#if LEANSDR_USE_NEON_INTRINSICS && LEANSDR_ENABLE_RUNTIME_DISPATCH
    if (leansdr::use_neon()) {
        // NEON implementation
        process_with_neon(data, count);
    } else {
        // Scalar fallback
        process_scalar(data, count);
    }
#elif LEANSDR_USE_NEON_INTRINSICS
    // NEON always available (compile-time only)
    process_with_neon(data, count);
#else
    // No NEON support
    process_scalar(data, count);
#endif
}
```

## Compilation Instructions

### ARM32 with NEON (ARMv7-A)
```bash
g++ -mfpu=neon -march=armv7-a -Isrc -std=c++11 -O3 -o myapp myapp.cpp
```

### ARM64 (AArch64) - NEON is standard
```bash
g++ -march=armv8-a -Isrc -std=c++11 -O3 -o myapp myapp.cpp
```

### Cross-compilation for Raspberry Pi
```bash
arm-linux-gnueabihf-g++ -mfpu=neon -march=armv7-a -Isrc -std=c++11 -O3 -o myapp myapp.cpp
```

### x86/x64 (NEON disabled, scalar fallback used)
```bash
g++ -Isrc -std=c++11 -O3 -o myapp myapp.cpp
```

### With profiling enabled
```bash
g++ -DLEANSDR_ENABLE_PROFILING=1 -mfpu=neon -Isrc -std=c++11 -O3 -o myapp myapp.cpp
```

## Configuration Macros

You can customize behavior with these defines (before including headers):

```cpp
#define LEANSDR_ENABLE_AUTO_FALLBACK 1      // Enable automatic fallback (default: 1)
#define LEANSDR_ENABLE_RUNTIME_DISPATCH 1   // Enable runtime dispatch (default: 1)
#define LEANSDR_ENABLE_PROFILING 1          // Enable profiling macros (default: 0)

#include "leansdr/config.h"
```

## Performance Tips

1. **Alignment**: Use 16-byte aligned memory for best performance
   ```cpp
   float* aligned_buffer = (float*)aligned_alloc(16, size * sizeof(float));
   ```

2. **Prefetching**: Use prefetch hints for large datasets
   ```cpp
   LEANSDR_PREFETCH_READ(&data[i + 64]);
   ```

3. **Branch hints**: Use likely/unlikely for predictable branches
   ```cpp
   if (LEANSDR_LIKELY(count >= 4)) {
       // SIMD path
   }
   ```

4. **Handle remainders**: Always process non-SIMD-aligned remainders
   ```cpp
   size_t simd_count = (count / 4) * 4;
   // ... SIMD loop ...
   for (size_t i = simd_count; i < count; ++i) {
       // Scalar remainder
   }
   ```

## Architecture Support Matrix

| Architecture | NEON Support | Detection Method | Notes |
|--------------|--------------|------------------|-------|
| ARMv7-A | Optional | Runtime (/proc/cpuinfo) | Requires -mfpu=neon flag |
| ARMv8-A (AArch64) | Standard | Compile-time | Always available |
| Raspberry Pi 2/3 | Yes | Runtime | ARMv7-A with NEON |
| Raspberry Pi 4 | Yes | Compile-time | ARMv8-A (64-bit) |
| x86/x64 | No | Compile-time | Uses scalar fallback |

## Debugging and Profiling

### Enable debug logging
```cpp
// Compile with debug mode (no -DNDEBUG)
g++ -g -Isrc -std=c++11 -o myapp myapp.cpp

// Or define LEANSDR_DEBUG explicitly
g++ -DLEANSDR_DEBUG=1 -Isrc -std=c++11 -o myapp myapp.cpp
```

### Enable profiling
```cpp
#define LEANSDR_ENABLE_PROFILING 1
#include "leansdr/config.h"

void my_function() {
    LEANSDR_PROFILE_START(computation);

    // ... expensive computation ...

    LEANSDR_PROFILE_END(computation);
    // Prints: [PROFILE] computation: 0.123456 seconds
}
```

### Check configuration at runtime
```cpp
#include "leansdr/config.h"
#include <stdio.h>

int main() {
    printf("%s\n", leansdr::get_build_config());
    return 0;
}
```

## Best Practices

1. **Always provide scalar fallback**: Your code should work on all platforms
2. **Use runtime dispatch**: Allows single binary to work on multiple ARM variants
3. **Test on target hardware**: NEON behavior can vary between ARM implementations
4. **Profile before optimizing**: Use profiling macros to identify bottlenecks
5. **Handle alignment**: Use aligned memory allocations for best SIMD performance
6. **Process in chunks**: SIMD works best on medium to large data sets
7. **Mind the remainder**: Always handle non-multiple-of-4 sizes correctly

## Integration with Existing LeanSDR Code

To integrate with existing LeanSDR DSP blocks:

```cpp
#include "leansdr/framework.h"
#include "leansdr/config.h"
#include "leansdr/neon_helpers.h"

template<typename T>
struct my_optimized_block : runnable {
    void run() {
        // ... get input/output pointers ...

        #if LEANSDR_USE_NEON_INTRINSICS
        if (leansdr::use_neon()) {
            process_neon(in, out, count);
        } else {
            process_scalar(in, out, count);
        }
        #else
        process_scalar(in, out, count);
        #endif

        // ... update pipe positions ...
    }
};
```

## License

This infrastructure is part of LeanSDR and is licensed under GPL v3 (same as LeanSDR).

## Future Enhancements

Potential additions:
- SVE (Scalable Vector Extension) support for newer ARM processors
- SSE/AVX implementations for x86 architectures
- Automatic benchmarking and selection of fastest implementation
- Additional DSP primitives (FFT, correlation, filters)
- NEON assembly optimizations for critical inner loops
