# LeanSDR NEON Benchmark Suite

Comprehensive micro-benchmarking suite for measuring NEON SIMD optimization performance in LeanSDR.

## Overview

This benchmark suite measures the performance improvement of ARM NEON optimizations compared to scalar (non-vectorized) implementations for critical DSP operations in LeanSDR.

### Benchmarked Operations

1. **FIR Filter** - Complex convolution (highest impact on DVB-S performance)
2. **AGC Power Measurement** - Power calculation for automatic gain control
3. **Complex Multiplication** - Complex number arithmetic
4. **MPEG Sync Search** - Byte pattern matching for transport stream sync
5. **Horizontal Sum** - Vector reduction operations
6. **Dot Product** - Real-valued multiply-accumulate
7. **Viterbi ACS** - Add-Compare-Select operations for Viterbi decoding

### Test Sizes

- **Small** (256-2048 elements): L1 cache-friendly, shows peak NEON performance
- **Medium** (2048-8192 elements): L2 cache, realistic for streaming
- **Large** (8192-32768 elements): Main memory, shows memory bandwidth limits

## Quick Start

### Build and Run

```bash
cd /home/user/leansdr/test
./run_benchmark_neon.sh
```

This will:
1. Build both scalar and NEON versions
2. Run comprehensive benchmarks
3. Generate CSV results
4. Create a comparison report
5. Prepare data for plotting

### Manual Build

If you prefer to build manually:

```bash
# Scalar version (baseline)
g++ -O3 -I../src benchmark_neon.cc -o benchmark_neon_scalar

# NEON version (ARM only)
# For ARMv7 (Raspberry Pi 2/3):
g++ -O3 -march=armv7-a -mfpu=neon -ftree-vectorize -I../src \
    benchmark_neon.cc -o benchmark_neon_neon

# For ARMv8/AArch64 (Raspberry Pi 4/5):
g++ -O3 -march=armv8-a -ftree-vectorize -I../src \
    benchmark_neon.cc -o benchmark_neon_neon

# Run
./benchmark_neon_scalar > results_scalar.csv
./benchmark_neon_neon > results_neon.csv
```

## Output Format

### CSV Format

Results are saved in CSV format with the following columns:

```
operation,param1,param2,scalar_cycles,neon_cycles,speedup
```

- **operation**: Name of the benchmarked operation
- **param1**: First parameter (e.g., number of filter coefficients, Viterbi states)
- **param2**: Second parameter (e.g., number of samples)
- **scalar_cycles**: CPU cycles for scalar implementation
- **neon_cycles**: CPU cycles for NEON implementation (0 if not available)
- **speedup**: Ratio of scalar_cycles / neon_cycles

### Example Output

```csv
operation,param1,param2,scalar_cycles,neon_cycles,speedup
fir_filter,64,128,82534,21456,3.85
fir_filter,128,256,330145,87234,3.78
agc_power,0,256,1245,342,3.64
complex_multiply,0,256,1834,523,3.51
mpeg_sync,0,4096,45678,6543,6.98
horizontal_sum,0,256,678,312,2.17
dot_product,0,256,1123,365,3.08
viterbi_acs,64,0,3456,1678,2.06
```

## Interpreting Results

### Speedup Metrics

- **>4.0x**: Excellent NEON optimization (approaching theoretical 4x for float32)
- **2.0-4.0x**: Good NEON optimization (typical for most operations)
- **1.5-2.0x**: Moderate benefit (overhead from horizontal operations, memory bandwidth)
- **<1.5x**: Limited benefit (memory-bound, complex data dependencies)

### Operation-Specific Notes

#### FIR Filter
- **Expected speedup**: 3.5-4.0x
- **Why**: Perfect for NEON - parallel multiply-accumulate operations
- **Bottleneck**: Horizontal reduction at the end (4 cycles overhead)

