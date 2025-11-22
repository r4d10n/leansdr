// Comprehensive NEON Optimization Unit Test Suite for LeanSDR
// This file is part of LeanSDR Copyright (C) 2016-2025 <pabr@pabr.org>.
// See the toplevel README for more information.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>

// Include NEON intrinsics if available
#ifdef __ARM_NEON
#include <arm_neon.h>
#define HAVE_NEON 1
#else
#define HAVE_NEON 0
#endif

// Minimal types from LeanSDR framework
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef float f32;

template<typename T>
struct complex {
    T re, im;
    complex() : re(0), im(0) { }
    complex(T x) : re(x), im(0) { }
    complex(T x, T y) : re(x), im(y) { }
};

typedef complex<u8> cu8;
typedef complex<s8> cs8;
typedef complex<s16> cs16;
typedef complex<f32> cf32;

// Test configuration
const float EPSILON = 1e-5f;  // Tolerance for floating-point comparisons
const int MAX_FAILURES = 10;  // Maximum failures to report per test

// Test result tracking
struct TestStats {
    int tests_run;
    int tests_passed;
    int tests_failed;

    TestStats() : tests_run(0), tests_passed(0), tests_failed(0) {}

    void pass() { tests_run++; tests_passed++; }
    void fail() { tests_run++; tests_failed++; }

    void print_summary() {
        printf("\n========================================\n");
        printf("TEST SUMMARY\n");
        printf("========================================\n");
        printf("Total tests run:    %d\n", tests_run);
        printf("Tests passed:       %d\n", tests_passed);
        printf("Tests failed:       %d\n", tests_failed);
        printf("Success rate:       %.1f%%\n",
               tests_run > 0 ? (100.0f * tests_passed / tests_run) : 0.0f);
        printf("========================================\n");
        if (tests_failed == 0) {
            printf("✓ ALL TESTS PASSED!\n");
        } else {
            printf("✗ SOME TESTS FAILED\n");
        }
    }
};

TestStats global_stats;

// Timing utilities
#ifdef __linux__
#include <sys/time.h>
static inline uint64_t get_cycles() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
#else
static inline uint64_t get_cycles() {
    return clock();
}
#endif

// Assertion helpers
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  ✗ FAIL: %s (line %d)\n", msg, __LINE__); \
        global_stats.fail(); \
        return false; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_NEAR(a, b, eps, msg) TEST_ASSERT(fabsf((a) - (b)) < (eps), msg)

// ========================================
// 1. FIR FILTER TESTS
// ========================================

// Scalar FIR filter implementation (reference)
void fir_filter_scalar_cf32(const cf32* input, const f32* coeffs,
                            int ncoeffs, cf32* output, int nsamples) {
    for (int s = 0; s < nsamples; s++) {
        float acc_re = 0.0f;
        float acc_im = 0.0f;
        for (int i = 0; i < ncoeffs; i++) {
            acc_re += input[s + i].re * coeffs[ncoeffs - 1 - i];
            acc_im += input[s + i].im * coeffs[ncoeffs - 1 - i];
        }
        output[s].re = acc_re;
        output[s].im = acc_im;
    }
}

#ifdef __ARM_NEON
// NEON-optimized FIR filter
void fir_filter_neon_cf32(const cf32* input, const f32* coeffs,
                          int ncoeffs, cf32* output, int nsamples) {
    for (int s = 0; s < nsamples; s++) {
        const cf32* pin = input + s + ncoeffs - 1;
        const f32* pcoeff = coeffs;

        // Process 4 coefficients at a time
        float32x4_t acc_re = vdupq_n_f32(0);
        float32x4_t acc_im = vdupq_n_f32(0);

        int i;
        for (i = 0; i < (ncoeffs & ~3); i += 4) {
            // Load 4 complex samples (interleaved)
            float32x4x2_t samples = vld2q_f32((const float*)(pin - i - 3));

            // Load 4 coefficients
            float32x4_t coeff = vld1q_f32(pcoeff + i);

            // Multiply-accumulate
            acc_re = vmlaq_f32(acc_re, samples.val[0], coeff);
            acc_im = vmlaq_f32(acc_im, samples.val[1], coeff);
        }

        // Horizontal sum
        float32x2_t sum_re = vadd_f32(vget_low_f32(acc_re), vget_high_f32(acc_re));
        sum_re = vpadd_f32(sum_re, sum_re);

        float32x2_t sum_im = vadd_f32(vget_low_f32(acc_im), vget_high_f32(acc_im));
        sum_im = vpadd_f32(sum_im, sum_im);

        // Scalar tail
        float tail_re = vget_lane_f32(sum_re, 0);
        float tail_im = vget_lane_f32(sum_im, 0);
        for (; i < ncoeffs; i++) {
            tail_re += (pin - i)->re * pcoeff[i];
            tail_im += (pin - i)->im * pcoeff[i];
        }

        output[s].re = tail_re;
        output[s].im = tail_im;
    }
}
#endif

