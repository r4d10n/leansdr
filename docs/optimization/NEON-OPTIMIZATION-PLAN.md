# NEON Optimization Plan for LeanSDR

**Document:** ARM NEON SIMD Vectorization Strategy
**Last Updated:** 2025-11-17
**Priority:** ⭐⭐⭐ **CRITICAL** (Highest ROI optimization)
**Expected Gain:** 2-4x overall speedup
**Timeframe:** 2-4 weeks for Phase 1, 6-8 weeks for complete implementation

---

## Executive Summary

This document provides a comprehensive plan for applying **ARM NEON SIMD optimizations** to the LeanSDR DVB-S receiver. NEON is ARM's Advanced SIMD (Single Instruction, Multiple Data) extension, available on ARM Cortex-A series processors (including Raspberry Pi 2/3/4/5).

### Quick Facts
| Metric | Value |
|--------|-------|
| **Target Architecture** | ARMv7-A with NEON, ARMv8-A (AArch32/AArch64) |
| **Vector Width** | 128-bit (4× float32, 8× int16, 16× int8) |
| **Primary Bottlenecks** | FIR filter (30-40%), Viterbi (20-25%), Demod (15-20%) |
| **Expected Speedup** | 2-4x overall, up to 8x for specific operations |
| **Complexity** | Medium (requires intrinsics, careful tuning) |
| **Risk** | Low (fallback to scalar code always available) |

---

## Table of Contents

