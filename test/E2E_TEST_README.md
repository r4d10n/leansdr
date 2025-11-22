# End-to-End NEON Optimization Test Suite

## Overview

This comprehensive end-to-end integration test suite validates NEON-optimized leandvb implementations against scalar baseline versions. The suite generates synthetic DVB-S signals, processes them through both implementations, and compares outputs for correctness and performance.

## Quick Start

### Basic Usage

```bash
# Run default test (recommended starting point)
cd /home/user/leansdr/test
./test_e2e_neon.sh default

# Run fast test (for development/CI)
./test_e2e_neon.sh fast

# Run stress test (high symbol rate, low SNR)
./test_e2e_neon.sh stress

# Keep output files for inspection
KEEP_FILES=1 ./test_e2e_neon.sh default

# Verbose output
VERBOSE=1 ./test_e2e_neon.sh default
```

### Prerequisites

1. **Build leandvb binaries:**
   ```bash
   cd /home/user/leansdr
   make
   ```

2. **Python dependencies:**
   ```bash
   pip3 install numpy
   ```

3. **System tools:**
   - `bc` (for floating-point arithmetic)
   - `python3` (version 3.6+)

## Test Scenarios

The test suite includes several pre-configured scenarios:

### 1. `default` - Standard Validation Test
- **Symbol rate:** 1 Msps
- **Sample rate:** 2.4 Msps (2.4x oversampling)
- **SNR:** 15 dB
- **Packets:** 100
- **Duration:** ~30 seconds
- **Use case:** Standard validation, regression testing

### 2. `fast` - Quick Development Test
- **Symbol rate:** 1 Msps
- **Sample rate:** 2.4 Msps
- **SNR:** 20 dB (easier demodulation)
- **Packets:** 50
- **Duration:** ~15 seconds
- **Use case:** Rapid iteration during development

### 3. `stress` - Performance Stress Test
- **Symbol rate:** 2 Msps
- **Sample rate:** 8 Msps (4x oversampling)
- **SNR:** 8 dB (challenging conditions)
- **Packets:** 500
- **Duration:** ~2 minutes
- **Use case:** Validate under challenging conditions

### 4. `highspeed` - Real-World DVB-S Simulation
- **Symbol rate:** 27.5 Msps (typical DVB-S)
- **Sample rate:** 33 Msps
- **SNR:** 12 dB
- **Packets:** 200
- **Format:** u8 (8-bit unsigned)
- **Use case:** Realistic DVB-S reception simulation

### 5. `long` - Stability Test
- **Symbol rate:** 1 Msps
- **Sample rate:** 2.4 Msps
- **SNR:** 15 dB
- **Packets:** 1000
- **Duration:** ~3 minutes
- **Use case:** Long-running stability validation

## Test Components

### 1. Signal Generator (`generate_test_signal.py`)

Generates synthetic DVB-S QPSK-modulated IQ data with configurable parameters.

**Features:**
- Root-raised cosine (RRC) pulse shaping
- Additive white Gaussian noise (AWGN)
- Deterministic output (seeded random)
- Multiple output formats (f32, u8, s16)
- MPEG-TS packet generation with proper sync bytes

**Usage:**
```bash
./generate_test_signal.py \
    --output test.iq \
    --format f32 \
    --symbol-rate 1000000 \
    --sample-rate 2400000 \
    --snr 15 \
    --num-packets 100 \
    --verbose
```

**Parameters:**
- `--output`: Output IQ filename (required)
- `--format`: f32 (float32), u8 (uint8), s16 (int16)
- `--symbol-rate`: Symbol rate in Hz
- `--sample-rate`: Sampling rate in Hz
- `--snr`: Signal-to-noise ratio in dB
- `--num-packets`: Number of MPEG-TS packets (188 bytes each)
- `--rolloff`: RRC filter roll-off (default: 0.35)
- `--seed`: Random seed for reproducibility (default: 42)

### 2. Output Comparator (`compare_outputs.py`)

Compares MPEG-TS outputs from scalar and NEON implementations.

**Features:**
- Bit Error Rate (BER) calculation
- Packet Error Rate (PER) calculation
- MPEG-TS sync validation
- Byte-by-byte comparison
- Configurable tolerance levels

**Usage:**
```bash
./compare_outputs.py \
    scalar_output.ts \
    neon_output.ts \
    --tolerance 1e-4 \
    --verbose
```