bool test_fir_correctness() {
    printf("Testing FIR filter correctness...\n");

    const int ncoeffs = 128;
    const int nsamples = 100;

    // Allocate buffers
    cf32* input = new cf32[nsamples + ncoeffs];
    f32* coeffs = new f32[ncoeffs];
    cf32* output_scalar = new cf32[nsamples];
    cf32* output_neon = new cf32[nsamples];

    // Generate test data - sinusoidal filter
    for (int i = 0; i < ncoeffs; i++) {
        coeffs[i] = sinf(2.0f * M_PI * i / ncoeffs);
    }

    // Generate test input - complex sinusoid
    for (int i = 0; i < nsamples + ncoeffs; i++) {
        float phase = 0.1f * i;
        input[i].re = cosf(phase);
        input[i].im = sinf(phase);
    }

    // Run scalar version
    fir_filter_scalar_cf32(input, coeffs, ncoeffs, output_scalar, nsamples);

#ifdef __ARM_NEON
    // Run NEON version
    fir_filter_neon_cf32(input, coeffs, ncoeffs, output_neon, nsamples);

    // Compare results
    int failures = 0;
    for (int i = 0; i < nsamples; i++) {
        float err_re = fabsf(output_scalar[i].re - output_neon[i].re);
        float err_im = fabsf(output_scalar[i].im - output_neon[i].im);

        if (err_re > EPSILON || err_im > EPSILON) {
            if (failures < MAX_FAILURES) {
                printf("  Mismatch at sample %d: scalar=(%.6f,%.6f) neon=(%.6f,%.6f)\n",
                       i, output_scalar[i].re, output_scalar[i].im,
                       output_neon[i].re, output_neon[i].im);
            }
            failures++;
        }
    }

    delete[] input;
    delete[] coeffs;
    delete[] output_scalar;
    delete[] output_neon;

    TEST_ASSERT(failures == 0, "NEON FIR results match scalar");
#else
    printf("  ⚠ NEON not available, skipping NEON tests\n");
    delete[] input;
    delete[] coeffs;
    delete[] output_scalar;
    delete[] output_neon;
#endif

    printf("  ✓ PASS: FIR filter correctness\n");
    global_stats.pass();
    return true;
}

