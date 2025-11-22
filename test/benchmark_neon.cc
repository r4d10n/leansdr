// Comprehensive NEON Benchmarking Suite for LeanSDR
//
// This benchmark suite measures performance of NEON SIMD optimizations
// compared to scalar implementations across various DSP operations.
//
// Build:
//   Scalar:  g++ -O3 -I../src benchmark_neon.cc -o benchmark_neon_scalar
//   NEON:    g++ -O3 -I../src -march=armv7-a -mfpu=neon -ftree-vectorize benchmark_neon.cc -o benchmark_neon_neon
//
// Run:
//   ./benchmark_neon_scalar > results_scalar.csv
//   ./benchmark_neon_neon > results_neon.csv

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#define NEON_AVAILABLE 1
#else
#define NEON_AVAILABLE 0
#endif

// ============================================================================
// TIMING UTILITIES
// ============================================================================

// High-resolution timer
static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Estimate CPU cycles (approximate - assumes constant frequency)
static inline uint64_t get_cycles() {
    // On ARM, use generic timer if available, otherwise use wall time
#ifdef __aarch64__
    uint64_t cycles;
    asm volatile("mrs %0, cntvct_el0" : "=r" (cycles));
    return cycles;
#else
    // Fallback: estimate based on time (assumes 1.5 GHz for ARMv7)
    return get_time_ns() * 3 / 2;  // Rough estimate
#endif
}

// Prevent compiler optimization
static volatile int sink = 0;
template<typename T>
static void do_not_optimize(T* ptr) {
    sink += *ptr != 0;
}

// ============================================================================
// DATA GENERATION
// ============================================================================

// Generate realistic complex signal (not all zeros)
template<typename T>
void generate_complex_signal(T* data, int n, float freq = 0.1f, float phase = 0.0f) {
    for (int i = 0; i < n; ++i) {
        float t = i * freq + phase;
        data[i*2 + 0] = cosf(t) * 0.7f + cosf(t*3.1f) * 0.3f;  // Real
        data[i*2 + 1] = sinf(t) * 0.7f + sinf(t*3.1f) * 0.3f;  // Imag
    }
}

// Generate realistic filter coefficients (raised cosine)
void generate_filter_coeffs(float* coeffs, int ncoeffs, float rolloff = 0.35f) {
    int center = ncoeffs / 2;
    for (int i = 0; i < ncoeffs; ++i) {
        float t = (i - center) / 4.0f;
        if (fabs(t) < 1e-6f) {
            coeffs[i] = 1.0f;
        } else {
            float pit = M_PI * t;
            coeffs[i] = sinf(pit) / pit * cosf(pit * rolloff) / (1.0f - 4.0f * rolloff * rolloff * t * t);
        }
    }
    // Normalize
    float sum = 0.0f;
    for (int i = 0; i < ncoeffs; ++i) sum += coeffs[i];
    for (int i = 0; i < ncoeffs; ++i) coeffs[i] /= sum;
}

// Generate random MPEG-like data
void generate_mpeg_data(uint8_t* data, int n) {
    for (int i = 0; i < n; ++i) {
        data[i] = rand() & 0xFF;
        // Insert sync bytes every 188 bytes
        if (i % 188 == 0) data[i] = 0x47;
    }
}

// ============================================================================
// BENCHMARK 1: FIR FILTER (Complex Convolution)
// ============================================================================

// Scalar implementation
void fir_filter_scalar(const float* input, const float* coeffs, float* output,
                       int nsamples, int ncoeffs) {
    for (int n = 0; n < nsamples; ++n) {
        float acc_re = 0.0f;
        float acc_im = 0.0f;
        for (int i = 0; i < ncoeffs; ++i) {
            float in_re = input[(n + i) * 2 + 0];
            float in_im = input[(n + i) * 2 + 1];
            float c = coeffs[i];
            acc_re += in_re * c;
            acc_im += in_im * c;
        }
        output[n * 2 + 0] = acc_re;
        output[n * 2 + 1] = acc_im;
    }
}