#### AGC Power (re² + im²)
- **Expected speedup**: 3.5-4.0x
- **Why**: Pure parallel arithmetic, no horizontal operations
- **Note**: Processing 4 complex samples at once

#### Complex Multiplication
- **Expected speedup**: 3.0-3.5x
- **Why**: Good parallelism, but more complex arithmetic
- **Formula**: `(a.re*b.re - a.im*b.im) + i(a.re*b.im + a.im*b.re)`

#### MPEG Sync Search
- **Expected speedup**: 4.0-8.0x
- **Why**: Vectorized byte comparison (16 bytes at once)
- **Note**: Speedup varies based on sync pattern location

#### Horizontal Sum
- **Expected speedup**: 1.5-2.5x
- **Why**: Limited by horizontal reduction overhead
- **Bottleneck**: Need to sum 4 vector lanes into single scalar

#### Dot Product
- **Expected speedup**: 3.0-3.5x
- **Why**: Good use of multiply-accumulate, but horizontal reduction limits speedup
- **Note**: Similar to FIR filter but real-valued

#### Viterbi ACS
- **Expected speedup**: 1.8-2.5x
- **Why**: Integer operations, data dependencies, branch-heavy
- **Limitation**: Cannot fully vectorize due to unpredictable memory access patterns

## Performance Factors

### Cache Effects

Results will vary by data size:
- **L1 cache hit**: Best speedup (data always in fastest cache)
- **L2 cache hit**: Good speedup (slight memory latency)
- **Main memory**: Lower speedup (memory bandwidth bottleneck)

### CPU Frequency Scaling

For accurate results:
1. Disable CPU frequency scaling (if possible):
   ```bash
   # On Linux
   sudo cpupower frequency-set -g performance
   ```

2. Or run benchmarks multiple times and take the best result

### Thermal Throttling

Long benchmark runs may trigger thermal throttling:
- Results from early iterations may be faster
- Consider running in a cool environment
- Monitor CPU temperature: `vcgencmd measure_temp` (Raspberry Pi)

## Analyzing Results

### 1. View Summary Report

```bash
cat benchmark_results/benchmark_*_report.txt
```

This shows:
- Average speedup per operation
- Overall average speedup
- Detailed cycle counts

### 2. Plot Results

If you have gnuplot installed:

```bash
cd benchmark_results
gnuplot benchmark_*_plot.gnuplot
```

This generates `benchmark_speedup.png` with:
- Speedup by operation and data size
- Scalar vs NEON cycle comparison
- Speedup distribution histogram
- Per-operation scaling analysis

### 3. Import to Spreadsheet

Open the CSV files in Excel, LibreOffice Calc, or Google Sheets:
- `*_neon.csv` - Full NEON results
- `*_scalar.csv` - Scalar-only results
- `*_plotdata.csv` - Simplified format for plotting

Create charts:
- Bar chart: Speedup by operation
- Line chart: Cycles vs data size
- Scatter plot: Scalar cycles vs NEON cycles

## Comparing Across Platforms

### Raspberry Pi Versions

Run on multiple devices and compare:

```bash
# On Raspberry Pi 3 (ARMv7, 1.2 GHz)
./run_benchmark_neon.sh

# On Raspberry Pi 4 (ARMv8, 1.5 GHz)
./run_benchmark_neon.sh

# On Raspberry Pi 5 (ARMv8.2, 2.4 GHz)
./run_benchmark_neon.sh
```

Compare:
- Absolute cycle counts (lower is better)
- Speedup ratios (should be similar across platforms)
- Throughput (samples/second, higher is better)

### x86 vs ARM

To compare scalar performance:

```bash
# On x86 desktop
g++ -O3 -march=native -I../src benchmark_neon.cc -o benchmark_x86
./benchmark_x86 > results_x86.csv

# On ARM device
./benchmark_neon_scalar > results_arm_scalar.csv

# Compare scalar_cycles column
```

## Expected Results

### Raspberry Pi 4 (ARMv8, 1.5 GHz Cortex-A72)

