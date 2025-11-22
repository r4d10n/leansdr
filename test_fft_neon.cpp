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

// ====================================================================
// NEON FFT Test and Benchmark Program
// ====================================================================
//
// This program tests the correctness and measures the performance
// of the NEON-optimized FFT implementation.
//
// Compilation:
//
//   1. NEON-only (custom implementation):
//      g++ -I../src -march=armv7-a -mfpu=neon -O3 -o test_fft_neon test_fft_neon.cpp
//
//   2. With Ne10 library:
//      g++ -I../src -DLEANSDR_USE_NE10 -march=armv7-a -mfpu=neon -O3 -lNE10 -o test_fft_neon test_fft_neon.cpp
//
//   3. Scalar fallback (x86 or ARM without NEON):
//      g++ -I../src -O3 -o test_fft_neon test_fft_neon.cpp
//
// Usage:
//   ./test_fft_neon [fft_size] [iterations]
//
//   Default: fft_size=1024, iterations=10000
//
// ====================================================================

#include "leansdr/fft_neon.h"
#include "leansdr/dsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

using namespace leansdr;

// ====================================================================
// Test Functions
// ====================================================================

// Test 1: Correctness - Compare NEON vs Scalar FFT
bool test_correctness(int fft_size) {
  printf("Test 1: Correctness (FFT size %d)\n", fft_size);

  complex<float> *data_neon = new complex<float>[fft_size];
  complex<float> *data_scalar = new complex<float>[fft_size];

  // Generate test signal: sum of multiple sinusoids
  for (int i = 0; i < fft_size; i++) {
    float t = (float)i / fft_size;
    float signal = 0.0f;
    signal += 1.0f * sinf(2.0f * M_PI * 10.0f * t);  // 10 Hz
    signal += 0.5f * sinf(2.0f * M_PI * 50.0f * t);  // 50 Hz
    signal += 0.3f * cosf(2.0f * M_PI * 120.0f * t); // 120 Hz
    data_neon[i].re = signal;
    data_neon[i].im = 0.0f;
    data_scalar[i] = data_neon[i];
  }

  // Perform FFT with both implementations
  cfft_engine_neon<float> fft_neon(fft_size);
  cfft_engine<float> fft_scalar(fft_size);

  fft_neon.inplace(data_neon, false);
  fft_scalar.inplace(data_scalar, false);

  // Compare results
  float max_error = 0.0f;
  float avg_error = 0.0f;
  for (int i = 0; i < fft_size; i++) {
    float err_re = fabsf(data_neon[i].re - data_scalar[i].re);
    float err_im = fabsf(data_neon[i].im - data_scalar[i].im);
    float err = sqrtf(err_re * err_re + err_im * err_im);
    if (err > max_error) max_error = err;
    avg_error += err;
  }
  avg_error /= fft_size;

  bool passed = (max_error < 1e-3f);
  printf("  Max error: %.6e\n", max_error);
  printf("  Avg error: %.6e\n", avg_error);
  printf("  Result: %s\n\n", passed ? "PASSED" : "FAILED");

  delete[] data_neon;
  delete[] data_scalar;

  return passed;
}

// Test 2: Round-trip - FFT followed by IFFT should restore original
bool test_roundtrip(int fft_size) {
  printf("Test 2: Round-trip (FFT → IFFT)\n");

  complex<float> *data = new complex<float>[fft_size];
  complex<float> *original = new complex<float>[fft_size];

  // Generate random data
  for (int i = 0; i < fft_size; i++) {
    data[i].re = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    data[i].im = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    original[i] = data[i];
  }

  // FFT → IFFT
  cfft_engine_neon<float> fft(fft_size);
  fft.inplace(data, false);  // Forward
  fft.inplace(data, true);   // Inverse

  // Compare with original
  float max_error = 0.0f;
  for (int i = 0; i < fft_size; i++) {
    float err_re = fabsf(data[i].re - original[i].re);
    float err_im = fabsf(data[i].im - original[i].im);
    float err = sqrtf(err_re * err_re + err_im * err_im);
    if (err > max_error) max_error = err;
  }

  bool passed = (max_error < 1e-3f);
  printf("  Max error: %.6e\n", max_error);
  printf("  Result: %s\n\n", passed ? "PASSED" : "FAILED");

  delete[] data;
  delete[] original;

  return passed;
}