// NEON implementation
#ifdef __ARM_NEON
void fir_filter_neon(const float* input, const float* coeffs, float* output,
                     int nsamples, int ncoeffs) {
    for (int n = 0; n < nsamples; ++n) {
        float32x4_t acc_re = vdupq_n_f32(0.0f);
        float32x4_t acc_im = vdupq_n_f32(0.0f);

        // Process 4 coefficients at a time
        int i;
        for (i = 0; i + 4 <= ncoeffs; i += 4) {
            // Load 4 complex samples (interleaved re,im,re,im,re,im,re,im)
            float32x4x2_t samples = vld2q_f32(&input[(n + i) * 2]);
            // samples.val[0] = {re0, re1, re2, re3}
            // samples.val[1] = {im0, im1, im2, im3}

            // Load 4 coefficients
            float32x4_t c = vld1q_f32(&coeffs[i]);

            // Multiply-accumulate
            acc_re = vmlaq_f32(acc_re, samples.val[0], c);
            acc_im = vmlaq_f32(acc_im, samples.val[1], c);
        }

        // Horizontal sum
        float32x2_t sum_re = vadd_f32(vget_low_f32(acc_re), vget_high_f32(acc_re));
        sum_re = vpadd_f32(sum_re, sum_re);
        float32x2_t sum_im = vadd_f32(vget_low_f32(acc_im), vget_high_f32(acc_im));
        sum_im = vpadd_f32(sum_im, sum_im);

        float tail_re = vget_lane_f32(sum_re, 0);
        float tail_im = vget_lane_f32(sum_im, 0);

        // Scalar tail for remaining coefficients
        for (; i < ncoeffs; ++i) {
            tail_re += input[(n + i) * 2 + 0] * coeffs[i];
            tail_im += input[(n + i) * 2 + 1] * coeffs[i];
        }

        output[n * 2 + 0] = tail_re;
        output[n * 2 + 1] = tail_im;
    }
}
#endif

void benchmark_fir_filter(int ncoeffs, int nsamples) {
    const int warmup = 100;
    const int iterations = 1000;

    // Allocate and initialize data
    float* input = new float[(nsamples + ncoeffs) * 2];
    float* coeffs = new float[ncoeffs];
    float* output = new float[nsamples * 2];

    generate_complex_signal(input, nsamples + ncoeffs, 0.123f);
    generate_filter_coeffs(coeffs, ncoeffs);

    // Warm-up (stabilize cache, CPU frequency)
    for (int i = 0; i < warmup; ++i) {
        fir_filter_scalar(input, coeffs, output, nsamples, ncoeffs);
    }

    // Benchmark scalar
    uint64_t start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        fir_filter_scalar(input, coeffs, output, nsamples, ncoeffs);
    }
    uint64_t end = get_cycles();
    double scalar_cycles = (double)(end - start) / iterations;
    do_not_optimize(output);

    double neon_cycles = 0.0;
    double speedup = 1.0;

#ifdef __ARM_NEON
    // Warm-up NEON
    for (int i = 0; i < warmup; ++i) {
        fir_filter_neon(input, coeffs, output, nsamples, ncoeffs);
    }

    // Benchmark NEON
    start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        fir_filter_neon(input, coeffs, output, nsamples, ncoeffs);
    }
    end = get_cycles();
    neon_cycles = (double)(end - start) / iterations;
    do_not_optimize(output);

    speedup = scalar_cycles / neon_cycles;
#endif

    printf("fir_filter,%d,%d,%.0f,%.0f,%.2f\n",
           ncoeffs, nsamples, scalar_cycles, neon_cycles, speedup);

    delete[] input;
    delete[] coeffs;
    delete[] output;
}

// ============================================================================
// BENCHMARK 2: AGC POWER MEASUREMENT
// ============================================================================

// Scalar power measurement
void agc_power_scalar(const float* input, float* power, int nsamples) {
    for (int i = 0; i < nsamples; ++i) {
        float re = input[i * 2 + 0];
        float im = input[i * 2 + 1];
        power[i] = re * re + im * im;
    }
}