Typical speedups:
- FIR filter (128 taps): **3.8x**
- AGC power: **3.6x**
- Complex multiply: **3.4x**
- MPEG sync: **6.5x**
- Horizontal sum: **2.1x**
- Dot product: **3.2x**
- Viterbi ACS: **2.0x**

Overall average: **~3.2x**

### Raspberry Pi 3 (ARMv7, 1.2 GHz Cortex-A53)

Typical speedups:
- FIR filter: **3.5x**
- AGC power: **3.4x**
- Complex multiply: **3.1x**
- Overall average: **~2.8x**

(Slightly lower due to less aggressive NEON implementation in Cortex-A53)

## Troubleshooting

### Build Errors

**Error: `arm_neon.h` not found**
- Solution: Not on ARM platform, or compiler not configured for NEON
- Fix: Use `-march=armv7-a -mfpu=neon` flags

**Error: undefined reference to `clock_gettime`**
- Solution: Need to link with `-lrt` on some systems
- Fix: `g++ ... -lrt -o benchmark_neon`

### Runtime Issues

**Speedup is 1.0x (no improvement)**
- Check: Is NEON actually enabled?
  ```bash
  grep -i neon /proc/cpuinfo  # Should show "neon" in features
  objdump -d benchmark_neon_neon | grep vmla  # Should show NEON instructions
  ```

**Speedup is lower than expected**
- Possible causes:
  1. CPU frequency scaling (check: `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`)
  2. Thermal throttling (check: `vcgencmd measure_temp`)
  3. Memory bandwidth bottleneck (try smaller data sizes)
  4. Background processes (close unnecessary applications)

**Benchmark crashes or produces wrong results**
- Check: Memory alignment (NEON requires 4-byte alignment for float32)
- Check: Buffer overruns (verify array bounds)
- Try: Running with debug build first

## Advanced Usage

### Custom Benchmarks

Add your own benchmarks by:

1. Implementing scalar and NEON versions of your function
2. Adding a benchmark function following the existing pattern
3. Calling it from `main()` with different parameters

Example:

```cpp
void my_custom_operation_scalar(const float* in, float* out, int n) {
    // Your scalar implementation
}

#ifdef __ARM_NEON
void my_custom_operation_neon(const float* in, float* out, int n) {
    // Your NEON implementation
}
#endif

void benchmark_my_custom_operation(int size) {
    // Setup, warmup, timing, reporting
    // Follow pattern from existing benchmarks
}
```

### Profiling Integration

Use with Linux `perf` for detailed analysis:

```bash
# Profile scalar version
perf record -g ./benchmark_neon_scalar
perf report

# Profile NEON version
perf record -g ./benchmark_neon_neon
perf report

# Check NEON instruction usage
perf stat -e instructions,cycles,branches,branch-misses ./benchmark_neon_neon
```

### Continuous Integration

Add to CI pipeline:

```bash
# In .gitlab-ci.yml or .github/workflows/
test_neon:
  script:
    - cd test
    - ./run_benchmark_neon.sh
    - # Check that speedup meets threshold
    - awk -F',' '$6 < 2.0 {exit 1}' benchmark_results/*_neon.csv
```

## References

### ARM NEON Documentation

- [ARM NEON Intrinsics Reference](https://developer.arm.com/architectures/instruction-sets/intrinsics/)
- [ARM NEON Optimization Guide](https://developer.arm.com/documentation/den0018/a/)
- [Coding for NEON Series](https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/coding-for-neon---part-1-load-and-stores)

### LeanSDR Documentation

- [NEON Optimization Plan](/home/user/leansdr/docs/optimization/NEON-OPTIMIZATION-PLAN.md)
- [Architecture Overview](/home/user/leansdr/docs/architecture/)

## License

This benchmark suite is part of LeanSDR and follows the same GPL-3.0 license.

---

**Last Updated**: 2025-11-22
**Author**: LeanSDR Optimization Team