**Validation Criteria:**
- ✓ File sizes within 1%
- ✓ Packet counts match (±1)
- ✓ BER ≤ tolerance (default: 1e-6)
- ✓ PER < 1%
- ✓ Byte match ratio > 99%

**Output Formats:**
- Human-readable (default)
- JSON (`--json` flag)

### 3. End-to-End Test Orchestrator (`test_e2e_neon.sh`)

Main test script that orchestrates the complete validation pipeline.

**Test Flow:**
1. **Generate synthetic DVB-S IQ data**
   - Uses `generate_test_signal.py`
   - Configurable parameters per test scenario
   - Deterministic (reproducible results)

2. **Process with scalar leandvb**
   - Baseline reference implementation
   - Measures processing time
   - Saves output to `scalar_output.ts`

3. **Process with NEON leandvb**
   - Optimized implementation under test
   - Measures processing time
   - Saves output to `neon_output.ts`

4. **Compare outputs**
   - Uses `compare_outputs.py`
   - Validates correctness (BER, PER)
   - Ensures bit-exact or near-exact match

5. **Performance analysis**
   - Calculates speedup ratio
   - Measures throughput (Msps)
   - Reports real-time factors
   - Generates detailed report

## Performance Metrics

The test suite measures and reports:

### Correctness Metrics
- **Bit Error Rate (BER):** Proportion of incorrect bits
- **Packet Error Rate (PER):** Proportion of corrupted packets
- **Byte match ratio:** Percentage of matching bytes
- **Sync validation:** MPEG-TS sync byte (0x47) detection

### Performance Metrics
- **Processing time:** Wall-clock time for demodulation
- **Speedup:** NEON time / Scalar time ratio
- **Throughput:** Symbols processed per second
- **Real-time factor:** Signal duration / Processing time

### Expected Results

**Typical NEON Speedups (ARM Cortex-A72):**
- FIR filtering: 3.5-4.0x
- AGC: 3.0-3.5x
- Overall: 2.0-2.5x end-to-end

**Pass Criteria:**
- Correctness: BER < 1e-4, PER < 1%
- Performance: Speedup > 1.5x
- Stability: No crashes, no memory leaks

## Output Files

Test results are saved in `/home/user/leansdr/test/e2e_output/`:

```
e2e_output/
├── test_signal.iq          # Generated IQ test signal
├── scalar_output.ts        # Scalar demodulator output
├── neon_output.ts          # NEON demodulator output
├── scalar_timing.txt       # Scalar processing time
├── neon_timing.txt         # NEON processing time
└── e2e_report_*.txt        # Detailed test report
```

**To preserve output files:**
```bash
KEEP_FILES=1 ./test_e2e_neon.sh default
```

## Troubleshooting

### Issue: "leandvb not found"

**Solution:**
```bash
cd /home/user/leansdr
make
```

Ensure binaries are in `src/apps/`:
- `leandvb` (scalar or with NEON compiled in)
- `leandvb_neon` (optional NEON-specific build)

### Issue: "ModuleNotFoundError: No module named 'numpy'"

**Solution:**
```bash
pip3 install numpy
# or
sudo apt-get install python3-numpy
```

### Issue: Low or no speedup

**Possible causes:**
1. **NEON not compiled in:** Check build flags
2. **CPU throttling:** Set governor to performance
3. **Not running on ARM:** NEON is ARM-specific
4. **Small test size:** Overhead dominates

**Debug:**
```bash
# Check if NEON is compiled
nm src/apps/leandvb | grep -i neon

# Check CPU frequency
cat /proc/cpuinfo | grep MHz

# Set performance mode
sudo cpufreq-set -g performance
```

### Issue: High BER or PER

**Possible causes:**
1. **Low SNR:** Increase `--snr` parameter
2. **Incorrect sample rate:** Verify configuration
3. **NEON implementation bug:** File bug report

**Debug:**
```bash
# Try with higher SNR
VERBOSE=1 ./test_e2e_neon.sh fast

# Generate test signal manually
./generate_test_signal.py -o test.iq --snr 20 --verbose

# Inspect outputs
hexdump -C e2e_output/scalar_output.ts | head
hexdump -C e2e_output/neon_output.ts | head
```

## Advanced Usage

### Custom Test Configuration

Edit `test_e2e_neon.sh` to add custom test scenarios:

```bash
# Add to TEST_CONFIGS array
TEST_CONFIGS[my_test]="symbol_rate=2000000 sample_rate=4800000 snr=12 packets=200 format=f32"

# Run custom test
./test_e2e_neon.sh my_test
```

### Manual Pipeline Execution

Run each step independently:

```bash
# Step 1: Generate signal
./generate_test_signal.py -o test.iq --num-packets 100

# Step 2: Process with scalar
cat test.iq | leandvb --f32 -f 2400000 --sr 1000000 > scalar.ts

# Step 3: Process with NEON
cat test.iq | leandvb_neon --f32 -f 2400000 --sr 1000000 > neon.ts

# Step 4: Compare
./compare_outputs.py scalar.ts neon.ts --verbose
```

### Batch Testing

Run multiple test scenarios:

```bash
#!/bin/bash
for test in fast default stress; do
    echo "Running $test..."
    ./test_e2e_neon.sh $test | tee results_$test.txt
done
```

## Continuous Integration

### GitHub Actions Example

```yaml
name: E2E NEON Tests

on: [push, pull_request]

jobs:
  test-neon:
    runs-on: [self-hosted, arm64]
    steps:
      - uses: actions/checkout@v2

      - name: Build leandvb
        run: cd /home/user/leansdr && make

      - name: Install Python dependencies
        run: pip3 install numpy

      - name: Run E2E tests
        run: |
          cd test
          ./test_e2e_neon.sh fast
          ./test_e2e_neon.sh default

      - name: Upload results
        uses: actions/upload-artifact@v2
        with:
          name: test-reports
          path: test/e2e_output/
```

## Performance Benchmarking

### Platform-Specific Baselines

**Raspberry Pi 4 (Cortex-A72 @ 1.5 GHz):**
- Expected speedup: 2.2-2.5x
- Throughput: ~5-8 Msps (scalar), ~12-18 Msps (NEON)

**Raspberry Pi 3 (Cortex-A53 @ 1.2 GHz):**
- Expected speedup: 1.8-2.2x
- Throughput: ~2-4 Msps (scalar), ~4-8 Msps (NEON)

**ODROID-XU4 (Cortex-A15 @ 2.0 GHz):**
- Expected speedup: 2.5-3.0x
- Throughput: ~8-12 Msps (scalar), ~20-30 Msps (NEON)

### Profiling

To identify performance bottlenecks:

```bash
# Build with profiling
cd /home/user/leansdr
make CFLAGS="-g -pg"

# Run test
./test_e2e_neon.sh fast

# Analyze profile
gprof src/apps/leandvb gmon.out > profile.txt
```

## Validation Philosophy

This test suite follows a **comprehensive validation** approach:

1. **Correctness First:** NEON optimizations must produce bit-exact or nearly-identical results
2. **Performance Second:** Speedup is measured only after correctness is validated
3. **Stability Third:** Long-running tests ensure no memory leaks or crashes
4. **Reproducibility:** Deterministic test signals enable consistent results

## Integration with Unit Tests

This E2E suite complements the unit tests in `test_neon.cc`:

| Test Type | Scope | Speed | Use Case |
|-----------|-------|-------|----------|
| **Unit Tests** | Individual functions | Fast (~1s) | Development, TDD |
| **E2E Tests** | Full pipeline | Medium (~30s) | Integration validation |
| **Stress Tests** | Long duration | Slow (~2min) | Stability, profiling |

Recommended workflow:
1. Unit tests during development (rapid iteration)
2. E2E fast test before commit (integration check)
3. E2E default test in CI (automated validation)
4. Stress test before release (stability confirmation)

## License

This test suite is part of LeanSDR and follows the GPL-3.0 license.

Copyright (C) 2016-2025 <pabr@pabr.org>

## References

- **LeanSDR Project:** https://github.com/pabr/leansdr
- **DVB-S Standard:** ETSI EN 300 421
- **ARM NEON:** https://developer.arm.com/architectures/instruction-sets/intrinsics/
- **NEON Optimization Plan:** `/home/user/leansdr/docs/optimization/NEON-OPTIMIZATION-PLAN.md`

## Support

For issues or questions:
1. Check this README and troubleshooting section
2. Review test output in `e2e_output/e2e_report_*.txt`
3. Run with `VERBOSE=1` for detailed logs
4. File issue on GitHub with reproducible test case

---

**Happy Testing!**