// NEON power measurement
#ifdef __ARM_NEON
void agc_power_neon(const float* input, float* power, int nsamples) {
    int i;
    for (i = 0; i + 4 <= nsamples; i += 4) {
        // Load 4 complex samples
        float32x4x2_t samples = vld2q_f32(&input[i * 2]);

        // Calculate re^2 + im^2
        float32x4_t re2 = vmulq_f32(samples.val[0], samples.val[0]);
        float32x4_t im2 = vmulq_f32(samples.val[1], samples.val[1]);
        float32x4_t pwr = vaddq_f32(re2, im2);

        // Store power
        vst1q_f32(&power[i], pwr);
    }

    // Scalar tail
    for (; i < nsamples; ++i) {
        float re = input[i * 2 + 0];
        float im = input[i * 2 + 1];
        power[i] = re * re + im * im;
    }
}
#endif

void benchmark_agc_power(int nsamples) {
    const int warmup = 100;
    const int iterations = 10000;

    float* input = new float[nsamples * 2];
    float* power = new float[nsamples];

    generate_complex_signal(input, nsamples, 0.234f);

    // Warm-up
    for (int i = 0; i < warmup; ++i) {
        agc_power_scalar(input, power, nsamples);
    }

    // Benchmark scalar
    uint64_t start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        agc_power_scalar(input, power, nsamples);
    }
    uint64_t end = get_cycles();
    double scalar_cycles = (double)(end - start) / iterations;
    do_not_optimize(power);

    double neon_cycles = 0.0;
    double speedup = 1.0;

#ifdef __ARM_NEON
    for (int i = 0; i < warmup; ++i) {
        agc_power_neon(input, power, nsamples);
    }

    start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        agc_power_neon(input, power, nsamples);
    }
    end = get_cycles();
    neon_cycles = (double)(end - start) / iterations;
    do_not_optimize(power);

    speedup = scalar_cycles / neon_cycles;
#endif

    printf("agc_power,0,%d,%.0f,%.0f,%.2f\n",
           nsamples, scalar_cycles, neon_cycles, speedup);

    delete[] input;
    delete[] power;
}

// ============================================================================
// BENCHMARK 3: COMPLEX MULTIPLICATION
// ============================================================================

// Scalar complex multiply
void complex_multiply_scalar(const float* a, const float* b, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        float a_re = a[i * 2 + 0];
        float a_im = a[i * 2 + 1];
        float b_re = b[i * 2 + 0];
        float b_im = b[i * 2 + 1];
        out[i * 2 + 0] = a_re * b_re - a_im * b_im;
        out[i * 2 + 1] = a_re * b_im + a_im * b_re;
    }
}

// NEON complex multiply
#ifdef __ARM_NEON
void complex_multiply_neon(const float* a, const float* b, float* out, int n) {
    int i;
    for (i = 0; i + 4 <= n; i += 4) {
        float32x4x2_t va = vld2q_f32(&a[i * 2]);
        float32x4x2_t vb = vld2q_f32(&b[i * 2]);

        // out.re = a.re * b.re - a.im * b.im
        float32x4_t re = vmulq_f32(va.val[0], vb.val[0]);
        re = vmlsq_f32(re, va.val[1], vb.val[1]);  // re -= a.im * b.im

        // out.im = a.re * b.im + a.im * b.re
        float32x4_t im = vmulq_f32(va.val[0], vb.val[1]);
        im = vmlaq_f32(im, va.val[1], vb.val[0]);  // im += a.im * b.re

        float32x4x2_t result;
        result.val[0] = re;
        result.val[1] = im;
        vst2q_f32(&out[i * 2], result);
    }

    // Scalar tail
    for (; i < n; ++i) {
        float a_re = a[i * 2 + 0];
        float a_im = a[i * 2 + 1];
        float b_re = b[i * 2 + 0];
        float b_im = b[i * 2 + 1];
        out[i * 2 + 0] = a_re * b_re - a_im * b_im;
        out[i * 2 + 1] = a_re * b_im + a_im * b_re;
    }
}
#endif

