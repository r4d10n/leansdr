# SDR Optimization Quick Reference Guide

## Overview
Fast-track guide for implementing NEON optimizations in LeanSDR's SDR blocks.

---

## Critical Components by Priority

### Priority 1: FIR Sampler (HIGHEST IMPACT)
**Location:** Lines 645-665 in sdr.h
**Current Time:** ~44 cycles/symbol
**NEON Time:** ~11 cycles/symbol
**Impact:** 3-4x speedup on 20-30% of code
**Complexity:** Medium

```cpp
// Current bottleneck loop
while ( pc < pcend )
  acc += (*pc++) * (*pin++);

// NEON approach: Process 4 complex taps per iteration
// Load interleaved coefficients and samples
// Use vfmaq_f32 + vfmsq_f32 for complex multiply-accumulate
// Horizontal sum at end
```

**Expected Results:** +25-35% overall throughput

---

### Priority 2: AGC Power Loop (MEDIUM IMPACT)
**Location:** Lines 256-259 (simple_agc) and 177-179 (ss_estimator)
**Current Time:** ~3-4 cycles per sample
**NEON Time:** ~1 cycle per sample
**Impact:** 3-4x speedup on 5-10% of code
**Complexity:** Low

```cpp
// Current loop
for ( ; pin<pend; ++pin ) 
  amp2 += pin->re*pin->re + pin->im*pin->im;

// NEON: Load 4 samples, vfmaq_f32 for both I and Q
```

**Expected Results:** +5-10% overall throughput

---

### Priority 3: Mueller-Müller TED (LOW IMPACT)
**Location:** Lines 822-833
**Current Time:** ~6-8 cycles/symbol
**NEON Time:** ~3-4 cycles/symbol
**Impact:** 2x speedup on ~5% of code
**Complexity:** Medium

```cpp
// Two dot products: (p-history) · c, and (c-history) · p
// Use float32x2_t and vdot_f32 (ARMv8.2+)
```

**Expected Results:** +2-3% overall throughput

---

## NEON Instruction Set Reference

### Complex Multiply-Accumulate Pattern
```cpp
// (a+bi) * (c+di) = (ac-bd) + (ad+bc)i

float32x4_t acc_r = vdupq_n_f32(0);
float32x4_t acc_i = vdupq_n_f32(0);

// Load 4 complex numbers
float32x4_t c_r = vld1q_f32(&coeff[0].re);  // C0.re, C1.re, C2.re, C3.re
float32x4_t c_i = vld1q_f32(&coeff[0].im);  // C0.im, C1.im, C2.im, C3.im
float32x4_t s_r = vld1q_f32(&sample[0].re); // S0.re, S1.re, S2.re, S3.re
float32x4_t s_i = vld1q_f32(&sample[0].im); // S0.im, S1.im, S2.im, S3.im

// Compute: Real = c.re*s.re - c.im*s.im
acc_r = vfmaq_f32(acc_r, c_r, s_r);  // += c.re * s.re
acc_r = vfmsq_f32(acc_r, c_i, s_i);  // -= c.im * s.im

// Compute: Imag = c.re*s.im + c.im*s.re
acc_i = vfmaq_f32(acc_i, c_r, s_i);  // += c.re * s.im
acc_i = vfmaq_f32(acc_i, c_i, s_r);  // += c.im * s.re

// Horizontal sum
float sum_r = vaddvq_f32(acc_r);  // ARMv8.3+
float sum_i = vaddvq_f32(acc_i);
```

### Power Accumulation Pattern
```cpp
float32x4_t pwr = vdupq_n_f32(0);

for (int i = 0; i < n; i += 4) {
  float32x4_t re = vld1q_f32(&samples[i].re);
  float32x4_t im = vld1q_f32(&samples[i].im);
  pwr = vfmaq_f32(pwr, re, re);  // pwr += re*re
  pwr = vfmaq_f32(pwr, im, im);  // pwr += im*im
}

// Sum lanes
float result = (vgetq_lane_f32(pwr, 0) +
                vgetq_lane_f32(pwr, 1) +
                vgetq_lane_f32(pwr, 2) +
                vgetq_lane_f32(pwr, 3));
```

### Conditional Compilation
```cpp
#ifdef __ARM_NEON__
  // NEON-optimized version
  // Use vfmaq_f32, vfmsq_f32, vld1q_f32, etc.
#else
  // Fallback scalar version
#endif
```

---

## Memory Layout Optimization

