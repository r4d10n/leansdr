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

#ifndef LEANSDR_NEON_EXAMPLE_H
#define LEANSDR_NEON_EXAMPLE_H

#include "leansdr/config.h"
#include "leansdr/math.h"

#if LEANSDR_USE_NEON_INTRINSICS
#include "leansdr/neon_helpers.h"
#endif

namespace leansdr {

// ======================================================================
// Example 1: Complex Multiply-Accumulate with SIMD Optimization
// ======================================================================

// Complex multiply-accumulate: result = a + b * c
template<typename T>
inline void complex_macc_scalar(complex<T>* LEANSDR_RESTRICT result,
                                const complex<T>* LEANSDR_RESTRICT a,
                                const complex<T>* LEANSDR_RESTRICT b,
                                const complex<T>* LEANSDR_RESTRICT c,
                                size_t count) {
  for (size_t i = 0; i < count; ++i) {
    T re = a[i].re + (b[i].re * c[i].re - b[i].im * c[i].im);
    T im = a[i].im + (b[i].re * c[i].im + b[i].im * c[i].re);
    result[i].re = re;
    result[i].im = im;
  }
}

#if LEANSDR_USE_NEON_INTRINSICS

// NEON-optimized version for float32
inline void complex_macc_neon(complex<float>* LEANSDR_RESTRICT result,
                              const complex<float>* LEANSDR_RESTRICT a,
                              const complex<float>* LEANSDR_RESTRICT b,
                              const complex<float>* LEANSDR_RESTRICT c,
                              size_t count) {
  using namespace neon;

  size_t i = 0;
  const size_t simd_count = (count / 4) * 4;  // Process 4 complex numbers at a time

  // SIMD loop
  for (; i < simd_count; i += 4) {
    cf32x4_sep_t va = vload_cf32_separated(a + i);
    cf32x4_sep_t vb = vload_cf32_separated(b + i);
    cf32x4_sep_t vc = vload_cf32_separated(c + i);

    cf32x4_sep_t vresult = vcmadd_f32_sep(va, vb, vc);

    vstore_cf32_separated(result + i, vresult);
  }

  // Scalar remainder
  for (; i < count; ++i) {
    float re = a[i].re + (b[i].re * c[i].re - b[i].im * c[i].im);
    float im = a[i].im + (b[i].re * c[i].im + b[i].im * c[i].re);
    result[i].re = re;
    result[i].im = im;
  }
}

#endif

// Runtime-dispatched version
inline void complex_macc(complex<float>* LEANSDR_RESTRICT result,
                         const complex<float>* LEANSDR_RESTRICT a,
                         const complex<float>* LEANSDR_RESTRICT b,
                         const complex<float>* LEANSDR_RESTRICT c,
                         size_t count) {
#if LEANSDR_USE_NEON_INTRINSICS && LEANSDR_ENABLE_RUNTIME_DISPATCH
  if (use_neon()) {
    complex_macc_neon(result, a, b, c, count);
  } else {
    complex_macc_scalar(result, a, b, c, count);
  }
#elif LEANSDR_USE_NEON_INTRINSICS
  complex_macc_neon(result, a, b, c, count);
#else
  complex_macc_scalar(result, a, b, c, count);
#endif
}

// ======================================================================
// Example 2: Dot Product with SIMD Optimization
// ======================================================================

// Complex dot product: sum of (a[i] * conj(b[i]))
template<typename T>
inline complex<T> complex_dot_scalar(const complex<T>* LEANSDR_RESTRICT a,
                                     const complex<T>* LEANSDR_RESTRICT b,
                                     size_t count) {
  complex<T> sum(0, 0);
  for (size_t i = 0; i < count; ++i) {
    sum.re += a[i].re * b[i].re + a[i].im * b[i].im;
    sum.im += a[i].im * b[i].re - a[i].re * b[i].im;
  }
  return sum;
}

#if LEANSDR_USE_NEON_INTRINSICS

inline complex<float> complex_dot_neon(const complex<float>* LEANSDR_RESTRICT a,
                                       const complex<float>* LEANSDR_RESTRICT b,
                                       size_t count) {
  using namespace neon;

  size_t i = 0;
  const size_t simd_count = (count / 4) * 4;

  // Accumulators
  float32x4_t sum_re = vzero_f32();
  float32x4_t sum_im = vzero_f32();

  // SIMD loop
  for (; i < simd_count; i += 4) {
    cf32x4_sep_t va = vload_cf32_separated(a + i);
    cf32x4_sep_t vb = vload_cf32_separated(b + i);

    // a * conj(b) = (a.re * b.re + a.im * b.im, a.im * b.re - a.re * b.im)
    sum_re = vmadd_f32(sum_re, va.re, vb.re);  // sum_re += a.re * b.re
    sum_re = vmadd_f32(sum_re, va.im, vb.im);  // sum_re += a.im * b.im

    sum_im = vmadd_f32(sum_im, va.im, vb.re);  // sum_im += a.im * b.re
    sum_im = vmsub_f32(sum_im, va.re, vb.im);  // sum_im -= a.re * b.im
  }

  // Horizontal reduction
  complex<float> sum;
  sum.re = vhsum_f32(sum_re);
  sum.im = vhsum_f32(sum_im);

  // Scalar remainder
  for (; i < count; ++i) {
    sum.re += a[i].re * b[i].re + a[i].im * b[i].im;
    sum.im += a[i].im * b[i].re - a[i].re * b[i].im;
  }

  return sum;
}

#endif

inline complex<float> complex_dot(const complex<float>* LEANSDR_RESTRICT a,
                                  const complex<float>* LEANSDR_RESTRICT b,
                                  size_t count) {
#if LEANSDR_USE_NEON_INTRINSICS && LEANSDR_ENABLE_RUNTIME_DISPATCH
  if (use_neon()) {
    return complex_dot_neon(a, b, count);
  } else {
    return complex_dot_scalar(a, b, count);
  }
#elif LEANSDR_USE_NEON_INTRINSICS
  return complex_dot_neon(a, b, count);
#else
  return complex_dot_scalar(a, b, count);
#endif
}

// ======================================================================
// Example 3: Vector Magnitude Squared with SIMD
// ======================================================================

template<typename T>
inline void magsq_scalar(T* LEANSDR_RESTRICT out,
                         const complex<T>* LEANSDR_RESTRICT in,
                         size_t count) {
  for (size_t i = 0; i < count; ++i) {
    out[i] = in[i].re * in[i].re + in[i].im * in[i].im;
  }
}

#if LEANSDR_USE_NEON_INTRINSICS

inline void magsq_neon(float* LEANSDR_RESTRICT out,
                       const complex<float>* LEANSDR_RESTRICT in,
                       size_t count) {
  using namespace neon;

  size_t i = 0;
  const size_t simd_count = (count / 4) * 4;

  // SIMD loop
  for (; i < simd_count; i += 4) {
    cf32x4_sep_t v = vload_cf32_separated(in + i);
    float32x4_t magsq = vcmagsq_f32_sep(v);
    vstore_f32(out + i, magsq);
  }

  // Scalar remainder
  for (; i < count; ++i) {
    out[i] = in[i].re * in[i].re + in[i].im * in[i].im;
  }
}

#endif

inline void magsq(float* LEANSDR_RESTRICT out,
                  const complex<float>* LEANSDR_RESTRICT in,
                  size_t count) {
#if LEANSDR_USE_NEON_INTRINSICS && LEANSDR_ENABLE_RUNTIME_DISPATCH
  if (use_neon()) {
    magsq_neon(out, in, count);
  } else {
    magsq_scalar(out, in, count);
  }
#elif LEANSDR_USE_NEON_INTRINSICS
  magsq_neon(out, in, count);
#else
  magsq_scalar(out, in, count);
#endif
}

// ======================================================================
// Example 4: FIR Filter Implementation
// ======================================================================

// Simple FIR filter with SIMD optimization
template<typename T>
class fir_filter {
public:
  fir_filter(const T* coeffs, size_t num_taps)
    : taps(num_taps), history_size(num_taps) {

    // Allocate aligned memory for coefficients
    #if LEANSDR_USE_NEON_INTRINSICS
      coefficients = (T*)aligned_alloc(LEANSDR_SIMD_ALIGN, num_taps * sizeof(T));
      history = (T*)aligned_alloc(LEANSDR_SIMD_ALIGN, num_taps * sizeof(T));
    #else
      coefficients = new T[num_taps];
      history = new T[num_taps];
    #endif

    // Copy coefficients
    for (size_t i = 0; i < num_taps; ++i) {
      coefficients[i] = coeffs[i];
      history[i] = 0;
    }

    history_index = 0;
  }