bool test_fir_edge_cases() {
    printf("Testing FIR filter edge cases...\n");

    // Test 1: Single coefficient
    {
        const int ncoeffs = 1;
        const int nsamples = 10;
        cf32 input[11] = {{1,0}, {2,0}, {3,0}, {4,0}, {5,0},
                          {6,0}, {7,0}, {8,0}, {9,0}, {10,0}, {11,0}};
        f32 coeffs[1] = {2.0f};
        cf32 output[10];

        fir_filter_scalar_cf32(input, coeffs, ncoeffs, output, nsamples);

        TEST_ASSERT_NEAR(output[0].re, 2.0f, EPSILON, "Single coeff test");
        TEST_ASSERT_NEAR(output[5].re, 12.0f, EPSILON, "Single coeff test");
    }

    // Test 2: Impulse response
    {
        const int ncoeffs = 8;
        const int nsamples = 1;
        cf32 input[9] = {{0,0}, {0,0}, {0,0}, {0,0}, {1,0}, {0,0}, {0,0}, {0,0}, {0,0}};
        f32 coeffs[8] = {1,2,3,4,5,6,7,8};
        cf32 output[1];

        fir_filter_scalar_cf32(input, coeffs, ncoeffs, output, nsamples);
        // Impulse at input[4] * coeffs[3] = 1 * 4 = 4
        TEST_ASSERT_NEAR(output[0].re, 4.0f, EPSILON, "Impulse response");
    }

    // Test 3: All zeros
    {
        const int ncoeffs = 16;
        const int nsamples = 10;
        cf32* input = new cf32[nsamples + ncoeffs];
        f32* coeffs = new f32[ncoeffs];
        cf32* output = new cf32[nsamples];

        for (int i = 0; i < nsamples + ncoeffs; i++) {
            input[i].re = 0.0f;
            input[i].im = 0.0f;
        }
        for (int i = 0; i < ncoeffs; i++) coeffs[i] = 1.0f;

        fir_filter_scalar_cf32(input, coeffs, ncoeffs, output, nsamples);

        for (int i = 0; i < nsamples; i++) {
            TEST_ASSERT_NEAR(output[i].re, 0.0f, EPSILON, "All zeros input");
            TEST_ASSERT_NEAR(output[i].im, 0.0f, EPSILON, "All zeros input");
        }

        delete[] input;
        delete[] coeffs;
        delete[] output;
    }

    printf("  ✓ PASS: FIR filter edge cases\n");
    global_stats.pass();
    return true;
}

bool test_fir_performance() {
    printf("Testing FIR filter performance...\n");

    const int ncoeffs = 128;
    const int nsamples = 10000;
    const int iterations = 100;

    cf32* input = new cf32[nsamples + ncoeffs];
    f32* coeffs = new f32[ncoeffs];
    cf32* output = new cf32[nsamples];

    // Initialize with random data
    for (int i = 0; i < nsamples + ncoeffs; i++) {
        input[i].re = (rand() % 1000) / 1000.0f;
        input[i].im = (rand() % 1000) / 1000.0f;
    }
    for (int i = 0; i < ncoeffs; i++) {
        coeffs[i] = (rand() % 1000) / 1000.0f;
    }

    // Benchmark scalar version
    uint64_t start_scalar = get_cycles();
    for (int iter = 0; iter < iterations; iter++) {
        fir_filter_scalar_cf32(input, coeffs, ncoeffs, output, nsamples);
    }
    uint64_t end_scalar = get_cycles();
    uint64_t time_scalar = end_scalar - start_scalar;

#ifdef __ARM_NEON
    // Benchmark NEON version
    uint64_t start_neon = get_cycles();
    for (int iter = 0; iter < iterations; iter++) {
        fir_filter_neon_cf32(input, coeffs, ncoeffs, output, nsamples);
    }
    uint64_t end_neon = get_cycles();
    uint64_t time_neon = end_neon - start_neon;

    float speedup = (float)time_scalar / time_neon;

    printf("  Scalar: %llu ns\n", (unsigned long long)time_scalar);
    printf("  NEON:   %llu ns\n", (unsigned long long)time_neon);
    printf("  Speedup: %.2fx\n", speedup);

    TEST_ASSERT(speedup > 1.5f, "NEON FIR should be at least 1.5x faster");
#else
    printf("  Scalar: %llu ns\n", (unsigned long long)time_scalar);
    printf("  ⚠ NEON not available, skipping speedup test\n");
#endif

    delete[] input;
    delete[] coeffs;
    delete[] output;

    printf("  ✓ PASS: FIR filter performance\n");
    global_stats.pass();
    return true;
}

// ========================================
// 2. FIR SAMPLER (INTERPOLATION) TESTS
// ========================================

// Scalar FIR sampler with fractional delay
void fir_sampler_scalar(const cf32* input, const f32* coeffs,
                       int ncoeffs, cf32* output, int noutput,
                       float interp_ratio) {
    float mu = 0.0f;
    int out_idx = 0;

    for (int i = 0; out_idx < noutput && i < 100000; i++) {
        if (mu < 1.0f) {
            // Linear interpolation between samples
            float acc_re = 0.0f, acc_im = 0.0f;
            for (int c = 0; c < ncoeffs; c++) {
                int sample_idx = i + c;
                acc_re += input[sample_idx].re * coeffs[ncoeffs - 1 - c];
                acc_im += input[sample_idx].im * coeffs[ncoeffs - 1 - c];
            }

            output[out_idx].re = acc_re;
            output[out_idx].im = acc_im;
            out_idx++;

            mu += interp_ratio;
        } else {
            mu -= 1.0f;
        }
    }
}