void benchmark_complex_multiply(int nsamples) {
    const int warmup = 100;
    const int iterations = 10000;

    float* a = new float[nsamples * 2];
    float* b = new float[nsamples * 2];
    float* out = new float[nsamples * 2];

    generate_complex_signal(a, nsamples, 0.111f);
    generate_complex_signal(b, nsamples, 0.222f);

    for (int i = 0; i < warmup; ++i) {
        complex_multiply_scalar(a, b, out, nsamples);
    }

    uint64_t start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        complex_multiply_scalar(a, b, out, nsamples);
    }
    uint64_t end = get_cycles();
    double scalar_cycles = (double)(end - start) / iterations;
    do_not_optimize(out);

    double neon_cycles = 0.0;
    double speedup = 1.0;

#ifdef __ARM_NEON
    for (int i = 0; i < warmup; ++i) {
        complex_multiply_neon(a, b, out, nsamples);
    }

    start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        complex_multiply_neon(a, b, out, nsamples);
    }
    end = get_cycles();
    neon_cycles = (double)(end - start) / iterations;
    do_not_optimize(out);

    speedup = scalar_cycles / neon_cycles;
#endif

    printf("complex_multiply,0,%d,%.0f,%.0f,%.2f\n",
           nsamples, scalar_cycles, neon_cycles, speedup);

    delete[] a;
    delete[] b;
    delete[] out;
}

// ============================================================================
// BENCHMARK 4: MPEG SYNC SEARCH
// ============================================================================

// Scalar MPEG sync search
int mpeg_sync_scalar(const uint8_t* data, int length) {
    for (int i = 0; i < length - 376; ++i) {
        if (data[i] == 0x47 && data[i + 188] == 0x47 && data[i + 376] == 0x47) {
            return i;
        }
    }
    return -1;
}

// NEON MPEG sync search
#ifdef __ARM_NEON
int mpeg_sync_neon(const uint8_t* data, int length) {
    uint8x16_t sync = vdupq_n_u8(0x47);

    for (int i = 0; i < length - 376 - 16; i += 16) {
        uint8x16_t chunk = vld1q_u8(&data[i]);
        uint8x16_t cmp = vceqq_u8(chunk, sync);

        // Check if any bytes matched
        uint64x2_t mask = vreinterpretq_u64_u8(cmp);
        if (vgetq_lane_u64(mask, 0) | vgetq_lane_u64(mask, 1)) {
            // Scan this block with scalar code
            for (int j = 0; j < 16 && i + j < length - 376; ++j) {
                if (data[i + j] == 0x47 &&
                    data[i + j + 188] == 0x47 &&
                    data[i + j + 376] == 0x47) {
                    return i + j;
                }
            }
        }
    }

    // Scalar tail
    for (int i = ((length - 376) & ~15); i < length - 376; ++i) {
        if (data[i] == 0x47 && data[i + 188] == 0x47 && data[i + 376] == 0x47) {
            return i;
        }
    }

    return -1;
}
#endif

void benchmark_mpeg_sync(int length) {
    const int warmup = 10;
    const int iterations = 1000;

    uint8_t* data = new uint8_t[length];
    generate_mpeg_data(data, length);

    // Ensure a sync pattern exists at a known location
    int sync_pos = length / 2;
    data[sync_pos] = 0x47;
    data[sync_pos + 188] = 0x47;
    data[sync_pos + 376] = 0x47;

    for (int i = 0; i < warmup; ++i) {
        volatile int pos = mpeg_sync_scalar(data, length);
        (void)pos;
    }

    uint64_t start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        volatile int pos = mpeg_sync_scalar(data, length);
        (void)pos;
    }
    uint64_t end = get_cycles();
    double scalar_cycles = (double)(end - start) / iterations;

    double neon_cycles = 0.0;
    double speedup = 1.0;

#ifdef __ARM_NEON
    for (int i = 0; i < warmup; ++i) {
        volatile int pos = mpeg_sync_neon(data, length);
        (void)pos;
    }

    start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        volatile int pos = mpeg_sync_neon(data, length);
        (void)pos;
    }
    end = get_cycles();
    neon_cycles = (double)(end - start) / iterations;

    speedup = scalar_cycles / neon_cycles;