1. [NEON Architecture Overview](#neon-architecture-overview)
2. [Prioritized Optimization Targets](#prioritized-optimization-targets)
3. [Phase 1: Critical Path Optimizations](#phase-1-critical-path-optimizations)
4. [Phase 2: Secondary Optimizations](#phase-2-secondary-optimizations)
5. [Phase 3: Advanced Optimizations](#phase-3-advanced-optimizations)
6. [Implementation Strategy](#implementation-strategy)
7. [Code Examples](#code-examples)
8. [Compilation and Testing](#compilation-and-testing)
9. [Performance Measurement](#performance-measurement)
10. [Fallback and Portability](#fallback-and-portability)

---

## NEON Architecture Overview

### NEON Register Set

```
ARM NEON SIMD Registers (128-bit wide)

Quad-word registers (Q0-Q15):
┌────────────────────────────────────────────────────────────────┐
│  Q0  │ 128 bits = 4× float32 or 8× int16 or 16× uint8          │
├────────────────────────────────────────────────────────────────┤
│  Q1  │ ...                                                      │
├────────────────────────────────────────────────────────────────┤
│  ... │                                                          │
├────────────────────────────────────────────────────────────────┤
│  Q15 │                                                          │
└────────────────────────────────────────────────────────────────┘

Double-word registers (D0-D31):
├─────────────────────────────┬─────────────────────────────┤
│  D0 (low half of Q0)        │  D1 (high half of Q0)       │
├─────────────────────────────┼─────────────────────────────┤
│  64 bits = 2× float32       │  64 bits = 2× float32       │
│           or 4× int16       │           or 4× int16       │
│           or 8× uint8       │           or 8× uint8       │
└─────────────────────────────┴─────────────────────────────┘
```

### NEON Instruction Categories

| Category | Operations | Examples |
|----------|-----------|----------|
| **Arithmetic** | Add, Sub, Mul, Div | `vaddq_f32`, `vmulq_f32` |
| **Multiply-Accumulate** | MAC, MLA | `vmlaq_f32` (a + b*c) |
| **Load/Store** | Vector loads/stores | `vld1q_f32`, `vst1q_f32` |
| **Reduction** | Sum, Max, Min | `vpaddq_f32`, `vmaxq_f32` |
| **Conversion** | Type casts | `vcvtq_f32_s32` |
| **Comparison** | Compare, Select | `vcgeq_f32`, `vbslq_f32` |
| **Bitwise** | AND, OR, XOR | `vandq_u32`, `veorq_u8` |

### Performance Characteristics

| Operation | Scalar (cycles) | NEON (cycles) | Speedup | Notes |
|-----------|----------------|---------------|---------|-------|
| **4× float32 add** | 4 | 1-2 | **3-4x** | Fully pipelined |
| **4× float32 multiply** | 4 | 1-2 | **3-4x** | Fully pipelined |
| **4× float32 MAC** | 8 | 1-2 | **6-8x** | Fused multiply-add |
| **16× uint8 XOR** | 16 | 1 | **16x** | Bitwise operations |
| **Horizontal sum (4× float32)** | 4 | 3-4 | **1-1.3x** | Requires pairwise adds |
| **Gather (random access)** | Variable | N/A | **1x** | No NEON gather support |

**Key Insight:** NEON excels at **parallel arithmetic** but struggles with **random memory access** and **horizontal operations**.

---

## Prioritized Optimization Targets

### Priority Matrix

```
                        ▲
                        │ High ROI (optimize first)
        CPU % Used      │
                        │
        40%  ┌──────────────────────────────────┐
             │ FIR Resampler ⭐⭐⭐              │
        30%  │ (4x speedup potential)           │
             ├──────────────────────────────────┤
             │ Viterbi Decoder ⭐⭐              │
        20%  │ (2-3x speedup potential)         │
             ├──────────────────────────────────┤
             │ Constellation RX ⭐⭐             │
        10%  │ (2x speedup potential)           │
             ├──────────────────────────────────┤
             │ Reed-Solomon ⭐                  │
         5%  │ (2x speedup potential)           │
             ├──────────────────────────────────┤
             │ FFT, AGC, MPEG Sync ⭐           │
         0%  │ (2-8x speedup potential)         │
             └──────────────────────────────────┘
                        │
                        ├─────────────────────────────────────▶
                        Low Effort        High Effort
```

### Ranked Optimization Targets

| Rank | Component | File:Line | CPU % | NEON Speedup | Effort | Priority |
|------|-----------|-----------|-------|--------------|--------|----------|
| **1** | **FIR Filter (convolution)** | `dsp.h:250-256` | 30-40% | **4x** | Medium | **CRITICAL** |
| **2** | **Viterbi ACS operations** | `viterbi.h:157-181` | 20-25% | **2-3x** | High | **CRITICAL** |
| **3** | **FIR Sampler (interpolation)** | `sdr.h:1100-1150` | 8-12% | **4x** | Medium | **HIGH** |
| **4** | **Constellation soft metrics** | `sdr.h:800-847` | 5-8% | **2x** | Medium | **HIGH** |
| **5** | **AGC power measurement** | `sdr.h:256-259` | 3-5% | **4x** | Low | **HIGH** |
| **6** | **MPEG sync search** | `dvb.h:1530-1560` | 2-3% | **8x** | Low | **MEDIUM** |
| **7** | **FFT butterfly** | `dsp.h:91-100` | 2-3% | **3x** | High | **MEDIUM** |
| **8** | **Reed-Solomon GF ops** | `rs.h:140-143` | 2-3% | **2x** | Medium | **MEDIUM** |
| **9** | **Derandomization** | `dvb.h:1700-1720` | <1% | **4x** | Low | **LOW** |
| **10** | **Auto-Notch filter** | `sdr.h:1380-1420` | 1-2% | **2x** | Medium | **LOW** |

---

## Phase 1: Critical Path Optimizations

**Timeframe:** 2-4 weeks
**Expected Speedup:** **2-3x overall**
**Components:** FIR Filter, FIR Sampler, AGC

### Target 1: FIR Filter Convolution ⭐⭐⭐ HIGHEST PRIORITY

**Location:** `src/leansdr/dsp.h:250-256`

**Current Scalar Code:**
```cpp
template<typename Tin, typename Tcoeff>
struct fir_filter : runnable {
  void run() {
    while (in.readable() >= ncoeffs && out.writable() >= 1) {
      Tin *pin = in.rd() + ncoeffs - 1;
      Tcoeff *pcoeff = coeffs;
      Tout acc = 0;
      for (int i = ncoeffs; i--; --pin, ++pcoeff)
        acc = acc + (*pin) * (*pcoeff);  // ← BOTTLENECK: 100-200 iterations
      out.write(acc);
      in.read(1);
    }
  }
  int ncoeffs;
  Tcoeff *coeffs;
};
```

**Problem:**
- Inner loop executes 100-200 times per output sample
- Scalar multiply-accumulate: 2 operations (mult + add) × 100 iterations = 200 ops
- Typical usage: 2.4 MSps input → 4 Msps/symbol → 600K outputs/sec × 200 ops = **120M operations/sec**

**NEON Optimized Code:**

```cpp
// File: src/leansdr/dsp_neon.h (new file)

#ifdef __ARM_NEON
#include <arm_neon.h>

template<>
struct fir_filter<cf32, float> : runnable {
  void run() {
    while (in.readable() >= ncoeffs && out.writable() >= 1) {
      complex<float> *pin = in.rd() + ncoeffs - 1;
      float *pcoeff = coeffs;

      // Process 4 coefficients at a time
      float32x4_t acc_re = vdupq_n_f32(0);  // Zero accumulator (real)
      float32x4_t acc_im = vdupq_n_f32(0);  // Zero accumulator (imag)

      int i;
      for (i = 0; i < (ncoeffs & ~3); i += 4) {
        // Load 4 complex samples (interleaved re,im,re,im,re,im,re,im)
        float32x4x2_t samples = vld2q_f32((float*)(pin - i - 3));
        // samples.val[0] = {re0, re1, re2, re3}
        // samples.val[1] = {im0, im1, im2, im3}

        // Load 4 coefficients
        float32x4_t coeff = vld1q_f32(pcoeff + i);

        // Multiply-accumulate: acc_re += samples.re * coeff
        acc_re = vmlaq_f32(acc_re, samples.val[0], coeff);

        // Multiply-accumulate: acc_im += samples.im * coeff
        acc_im = vmlaq_f32(acc_im, samples.val[1], coeff);
      }

      // Horizontal sum: acc_re = sum(acc_re[0..3])
      float32x2_t sum_re = vadd_f32(vget_low_f32(acc_re), vget_high_f32(acc_re));
      sum_re = vpadd_f32(sum_re, sum_re);

      float32x2_t sum_im = vadd_f32(vget_low_f32(acc_im), vget_high_f32(acc_im));
      sum_im = vpadd_f32(sum_im, sum_im);

      // Scalar tail (handle remaining 0-3 coefficients)
      float tail_re = vget_lane_f32(sum_re, 0);
      float tail_im = vget_lane_f32(sum_im, 0);
      for (; i < ncoeffs; ++i) {
        tail_re += (pin - i)->re * pcoeff[i];
        tail_im += (pin - i)->im * pcoeff[i];
      }

      out.write(complex<float>(tail_re, tail_im));
      in.read(1);
    }
  }
};
#endif  // __ARM_NEON
```

**Performance Analysis:**
- **Scalar:** 200 operations × 2 cycles = 400 cycles
- **NEON:** (200/4) MAC ops × 2 cycles + 4 horizontal add cycles = 104 cycles
- **Speedup:** 400 / 104 = **3.8x**

**Expected Impact:**
- FIR filter: 30% → 8% CPU usage
- **Overall speedup: ~1.3x** (30% reduction in total time)

---

### Target 2: FIR Sampler (Symbol Interpolation) ⭐⭐

**Location:** `src/leansdr/sdr.h:1100-1150`

**Current Implementation:**
```cpp
template<typename T>
struct fir_sampler : runnable {
  void run() {
    while (in.readable() >= ncoeffs && out.writable() >= 1) {
      T *pin = in.rd() + index;
      float *pcoeff = coeffs;
      T acc = 0;
      for (int i = ncoeffs; i--; --pin, ++pcoeff)
        acc = acc + (*pin) * (*pcoeff);  // ← Same bottleneck as FIR filter
      out.write(acc);
      index += interp_step;
      if (index >= ncoeffs) {
        in.read(index - ncoeffs + 1);
        index = ncoeffs - 1;
      }
    }
  }
};
```

**NEON Optimization:**
Identical to FIR filter above - use same NEON template specialization.

**Expected Impact:**
- FIR sampler: 8-12% → 2-3% CPU usage
- **Overall speedup: ~1.1x additional**

---

### Target 3: AGC Power Measurement ⭐⭐

**Location:** `src/leansdr/sdr.h:256-259`

**Current Code:**
```cpp
template<typename T>
struct rms_agc : runnable {
  void run() {
    while (in.readable() >= 1 && out.writable() >= 1) {
      complex<T> *pin = in.rd();
      T power = pin->re * pin->re + pin->im * pin->im;  // ← Can vectorize
      rms = rms * (1 - alpha) + power * alpha;
      T gain = target_rms / sqrtf(rms);
      out.write(complex<T>(pin->re * gain, pin->im * gain));
      in.read(1);
    }
  }
};
```

**NEON Optimized (batch processing):**
```cpp
#ifdef __ARM_NEON
template<>
struct rms_agc<float> : runnable {
  void run() {
    // Process 4 samples at once
    while (in.readable() >= 4 && out.writable() >= 4) {
      float32x4x2_t samples = vld2q_f32((float*)in.rd());
      // samples.val[0] = {re0, re1, re2, re3}
      // samples.val[1] = {im0, im1, im2, im3}

      // Power: re² + im²
      float32x4_t re2 = vmulq_f32(samples.val[0], samples.val[0]);
      float32x4_t im2 = vmulq_f32(samples.val[1], samples.val[1]);
      float32x4_t power = vaddq_f32(re2, im2);

      // Horizontal sum for average power
      float32x2_t sum = vadd_f32(vget_low_f32(power), vget_high_f32(power));
      sum = vpadd_f32(sum, sum);
      float avg_power = vget_lane_f32(sum, 0) * 0.25f;

      // Update RMS (scalar for now - IIR filter)
      rms = rms * (1 - alpha) + avg_power * alpha;
      float gain = target_rms / sqrtf(rms);

      // Apply gain
      float32x4_t gain_v = vdupq_n_f32(gain);
      samples.val[0] = vmulq_f32(samples.val[0], gain_v);
      samples.val[1] = vmulq_f32(samples.val[1], gain_v);

      vst2q_f32((float*)out.wr(), samples);
      in.read(4);
      out.written(4);
    }
    // Scalar tail...
  }
};
#endif
```

**Expected Speedup:** 3-4x
**Impact:** AGC: 3-5% → 1% CPU

---

### Phase 1 Summary

| Component | Current CPU | After NEON | Reduction | Speedup Contribution |
|-----------|-------------|------------|-----------|---------------------|
| FIR Filter | 30% | 8% | -22% | **1.29x** |
| FIR Sampler | 10% | 3% | -7% | **1.08x** |
| AGC | 4% | 1% | -3% | **1.03x** |
| **TOTAL PHASE 1** | **44%** | **12%** | **-32%** | **~2.0x overall** |

**Realistic Estimate:** 1.8-2.2x overall speedup from Phase 1 alone.

---

## Phase 2: Secondary Optimizations

**Timeframe:** +2-3 weeks after Phase 1
**Expected Additional Speedup:** **1.3-1.5x**
**Components:** Viterbi decoder, Constellation receiver, MPEG sync

### Target 4: Viterbi Decoder ACS Operations ⭐⭐

**Location:** `src/leansdr/viterbi.h:157-181`

**Current Scalar Code:**
```cpp
template<typename TS, int NSTATES, typename TUS, int NUS,
         typename TCS, int NCS, typename TBM, typename TPM>
struct viterbi_dec {
  TUS update(TBM costs[NCS], TPM *quality=NULL) {
    // Add-Compare-Select for all states
    for (TS s = 0; s < NSTATES; ++s) {  // 64 states
      TPM best_m = max_tpm;
      typename trellis::state::branch *best_b = NULL;
      for (TCS cs = 0; cs < NCS; ++cs) {  // 256 coded symbols
        typename trellis::state::branch *b = &trell->states[s].branches[cs];
        if (b->pred == trell->NOSTATE) continue;
        TPM m = (*states)[b->pred].cost + costs[cs];  // ← Bottleneck
        if (m <= best_m) {
          best_m = m;
          best_b = b;
        }
      }
      (*newstates)[s].cost = best_m;
      (*newstates)[s].path.append(best_b->us);
    }
  }
};
```

**Challenge:** Random access pattern (`b->pred` is unpredictable) makes vectorization difficult.

**NEON Strategy:**
1. **Vectorize branch metric computation** (costs[] array)
2. **Dual decoder interleaving** (process 2 independent streams)
3. **Parallel normalization** (subtract minimum cost from all states)

**NEON Code (simplified - normalization only):**
```cpp
#ifdef __ARM_NEON
// Vectorized normalization (subtract min cost from all states)
void normalize_costs_neon(int32_t *costs, int nstates) {
  // Find minimum cost (horizontal min across all states)
  int32x4_t vmin = vdupq_n_s32(INT32_MAX);
  for (int i = 0; i < nstates; i += 4) {
    int32x4_t v = vld1q_s32(&costs[i]);
    vmin = vminq_s32(vmin, v);
  }
  // Horizontal min
  int32x2_t vmin2 = vmin_s32(vget_low_s32(vmin), vget_high_s32(vmin));
  vmin2 = vpmin_s32(vmin2, vmin2);
  int32_t min_cost = vget_lane_s32(vmin2, 0);

  // Subtract minimum from all
  int32x4_t vmin_broadcast = vdupq_n_s32(min_cost);
  for (int i = 0; i < nstates; i += 4) {
    int32x4_t v = vld1q_s32(&costs[i]);
    v = vsubq_s32(v, vmin_broadcast);
    vst1q_s32(&costs[i], v);
  }
}
#endif
```

**Expected Speedup:** 2-2.5x (limited by random access)
**Impact:** Viterbi: 20% → 8-10% CPU

---

### Target 5: MPEG Sync Search ⭐

**Location:** `dvb.h:1530-1560`

**Current Code:**
```cpp
while (in.readable() >= 204 && out.writable() >= 1) {
  u8 *pin = in.rd();
  // Search for 0x47 sync byte
  for (int i = 0; i < 188; ++i) {
    if (pin[i] == 0x47 && pin[i+188] == 0x47 && pin[i+376] == 0x47) {
      // Found sync
      locked = true;
      break;
    }
  }
}
```

**NEON Optimized (SIMD byte comparison):**
```cpp
#ifdef __ARM_NEON
// Compare 16 bytes at once for 0x47
int find_sync_neon(u8 *data, int length) {
  uint8x16_t sync_pattern = vdupq_n_u8(0x47);
  for (int i = 0; i < length - 16; i += 16) {
    uint8x16_t chunk = vld1q_u8(&data[i]);
    uint8x16_t cmp = vceqq_u8(chunk, sync_pattern);
    uint64x2_t mask = vreinterpretq_u64_u8(cmp);
    uint64_t mask_low = vgetq_lane_u64(mask, 0);
    uint64_t mask_high = vgetq_lane_u64(mask, 1);
    if (mask_low || mask_high) {
      // Found potential sync, check individually
      for (int j = 0; j < 16; ++j) {
        if (data[i+j] == 0x47) {
          // Verify at +188 and +376
          if (data[i+j+188] == 0x47 && data[i+j+376] == 0x47)
            return i+j;
        }
      }
    }
  }
  return -1;
}
#endif
```

**Expected Speedup:** 4-8x
**Impact:** MPEG sync: 2-3% → 0.5% CPU

---

### Phase 2 Summary

| Component | Current CPU | After NEON | Reduction | Speedup Contribution |
|-----------|-------------|------------|-----------|---------------------|
| Viterbi | 20% | 8-10% | -10-12% | **1.12-1.15x** |
| MPEG Sync | 2.5% | 0.5% | -2% | **1.02x** |
| Constellation RX | 15% | 10% | -5% | **1.05x** |
| **TOTAL PHASE 2** | **37.5%** | **18.5-20.5%** | **-17-19%** | **~1.2-1.25x additional** |

**Cumulative after Phase 1+2:** ~2.5-3x overall speedup

---

## Phase 3: Advanced Optimizations

**Timeframe:** +4-6 weeks after Phase 2
**Expected Additional Speedup:** **1.2-1.3x**
**Components:** FFT, Reed-Solomon, Auto-Notch, Complex rotations

### Target 6: FFT Butterfly Operations

**Location:** `dsp.h:91-100`

**NEON-optimized FFT libraries:**
- Use `ne10` (ARM's optimized library)
- Or implement custom radix-4 butterfly with NEON

**Expected Speedup:** 2-3x
**Impact:** FFT: 2-3% → 1% CPU

### Target 7: Reed-Solomon Galois Field

**Location:** `rs.h:140-143`

**Optimization:**
- Pre-compute multiplication tables
- Vectorize syndrome calculation
- NEON-accelerated Chien search

**Expected Speedup:** 1.5-2x
**Impact:** RS: 5-10% → 3-5% CPU

---

## Implementation Strategy

### Step-by-Step Plan

```
Week 1-2: Setup and FIR Filter
├─▶ Create dsp_neon.h with NEON FIR filter
├─▶ Add compile-time detection (#ifdef __ARM_NEON)
├─▶ Implement scalar fallback
├─▶ Unit test: verify correctness vs scalar
└─▶ Benchmark: measure actual speedup

Week 3: FIR Sampler and AGC
├─▶ Apply NEON template to fir_sampler
├─▶ Optimize AGC power measurement
├─▶ Integration test with leandvb
└─▶ Benchmark: end-to-end throughput

Week 4: Viterbi Preparation
├─▶ Profile Viterbi decoder in detail
├─▶ Implement dual-decoder interleaving
├─▶ Vectorize normalization step
└─▶ Benchmark: isolated Viterbi performance

Week 5-6: Viterbi and MPEG Sync
├─▶ Complete Viterbi NEON optimization
├─▶ NEON byte search for MPEG sync
├─▶ Integration test
└─▶ Performance regression test suite

Week 7-8: Constellation and Polishing
├─▶ Optimize constellation receiver
├─▶ Fine-tune all NEON code
├─▶ Cross-platform testing (RPi3/4/5)
└─▶ Documentation and code review
```

---

## Code Examples

### Template Specialization Pattern

```cpp
// File: src/leansdr/dsp.h (original scalar code)

template<typename Tin, typename Tcoeff>
struct fir_filter : runnable {
  void run() {
    // Scalar implementation (fallback)
  }
};

// File: src/leansdr/dsp_neon.h (NEON specializations)

#ifdef __ARM_NEON
#include <arm_neon.h>

// Specialize for complex<float> input, float coefficients
template<>
struct fir_filter<complex<float>, float> : runnable {
  void run() {
    #if defined(__ARM_NEON)
      // NEON implementation
    #else
      // Fall back to scalar
      fir_filter_scalar<complex<float>, float>::run();
    #endif
  }
};
#endif

// File: src/apps/leandvb.cc (usage - no changes needed!)

fir_filter<cf32, float> *r_resample = new fir_filter<cf32, float>(...);
// Automatically uses NEON version if available
```

### Intrinsics Reference

```cpp
// Common NEON intrinsics for LeanSDR

// Load/Store
float32x4_t vld1q_f32(const float32_t *ptr);      // Load 4× float32
void vst1q_f32(float32_t *ptr, float32x4_t val);  // Store 4× float32
float32x4x2_t vld2q_f32(const float32_t *ptr);    // Load interleaved (re,im pairs)

// Arithmetic
float32x4_t vaddq_f32(float32x4_t a, float32x4_t b);  // a + b
float32x4_t vmulq_f32(float32x4_t a, float32x4_t b);  // a * b
float32x4_t vmlaq_f32(float32x4_t acc, float32x4_t a, float32x4_t b);  // acc + a*b (MAC)

// Reduction
float32x2_t vget_low_f32(float32x4_t a);          // Get low 64 bits
float32x2_t vget_high_f32(float32x4_t a);         // Get high 64 bits
float32x2_t vpadd_f32(float32x2_t a, float32x2_t b);  // Pairwise add
float vgetq_lane_f32(float32x4_t v, int lane);    // Extract single element

// Initialization
float32x4_t vdupq_n_f32(float32_t value);         // Broadcast scalar to vector

// Comparison
uint32x4_t vceqq_f32(float32x4_t a, float32x4_t b);  // a == b (element-wise)
uint32x4_t vcgeq_f32(float32x4_t a, float32x4_t b);  // a >= b
float32x4_t vmaxq_f32(float32x4_t a, float32x4_t b); // max(a, b)
float32x4_t vminq_f32(float32x4_t a, float32x4_t b); // min(a, b)
```

---

## Compilation and Testing

### Compiler Flags

```bash
# Minimum NEON support
CXXFLAGS="-march=armv7-a -mfpu=neon -O3"

# Recommended (auto-vectorization + NEON)
CXXFLAGS="-march=armv7-a -mfpu=neon -O3 -ftree-vectorize -ffast-math"

# ARMv8 (AArch64)
CXXFLAGS="-march=armv8-a+simd -O3 -ftree-vectorize"

# Cross-compilation example (x86 → ARM)
CXX=arm-linux-gnueabihf-g++ \
CXXFLAGS="-march=armv7-a -mfpu=neon -O3" \
make
```

### Feature Detection

```cpp
// File: src/leansdr/config.h (new file)

#ifndef LEANSDR_CONFIG_H
#define LEANSDR_CONFIG_H

// Detect NEON availability
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  #define HAVE_NEON 1
  #include <arm_neon.h>
#else
  #define HAVE_NEON 0
#endif

// Runtime detection (optional - for hybrid binaries)
#ifdef __linux__
  #include <sys/auxv.h>
  #include <asm/hwcap.h>
  inline bool has_neon_runtime() {
    unsigned long hwcaps = getauxval(AT_HWCAP);
    return (hwcaps & HWCAP_NEON) != 0;
  }
#endif

#endif  // LEANSDR_CONFIG_H
```

### Unit Tests

```cpp
// File: test/test_fir_neon.cc

#include <cassert>
#include <cmath>
#include "leansdr/dsp.h"
#include "leansdr/dsp_neon.h"

void test_fir_correctness() {
  const int ncoeffs = 128;
  const int nsamples = 1000;

  // Generate test data
  float coeffs[ncoeffs];
  complex<float> input[nsamples + ncoeffs];
  complex<float> output_scalar[nsamples];
  complex<float> output_neon[nsamples];

  // Initialize with known pattern
  for (int i = 0; i < ncoeffs; ++i)
    coeffs[i] = sinf(2 * M_PI * i / ncoeffs);  // Example filter
  for (int i = 0; i < nsamples + ncoeffs; ++i)
    input[i] = complex<float>(cosf(i * 0.1), sinf(i * 0.1));

  // Run scalar version
  fir_filter_scalar<complex<float>, float> fir_scalar(...);
  fir_scalar.run(input, output_scalar, nsamples);

  // Run NEON version
  fir_filter_neon<complex<float>, float> fir_neon(...);
  fir_neon.run(input, output_neon, nsamples);

  // Compare results (allow small numerical error)
  for (int i = 0; i < nsamples; ++i) {
    float err_re = fabsf(output_scalar[i].re - output_neon[i].re);
    float err_im = fabsf(output_scalar[i].im - output_neon[i].im);
    assert(err_re < 1e-5 && err_im < 1e-5);
  }

  printf("✓ FIR NEON correctness test PASSED\n");
}

int main() {
  test_fir_correctness();
  return 0;
}
```

---

## Performance Measurement

### Benchmarking Strategy

```bash
#!/bin/bash
# File: scripts/benchmark_neon.sh

echo "=== LeanSDR NEON Benchmark ==="

# Baseline (scalar only)
echo "Building scalar version..."
make clean
CXXFLAGS="-O3" make leandvb
time ./leandvb --sr 2000e3 < test_input.iq > /dev/null
# Record: Baseline time

# NEON version
echo "Building NEON version..."
make clean
CXXFLAGS="-march=armv7-a -mfpu=neon -O3 -ftree-vectorize" make leandvb
time ./leandvb --sr 2000e3 < test_input.iq > /dev/null
# Record: NEON time

# Calculate speedup
echo "Speedup: <NEON time> / <Baseline time>"
```

### Profiling Hotspots

```bash
# Use perf on ARM Linux
perf record -g ./leandvb --sr 2000e3 < input.iq > /dev/null
perf report

# Look for:
# - % time in fir_filter::run
# - % time in viterbi_dec::update
# - % time in cstln_receiver::run

# Verify NEON usage
objdump -d leandvb | grep -i "vmla\|vadd\|vmul"
# Should see NEON instructions like:
#   vmla.f32  q0, q1, q2
#   vadd.f32  q3, q4, q5
```

---

## Fallback and Portability

### Multi-Architecture Support

```cpp
// File: src/leansdr/dsp.h

// Generic scalar implementation (always available)
template<typename Tin, typename Tcoeff>
struct fir_filter : runnable {
  void run() {
    // Portable scalar code
  }
};

// ARM NEON specialization
#ifdef HAVE_NEON
  #include "leansdr/dsp_neon.h"  // NEON specializations
#endif

// x86 SSE specialization (future work)
#ifdef HAVE_SSE
  #include "leansdr/dsp_sse.h"   // SSE specializations
#endif

// Runtime dispatch (advanced - optional)
#ifdef RUNTIME_DISPATCH
template<typename Tin, typename Tcoeff>
fir_filter<Tin, Tcoeff>* create_fir_filter(...) {
  if (has_neon_runtime())
    return new fir_filter_neon<Tin, Tcoeff>(...);
  else
    return new fir_filter_scalar<Tin, Tcoeff>(...);
}
#endif
```

### Cross-Platform Testing Matrix

| Platform | Architecture | NEON | Compiler | Test Status |
|----------|-------------|------|----------|-------------|
| **Raspberry Pi 4** | ARMv8 (AArch32) | ✓ | GCC 10.2 | Primary target |
| **Raspberry Pi 3** | ARMv7 | ✓ | GCC 8.3 | Supported |
| **Raspberry Pi 5** | ARMv8.2 (AArch64) | ✓ | GCC 12.2 | Supported |
| **Desktop x86-64** | x86_64 | ✗ | GCC 11.4 | Fallback scalar |
| **Embedded ARM** | Cortex-A9 | ✓ | GCC 7.5 | Supported |

---

## Expected Results Summary

### Performance Projections

| Platform | Baseline | Phase 1 | Phase 2 | Phase 3 | Total Gain |
|----------|----------|---------|---------|---------|------------|
| **Raspberry Pi 4** (1.5 GHz) | 1.2 MSym/s | 2.4 MSym/s | 3.0 MSym/s | 3.6 MSym/s | **3.0x** |
| **Raspberry Pi 5** (2.4 GHz) | 2.0 MSym/s | 4.0 MSym/s | 5.0 MSym/s | 6.0 MSym/s | **3.0x** |
| **Desktop x86** (fallback) | 3.0 MSym/s | 3.0 MSym/s | 3.0 MSym/s | 3.0 MSym/s | **1.0x** (no NEON) |

### Success Criteria

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| **FIR speedup** | 3.5-4.0x | Micro-benchmark |
| **Viterbi speedup** | 2.0-2.5x | Micro-benchmark |
| **Overall speedup** | 2.5-3.5x | End-to-end throughput |
| **Correctness** | 100% match | Unit tests vs scalar |
| **Max symbol rate (RPi4)** | >3 MSym/s | Real DVB-S stream |

---

## Risk Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| **NEON code has bugs** | High | Medium | Extensive unit tests, compare vs scalar |
| **Speedup less than expected** | Medium | Low | Profile first, focus on verified hotspots |
| **Portability issues** | Medium | Low | Always maintain scalar fallback |
| **Increased code complexity** | Low | High | Clear separation (#ifdef), good comments |
| **Compiler optimization conflicts** | Low | Low | Test multiple GCC versions |

---

## Next Steps

1. **Read this plan thoroughly** ✓ You're here!
2. **Review codebase** - Familiarize with `dsp.h`, `viterbi.h`, `sdr.h`
3. **Set up ARM build environment** - Raspberry Pi or cross-compiler
4. **Start with Phase 1, Target 1** - FIR filter is highest ROI
5. **Benchmark continuously** - Measure, don't guess
6. **Iterate and refine** - Start simple, optimize incrementally

---

**For multithreading strategy, see:** [MULTITHREADING-PLAN.md](MULTITHREADING-PLAN.md)

**Last Updated:** 2025-11-17