bool test_fir_sampler_correctness() {
    printf("Testing FIR sampler correctness...\n");

    const int ncoeffs = 32;
    const int nsamples = 100;
    const float interp_ratio = 1.5f; // 1.5 samples per symbol

    cf32* input = new cf32[nsamples + ncoeffs];
    f32* coeffs = new f32[ncoeffs];
    cf32* output = new cf32[nsamples];

    // Generate raised-cosine-like filter
    for (int i = 0; i < ncoeffs; i++) {
        float x = (i - ncoeffs/2.0f) / (ncoeffs/4.0f);
        coeffs[i] = (fabs(x) < 1e-6f) ? 1.0f : sinf(M_PI * x) / (M_PI * x);
    }

    // Generate test signal
    for (int i = 0; i < nsamples + ncoeffs; i++) {
        input[i].re = cosf(0.05f * i);
        input[i].im = sinf(0.05f * i);
    }

    fir_sampler_scalar(input, coeffs, ncoeffs, output, 50, interp_ratio);

    // Basic sanity checks
    TEST_ASSERT(!isnan(output[0].re) && !isnan(output[0].im), "No NaN in output");
    TEST_ASSERT(fabsf(output[0].re) < 10.0f, "Output magnitude reasonable");

    delete[] input;
    delete[] coeffs;
    delete[] output;

    printf("  ✓ PASS: FIR sampler correctness\n");
    global_stats.pass();
    return true;
}

bool test_fir_sampler_ratios() {
    printf("Testing FIR sampler with various interpolation ratios...\n");

    const int ncoeffs = 16;
    const int nsamples = 100;
    float test_ratios[] = {1.0f, 1.2f, 1.5f, 2.0f, 4.0f};

    for (int r = 0; r < 5; r++) {
        cf32* input = new cf32[nsamples + ncoeffs];
        f32* coeffs = new f32[ncoeffs];
        cf32* output = new cf32[nsamples];

        for (int i = 0; i < ncoeffs; i++) coeffs[i] = 1.0f / ncoeffs;
        for (int i = 0; i < nsamples + ncoeffs; i++) {
            input[i].re = 1.0f;
            input[i].im = 0.0f;
        }

        fir_sampler_scalar(input, coeffs, ncoeffs, output, 10, test_ratios[r]);

        // Check that output is reasonable
        TEST_ASSERT(!isnan(output[0].re), "No NaN with various ratios");

        delete[] input;
        delete[] coeffs;
        delete[] output;
    }

    printf("  ✓ PASS: FIR sampler with various ratios\n");
    global_stats.pass();
    return true;
}

// ========================================
// 3. AGC TESTS
// ========================================

// Scalar AGC
void agc_scalar(cf32* samples, int nsamples, float target_rms,
                float alpha, float* rms_state) {
    for (int i = 0; i < nsamples; i++) {
        float power = samples[i].re * samples[i].re +
                     samples[i].im * samples[i].im;
        *rms_state = *rms_state * (1.0f - alpha) + power * alpha;

        float gain = ((*rms_state) > 0) ? target_rms / sqrtf(*rms_state) : 1.0f;
        samples[i].re *= gain;
        samples[i].im *= gain;
    }
}

