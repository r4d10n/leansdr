// This file is part of LeanSDR Copyright (C) 2016-2018 <pabr@pabr.org>.
// See the toplevel README for more information.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef LEANSDR_NEON_HELPERS_H
#define LEANSDR_NEON_HELPERS_H

#include "leansdr/config.h"

#if LEANSDR_USE_NEON_INTRINSICS

#include <arm_neon.h>
#include "leansdr/math.h"

namespace leansdr {
namespace neon {

// ======================================================================
// Type Definitions and Aliases
// ======================================================================

// Complex float32 representation as interleaved vector
// Layout: [re0, im0, re1, im1] for two complex numbers
typedef float32x4_t cf32x2_t;

// Complex float32 as separate real/imaginary vectors
struct cf32x4_sep_t {
  float32x4_t re;
  float32x4_t im;
};

// Complex int16 representation
// Layout: [re0, im0, re1, im1, re2, im2, re3, im3] for four complex numbers
typedef int16x8_t ci16x4_t;

// ======================================================================
// Vector Initialization
// ======================================================================

// Set all elements to zero
LEANSDR_FORCE_INLINE float32x4_t vzero_f32() {
  return vdupq_n_f32(0.0f);
}

LEANSDR_FORCE_INLINE int32x4_t vzero_s32() {
  return vdupq_n_s32(0);
}

LEANSDR_FORCE_INLINE int16x8_t vzero_s16() {
  return vdupq_n_s16(0);
}

// Set all elements to the same value
LEANSDR_FORCE_INLINE float32x4_t vset1_f32(float val) {
  return vdupq_n_f32(val);
}

LEANSDR_FORCE_INLINE int32x4_t vset1_s32(int32_t val) {
  return vdupq_n_s32(val);
}

LEANSDR_FORCE_INLINE int16x8_t vset1_s16(int16_t val) {
  return vdupq_n_s16(val);
}

// Set elements individually (from low to high)
LEANSDR_FORCE_INLINE float32x4_t vset_f32(float v0, float v1, float v2, float v3) {
  const float values[4] = {v0, v1, v2, v3};
  return vld1q_f32(values);
}

LEANSDR_FORCE_INLINE int32x4_t vset_s32(int32_t v0, int32_t v1, int32_t v2, int32_t v3) {
  const int32_t values[4] = {v0, v1, v2, v3};
  return vld1q_s32(values);
}

// ======================================================================
// Memory Operations - Load/Store
// ======================================================================

// Load aligned data
LEANSDR_FORCE_INLINE float32x4_t vload_f32(const float* LEANSDR_RESTRICT ptr) {
  LEANSDR_ASSERT_MSG(((uintptr_t)ptr & 15) == 0, "Address must be 16-byte aligned");
  return vld1q_f32(ptr);
}

LEANSDR_FORCE_INLINE int32x4_t vload_s32(const int32_t* LEANSDR_RESTRICT ptr) {
  LEANSDR_ASSERT_MSG(((uintptr_t)ptr & 15) == 0, "Address must be 16-byte aligned");
  return vld1q_s32(ptr);
}

LEANSDR_FORCE_INLINE int16x8_t vload_s16(const int16_t* LEANSDR_RESTRICT ptr) {
  LEANSDR_ASSERT_MSG(((uintptr_t)ptr & 15) == 0, "Address must be 16-byte aligned");
  return vld1q_s16(ptr);
}

// Load unaligned data (may be slower but safer)
LEANSDR_FORCE_INLINE float32x4_t vloadu_f32(const float* LEANSDR_RESTRICT ptr) {
  return vld1q_f32(ptr);
}

LEANSDR_FORCE_INLINE int32x4_t vloadu_s32(const int32_t* LEANSDR_RESTRICT ptr) {
  return vld1q_s32(ptr);
}

LEANSDR_FORCE_INLINE int16x8_t vloadu_s16(const int16_t* LEANSDR_RESTRICT ptr) {
  return vld1q_s16(ptr);
}

// Store aligned data
LEANSDR_FORCE_INLINE void vstore_f32(float* LEANSDR_RESTRICT ptr, float32x4_t v) {
  LEANSDR_ASSERT_MSG(((uintptr_t)ptr & 15) == 0, "Address must be 16-byte aligned");
  vst1q_f32(ptr, v);
}

LEANSDR_FORCE_INLINE void vstore_s32(int32_t* LEANSDR_RESTRICT ptr, int32x4_t v) {
  LEANSDR_ASSERT_MSG(((uintptr_t)ptr & 15) == 0, "Address must be 16-byte aligned");
  vst1q_s32(ptr, v);
}

LEANSDR_FORCE_INLINE void vstore_s16(int16_t* LEANSDR_RESTRICT ptr, int16x8_t v) {
  LEANSDR_ASSERT_MSG(((uintptr_t)ptr & 15) == 0, "Address must be 16-byte aligned");
  vst1q_s16(ptr, v);
}

// Store unaligned data
LEANSDR_FORCE_INLINE void vstoreu_f32(float* LEANSDR_RESTRICT ptr, float32x4_t v) {
  vst1q_f32(ptr, v);
}

LEANSDR_FORCE_INLINE void vstoreu_s32(int32_t* LEANSDR_RESTRICT ptr, int32x4_t v) {
  vst1q_s32(ptr, v);
}

LEANSDR_FORCE_INLINE void vstoreu_s16(int16_t* LEANSDR_RESTRICT ptr, int16x8_t v) {
  vst1q_s16(ptr, v);
}

// ======================================================================
// Complex Number Load/Store Operations
// ======================================================================

// Load two complex<float> numbers as interleaved vector [re0, im0, re1, im1]
LEANSDR_FORCE_INLINE cf32x2_t vload_cf32_interleaved(const complex<float>* LEANSDR_RESTRICT ptr) {
  return vld1q_f32(reinterpret_cast<const float*>(ptr));
}

// Store two complex<float> numbers from interleaved vector
LEANSDR_FORCE_INLINE void vstore_cf32_interleaved(complex<float>* LEANSDR_RESTRICT ptr, cf32x2_t v) {
  vst1q_f32(reinterpret_cast<float*>(ptr), v);
}

// Load four complex<float> numbers as separated real and imaginary parts
LEANSDR_FORCE_INLINE cf32x4_sep_t vload_cf32_separated(const complex<float>* LEANSDR_RESTRICT ptr) {
  // Load interleaved data
  float32x4x2_t data = vld2q_f32(reinterpret_cast<const float*>(ptr));
  cf32x4_sep_t result;
  result.re = data.val[0];  // Real parts
  result.im = data.val[1];  // Imaginary parts
  return result;
}

// Store four complex<float> numbers from separated real and imaginary parts
LEANSDR_FORCE_INLINE void vstore_cf32_separated(complex<float>* LEANSDR_RESTRICT ptr, cf32x4_sep_t v) {
  float32x4x2_t data;
  data.val[0] = v.re;
  data.val[1] = v.im;
  vst2q_f32(reinterpret_cast<float*>(ptr), data);
}

// Load four complex<int16_t> numbers as interleaved vector
LEANSDR_FORCE_INLINE ci16x4_t vload_ci16_interleaved(const complex<int16_t>* LEANSDR_RESTRICT ptr) {
  return vld1q_s16(reinterpret_cast<const int16_t*>(ptr));
}

// Store four complex<int16_t> numbers from interleaved vector
LEANSDR_FORCE_INLINE void vstore_ci16_interleaved(complex<int16_t>* LEANSDR_RESTRICT ptr, ci16x4_t v) {
  vst1q_s16(reinterpret_cast<int16_t*>(ptr), v);
}

// ======================================================================
// Basic Arithmetic Operations
// ======================================================================

// Addition
LEANSDR_FORCE_INLINE float32x4_t vadd_f32(float32x4_t a, float32x4_t b) {
  return vaddq_f32(a, b);
}

LEANSDR_FORCE_INLINE int32x4_t vadd_s32(int32x4_t a, int32x4_t b) {
  return vaddq_s32(a, b);
}

LEANSDR_FORCE_INLINE int16x8_t vadd_s16(int16x8_t a, int16x8_t b) {
  return vaddq_s16(a, b);
}

// Subtraction
LEANSDR_FORCE_INLINE float32x4_t vsub_f32(float32x4_t a, float32x4_t b) {
  return vsubq_f32(a, b);
}

LEANSDR_FORCE_INLINE int32x4_t vsub_s32(int32x4_t a, int32x4_t b) {
  return vsubq_s32(a, b);
}

LEANSDR_FORCE_INLINE int16x8_t vsub_s16(int16x8_t a, int16x8_t b) {
  return vsubq_s16(a, b);
}

// Multiplication
LEANSDR_FORCE_INLINE float32x4_t vmul_f32(float32x4_t a, float32x4_t b) {
  return vmulq_f32(a, b);
}

LEANSDR_FORCE_INLINE int32x4_t vmul_s32(int32x4_t a, int32x4_t b) {
  return vmulq_s32(a, b);
}

LEANSDR_FORCE_INLINE int16x8_t vmul_s16(int16x8_t a, int16x8_t b) {
  return vmulq_s16(a, b);
}

// Multiply-add: a + b * c
LEANSDR_FORCE_INLINE float32x4_t vmadd_f32(float32x4_t a, float32x4_t b, float32x4_t c) {
  return vmlaq_f32(a, b, c);
}

// Multiply-subtract: a - b * c
LEANSDR_FORCE_INLINE float32x4_t vmsub_f32(float32x4_t a, float32x4_t b, float32x4_t c) {
  return vmlsq_f32(a, b, c);
}

// Negate
LEANSDR_FORCE_INLINE float32x4_t vneg_f32(float32x4_t a) {
  return vnegq_f32(a);
}

LEANSDR_FORCE_INLINE int32x4_t vneg_s32(int32x4_t a) {
  return vnegq_s32(a);
}

LEANSDR_FORCE_INLINE int16x8_t vneg_s16(int16x8_t a) {
  return vnegq_s16(a);
}

// ======================================================================
// Complex Number Arithmetic (Interleaved Format)
// ======================================================================

// Complex addition: (a.re + b.re, a.im + b.im) for two complex numbers
LEANSDR_FORCE_INLINE cf32x2_t vcadd_f32(cf32x2_t a, cf32x2_t b) {
  return vaddq_f32(a, b);
}

// Complex subtraction: (a.re - b.re, a.im - b.im) for two complex numbers
LEANSDR_FORCE_INLINE cf32x2_t vcsub_f32(cf32x2_t a, cf32x2_t b) {
  return vsubq_f32(a, b);
}

// Complex multiplication: (a.re*b.re - a.im*b.im, a.re*b.im + a.im*b.re)
// Operates on two complex numbers in interleaved format
LEANSDR_FORCE_INLINE cf32x2_t vcmul_f32(cf32x2_t a, cf32x2_t b) {
  // a = [a.re0, a.im0, a.re1, a.im1]
  // b = [b.re0, b.im0, b.re1, b.im1]

  // Swap real and imaginary parts of b: [b.im0, b.re0, b.im1, b.re1]
  float32x4_t b_swap = vrev64q_f32(b);

  // ac_bd = [a.re0*b.re0, a.im0*b.im0, a.re1*b.re1, a.im1*b.im1]
  float32x4_t ac_bd = vmulq_f32(a, b);

  // ad_bc = [a.re0*b.im0, a.im0*b.re0, a.re1*b.im1, a.im1*b.re1]
  float32x4_t ad_bc = vmulq_f32(a, b_swap);

  // Real parts: ac - bd (negate imaginary multiplications)
  // Imaginary parts: ad + bc
  // Use negation mask: [1, -1, 1, -1] for real parts
  const uint32_t neg_mask_data[4] = {0x00000000, 0x80000000, 0x00000000, 0x80000000};
  float32x4_t neg_mask = vreinterpretq_f32_u32(vld1q_u32(neg_mask_data));
  float32x4_t ac_bd_neg = vreinterpretq_f32_u32(
    veorq_u32(vreinterpretq_u32_f32(ac_bd), vreinterpretq_u32_f32(neg_mask))
  );

  // Horizontal add pairs: [(ac-bd), (ad+bc), (ac-bd), (ad+bc)]
  float32x4_t result = vpaddq_f32(ac_bd_neg, ad_bc);

  return result;
}

// Complex conjugate: (a.re, -a.im)
LEANSDR_FORCE_INLINE cf32x2_t vcconj_f32(cf32x2_t a) {
  // Negate imaginary parts using mask: [0, -1, 0, -1]
  const uint32_t neg_mask_data[4] = {0x00000000, 0x80000000, 0x00000000, 0x80000000};
  float32x4_t neg_mask = vreinterpretq_f32_u32(vld1q_u32(neg_mask_data));
  return vreinterpretq_f32_u32(
    veorq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(neg_mask))
  );
}

// Complex magnitude squared: re*re + im*im
LEANSDR_FORCE_INLINE float32x4_t vcmagsq_f32(cf32x2_t a) {
  float32x4_t sq = vmulq_f32(a, a);
  // Add pairs: [re0*re0 + im0*im0, re1*re1 + im1*im1, ...]
  return vpaddq_f32(sq, sq);
}

// ======================================================================
// Complex Number Arithmetic (Separated Format)
// ======================================================================

// Complex multiplication using separated real/imaginary parts
// result.re = a.re * b.re - a.im * b.im
// result.im = a.re * b.im + a.im * b.re
LEANSDR_FORCE_INLINE cf32x4_sep_t vcmul_f32_sep(cf32x4_sep_t a, cf32x4_sep_t b) {
  cf32x4_sep_t result;

  float32x4_t ac = vmulq_f32(a.re, b.re);  // a.re * b.re
  float32x4_t bd = vmulq_f32(a.im, b.im);  // a.im * b.im
  float32x4_t ad = vmulq_f32(a.re, b.im);  // a.re * b.im
  float32x4_t bc = vmulq_f32(a.im, b.re);  // a.im * b.re

  result.re = vsubq_f32(ac, bd);  // ac - bd
  result.im = vaddq_f32(ad, bc);  // ad + bc

  return result;
}

// Complex multiply-add using separated format: result = a + b * c
LEANSDR_FORCE_INLINE cf32x4_sep_t vcmadd_f32_sep(cf32x4_sep_t a, cf32x4_sep_t b, cf32x4_sep_t c) {
  cf32x4_sep_t result;

  // result.re = a.re + (b.re * c.re - b.im * c.im)
  result.re = vmlaq_f32(a.re, b.re, c.re);   // a.re + b.re * c.re
  result.re = vmlsq_f32(result.re, b.im, c.im); // - b.im * c.im

  // result.im = a.im + (b.re * c.im + b.im * c.re)
  result.im = vmlaq_f32(a.im, b.re, c.im);   // a.im + b.re * c.im
  result.im = vmlaq_f32(result.im, b.im, c.re); // + b.im * c.re

  return result;
}

// Complex conjugate (separated format)
LEANSDR_FORCE_INLINE cf32x4_sep_t vcconj_f32_sep(cf32x4_sep_t a) {
  cf32x4_sep_t result;
  result.re = a.re;
  result.im = vnegq_f32(a.im);
  return result;
}

// Complex magnitude squared (separated format)
LEANSDR_FORCE_INLINE float32x4_t vcmagsq_f32_sep(cf32x4_sep_t a) {
  float32x4_t re_sq = vmulq_f32(a.re, a.re);
  float32x4_t im_sq = vmulq_f32(a.im, a.im);
  return vaddq_f32(re_sq, im_sq);
}

// ======================================================================
// Horizontal Reduction Operations
// ======================================================================

// Horizontal sum: return sum of all elements in vector
LEANSDR_FORCE_INLINE float vhsum_f32(float32x4_t v) {
  #if LEANSDR_ARM64
    // AArch64 has more efficient horizontal operations
    return vaddvq_f32(v);
  #else
    // ARMv7 NEON approach
    float32x2_t sum_pairs = vadd_f32(vget_low_f32(v), vget_high_f32(v));
    float32x2_t sum_final = vpadd_f32(sum_pairs, sum_pairs);
    return vget_lane_f32(sum_final, 0);
  #endif
}

LEANSDR_FORCE_INLINE int32_t vhsum_s32(int32x4_t v) {
  #if LEANSDR_ARM64
    return vaddvq_s32(v);
  #else
    int32x2_t sum_pairs = vadd_s32(vget_low_s32(v), vget_high_s32(v));
    int32x2_t sum_final = vpadd_s32(sum_pairs, sum_pairs);
    return vget_lane_s32(sum_final, 0);
  #endif
}

// Horizontal maximum: return maximum of all elements
LEANSDR_FORCE_INLINE float vhmax_f32(float32x4_t v) {
  #if LEANSDR_ARM64
    return vmaxvq_f32(v);
  #else
    float32x2_t max_pairs = vmax_f32(vget_low_f32(v), vget_high_f32(v));
    float32x2_t max_final = vpmax_f32(max_pairs, max_pairs);
    return vget_lane_f32(max_final, 0);
  #endif
}

// Horizontal minimum: return minimum of all elements
LEANSDR_FORCE_INLINE float vhmin_f32(float32x4_t v) {
  #if LEANSDR_ARM64
    return vminvq_f32(v);
  #else
    float32x2_t min_pairs = vmin_f32(vget_low_f32(v), vget_high_f32(v));
    float32x2_t min_final = vpmin_f32(min_pairs, min_pairs);
    return vget_lane_f32(min_final, 0);
  #endif
}

// ======================================================================
// Type Conversion Operations
// ======================================================================

// Convert int32 to float32
LEANSDR_FORCE_INLINE float32x4_t vcvt_f32_s32(int32x4_t v) {
  return vcvtq_f32_s32(v);
}

// Convert float32 to int32 (truncate)
LEANSDR_FORCE_INLINE int32x4_t vcvt_s32_f32(float32x4_t v) {
  return vcvtq_s32_f32(v);
}

// Convert float32 to int32 (round to nearest)
LEANSDR_FORCE_INLINE int32x4_t vcvt_s32_f32_round(float32x4_t v) {
  return vcvtnq_s32_f32(v);
}

// Widen int16 to int32 (lower half)
LEANSDR_FORCE_INLINE int32x4_t vcvt_s32_s16_low(int16x8_t v) {
  return vmovl_s16(vget_low_s16(v));
}

// Widen int16 to int32 (upper half)
LEANSDR_FORCE_INLINE int32x4_t vcvt_s32_s16_high(int16x8_t v) {
  return vmovl_s16(vget_high_s16(v));
}

// Narrow int32 to int16 (saturating)
LEANSDR_FORCE_INLINE int16x4_t vcvt_s16_s32_sat(int32x4_t v) {
  return vqmovn_s32(v);
}

// Narrow two int32x4 to one int16x8 (saturating)
LEANSDR_FORCE_INLINE int16x8_t vcvt_s16_s32_sat_x2(int32x4_t low, int32x4_t high) {
  return vcombine_s16(vqmovn_s32(low), vqmovn_s32(high));
}

// ======================================================================
// Bitwise Operations
// ======================================================================

// Bitwise AND
LEANSDR_FORCE_INLINE float32x4_t vand_f32(float32x4_t a, float32x4_t b) {
  return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}

// Bitwise OR
LEANSDR_FORCE_INLINE float32x4_t vor_f32(float32x4_t a, float32x4_t b) {
  return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}

// Bitwise XOR
LEANSDR_FORCE_INLINE float32x4_t vxor_f32(float32x4_t a, float32x4_t b) {
  return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}

// Bitwise AND NOT (a & ~b)
LEANSDR_FORCE_INLINE float32x4_t vandnot_f32(float32x4_t a, float32x4_t b) {
  return vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}

// ======================================================================
// Comparison and Selection Operations
// ======================================================================

// Compare equal
LEANSDR_FORCE_INLINE uint32x4_t vcmpeq_f32(float32x4_t a, float32x4_t b) {
  return vceqq_f32(a, b);
}

// Compare greater than
LEANSDR_FORCE_INLINE uint32x4_t vcmpgt_f32(float32x4_t a, float32x4_t b) {
  return vcgtq_f32(a, b);
}

// Compare greater than or equal
LEANSDR_FORCE_INLINE uint32x4_t vcmpge_f32(float32x4_t a, float32x4_t b) {
  return vcgeq_f32(a, b);
}

// Select: return a where mask is true, b where mask is false
LEANSDR_FORCE_INLINE float32x4_t vselect_f32(uint32x4_t mask, float32x4_t a, float32x4_t b) {
  return vbslq_f32(mask, a, b);
}

// Min/Max
LEANSDR_FORCE_INLINE float32x4_t vmin_f32(float32x4_t a, float32x4_t b) {
  return vminq_f32(a, b);
}

LEANSDR_FORCE_INLINE float32x4_t vmax_f32(float32x4_t a, float32x4_t b) {
  return vmaxq_f32(a, b);
}

// ======================================================================
// Advanced Operations
// ======================================================================

// Reciprocal estimate (fast but approximate)
LEANSDR_FORCE_INLINE float32x4_t vrecip_est_f32(float32x4_t v) {
  return vrecpeq_f32(v);
}

// Reciprocal (with Newton-Raphson refinement for better accuracy)
LEANSDR_FORCE_INLINE float32x4_t vrecip_f32(float32x4_t v) {
  float32x4_t recip = vrecpeq_f32(v);
  // Newton-Raphson iteration: x1 = x0 * (2 - v * x0)
  recip = vmulq_f32(recip, vrecpsq_f32(v, recip));
  return recip;
}

// Reciprocal square root estimate
LEANSDR_FORCE_INLINE float32x4_t vrsqrt_est_f32(float32x4_t v) {
  return vrsqrteq_f32(v);
}

// Reciprocal square root (with Newton-Raphson refinement)
LEANSDR_FORCE_INLINE float32x4_t vrsqrt_f32(float32x4_t v) {
  float32x4_t rsqrt = vrsqrteq_f32(v);
  // Newton-Raphson iteration
  rsqrt = vmulq_f32(rsqrt, vrsqrtsq_f32(vmulq_f32(v, rsqrt), rsqrt));
  return rsqrt;
}

// Square root (using reciprocal square root)
LEANSDR_FORCE_INLINE float32x4_t vsqrt_f32(float32x4_t v) {
  return vmulq_f32(v, vrsqrt_f32(v));
}

// Absolute value
LEANSDR_FORCE_INLINE float32x4_t vabs_f32(float32x4_t v) {
  return vabsq_f32(v);
}

LEANSDR_FORCE_INLINE int32x4_t vabs_s32(int32x4_t v) {
  return vabsq_s32(v);
}

// ======================================================================
// Shuffles and Permutations
// ======================================================================

// Reverse elements in vector
LEANSDR_FORCE_INLINE float32x4_t vreverse_f32(float32x4_t v) {
  // Reverse 64-bit pairs, then reverse within pairs
  float32x4_t rev64 = vrev64q_f32(v);
  return vextq_f32(rev64, rev64, 2);
}

// Extract lower half
LEANSDR_FORCE_INLINE float32x2_t vget_low_f32x2(float32x4_t v) {
  return vget_low_f32(v);
}

// Extract upper half
LEANSDR_FORCE_INLINE float32x2_t vget_high_f32x2(float32x4_t v) {
  return vget_high_f32(v);
}

// Combine two halves
LEANSDR_FORCE_INLINE float32x4_t vcombine_f32x4(float32x2_t low, float32x2_t high) {
  return vcombine_f32(low, high);
}

// Duplicate low half to both halves
LEANSDR_FORCE_INLINE float32x4_t vdup_low_f32(float32x4_t v) {
  float32x2_t low = vget_low_f32(v);
  return vcombine_f32(low, low);
}

// Duplicate high half to both halves
LEANSDR_FORCE_INLINE float32x4_t vdup_high_f32(float32x4_t v) {
  float32x2_t high = vget_high_f32(v);
  return vcombine_f32(high, high);
}

// ======================================================================
// Utility Functions
// ======================================================================

// Check if all elements are zero
LEANSDR_FORCE_INLINE bool vall_zero_f32(float32x4_t v) {
  float32x4_t zero = vzero_f32();
  uint32x4_t cmp = vceqq_f32(v, zero);
  uint64x2_t cmp64 = vreinterpretq_u64_u32(cmp);
  return vgetq_lane_u64(cmp64, 0) == ~0ULL && vgetq_lane_u64(cmp64, 1) == ~0ULL;
}

// Check if any element is zero
LEANSDR_FORCE_INLINE bool vany_zero_f32(float32x4_t v) {
  float32x4_t zero = vzero_f32();
  uint32x4_t cmp = vceqq_f32(v, zero);
  uint64x2_t cmp64 = vreinterpretq_u64_u32(cmp);
  return vgetq_lane_u64(cmp64, 0) != 0 || vgetq_lane_u64(cmp64, 1) != 0;
}

// ======================================================================
// Inline Assembly Hints for Critical Paths
// ======================================================================

// Hint to compiler that this is a hot path
#define LEANSDR_NEON_HOT_PATH __attribute__((hot))

// Hint to compiler about alignment
#define LEANSDR_NEON_ASSUME_ALIGNED(ptr, align) \
  __builtin_assume_aligned((ptr), (align))

// Hint that loop iterations are independent (for vectorization)
#define LEANSDR_NEON_IVDEP _Pragma("GCC ivdep")

// Unroll loop hint
#define LEANSDR_NEON_UNROLL(n) _Pragma(LEANSDR_STRINGIFY(GCC unroll n))

// Helper for pragma stringification
#define LEANSDR_STRINGIFY(x) LEANSDR_STRINGIFY2(x)
#define LEANSDR_STRINGIFY2(x) #x

} // namespace neon
} // namespace leansdr

#endif // LEANSDR_USE_NEON_INTRINSICS

#endif // LEANSDR_NEON_HELPERS_H