### Current (Interleaved)
```
[R0, I0, R1, I1, R2, I2, ...]
```
Issue: SIMD loads require deinterleaving

### Proposed (Planar)
```
[R0, R1, R2, ... | I0, I1, I2, ...]
```
Benefit: Direct SIMD loads without shuffling

**Implementation:** Use only for internal FIR buffers, not pipeline interface

---

## Compilation Flags

### Enable NEON
```bash
GCC:   -mfpu=neon -march=armv7-a
Clang: -march=armv8-a+simd  # ARMv8 with NEON
```

### Auto-vectorization hints
```cpp
#pragma omp simd reduction(+:acc)
for (...) { acc += ...; }
```

### Restrict pointer optimization
```cpp
void function(float * __restrict a, float * __restrict b) {
  // Compiler assumes no aliasing
}
```

---

## Architecture-Specific Performance

| CPU | Max Freq | NEON | Notes |
|-----|----------|------|-------|
| Cortex-A53 | 1.2 GHz | ARMv7/v8 | In-order, low power |
| Cortex-A72 | 2.0 GHz | ARMv8 | Out-of-order, high perf |
| Cortex-A76+ | 3.0+ GHz | ARMv8.2+ | Dual-issue, SIMD latency hiding |

**FIR performance estimate:** 1-2 symbols/MHz/tap
- A53: ~60 symbols/ms for 21-tap FIR
- A72: ~120 symbols/ms for 21-tap FIR

---

## Profiling Guide

### Using `perf` to identify bottlenecks
```bash
perf record -e cycles,instructions ./leandvb ...
perf report --stdio | head -30
```

### Expected bottleneck distribution
- FIR sampler: 20-30% (TARGET: reduce to 5-10%)
- TED calculation: 5-10%
- Constellation lookup: <1%
- AGC: 5-10% (TARGET: reduce to 1-5%)
- Other: 50%

### After NEON optimization
- FIR sampler: 5-10% (3-4x speedup)
- AGC: 1-5% (3-4x speedup)
- Overall: 30-40% speedup achievable

---

## Testing Checklist

- [ ] Implement FIR NEON version
- [ ] Verify output bit-exact with scalar version
- [ ] Benchmark: scalar vs NEON
- [ ] Check CPU detection (runtime NEON enablement)
- [ ] Profile full pipeline with perf
- [ ] Test edge cases (various ncoeffs, oversampling)
- [ ] Verify on target CPU (Cortex-A53/A72)
- [ ] Document performance gains

---

## File Modifications Summary

### New Files
- `src/leansdr/fir_neon.h` - NEON FIR sampler template
- `src/leansdr/agc_neon.h` - NEON AGC implementations
- `src/leansdr/ted_neon.h` - NEON timing recovery helpers

### Modified Files
- `src/leansdr/sdr.h` - Add NEON ifdef guards in hot loops
- `CMakeLists.txt` - Add -mfpu=neon compiler flag

### Backward Compatibility
- All NEON code inside `#ifdef __ARM_NEON__`
- Fallback to scalar code on non-ARM platforms
- Runtime detection recommended for deployments

---

## Expected Results

### FIR Sampler Only
- **Speedup:** 3-4x
- **Code coverage:** 25-30%
- **Overall throughput gain:** 20-30%

### FIR + AGC
- **Speedup:** 3-4x each
- **Code coverage:** 35-40%
- **Overall throughput gain:** 30-45%

### Full Suite (FIR + AGC + TED)
- **Speedup:** 2-4x per component
- **Code coverage:** 40-50%
- **Overall throughput gain:** 40-60%

### Example: Raspberry Pi 4
- Cortex-A72 (2 GHz)
- Current: 1.2M symbols/sec
- After FIR+AGC: 1.6M symbols/sec (+33%)
- After full suite: 1.9M symbols/sec (+58%)

---

## Common Pitfalls to Avoid

1. **Memory aliasing:** Use `__restrict` to help compiler
2. **Unaligned loads:** Ensure 16-byte alignment for vld1q
3. **Interleaved format:** Deinterleave before NEON, reinterleave after
4. **Scalar remainders:** Handle non-4x block sizes
5. **Cache efficiency:** Minimize LUT thrashing in inner loop
6. **Register pressure:** Don't accumulate >4 NEON registers

---

## Further Reading

- ARM NEON Intrinsics: https://developer.arm.com/architectures/instruction-sets/intrinsics/
- GCC auto-vectorization: https://gcc.gnu.org/projects/tree-ssa/vectorization.html
- Optimization guide: ARM Cortex-A series optimization manual