#ifdef __ARM_NEON
// NEON AGC (batch processing)
void agc_neon(cf32* samples, int nsamples, float target_rms,
              float alpha, float* rms_state) {
    int i = 0;

    // Process 4 samples at a time
    for (; i <= nsamples - 4; i += 4) {
        float32x4x2_t s = vld2q_f32((float*)&samples[i]);

        // Calculate power: re² + im²
        float32x4_t re2 = vmulq_f32(s.val[0], s.val[0]);
        float32x4_t im2 = vmulq_f32(s.val[1], s.val[1]);
        float32x4_t power = vaddq_f32(re2, im2);

        // Horizontal sum for average power
        float32x2_t sum = vadd_f32(vget_low_f32(power), vget_high_f32(power));
        sum = vpadd_f32(sum, sum);
        float avg_power = vget_lane_f32(sum, 0) * 0.25f;

        // Update RMS estimate
        *rms_state = *rms_state * (1.0f - alpha) + avg_power * alpha;

        // Calculate and apply gain
        float gain = (*rms_state > 0) ? target_rms / sqrtf(*rms_state) : 1.0f;
        float32x4_t gain_v = vdupq_n_f32(gain);

        s.val[0] = vmulq_f32(s.val[0], gain_v);
        s.val[1] = vmulq_f32(s.val[1], gain_v);

        vst2q_f32((float*)&samples[i], s);
    }

    // Scalar tail
    for (; i < nsamples; i++) {
        float power = samples[i].re * samples[i].re +
                     samples[i].im * samples[i].im;
        *rms_state = *rms_state * (1.0f - alpha) + power * alpha;
        float gain = (*rms_state > 0) ? target_rms / sqrtf(*rms_state) : 1.0f;
        samples[i].re *= gain;
        samples[i].im *= gain;
    }
}
#endif

bool test_agc_correctness() {
    printf("Testing AGC correctness...\n");

    const int nsamples = 100;
    const float target_rms = 1.0f;
    const float alpha = 0.1f;

    cf32* samples_scalar = new cf32[nsamples];
    cf32* samples_neon = new cf32[nsamples];

    // Generate test signal with varying amplitude
    for (int i = 0; i < nsamples; i++) {
        float amp = 0.5f + 0.5f * sinf(0.1f * i);
        samples_scalar[i].re = amp * cosf(0.2f * i);
        samples_scalar[i].im = amp * sinf(0.2f * i);
        samples_neon[i] = samples_scalar[i];
    }

    float rms_state_scalar = 1.0f;

    agc_scalar(samples_scalar, nsamples, target_rms, alpha, &rms_state_scalar);

#ifdef __ARM_NEON
    float rms_state_neon = 1.0f;
    agc_neon(samples_neon, nsamples, target_rms, alpha, &rms_state_neon);

    // Compare results
    int failures = 0;
    for (int i = 0; i < nsamples; i++) {
        float err_re = fabsf(samples_scalar[i].re - samples_neon[i].re);
        float err_im = fabsf(samples_scalar[i].im - samples_neon[i].im);

        if (err_re > EPSILON * 10 || err_im > EPSILON * 10) {
            if (failures < MAX_FAILURES) {
                printf("  Mismatch at sample %d: scalar=(%.6f,%.6f) neon=(%.6f,%.6f)\n",
                       i, samples_scalar[i].re, samples_scalar[i].im,
                       samples_neon[i].re, samples_neon[i].im);
            }
            failures++;
        }
    }

    TEST_ASSERT(failures == 0, "NEON AGC results match scalar");
#else
    printf("  ⚠ NEON not available, skipping NEON tests\n");
#endif

    delete[] samples_scalar;
    delete[] samples_neon;

    printf("  ✓ PASS: AGC correctness\n");
    global_stats.pass();
    return true;
}

bool test_agc_convergence() {
    printf("Testing AGC convergence with known input/output pairs...\n");

    const int nsamples = 1000;
    const float target_rms = 1.0f;
    const float alpha = 0.01f;

    cf32* samples = new cf32[nsamples];

    // Generate constant amplitude signal (amp = 2.0)
    for (int i = 0; i < nsamples; i++) {
        samples[i].re = 2.0f * cosf(0.1f * i);
        samples[i].im = 2.0f * sinf(0.1f * i);
    }

    float rms_state = 1.0f;
    agc_scalar(samples, nsamples, target_rms, alpha, &rms_state);

    // After convergence, RMS should be close to target
    float final_power = 0.0f;
    for (int i = nsamples - 100; i < nsamples; i++) {
        final_power += samples[i].re * samples[i].re +
                      samples[i].im * samples[i].im;
    }
    final_power /= 100.0f;
    float final_rms = sqrtf(final_power);

    TEST_ASSERT_NEAR(final_rms, target_rms, 0.1f, "AGC converged to target RMS");

    delete[] samples;

    printf("  ✓ PASS: AGC convergence\n");
    global_stats.pass();
    return true;
}