#endif

    printf("mpeg_sync,0,%d,%.0f,%.0f,%.2f\n",
           length, scalar_cycles, neon_cycles, speedup);

    delete[] data;
}

// ============================================================================
// BENCHMARK 5: HORIZONTAL SUM (Common reduction operation)
// ============================================================================

// Scalar horizontal sum
float horizontal_sum_scalar(const float* data, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

// NEON horizontal sum
#ifdef __ARM_NEON
float horizontal_sum_neon(const float* data, int n) {
    float32x4_t vsum = vdupq_n_f32(0.0f);

    int i;
    for (i = 0; i + 4 <= n; i += 4) {
        float32x4_t v = vld1q_f32(&data[i]);
        vsum = vaddq_f32(vsum, v);
    }

    // Horizontal reduction
    float32x2_t sum2 = vadd_f32(vget_low_f32(vsum), vget_high_f32(vsum));
    sum2 = vpadd_f32(sum2, sum2);
    float sum = vget_lane_f32(sum2, 0);

    // Scalar tail
    for (; i < n; ++i) {
        sum += data[i];
    }

    return sum;
}
#endif

void benchmark_horizontal_sum(int nsamples) {
    const int warmup = 100;
    const int iterations = 10000;

    float* data = new float[nsamples];
    for (int i = 0; i < nsamples; ++i) {
        data[i] = sinf(i * 0.1f);
    }

    for (int i = 0; i < warmup; ++i) {
        volatile float s = horizontal_sum_scalar(data, nsamples);
        (void)s;
    }

    uint64_t start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        volatile float s = horizontal_sum_scalar(data, nsamples);
        (void)s;
    }
    uint64_t end = get_cycles();
    double scalar_cycles = (double)(end - start) / iterations;

    double neon_cycles = 0.0;
    double speedup = 1.0;

#ifdef __ARM_NEON
    for (int i = 0; i < warmup; ++i) {
        volatile float s = horizontal_sum_neon(data, nsamples);
        (void)s;
    }

    start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        volatile float s = horizontal_sum_neon(data, nsamples);
        (void)s;
    }
    end = get_cycles();
    neon_cycles = (double)(end - start) / iterations;

    speedup = scalar_cycles / neon_cycles;
#endif

    printf("horizontal_sum,0,%d,%.0f,%.0f,%.2f\n",
           nsamples, scalar_cycles, neon_cycles, speedup);

    delete[] data;
}

// ============================================================================
// BENCHMARK 6: DOT PRODUCT (Real-valued)
// ============================================================================

// Scalar dot product
float dot_product_scalar(const float* a, const float* b, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// NEON dot product
#ifdef __ARM_NEON
float dot_product_neon(const float* a, const float* b, int n) {
    float32x4_t vsum = vdupq_n_f32(0.0f);

    int i;
    for (i = 0; i + 4 <= n; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        vsum = vmlaq_f32(vsum, va, vb);
    }

    // Horizontal reduction
    float32x2_t sum2 = vadd_f32(vget_low_f32(vsum), vget_high_f32(vsum));
    sum2 = vpadd_f32(sum2, sum2);
    float sum = vget_lane_f32(sum2, 0);

    // Scalar tail
    for (; i < n; ++i) {
        sum += a[i] * b[i];
    }

    return sum;
}
#endif

void benchmark_dot_product(int nsamples) {
    const int warmup = 100;
    const int iterations = 10000;

    float* a = new float[nsamples];
    float* b = new float[nsamples];

    for (int i = 0; i < nsamples; ++i) {
        a[i] = sinf(i * 0.1f);
        b[i] = cosf(i * 0.1f);
    }

    for (int i = 0; i < warmup; ++i) {
        volatile float s = dot_product_scalar(a, b, nsamples);
        (void)s;
    }

    uint64_t start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        volatile float s = dot_product_scalar(a, b, nsamples);
        (void)s;
    }
    uint64_t end = get_cycles();
    double scalar_cycles = (double)(end - start) / iterations;

    double neon_cycles = 0.0;
    double speedup = 1.0;

#ifdef __ARM_NEON
    for (int i = 0; i < warmup; ++i) {
        volatile float s = dot_product_neon(a, b, nsamples);
        (void)s;
    }

    start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        volatile float s = dot_product_neon(a, b, nsamples);
        (void)s;
    }
    end = get_cycles();
    neon_cycles = (double)(end - start) / iterations;

    speedup = scalar_cycles / neon_cycles;
