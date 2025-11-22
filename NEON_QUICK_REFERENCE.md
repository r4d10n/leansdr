# LeanSDR NEON Quick Reference Card

## Header Inclusion

```cpp
#include "leansdr/config.h"        // Configuration and detection
#include "leansdr/neon_helpers.h"  // NEON helper functions
#include "leansdr/neon_example.h"  // Example implementations
```

## Runtime Detection

```cpp
// Check if NEON is available
if (leansdr::use_neon()) {
    // Use NEON-optimized code
}

// Print build configuration
printf("%s\n", leansdr::get_build_config());
```

## Common Patterns

### Pattern 1: Simple Vector Operation

```cpp
#if LEANSDR_USE_NEON_INTRINSICS
using namespace leansdr::neon;

void scale_add(float* out, const float* in, float scale, float offset, size_t n) {
    size_t i = 0;
    float32x4_t vscale = vset1_f32(scale);
    float32x4_t voffset = vset1_f32(offset);

    for (; i + 4 <= n; i += 4) {
        float32x4_t v = vload_f32(in + i);
        v = vmul_f32(v, vscale);
        v = vadd_f32(v, voffset);
        vstore_f32(out + i, v);
    }

    for (; i < n; ++i) {
        out[i] = in[i] * scale + offset;
    }
}
#endif
```

### Pattern 2: Complex Multiplication

```cpp
#if LEANSDR_USE_NEON_INTRINSICS
using namespace leansdr::neon;

void cmul(complex<float>* out, const complex<float>* a,
          const complex<float>* b, size_t n) {
    size_t i = 0;

    for (; i + 4 <= n; i += 4) {
        cf32x4_sep_t va = vload_cf32_separated(a + i);
        cf32x4_sep_t vb = vload_cf32_separated(b + i);
        cf32x4_sep_t vr = vcmul_f32_sep(va, vb);
        vstore_cf32_separated(out + i, vr);
    }

    for (; i < n; ++i) {
        out[i] = a[i] * b[i];
    }
}
#endif
```

### Pattern 3: Dot Product with Reduction

```cpp
#if LEANSDR_USE_NEON_INTRINSICS
using namespace leansdr::neon;

float dot_product(const float* a, const float* b, size_t n) {
    size_t i = 0;
    float32x4_t sum = vzero_f32();

    for (; i + 4 <= n; i += 4) {
        float32x4_t va = vload_f32(a + i);
        float32x4_t vb = vload_f32(b + i);
        sum = vmadd_f32(sum, va, vb);
    }

    float result = vhsum_f32(sum);

    for (; i < n; ++i) {
        result += a[i] * b[i];
    }

    return result;
}
#endif
```

## Most Common Functions

### Initialization
```cpp
vzero_f32()              // [0, 0, 0, 0]
vset1_f32(2.0f)         // [2, 2, 2, 2]
vset_f32(1,2,3,4)       // [1, 2, 3, 4]
```

### Memory
```cpp
vload_f32(ptr)          // Load aligned
vloadu_f32(ptr)         // Load unaligned
vstore_f32(ptr, v)      // Store aligned
vstoreu_f32(ptr, v)     // Store unaligned
```

### Arithmetic
```cpp
vadd_f32(a, b)          // a + b
vsub_f32(a, b)          // a - b
vmul_f32(a, b)          // a * b
vmadd_f32(a, b, c)      // a + b*c
vmsub_f32(a, b, c)      // a - b*c
vneg_f32(a)             // -a
```

### Complex (Separated Format)
```cpp
vcmul_f32_sep(a, b)     // Complex multiply
vcmadd_f32_sep(a,b,c)   // a + b*c (complex)
vcconj_f32_sep(a)       // Complex conjugate
vcmagsq_f32_sep(a)      // |a|^2
```

### Reductions
```cpp
vhsum_f32(v)            // Sum all elements
vhmax_f32(v)            // Maximum element
vhmin_f32(v)            // Minimum element
```

### Advanced
```cpp
vrecip_f32(v)           // 1/v
vsqrt_f32(v)            // sqrt(v)
vabs_f32(v)             // |v|
vmin_f32(a, b)          // min(a, b)
vmax_f32(a, b)          // max(a, b)
```

## Configuration Macros

```cpp
LEANSDR_ARM             // 1 if ARM architecture
LEANSDR_ARM64           // 1 if ARM 64-bit
LEANSDR_X86             // 1 if x86 architecture
LEANSDR_USE_NEON_INTRINSICS  // 1 if NEON available

LEANSDR_SIMD_WIDTH      // 128 for NEON
LEANSDR_SIMD_ALIGN      // 16 for NEON
LEANSDR_FLOAT32_PER_SIMD  // 4 for NEON
```

## Compiler Attributes

```cpp
LEANSDR_FORCE_INLINE    // Force function inlining
LEANSDR_ALIGNED(16)     // 16-byte alignment
LEANSDR_RESTRICT        // No pointer aliasing
LEANSDR_LIKELY(cond)    // Branch likely true
LEANSDR_UNLIKELY(cond)  // Branch likely false
```

## Debug/Profiling

```cpp
LEANSDR_ASSERT(cond)                    // Runtime assertion
LEANSDR_LOG_DEBUG("value=%d", x)        // Debug logging
LEANSDR_PROFILE_START(my_func)          // Start timing
LEANSDR_PROFILE_END(my_func)            // End timing
```

## Complete Example: Optimized FIR Filter

```cpp
#include "leansdr/config.h"
#include "leansdr/neon_helpers.h"

void fir_filter(float* out, const float* in, const float* coeff,
                size_t n, size_t taps) {
#if LEANSDR_USE_NEON_INTRINSICS
    using namespace leansdr::neon;

    for (size_t i = 0; i < n; ++i) {
        size_t j = 0;
        float32x4_t sum = vzero_f32();

        // SIMD loop
        for (; j + 4 <= taps; j += 4) {
            float32x4_t vin = vloadu_f32(in + i - j);
            float32x4_t vc = vload_f32(coeff + j);
            sum = vmadd_f32(sum, vin, vc);
        }

        float result = vhsum_f32(sum);

        // Scalar remainder
        for (; j < taps; ++j) {
            result += in[i - j] * coeff[j];
        }

        out[i] = result;
    }
#else
    // Scalar fallback
    for (size_t i = 0; i < n; ++i) {
        float sum = 0;
        for (size_t j = 0; j < taps; ++j) {
            sum += in[i - j] * coeff[j];
        }
        out[i] = sum;
    }
#endif
}
```

## Compilation Quick Reference

```bash
# ARM32 + NEON
g++ -mfpu=neon -march=armv7-a -Isrc -O3 app.cpp

# ARM64 (NEON standard)
g++ -march=armv8-a -Isrc -O3 app.cpp

# x86 (scalar fallback)
g++ -Isrc -O3 app.cpp

# With debug
g++ -g -DLEANSDR_DEBUG=1 -Isrc app.cpp

# With profiling
g++ -DLEANSDR_ENABLE_PROFILING=1 -Isrc -O3 app.cpp
```

## Performance Checklist

- [ ] Use 16-byte aligned memory (`aligned_alloc(16, size)`)
- [ ] Process data in chunks of 4 floats
- [ ] Handle remainder with scalar code
- [ ] Use separated format for complex operations
- [ ] Add prefetch hints for large arrays
- [ ] Profile before and after optimization
- [ ] Test on actual ARM hardware
- [ ] Verify results match scalar version