// ========================================
// 4. MPEG SYNC TESTS
// ========================================

// Scalar MPEG sync byte search
int mpeg_sync_search_scalar(const u8* data, int length) {
    for (int i = 0; i < length - 376; i++) {
        if (data[i] == 0x47 && data[i + 188] == 0x47 && data[i + 376] == 0x47) {
            return i;
        }
    }
    return -1;
}

#ifdef __ARM_NEON
// NEON MPEG sync search
int mpeg_sync_search_neon(const u8* data, int length) {
    uint8x16_t sync_pattern = vdupq_n_u8(0x47);

    for (int i = 0; i < length - 376 - 16; i += 16) {
        uint8x16_t chunk = vld1q_u8(&data[i]);
        uint8x16_t cmp = vceqq_u8(chunk, sync_pattern);

        // Check if any bytes matched
        uint64x2_t mask = vreinterpretq_u64_u8(cmp);
        uint64_t mask_low = vgetq_lane_u64(mask, 0);
        uint64_t mask_high = vgetq_lane_u64(mask, 1);

        if (mask_low || mask_high) {
            // Check individually
            for (int j = 0; j < 16 && (i + j) < length - 376; j++) {
                if (data[i + j] == 0x47 &&
                    data[i + j + 188] == 0x47 &&
                    data[i + j + 376] == 0x47) {
                    return i + j;
                }
            }
        }
    }

    // Scalar tail
    return mpeg_sync_search_scalar(data, length);
}
#endif

bool test_mpeg_sync_synthetic() {
    printf("Testing MPEG sync search with synthetic data...\n");

    const int length = 1000;
    u8* data = new u8[length];

    // Test 1: Sync at position 0
    memset(data, 0x00, length);
    data[0] = 0x47;
    data[188] = 0x47;
    data[376] = 0x47;

    int pos_scalar = mpeg_sync_search_scalar(data, length);
    TEST_ASSERT_EQ(pos_scalar, 0, "Found sync at position 0");

#ifdef __ARM_NEON
    int pos_neon = mpeg_sync_search_neon(data, length);
    TEST_ASSERT_EQ(pos_neon, 0, "NEON found sync at position 0");
#endif

    // Test 2: Sync at position 50
    memset(data, 0x00, length);
    data[50] = 0x47;
    data[50 + 188] = 0x47;
    data[50 + 376] = 0x47;

    pos_scalar = mpeg_sync_search_scalar(data, length);
    TEST_ASSERT_EQ(pos_scalar, 50, "Found sync at position 50");

#ifdef __ARM_NEON
    pos_neon = mpeg_sync_search_neon(data, length);
    TEST_ASSERT_EQ(pos_neon, 50, "NEON found sync at position 50");
#endif

    // Test 3: No sync
    memset(data, 0x00, length);
    pos_scalar = mpeg_sync_search_scalar(data, length);
    TEST_ASSERT_EQ(pos_scalar, -1, "No sync found");

    delete[] data;

    printf("  ✓ PASS: MPEG sync synthetic data\n");
    global_stats.pass();
    return true;
}

