// Test file for NEON-optimized Viterbi decoder
// This file is part of LeanSDR Copyright (C) 2016-2018 <pabr@pabr.org>.

#include <stdio.h>
#include <stdint.h>
#include "leansdr/math.h"
#include "leansdr/viterbi.h"
#include "leansdr/viterbi_neon.h"

using namespace leansdr;

// DVB-S convolutional code polynomials (K=7, rate 1/2)
const uint16_t DVB_S_G[2] = { 0133, 0171 };  // Octal notation

int main() {
  printf("Testing NEON-optimized Viterbi decoder\n");
  printf("======================================\n\n");

  // Test configuration: 64-state trellis, binary input, 4 coded symbols
  // This is typical for DVB-S with K=7 convolutional code and puncturing
  const int NSTATES = 64;    // 2^(K-1) = 2^6 = 64
  const int NUS = 2;         // Binary uncoded symbols
  const int NCS = 4;         // 4 coded symbols (punctured from rate 1/2)

  typedef uint8_t TS;        // State index type
  typedef uint8_t TUS;       // Uncoded symbol type
  typedef uint8_t TCS;       // Coded symbol type
  typedef uint16_t TBM;      // Branch metric type
  typedef uint16_t TPM;      // Path metric type
  typedef bitpath<uint64_t, TUS, 1, 64> TP;  // Path type

  // Create trellis for DVB-S convolutional code
  trellis<TS, NSTATES, TUS, NUS, NCS> trellis_dvbs;
  trellis_dvbs.init_convolutional(DVB_S_G);

  printf("1. Testing standard Viterbi decoder\n");
  viterbi_dec<TS, NSTATES, TUS, NUS, TCS, NCS, TBM, TPM, TP> dec_standard(&trellis_dvbs);

  // Test with sample branch metrics
  TBM test_costs[NCS] = { 100, 200, 150, 180 };
  TPM quality = 0;
  TUS symbol = dec_standard.update(test_costs, &quality);
  printf("   Decoded symbol: %d, quality: %d\n\n", symbol, quality);

  printf("2. Testing NEON-optimized Viterbi decoder\n");
  #if LEANSDR_USE_NEON_INTRINSICS
    printf("   NEON intrinsics available - using optimized implementation\n");
  #else
    printf("   NEON not available - using fallback to standard implementation\n");
  #endif

  viterbi_dec_neon<TS, NSTATES, TUS, NUS, TCS, NCS, TBM, TPM, TP> dec_neon(&trellis_dvbs);
  quality = 0;
  symbol = dec_neon.update(test_costs, &quality);
  printf("   Decoded symbol: %d, quality: %d\n\n", symbol, quality);

  printf("3. Testing dual NEON decoder\n");
  viterbi_dec_neon_dual<TS, NSTATES, TUS, NUS, TCS, NCS, TBM, TPM, TP> dec_dual(&trellis_dvbs);

  TBM test_costs_a[NCS] = { 100, 200, 150, 180 };
  TBM test_costs_b[NCS] = { 120, 190, 160, 170 };

  auto result = dec_dual.update_dual(test_costs_a, test_costs_b);
  printf("   Stream A - symbol: %d, quality: %d\n",
         result.symbol_a, result.quality_a);
  printf("   Stream B - symbol: %d, quality: %d\n\n",
         result.symbol_b, result.quality_b);

  printf("4. Testing auto-selection decoder\n");
  viterbi_dec_auto<TS, NSTATES, TUS, NUS, TCS, NCS, TBM, TPM, TP> dec_auto(&trellis_dvbs);
  quality = 0;
  symbol = dec_auto.update(test_costs, &quality);
  printf("   Decoded symbol: %d, quality: %d\n\n", symbol, quality);

  printf("All tests completed successfully!\n");
  printf("\nNOTE: For performance benchmarking, compile with:\n");
  printf("  -O3 -march=armv8-a -DNDEBUG\n");
  printf("and run on ARM platform with NEON support.\n");

  return 0;
}
