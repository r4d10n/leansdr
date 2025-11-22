// Reed-Solomon NEON Demo - Example usage of rs_neon.h
// This file is part of LeanSDR Copyright (C) 2016-2025 <pabr@pabr.org>.
// See the toplevel README for more information.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Minimal types needed
typedef unsigned char u8;

#include "../rs_neon.h"

using namespace leansdr;

// Helper function to print byte array
void print_bytes(const char* label, const u8* data, int len, int max_display = 16) {
    printf("%s: ", label);
    for (int i = 0; i < len && i < max_display; ++i) {
        printf("%02x ", data[i]);
    }
    if (len > max_display) {
        printf("... (%d more bytes)", len - max_display);
    }
    printf("\n");
}

// Helper function to count errors
int count_differences(const u8* a, const u8* b, int len) {
    int count = 0;
    for (int i = 0; i < len; ++i) {
        if (a[i] != b[i]) count++;
    }
    return count;
}

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("Reed-Solomon NEON Demo\n");
    printf("RS(204, 188, 8) for DVB-S\n");
    printf("========================================\n\n");

#if LEANSDR_HAS_NEON
    printf("NEON optimizations: ENABLED\n");
#else
    printf("NEON optimizations: DISABLED (using scalar fallback)\n");
#endif
    printf("\n");

    // Create RS engine
    rs_engine_neon rs;

    // ========================================
    // Example 1: Basic Encoding
    // ========================================
    printf("=== Example 1: Basic Encoding ===\n");

    u8 message1[204];

    // Fill with example data (e.g., "Hello, DVB-S!")
    const char* hello = "Hello, DVB-S! This is a Reed-Solomon encoded message.";
    memset(message1, 0, 188);
    strncpy((char*)message1, hello, 187);

    printf("Original message: %s\n", message1);

    // Encode - appends 16 parity bytes to message[188..203]
    rs.encode(message1);

    print_bytes("Parity bytes", message1 + 188, 16);

    // Verify encoding
    u8 syndromes1[16];
    bool corrupted1 = rs.syndromes(message1, syndromes1);

    printf("Message after encoding: %s\n", corrupted1 ? "CORRUPTED (error!)" : "VALID");
    printf("\n");

    // ========================================
    // Example 2: Error Detection
    // ========================================
    printf("=== Example 2: Error Detection ===\n");

    u8 message2[204];
    memset(message2, 0, 188);
    strcpy((char*)message2, "Test message for error detection");
    rs.encode(message2);

    printf("Original: %s\n", message2);

    // Introduce errors
    message2[10] ^= 0xFF;  // Flip all bits in byte 10
    message2[50] ^= 0x0F;  // Flip lower 4 bits in byte 50

    printf("Corrupted: ");
    for (int i = 0; i < 60; ++i) {
        if (i == 10 || i == 50) printf("[%02x] ", message2[i]);
        else if (i < 40) printf("%02x ", message2[i]);
    }
    printf("...\n");

    // Check syndromes
    u8 syndromes2[16];
    bool corrupted2 = rs.syndromes(message2, syndromes2);

    printf("Corruption detected: %s\n", corrupted2 ? "YES" : "NO");
    print_bytes("Syndromes", syndromes2, 16);
    printf("\n");

    // ========================================
    // Example 3: Error Correction
    // ========================================
    printf("=== Example 3: Error Correction ===\n");

    u8 original[204];
    u8 corrupted[204];

    // Create test message
    memset(original, 0, 188);
    for (int i = 0; i < 188; ++i) {
        original[i] = (i * 13 + 7) % 256;  // Pseudo-random pattern
    }

    // Encode
    rs.encode(original);
    memcpy(corrupted, original, 204);

    print_bytes("Original data", original, 188, 32);

    // Introduce 4 errors (RS can correct up to 8 errors)
    int error_positions[] = {20, 60, 100, 140};
    u8 error_values[] = {0x12, 0x34, 0x56, 0x78};

    printf("\nIntroducing 4 errors at positions:");
    for (int i = 0; i < 4; ++i) {
        printf(" %d", error_positions[i]);
        corrupted[error_positions[i]] ^= error_values[i];
    }
    printf("\n");

    print_bytes("Corrupted data", corrupted, 188, 32);

    // Compute syndromes
    u8 syndromes3[16];
    rs.syndromes(corrupted, syndromes3);

    // Correct errors
    u8 corrected[188];
    memcpy(corrected, corrupted, 188);

    int bits_corrected = 0;
    bool still_corrupted = rs.correct(syndromes3, corrected, corrupted, &bits_corrected);

    print_bytes("Corrected data", corrected, 188, 32);

    // Verify correction
    int errors_remaining = count_differences(original, corrected, 188);

    printf("\nCorrection results:\n");
    printf("  Bits corrected: %d\n", bits_corrected);
    printf("  Errors remaining: %d\n", errors_remaining);
    printf("  Status: %s\n", errors_remaining == 0 ? "SUCCESS" : "FAILED");
    printf("\n");

    // ========================================
    // Example 4: Performance Test
    // ========================================
    printf("=== Example 4: Performance Test ===\n");

    const int iterations = 10000;
    u8 perf_message[204];

    // Prepare test data
    for (int i = 0; i < 188; ++i) {
        perf_message[i] = (i * 17) % 256;
    }
    rs.encode(perf_message);

    // Introduce errors
    perf_message[25] ^= 0x55;
    perf_message[75] ^= 0xAA;
    perf_message[125] ^= 0x33;
    perf_message[175] ^= 0xCC;

    // Benchmark syndrome calculation
    clock_t start = clock();
    for (int i = 0; i < iterations; ++i) {
        u8 synd[16];
        rs.syndromes(perf_message, synd);
    }
    clock_t end = clock();

    double syndrome_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

    printf("Syndrome calculation:\n");
    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.2f ms\n", syndrome_time);
    printf("  Time per iteration: %.3f us\n", syndrome_time * 1000.0 / iterations);

    // Benchmark error correction
    u8 synd[16];
    rs.syndromes(perf_message, synd);

    start = clock();
    for (int i = 0; i < iterations; ++i) {
        u8 synd_tmp[16];
        u8 corrected_tmp[188];
        memcpy(synd_tmp, synd, 16);
        memcpy(corrected_tmp, perf_message, 188);
        rs.correct(synd_tmp, corrected_tmp, NULL, NULL);
    }
    end = clock();

    double correction_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

    printf("\nError correction:\n");
    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.2f ms\n", correction_time);
    printf("  Time per iteration: %.3f us\n", correction_time * 1000.0 / iterations);

    printf("\n");

    // ========================================
    // Example 5: Maximum Error Correction
    // ========================================
    printf("=== Example 5: Maximum Error Correction (8 errors) ===\n");

    u8 max_test[204];
    u8 max_original[204];

    memset(max_original, 0, 188);
    strcpy((char*)max_original, "Testing maximum error correction capability");
    rs.encode(max_original);
    memcpy(max_test, max_original, 204);

    // Introduce 8 errors (maximum correctable)
    int max_error_pos[] = {10, 30, 50, 70, 90, 110, 130, 150};
    printf("Introducing 8 errors at positions:");
    for (int i = 0; i < 8; ++i) {
        printf(" %d", max_error_pos[i]);
        max_test[max_error_pos[i]] ^= (0x11 * (i + 1));
    }
    printf("\n");

    // Correct
    u8 max_synd[16];
    rs.syndromes(max_test, max_synd);

    u8 max_corrected[188];
    memcpy(max_corrected, max_test, 188);

    int max_bits_corrected = 0;
    rs.correct(max_synd, max_corrected, max_test, &max_bits_corrected);

    int max_errors = count_differences(max_original, max_corrected, 188);

    printf("Results:\n");
    printf("  Bits corrected: %d\n", max_bits_corrected);
    printf("  Errors remaining: %d\n", max_errors);
    printf("  Status: %s\n", max_errors == 0 ? "SUCCESS - All 8 errors corrected!" : "FAILED");

    printf("\n========================================\n");
    printf("Demo complete!\n");
    printf("========================================\n");

    return 0;
}
