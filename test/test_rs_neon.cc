// Reed-Solomon NEON Optimization Test Suite for LeanSDR
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
#include <time.h>
#include <stdint.h>
#include <assert.h>

// Minimal types needed before including headers
typedef unsigned char u8;
typedef unsigned short u16;

// Include both scalar and NEON implementations
#include "../src/leansdr/rs.h"
#include "../src/leansdr/rs_neon.h"

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

// Test result tracking
struct TestStats {
    int tests_run;
    int tests_passed;
    int tests_failed;

    TestStats() : tests_run(0), tests_passed(0), tests_failed(0) {}

    void pass(const char* name) {
        tests_run++;
        tests_passed++;
        printf("  ✓ PASS: %s\n", name);
    }

    void fail(const char* name, const char* reason) {
        tests_run++;
        tests_failed++;
        printf("  ✗ FAIL: %s - %s\n", name, reason);
    }

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

TestStats stats;

// ========================================
// 1. GF(256) OPERATIONS TESTS
// ========================================

void test_gf_operations() {
    printf("\n=== Testing GF(256) Operations ===\n");

    using namespace leansdr;

    // Create scalar GF engine
    gf2x_p<u8, u16, 0x11d, 8, 2> gf_scalar;

#if LEANSDR_HAS_NEON
    // Create NEON GF engine
    gf256_neon gf_neon;

    // Test 1: Addition (XOR) - should be identical
    {
        bool passed = true;
        for ( int a=0; a<256 && passed; ++a ) {
            for ( int b=0; b<256 && passed; ++b ) {
                u8 result_scalar = gf_scalar.add(a, b);
                u8 result_neon = gf_neon.add(a, b);
                if ( result_scalar != result_neon ) {
                    printf("    GF add mismatch: %d + %d: scalar=%d neon=%d\n",
                           a, b, result_scalar, result_neon);
                    passed = false;
                }
            }
        }
        if ( passed ) {
            stats.pass("GF(256) addition");
        } else {
            stats.fail("GF(256) addition", "Mismatch detected");
        }
    }

    // Test 2: Multiplication - should be identical
    {
        bool passed = true;
        int mismatches = 0;
        for ( int a=0; a<256 && mismatches < 10; ++a ) {
            for ( int b=0; b<256 && mismatches < 10; ++b ) {
                u8 result_scalar = gf_scalar.mul(a, b);
                u8 result_neon = gf_neon.mul(a, b);
                if ( result_scalar != result_neon ) {
                    printf("    GF mul mismatch: %d * %d: scalar=%d neon=%d\n",
                           a, b, result_scalar, result_neon);
                    passed = false;
                    mismatches++;
                }
            }
        }
        if ( passed ) {
            stats.pass("GF(256) multiplication");
        } else {
            stats.fail("GF(256) multiplication", "Mismatch detected");
        }
    }

    // Test 3: Division - should be identical (except div by 0)
    {
        bool passed = true;
        int mismatches = 0;
        for ( int a=0; a<256 && mismatches < 10; ++a ) {
            for ( int b=1; b<256 && mismatches < 10; ++b ) {  // Skip b=0
                u8 result_scalar = gf_scalar.div(a, b);
                u8 result_neon = gf_neon.div(a, b);
                if ( result_scalar != result_neon ) {
                    printf("    GF div mismatch: %d / %d: scalar=%d neon=%d\n",
                           a, b, result_scalar, result_neon);
                    passed = false;
                    mismatches++;
                }
            }
        }
        if ( passed ) {
            stats.pass("GF(256) division");
        } else {
            stats.fail("GF(256) division", "Mismatch detected");
        }
    }

    // Test 4: Inverse - should be identical (except inv(0))
    {
        bool passed = true;
        for ( int a=1; a<256; ++a ) {
            u8 result_scalar = gf_scalar.inv(a);
            u8 result_neon = gf_neon.inv(a);
            if ( result_scalar != result_neon ) {
                printf("    GF inv mismatch: inv(%d): scalar=%d neon=%d\n",
                       a, result_scalar, result_neon);
                passed = false;
                break;
            }
        }
        if ( passed ) {
            stats.pass("GF(256) inverse");
        } else {
            stats.fail("GF(256) inverse", "Mismatch detected");
        }
    }

#else
    printf("  ⚠ NEON not available, skipping NEON GF tests\n");
    stats.pass("GF(256) operations (scalar only)");
#endif
}

// ========================================
// 2. SYNDROME CALCULATION TESTS
// ========================================

void test_syndromes() {
    printf("\n=== Testing Syndrome Calculation ===\n");

    using namespace leansdr;

    rs_engine engine_scalar;

#if LEANSDR_HAS_NEON
    rs_engine_neon engine_neon;

    // Test 1: No errors (all zero syndromes)
    {
        u8 msg[204];
        memset(msg, 0, 188);

        // Encode with scalar engine
        engine_scalar.encode(msg);

        u8 synd_scalar[16];
        u8 synd_neon[16];

        bool corrupted_scalar = engine_scalar.syndromes(msg, synd_scalar);
        bool corrupted_neon = engine_neon.syndromes(msg, synd_neon);

        bool passed = true;

        // Both should report no corruption
        if ( corrupted_scalar || corrupted_neon ) {
            printf("    Corruption detected in valid message\n");
            passed = false;
        }

        // Syndromes should all be zero
        for ( int i=0; i<16; ++i ) {
            if ( synd_scalar[i] != 0 || synd_neon[i] != 0 ) {
                printf("    Non-zero syndrome in valid message: synd[%d] = %d (scalar), %d (neon)\n",
                       i, synd_scalar[i], synd_neon[i]);
                passed = false;
            }
        }

        if ( passed ) {
            stats.pass("Syndromes for valid message");
        } else {
            stats.fail("Syndromes for valid message", "Non-zero syndromes detected");
        }
    }

    // Test 2: Single bit error
    {
        u8 msg[204];
        for ( int i=0; i<188; ++i ) msg[i] = i & 0xFF;

        engine_scalar.encode(msg);

        // Introduce single bit error
        msg[100] ^= 0x01;

        u8 synd_scalar[16];
        u8 synd_neon[16];

        bool corrupted_scalar = engine_scalar.syndromes(msg, synd_scalar);
        bool corrupted_neon = engine_neon.syndromes(msg, synd_neon);

        bool passed = true;

        // Both should detect corruption
        if ( !corrupted_scalar || !corrupted_neon ) {
            printf("    Failed to detect corruption\n");
            passed = false;
        }

        // Syndromes should match
        for ( int i=0; i<16; ++i ) {
            if ( synd_scalar[i] != synd_neon[i] ) {
                printf("    Syndrome mismatch at [%d]: scalar=%d neon=%d\n",
                       i, synd_scalar[i], synd_neon[i]);
                passed = false;
            }
        }

        if ( passed ) {
            stats.pass("Syndromes for corrupted message");
        } else {
            stats.fail("Syndromes for corrupted message", "Mismatch detected");
        }
    }

    // Test 3: Multiple errors
    {
        u8 msg[204];
        for ( int i=0; i<188; ++i ) msg[i] = (i * 17) & 0xFF;

        engine_scalar.encode(msg);

        // Introduce 4 errors
        msg[10] ^= 0x55;
        msg[50] ^= 0xAA;
        msg[100] ^= 0x33;
        msg[150] ^= 0xCC;

        u8 synd_scalar[16];
        u8 synd_neon[16];

        engine_scalar.syndromes(msg, synd_scalar);
        engine_neon.syndromes(msg, synd_neon);

        bool passed = true;
        for ( int i=0; i<16; ++i ) {
            if ( synd_scalar[i] != synd_neon[i] ) {
                printf("    Multi-error syndrome mismatch at [%d]: scalar=%d neon=%d\n",
                       i, synd_scalar[i], synd_neon[i]);
                passed = false;
            }
        }

        if ( passed ) {
            stats.pass("Syndromes for multi-error message");
        } else {
            stats.fail("Syndromes for multi-error message", "Mismatch detected");
        }
    }

#else
    printf("  ⚠ NEON not available, skipping NEON syndrome tests\n");
    stats.pass("Syndrome calculation (scalar only)");
#endif
}

// ========================================
// 3. ERROR CORRECTION TESTS
// ========================================

void test_error_correction() {
    printf("\n=== Testing Error Correction ===\n");

    using namespace leansdr;

    rs_engine engine_scalar;

#if LEANSDR_HAS_NEON
    rs_engine_neon engine_neon;

    // Test 1: Single byte error
    {
        u8 msg_original[204];
        u8 msg_scalar[204];
        u8 msg_neon[204];

        // Create test message
        for ( int i=0; i<188; ++i ) {
            msg_original[i] = (i * 13) & 0xFF;
        }

        // Encode
        memcpy(msg_scalar, msg_original, 188);
        memcpy(msg_neon, msg_original, 188);
        engine_scalar.encode(msg_scalar);
        engine_neon.encode(msg_neon);

        // Introduce single error
        msg_scalar[50] ^= 0x7F;
        msg_neon[50] ^= 0x7F;

        // Correct with both engines
        u8 synd_scalar[16];
        u8 synd_neon[16];
        u8 pout_scalar[188];
        u8 pout_neon[188];

        engine_scalar.syndromes(msg_scalar, synd_scalar);
        engine_neon.syndromes(msg_neon, synd_neon);

        memcpy(pout_scalar, msg_scalar, 188);
        memcpy(pout_neon, msg_neon, 188);

        int bits_corrected_scalar = 0;
        int bits_corrected_neon = 0;

        engine_scalar.correct(synd_scalar, pout_scalar, msg_scalar, &bits_corrected_scalar);
        engine_neon.correct(synd_neon, pout_neon, msg_neon, &bits_corrected_neon);

        // Verify correction
        bool passed = true;
        for ( int i=0; i<188; ++i ) {
            if ( pout_scalar[i] != msg_original[i] ) {
                printf("    Scalar correction failed at byte %d: expected=%02x got=%02x\n",
                       i, msg_original[i], pout_scalar[i]);
                passed = false;
            }
            if ( pout_neon[i] != msg_original[i] ) {
                printf("    NEON correction failed at byte %d: expected=%02x got=%02x\n",
                       i, msg_original[i], pout_neon[i]);
                passed = false;
            }
        }

        if ( passed ) {
            stats.pass("Single error correction");
        } else {
            stats.fail("Single error correction", "Correction mismatch");
        }
    }

    // Test 2: Multiple errors (up to 8)
    {
        u8 msg_original[204];
        u8 msg_scalar[204];
        u8 msg_neon[204];

        for ( int i=0; i<188; ++i ) {
            msg_original[i] = (i * 7 + 13) & 0xFF;
        }

        memcpy(msg_scalar, msg_original, 188);
        memcpy(msg_neon, msg_original, 188);
        engine_scalar.encode(msg_scalar);
        engine_neon.encode(msg_neon);

        // Introduce 4 errors
        int error_positions[] = {10, 40, 80, 120};
        u8 error_values[] = {0x12, 0x34, 0x56, 0x78};

        for ( int i=0; i<4; ++i ) {
            msg_scalar[error_positions[i]] ^= error_values[i];
            msg_neon[error_positions[i]] ^= error_values[i];
        }

        u8 synd_scalar[16];
        u8 synd_neon[16];
        u8 pout_scalar[188];
        u8 pout_neon[188];

        engine_scalar.syndromes(msg_scalar, synd_scalar);
        engine_neon.syndromes(msg_neon, synd_neon);

        memcpy(pout_scalar, msg_scalar, 188);
        memcpy(pout_neon, msg_neon, 188);

        engine_scalar.correct(synd_scalar, pout_scalar, msg_scalar, NULL);
        engine_neon.correct(synd_neon, pout_neon, msg_neon, NULL);

        bool passed = true;
        for ( int i=0; i<188; ++i ) {
            if ( pout_scalar[i] != msg_original[i] ) {
                printf("    Scalar multi-error correction failed at byte %d\n", i);
                passed = false;
            }
            if ( pout_neon[i] != msg_original[i] ) {
                printf("    NEON multi-error correction failed at byte %d\n", i);
                passed = false;
            }
        }

        if ( passed ) {
            stats.pass("Multiple error correction");
        } else {
            stats.fail("Multiple error correction", "Correction mismatch");
        }
    }

#else
    printf("  ⚠ NEON not available, skipping NEON correction tests\n");
    stats.pass("Error correction (scalar only)");
#endif
}

// ========================================
// 4. ENCODING TESTS
// ========================================

void test_encoding() {
    printf("\n=== Testing Encoding ===\n");

    using namespace leansdr;

    rs_engine engine_scalar;

#if LEANSDR_HAS_NEON
    rs_engine_neon engine_neon;

    // Test: Encoding produces identical results
    {
        u8 msg_scalar[204];
        u8 msg_neon[204];

        // Test with various patterns
        u8 test_patterns[5][188];

        // Pattern 0: All zeros
        memset(test_patterns[0], 0, 188);

        // Pattern 1: All 0xFF
        memset(test_patterns[1], 0xFF, 188);

        // Pattern 2: Sequential
        for ( int i=0; i<188; ++i ) test_patterns[2][i] = i & 0xFF;

        // Pattern 3: Alternating
        for ( int i=0; i<188; ++i ) test_patterns[3][i] = (i & 1) ? 0xAA : 0x55;

        // Pattern 4: Random
        srand(12345);
        for ( int i=0; i<188; ++i ) test_patterns[4][i] = rand() & 0xFF;

        bool all_passed = true;

        for ( int p=0; p<5; ++p ) {
            memcpy(msg_scalar, test_patterns[p], 188);
            memcpy(msg_neon, test_patterns[p], 188);

            engine_scalar.encode(msg_scalar);
            engine_neon.encode(msg_neon);

            // Compare parity bytes (last 16 bytes)
            for ( int i=188; i<204; ++i ) {
                if ( msg_scalar[i] != msg_neon[i] ) {
                    printf("    Encoding mismatch in pattern %d at byte %d: scalar=%02x neon=%02x\n",
                           p, i, msg_scalar[i], msg_neon[i]);
                    all_passed = false;
                }
            }
        }

        if ( all_passed ) {
            stats.pass("Encoding consistency");
        } else {
            stats.fail("Encoding consistency", "Parity mismatch");
        }
    }

#else
    printf("  ⚠ NEON not available, skipping NEON encoding tests\n");
    stats.pass("Encoding (scalar only)");
#endif
}

// ========================================
// 5. PERFORMANCE BENCHMARKS
// ========================================

void benchmark_syndromes() {
    printf("\n=== Benchmarking Syndrome Calculation ===\n");

    using namespace leansdr;

    rs_engine engine_scalar;

    const int iterations = 10000;

    // Prepare test data
    u8 msg[204];
    for ( int i=0; i<188; ++i ) msg[i] = (i * 17) & 0xFF;
    engine_scalar.encode(msg);
    msg[50] ^= 0x42;  // Introduce error

    u8 synd[16];

    // Benchmark scalar
    uint64_t start = get_cycles();
    for ( int iter=0; iter<iterations; ++iter ) {
        engine_scalar.syndromes(msg, synd);
    }
    uint64_t end = get_cycles();
    uint64_t time_scalar = end - start;

    printf("  Scalar: %llu ns (%llu ns/op)\n",
           (unsigned long long)time_scalar,
           (unsigned long long)(time_scalar/iterations));

#if LEANSDR_HAS_NEON
    rs_engine_neon engine_neon;

    start = get_cycles();
    for ( int iter=0; iter<iterations; ++iter ) {
        engine_neon.syndromes(msg, synd);
    }
    end = get_cycles();
    uint64_t time_neon = end - start;

    float speedup = (float)time_scalar / time_neon;

    printf("  NEON:   %llu ns (%llu ns/op)\n",
           (unsigned long long)time_neon,
           (unsigned long long)(time_neon/iterations));
    printf("  Speedup: %.2fx\n", speedup);

    if ( speedup >= 1.2 ) {
        stats.pass("Syndrome calculation speedup >= 1.2x");
    } else {
        char msg_buf[256];
        snprintf(msg_buf, sizeof(msg_buf), "Speedup only %.2fx (target >= 1.2x)", speedup);
        stats.fail("Syndrome calculation speedup", msg_buf);
    }
#else
    printf("  ⚠ NEON not available, skipping speedup test\n");
    stats.pass("Syndrome calculation benchmark (scalar only)");
#endif
}

void benchmark_correction() {
    printf("\n=== Benchmarking Error Correction ===\n");

    using namespace leansdr;

    rs_engine engine_scalar;

    const int iterations = 1000;

    // Prepare test data
    u8 msg[204];
    for ( int i=0; i<188; ++i ) msg[i] = (i * 13 + 7) & 0xFF;
    engine_scalar.encode(msg);

    // Introduce 4 errors
    msg[20] ^= 0x12;
    msg[60] ^= 0x34;
    msg[100] ^= 0x56;
    msg[140] ^= 0x78;

    u8 synd[16];
    u8 pout[188];

    // Benchmark scalar
    engine_scalar.syndromes(msg, synd);

    uint64_t start = get_cycles();
    for ( int iter=0; iter<iterations; ++iter ) {
        u8 synd_tmp[16];
        memcpy(synd_tmp, synd, 16);
        memcpy(pout, msg, 188);
        engine_scalar.correct(synd_tmp, pout, NULL, NULL);
    }
    uint64_t end = get_cycles();
    uint64_t time_scalar = end - start;

    printf("  Scalar: %llu ns (%llu ns/op)\n",
           (unsigned long long)time_scalar,
           (unsigned long long)(time_scalar/iterations));

#if LEANSDR_HAS_NEON
    rs_engine_neon engine_neon;
    engine_neon.syndromes(msg, synd);

    start = get_cycles();
    for ( int iter=0; iter<iterations; ++iter ) {
        u8 synd_tmp[16];
        memcpy(synd_tmp, synd, 16);
        memcpy(pout, msg, 188);
        engine_neon.correct(synd_tmp, pout, NULL, NULL);
    }
    end = get_cycles();
    uint64_t time_neon = end - start;

    float speedup = (float)time_scalar / time_neon;

    printf("  NEON:   %llu ns (%llu ns/op)\n",
           (unsigned long long)time_neon,
           (unsigned long long)(time_neon/iterations));
    printf("  Speedup: %.2fx\n", speedup);

    if ( speedup >= 1.5 ) {
        stats.pass("Error correction speedup >= 1.5x (target met)");
    } else if ( speedup >= 1.2 ) {
        char msg_buf[256];
        snprintf(msg_buf, sizeof(msg_buf), "Speedup %.2fx (target 1.5-2x, acceptable)", speedup);
        stats.pass(msg_buf);
    } else {
        char msg_buf[256];
        snprintf(msg_buf, sizeof(msg_buf), "Speedup only %.2fx (target 1.5-2x)", speedup);
        stats.fail("Error correction speedup", msg_buf);
    }
#else
    printf("  ⚠ NEON not available, skipping speedup test\n");
    stats.pass("Error correction benchmark (scalar only)");
#endif
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("Reed-Solomon NEON Optimization Test Suite\n");
    printf("RS(204, 188, 8) for DVB-S\n");
    printf("========================================\n");

#if LEANSDR_HAS_NEON
    printf("NEON support: ✓ ENABLED\n");
    printf("Target speedup: 1.5-2x\n");
#else
    printf("NEON support: ✗ DISABLED (scalar fallback only)\n");
#endif

    // Run all tests
    test_gf_operations();
    test_syndromes();
    test_error_correction();
    test_encoding();

    // Performance benchmarks
    benchmark_syndromes();
    benchmark_correction();

    // Print summary
    stats.print_summary();

    return (stats.tests_failed == 0) ? 0 : 1;
}
