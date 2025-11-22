// Example: NEON Integration in LeanSDR
// This demonstrates how to use the NEON integration layer

#include <stdio.h>
#include "leansdr/framework.h"
#include "leansdr/sdr.h"
#include "leansdr/dvb.h"
#include "leansdr/neon_integration.h"

using namespace leansdr;
using namespace leansdr::neon;

int main(int argc, char** argv) {
  printf("=== LeanSDR NEON Integration Example ===\n\n");

  // Step 1: Initialize NEON subsystem
  printf("Step 1: Initializing NEON...\n");
  init_neon(true);  // verbose=true to print capabilities

  printf("\n");

  // Step 2: Check capabilities
  printf("Step 2: Checking NEON availability...\n");
  printf("NEON available: %s\n", is_neon_available() ? "YES" : "NO");
  printf("Status: %s\n", get_neon_status_string());

  if (is_neon_available()) {
    printf("Expected speedup: %.1fx\n", estimate_neon_speedup());
    printf("Expected CPU reduction: %.1f%%\n", estimate_cpu_reduction_percent());
  }

  printf("\n");

  // Step 3: Create optimized components
  printf("Step 3: Creating optimized components...\n");

  scheduler sch;

  // Example buffers
  const int BUFFER_SIZE = 1024;
  pipebuf<complex<float>> buf_in(&sch, "input", BUFFER_SIZE);
  pipebuf<complex<float>> buf_out(&sch, "output", BUFFER_SIZE);

  // Create AGC with automatic NEON optimization
  printf("Creating AGC...\n");
  auto* agc = create_optimized_agc<float>(&sch, buf_in, buf_out);
  agc->out_rms = 1.0f;
  agc->bw = 0.001f;

  printf("AGC created using: %s\n",
         is_neon_available() ? "NEON acceleration" : "scalar fallback");

  printf("\n");

  // Step 4: Print configuration
  printf("Step 4: Current NEON configuration:\n");
  neon_config& cfg = get_neon_config();
  cfg.print(stdout);

  printf("\n");

  // Step 5: Cleanup
  printf("Step 5: Cleaning up...\n");
  delete agc;
  cleanup_neon();

  printf("\nExample completed successfully!\n");

  return 0;
}
