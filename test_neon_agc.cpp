// Test compilation of NEON-optimized AGC implementation
// Compile with: g++ -c -std=c++11 -I src test_neon_agc.cpp

#include "leansdr/framework.h"
#include "leansdr/sdr.h"

#ifdef __ARM_NEON
#include "leansdr/sdr_neon.h"
#define IMPLEMENTATION "NEON"
#else
#define IMPLEMENTATION "SCALAR"
#endif

#include <stdio.h>

int main() {
    printf("AGC Implementation: %s\n", IMPLEMENTATION);

    // This test just verifies the code compiles correctly
    // Actual functional testing requires ARM hardware with NEON

#ifdef __ARM_NEON
    printf("NEON intrinsics available: YES\n");
    printf("Template specialization: simple_agc<float>\n");
    printf("Expected speedup: 3-4x\n");
    printf("Expected CPU reduction: 3-5%%\n");
#else
    printf("NEON intrinsics available: NO\n");
    printf("Using scalar implementation\n");
#endif

    return 0;
}