#endif

    printf("dot_product,0,%d,%.0f,%.0f,%.2f\n",
           nsamples, scalar_cycles, neon_cycles, speedup);

    delete[] a;
    delete[] b;
}

// ============================================================================
// BENCHMARK 7: INTEGER ADD-COMPARE-SELECT (Viterbi-style)
// ============================================================================

// Scalar ACS
void viterbi_acs_scalar(const int32_t* prev_costs, const int32_t* branch_costs,
                        int32_t* new_costs, uint8_t* decisions, int nstates) {
    for (int s = 0; s < nstates; ++s) {
        int32_t cost0 = prev_costs[s * 2 + 0] + branch_costs[s * 2 + 0];
        int32_t cost1 = prev_costs[s * 2 + 1] + branch_costs[s * 2 + 1];
        if (cost0 <= cost1) {
            new_costs[s] = cost0;
            decisions[s] = 0;
        } else {
            new_costs[s] = cost1;
            decisions[s] = 1;
        }
    }
}

// NEON ACS
#ifdef __ARM_NEON
void viterbi_acs_neon(const int32_t* prev_costs, const int32_t* branch_costs,
                      int32_t* new_costs, uint8_t* decisions, int nstates) {
    int s;
    for (s = 0; s + 4 <= nstates; s += 4) {
        // Load previous costs and branch costs
        int32x4x2_t prev = vld2q_s32(&prev_costs[s * 2]);
        int32x4x2_t branch = vld2q_s32(&branch_costs[s * 2]);

        // Add
        int32x4_t cost0 = vaddq_s32(prev.val[0], branch.val[0]);
        int32x4_t cost1 = vaddq_s32(prev.val[1], branch.val[1]);

        // Compare and select
        uint32x4_t cmp = vcleq_s32(cost0, cost1);  // cost0 <= cost1
        int32x4_t min_cost = vbslq_s32(cmp, cost0, cost1);

        // Store costs
        vst1q_s32(&new_costs[s], min_cost);

        // Store decisions (simplified - just store comparison result)
        // In real Viterbi, we'd pack these bits more efficiently
        uint8x8_t dec = vmovn_u16(vcombine_u16(vmovn_u32(cmp), vmovn_u32(cmp)));
        decisions[s + 0] = vget_lane_u8(dec, 0) ? 0 : 1;
        decisions[s + 1] = vget_lane_u8(dec, 1) ? 0 : 1;
        decisions[s + 2] = vget_lane_u8(dec, 2) ? 0 : 1;
        decisions[s + 3] = vget_lane_u8(dec, 3) ? 0 : 1;
    }

    // Scalar tail
    for (; s < nstates; ++s) {
        int32_t cost0 = prev_costs[s * 2 + 0] + branch_costs[s * 2 + 0];
        int32_t cost1 = prev_costs[s * 2 + 1] + branch_costs[s * 2 + 1];
        if (cost0 <= cost1) {
            new_costs[s] = cost0;
            decisions[s] = 0;
        } else {
            new_costs[s] = cost1;
            decisions[s] = 1;
        }
    }
}
#endif

void benchmark_viterbi_acs(int nstates) {
    const int warmup = 100;
    const int iterations = 10000;

    int32_t* prev_costs = new int32_t[nstates * 2];
    int32_t* branch_costs = new int32_t[nstates * 2];
    int32_t* new_costs = new int32_t[nstates];
    uint8_t* decisions = new uint8_t[nstates];

    // Initialize with realistic values
    for (int i = 0; i < nstates * 2; ++i) {
        prev_costs[i] = rand() % 1000;
        branch_costs[i] = rand() % 100;
    }

    for (int i = 0; i < warmup; ++i) {
        viterbi_acs_scalar(prev_costs, branch_costs, new_costs, decisions, nstates);
    }

    uint64_t start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        viterbi_acs_scalar(prev_costs, branch_costs, new_costs, decisions, nstates);
    }
    uint64_t end = get_cycles();
    double scalar_cycles = (double)(end - start) / iterations;
    do_not_optimize(new_costs);

    double neon_cycles = 0.0;
    double speedup = 1.0;