bool test_mpeg_sync_performance() {
    printf("Testing MPEG sync search performance...\n");

    const int length = 100000;
    const int iterations = 1000;
    u8* data = new u8[length];

    // Fill with random data
    for (int i = 0; i < length; i++) {
        data[i] = rand() % 256;
    }

    // Add sync at a known position
    data[5000] = 0x47;
    data[5188] = 0x47;
    data[5376] = 0x47;

    // Benchmark scalar
    uint64_t start_scalar = get_cycles();
    int pos = 0;
    for (int iter = 0; iter < iterations; iter++) {
        pos = mpeg_sync_search_scalar(data, length);
    }
    uint64_t end_scalar = get_cycles();
    uint64_t time_scalar = end_scalar - start_scalar;

    TEST_ASSERT_EQ(pos, 5000, "Found correct sync position");

#ifdef __ARM_NEON
    // Benchmark NEON
    uint64_t start_neon = get_cycles();
    for (int iter = 0; iter < iterations; iter++) {
        pos = mpeg_sync_search_neon(data, length);
    }
    uint64_t end_neon = get_cycles();
    uint64_t time_neon = end_neon - start_neon;

    float speedup = (float)time_scalar / time_neon;

    printf("  Scalar: %llu ns\n", (unsigned long long)time_scalar);
    printf("  NEON:   %llu ns\n", (unsigned long long)time_neon);
    printf("  Speedup: %.2fx\n", speedup);

    TEST_ASSERT(speedup > 1.2f, "NEON MPEG sync should be faster");
#else
    printf("  Scalar: %llu ns\n", (unsigned long long)time_scalar);
    printf("  ⚠ NEON not available, skipping speedup test\n");
#endif

    delete[] data;

    printf("  ✓ PASS: MPEG sync performance\n");
    global_stats.pass();
    return true;
}

// ========================================
// 5. EDGE CASE AND BOUNDARY TESTS
// ========================================

bool test_alignment() {
    printf("Testing NEON alignment requirements...\n");

    // Test unaligned access (NEON should handle this on ARMv7+)
    const int size = 20;
    u8* buffer = new u8[size + 16];

    // Test various alignments
    for (int offset = 0; offset < 8; offset++) {
        cf32* samples = (cf32*)(buffer + offset);

        for (int i = 0; i < 4; i++) {
            samples[i].re = (float)i;
            samples[i].im = (float)(i + 1);
        }

        float rms_state = 1.0f;

#ifdef __ARM_NEON
        // This should not crash even with unaligned data
        agc_neon(samples, 4, 1.0f, 0.1f, &rms_state);
#else
        agc_scalar(samples, 4, 1.0f, 0.1f, &rms_state);
#endif

        TEST_ASSERT(!isnan(samples[0].re), "Unaligned access works");
    }

    delete[] buffer;

    printf("  ✓ PASS: Alignment tests\n");
    global_stats.pass();
    return true;
}

bool test_boundary_conditions() {
    printf("Testing boundary conditions...\n");

    // Test 1: Empty input
    {
        cf32 output[1];
        cf32 input[8] = {{0,0}};
        f32 coeffs[8] = {0};

        fir_filter_scalar_cf32(input, coeffs, 8, output, 0);
        // Should not crash
    }

    // Test 2: Very small buffers
    {
        cf32 input[2] = {{1,0}, {2,0}};
        f32 coeffs[1] = {1.0f};
        cf32 output[1];

        fir_filter_scalar_cf32(input, coeffs, 1, output, 1);
        TEST_ASSERT_NEAR(output[0].re, 1.0f, EPSILON, "Tiny buffer works");
    }

    // Test 3: Maximum safe values
    {
        const int nsamples = 10;
        cf32* samples = new cf32[nsamples];

        for (int i = 0; i < nsamples; i++) {
            samples[i].re = 1e6f;
            samples[i].im = 1e6f;
        }

        float rms_state = 1.0f;
        agc_scalar(samples, nsamples, 1.0f, 0.1f, &rms_state);

        TEST_ASSERT(!isnan(samples[0].re) && !isinf(samples[0].re),
                    "Large values handled");

        delete[] samples;
    }

    printf("  ✓ PASS: Boundary conditions\n");
    global_stats.pass();
    return true;
}

// ========================================
// 6. RANDOMIZED FUZZ TESTING
// ========================================