  ~fir_filter() {
    #if LEANSDR_USE_NEON_INTRINSICS
      free(coefficients);
      free(history);
    #else
      delete[] coefficients;
      delete[] history;
    #endif
  }

  T process_scalar(T input) {
    // Add new sample to history
    history[history_index] = input;

    // Compute dot product
    T result = 0;
    size_t h_idx = history_index;
    for (size_t i = 0; i < taps; ++i) {
      result += history[h_idx] * coefficients[i];
      h_idx = (h_idx == 0) ? (taps - 1) : (h_idx - 1);
    }

    // Update index
    history_index = (history_index + 1) % taps;

    return result;
  }

  T process(T input) {
    // For now, use scalar version
    // NEON version would require more complex index management
    return process_scalar(input);
  }

private:
  T* coefficients;
  T* history;
  size_t taps;
  size_t history_size;
  size_t history_index;
};

// ======================================================================
// Usage Pattern Template
// ======================================================================

/*
  USAGE EXAMPLE:

  #include "leansdr/config.h"
  #include "leansdr/neon_example.h"

  void my_dsp_function() {
    const size_t N = 1024;
    complex<float> a[N], b[N], c[N], result[N];

    // Initialize data...

    // Use optimized complex multiply-accumulate
    leansdr::complex_macc(result, a, b, c, N);

    // Use optimized dot product
    complex<float> dot = leansdr::complex_dot(a, b, N);

    // Use magnitude squared
    float magsq_values[N];
    leansdr::magsq(magsq_values, a, N);
  }

  COMPILATION:

  For ARM with NEON:
    g++ -mfpu=neon -march=armv7-a -Isrc -std=c++11 -O3 myfile.cpp

  For ARM64:
    g++ -march=armv8-a -Isrc -std=c++11 -O3 myfile.cpp

  For x86 (NEON disabled, uses scalar fallback):
    g++ -Isrc -std=c++11 -O3 myfile.cpp
*/

} // namespace leansdr

#endif // LEANSDR_NEON_EXAMPLE_H