#ifdef __ARM_NEON
    for (int i = 0; i < warmup; ++i) {
        viterbi_acs_neon(prev_costs, branch_costs, new_costs, decisions, nstates);
    }

    start = get_cycles();
    for (int i = 0; i < iterations; ++i) {
        viterbi_acs_neon(prev_costs, branch_costs, new_costs, decisions, nstates);
    }
    end = get_cycles();
    neon_cycles = (double)(end - start) / iterations;
    do_not_optimize(new_costs);

    speedup = scalar_cycles / neon_cycles;
#endif

    printf("viterbi_acs,%d,0,%.0f,%.0f,%.2f\n",
           nstates, scalar_cycles, neon_cycles, speedup);

    delete[] prev_costs;
    delete[] branch_costs;
    delete[] new_costs;
    delete[] decisions;
}

// ============================================================================
// MAIN - Run all benchmarks
// ============================================================================

int main(int argc, char** argv) {
    // Print header
    printf("# LeanSDR NEON Benchmark Results\n");
    printf("# Architecture: %s\n",
#ifdef __aarch64__
        "AArch64"
#elif defined(__ARM_ARCH_7A__)
        "ARMv7-A"
#elif defined(__arm__)
        "ARM"
#else
        "x86/x64"
#endif
    );
    printf("# NEON Available: %s\n", NEON_AVAILABLE ? "Yes" : "No");
    printf("#\n");
    printf("# Format: operation,param1,param2,scalar_cycles,neon_cycles,speedup\n");
    printf("operation,param1,param2,scalar_cycles,neon_cycles,speedup\n");

    // Seed random number generator
    srand(12345);

    // Small data sizes (L1 cache friendly)
    fprintf(stderr, "Running small size benchmarks (L1 cache)...\n");
    benchmark_fir_filter(64, 128);     // 64-tap filter, 128 samples
    benchmark_fir_filter(128, 256);    // 128-tap filter
    benchmark_agc_power(256);
    benchmark_complex_multiply(256);
    benchmark_horizontal_sum(256);
    benchmark_dot_product(256);

    // Medium data sizes (L2 cache)
    fprintf(stderr, "Running medium size benchmarks (L2 cache)...\n");
    benchmark_fir_filter(128, 1024);   // Common for DVB-S
    benchmark_fir_filter(256, 1024);   // Large filter
    benchmark_agc_power(2048);
    benchmark_complex_multiply(2048);
    benchmark_mpeg_sync(4096);
    benchmark_horizontal_sum(2048);
    benchmark_dot_product(2048);
    benchmark_viterbi_acs(64);         // DVB-S Viterbi has 64 states

    // Large data sizes (main memory)
    fprintf(stderr, "Running large size benchmarks (main memory)...\n");
    benchmark_fir_filter(256, 4096);
    benchmark_agc_power(8192);
    benchmark_complex_multiply(8192);
    benchmark_mpeg_sync(16384);
    benchmark_horizontal_sum(8192);
    benchmark_dot_product(8192);
    benchmark_viterbi_acs(256);        // Larger state space

    // Very large (streaming)
    fprintf(stderr, "Running streaming benchmarks...\n");
    benchmark_fir_filter(128, 16384);  // Typical streaming scenario
    benchmark_agc_power(32768);
    benchmark_complex_multiply(32768);

    fprintf(stderr, "Benchmarks complete!\n");
    fprintf(stderr, "\nTo analyze results:\n");
    fprintf(stderr, "  1. Compare scalar_cycles vs neon_cycles\n");
    fprintf(stderr, "  2. Check speedup column (>1.0 = NEON is faster)\n");
    fprintf(stderr, "  3. Plot results with gnuplot or spreadsheet\n");

    return 0;
}