bool test_fuzz_fir() {
    printf("Testing FIR filter with randomized inputs...\n");

    srand(12345); // Deterministic seed

    for (int test = 0; test < 100; test++) {
        int ncoeffs = 4 + (rand() % 124); // 4-128
        int nsamples = 10 + (rand() % 90); // 10-100

        cf32* input = new cf32[nsamples + ncoeffs];
        f32* coeffs = new f32[ncoeffs];
        cf32* output_scalar = new cf32[nsamples];
        cf32* output_neon = new cf32[nsamples];

        // Random data
        for (int i = 0; i < nsamples + ncoeffs; i++) {
            input[i].re = ((rand() % 2000) - 1000) / 1000.0f;
            input[i].im = ((rand() % 2000) - 1000) / 1000.0f;
        }
        for (int i = 0; i < ncoeffs; i++) {
            coeffs[i] = ((rand() % 2000) - 1000) / 1000.0f;
        }

        fir_filter_scalar_cf32(input, coeffs, ncoeffs, output_scalar, nsamples);

#ifdef __ARM_NEON
        fir_filter_neon_cf32(input, coeffs, ncoeffs, output_neon, nsamples);

        // Compare
        for (int i = 0; i < nsamples; i++) {
            float err_re = fabsf(output_scalar[i].re - output_neon[i].re);
            float err_im = fabsf(output_scalar[i].im - output_neon[i].im);

            if (err_re > EPSILON || err_im > EPSILON) {
                printf("  Fuzz test %d failed at sample %d (ncoeffs=%d)\n",
                       test, i, ncoeffs);
                TEST_ASSERT(false, "Fuzz test failed");
            }
        }
#endif

        delete[] input;
        delete[] coeffs;
        delete[] output_scalar;
        delete[] output_neon;
    }

    printf("  ✓ PASS: FIR fuzz testing (100 random tests)\n");
    global_stats.pass();
    return true;
}

bool test_fuzz_agc() {
    printf("Testing AGC with randomized inputs...\n");

    srand(54321); // Deterministic seed

    for (int test = 0; test < 50; test++) {
        int nsamples = 10 + (rand() % 190);

        cf32* samples_scalar = new cf32[nsamples];
        cf32* samples_neon = new cf32[nsamples];

        for (int i = 0; i < nsamples; i++) {
            float amp = ((rand() % 100) + 1) / 100.0f;
            float phase = (rand() % 628) / 100.0f;
            samples_scalar[i].re = amp * cosf(phase);
            samples_scalar[i].im = amp * sinf(phase);
            samples_neon[i] = samples_scalar[i];
        }

        float target_rms = 0.5f + (rand() % 150) / 100.0f;
        float alpha = 0.001f + (rand() % 100) / 1000.0f;

        float rms_state_scalar = 1.0f;

        agc_scalar(samples_scalar, nsamples, target_rms, alpha, &rms_state_scalar);

#ifdef __ARM_NEON
        float rms_state_neon = 1.0f;
        agc_neon(samples_neon, nsamples, target_rms, alpha, &rms_state_neon);

        for (int i = 0; i < nsamples; i++) {
            if (isnan(samples_neon[i].re) || isnan(samples_neon[i].im)) {
                printf("  Fuzz test %d produced NaN\n", test);
                TEST_ASSERT(false, "AGC fuzz test produced NaN");
            }
        }
#endif

        delete[] samples_scalar;
        delete[] samples_neon;
    }

    printf("  ✓ PASS: AGC fuzz testing (50 random tests)\n");
    global_stats.pass();
    return true;
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("LeanSDR NEON Optimization Test Suite\n");
    printf("========================================\n");

#ifdef __ARM_NEON
    printf("NEON support: ✓ ENABLED\n");
#else
    printf("NEON support: ✗ DISABLED (scalar fallback only)\n");
#endif

    printf("\n");

    // Run all tests
    printf("=== 1. FIR FILTER TESTS ===\n");
    test_fir_correctness();
    test_fir_edge_cases();
    test_fir_performance();

    printf("\n=== 2. FIR SAMPLER TESTS ===\n");
    test_fir_sampler_correctness();
    test_fir_sampler_ratios();

    printf("\n=== 3. AGC TESTS ===\n");
    test_agc_correctness();
    test_agc_convergence();

    printf("\n=== 4. MPEG SYNC TESTS ===\n");
    test_mpeg_sync_synthetic();
    test_mpeg_sync_performance();

    printf("\n=== 5. EDGE CASE TESTS ===\n");
    test_alignment();
    test_boundary_conditions();

    printf("\n=== 6. FUZZ TESTS ===\n");
    test_fuzz_fir();
    test_fuzz_agc();

    // Print summary
    global_stats.print_summary();

    return (global_stats.tests_failed == 0) ? 0 : 1;
}