// Test 3: Spectral peaks - Verify FFT detects correct frequencies
bool test_spectral_peaks(int fft_size) {
  printf("Test 3: Spectral peaks detection\n");

  complex<float> *data = new complex<float>[fft_size];

  // Generate signal with known frequencies
  const int test_freq1 = 10;  // Bin 10
  const int test_freq2 = 50;  // Bin 50
  for (int i = 0; i < fft_size; i++) {
    float t = (float)i / fft_size;
    data[i].re = sinf(2.0f * M_PI * test_freq1 * t) +
                 0.5f * sinf(2.0f * M_PI * test_freq2 * t);
    data[i].im = 0.0f;
  }

  // Perform FFT
  cfft_engine_neon<float> fft(fft_size);
  fft.inplace(data, false);

  // Find peaks
  int peak1_idx = 0, peak2_idx = 0;
  float peak1_mag = 0.0f, peak2_mag = 0.0f;

  for (int i = 1; i < fft_size / 2; i++) {
    float mag = sqrtf(data[i].re * data[i].re + data[i].im * data[i].im);
    if (mag > peak1_mag) {
      peak2_mag = peak1_mag;
      peak2_idx = peak1_idx;
      peak1_mag = mag;
      peak1_idx = i;
    } else if (mag > peak2_mag) {
      peak2_mag = mag;
      peak2_idx = i;
    }
  }

  // Verify peaks are at expected frequencies
  bool passed = (peak1_idx == test_freq1 || peak1_idx == test_freq2) &&
                (peak2_idx == test_freq1 || peak2_idx == test_freq2) &&
                (peak1_idx != peak2_idx);

  printf("  Expected peaks: bin %d and %d\n", test_freq1, test_freq2);
  printf("  Detected peaks: bin %d (mag=%.2f) and %d (mag=%.2f)\n",
         peak1_idx, peak1_mag, peak2_idx, peak2_mag);
  printf("  Result: %s\n\n", passed ? "PASSED" : "FAILED");

  delete[] data;

  return passed;
}

// ====================================================================
// Benchmark Function
// ====================================================================

void benchmark(int fft_size, int iterations) {
  printf("Benchmark: FFT size %d, %d iterations\n", fft_size, iterations);

  complex<float> *data = new complex<float>[fft_size];
  complex<float> *backup = new complex<float>[fft_size];

  // Generate test data
  for (int i = 0; i < fft_size; i++) {
    backup[i].re = sinf(2.0f * M_PI * 10 * i / fft_size);
    backup[i].im = cosf(2.0f * M_PI * 15 * i / fft_size);
  }

  // Benchmark NEON FFT
  cfft_engine_neon<float> fft_neon(fft_size);
  clock_t start_neon = clock();
  for (int i = 0; i < iterations; i++) {
    memcpy(data, backup, fft_size * sizeof(complex<float>));
    fft_neon.inplace(data, false);
  }
  clock_t end_neon = clock();
  double time_neon = (double)(end_neon - start_neon) / CLOCKS_PER_SEC;

  // Benchmark Scalar FFT
  cfft_engine<float> fft_scalar(fft_size);
  clock_t start_scalar = clock();
  for (int i = 0; i < iterations; i++) {
    memcpy(data, backup, fft_size * sizeof(complex<float>));
    fft_scalar.inplace(data, false);
  }
  clock_t end_scalar = clock();
  double time_scalar = (double)(end_scalar - start_scalar) / CLOCKS_PER_SEC;

  // Results
  printf("  NEON FFT:   %.3f seconds (%.2f µs/FFT)\n",
         time_neon, time_neon * 1e6 / iterations);
  printf("  Scalar FFT: %.3f seconds (%.2f µs/FFT)\n",
         time_scalar, time_scalar * 1e6 / iterations);
  printf("  Speedup:    %.2fx\n", time_scalar / time_neon);
  printf("  Throughput: %.2f FFTs/second\n\n", iterations / time_neon);

  delete[] data;
  delete[] backup;
}

// ====================================================================
// Main Function
// ====================================================================

int main(int argc, char **argv) {
  int fft_size = 1024;
  int iterations = 10000;

  if (argc > 1) fft_size = atoi(argv[1]);
  if (argc > 2) iterations = atoi(argv[2]);

  printf("====================================================================\n");
  printf("NEON FFT Test and Benchmark Program\n");
  printf("====================================================================\n\n");

  // Display build configuration
  printf("Build configuration:\n");
#ifdef LEANSDR_USE_NE10
  printf("  Ne10 library: ENABLED\n");
#else
  printf("  Ne10 library: DISABLED\n");
#endif

#ifdef LEANSDR_HAS_NEON
  printf("  Custom NEON:  ENABLED\n");
#else
  printf("  Custom NEON:  DISABLED\n");
#endif

#if !defined(LEANSDR_USE_NE10) && !defined(LEANSDR_HAS_NEON)
  printf("  Scalar only:  YES (no NEON support)\n");
#endif
  printf("\n");

  // Run tests
  printf("====================================================================\n");
  printf("Correctness Tests\n");
  printf("====================================================================\n\n");

  bool test1 = test_correctness(fft_size);
  bool test2 = test_roundtrip(fft_size);
  bool test3 = test_spectral_peaks(fft_size);

  if (test1 && test2 && test3) {
    printf("All tests PASSED!\n\n");
  } else {
    printf("Some tests FAILED!\n\n");
    return 1;
  }

  // Run benchmark
  printf("====================================================================\n");
  printf("Performance Benchmark\n");
  printf("====================================================================\n\n");

  benchmark(fft_size, iterations);

  // Additional benchmarks for different sizes
  printf("====================================================================\n");
  printf("Multi-size Benchmark\n");
  printf("====================================================================\n\n");

  int sizes[] = {256, 512, 1024, 2048, 4096};
  int bench_iters = 10000;

  for (int i = 0; i < 5; i++) {
    if (sizes[i] > fft_size * 4) {
      bench_iters = 1000;  // Fewer iterations for large FFTs
    }
    benchmark(sizes[i], bench_iters);
  }

  printf("====================================================================\n");
  printf("Testing Complete\n");
  printf("====================================================================\n");

  return 0;
}
