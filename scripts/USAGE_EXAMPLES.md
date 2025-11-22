# Performance Report Generator - Usage Examples

Complete guide with real-world examples for using the performance report generator.

## Table of Contents

1. [Basic Usage](#basic-usage)
2. [With Benchmark Suite](#with-benchmark-suite)
3. [Advanced Scenarios](#advanced-scenarios)
4. [Batch Processing](#batch-processing)
5. [Troubleshooting](#troubleshooting)

## Basic Usage

### Example 1: Simple Report from Single CSV

```bash
cd /home/user/leansdr/scripts
./generate_performance_report.py /path/to/benchmark_results.csv
```

Output:
- `performance_report.html` - Complete interactive report
- `charts/` - Directory with 5 PNG visualizations

### Example 2: Custom Output Location

```bash
./generate_performance_report.py benchmark_results.csv \
    --output /tmp/reports/my_report.html
```

This creates:
- `/tmp/reports/my_report.html`
- `/tmp/reports/charts/*.png`

### Example 3: Verbose Logging

For debugging and detailed execution information:

```bash
./generate_performance_report.py benchmark_results.csv --verbose
```

Output shows:
```
2025-11-22 01:17:05,583 - __main__ - INFO - Processing 1 input file(s)...
2025-11-22 01:17:05,583 - __main__ - INFO - Parsing: benchmark_results.csv
2025-11-22 01:17:05,590 - __main__ - INFO - Loaded 50 benchmark results
...
```

## With Benchmark Suite

### Example 4: Generate Report from Benchmark Run

After running the benchmark suite:

```bash
cd /home/user/leansdr/test
./run_benchmark_neon.sh

# Wait for benchmarks to complete...
# This creates benchmark_results/benchmark_<commit>_<timestamp>_*.csv

cd /home/user/leansdr/scripts

# Generate report from scalar results
./generate_performance_report.py \
    ../test/benchmark_results/benchmark_*_scalar.csv \
    --output scalar_results_report.html

# Or analyze NEON results
./generate_performance_report.py \
    ../test/benchmark_results/benchmark_*_neon.csv \
    --output neon_results_report.html
```

### Example 5: Comparative Analysis

Compare scalar and NEON results together:

```bash
# Get latest benchmark results
LATEST=$(ls -t ../test/benchmark_results/ | head -1 | sed 's/_scalar.csv//')

./generate_performance_report.py \
    "../test/benchmark_results/${LATEST}_scalar.csv" \
    "../test/benchmark_results/${LATEST}_neon.csv" \
    --output comparative_analysis.html
```

The report will show merged analysis across both implementations.

### Example 6: PDF Export

Generate both HTML and PDF versions:

```bash
./generate_performance_report.py benchmark_results.csv \
    --output report.html \
    --pdf report.pdf
```

Output:
- `report.html` - Interactive report for web viewing
- `report.pdf` - Printable version for documentation/sharing

First install PDF export dependencies:

```bash
pip install weasyprint
```

Note: PDF generation requires additional system packages on some platforms:

```bash
# Ubuntu/Debian
sudo apt-get install libffi-dev libcairo2-dev libpango1.0-dev

# macOS
brew install cairo
```

## Advanced Scenarios

### Example 7: Filtering Specific Operations

Analyze only FIR filter performance:

```bash
# Extract relevant rows
grep "^fir_filter" benchmark_results.csv > fir_results.csv
grep "^operation" benchmark_results.csv | cat - fir_results.csv > fir_filtered.csv

./generate_performance_report.py fir_filtered.csv \
    --output fir_analysis.html
```

### Example 8: Comparing Compiler Optimizations

Build with different flags and compare:

```bash
cd /home/user/leansdr/test

# Build with -O2
g++ -O2 -I../src benchmark_neon.cc -o benchmark_O2
./benchmark_O2 > results_O2.csv

# Build with -O3
g++ -O3 -I../src benchmark_neon.cc -o benchmark_O3
./benchmark_O3 > results_O3.csv

# Build with -Ofast
g++ -Ofast -I../src benchmark_neon.cc -o benchmark_Ofast
./benchmark_Ofast > results_Ofast.csv

# Compare all three
cd ../scripts
./generate_performance_report.py \
    ../test/results_O2.csv \
    ../test/results_O3.csv \
    ../test/results_Ofast.csv \
    --output optimization_flags_comparison.html
```

Report will show how compiler optimization levels affect performance.

### Example 9: Architecture Comparison

Compare performance across different ARM architectures:

```bash
# On ARMv7 (e.g., Raspberry Pi 2/3)
cd /home/user/leansdr/test
./run_benchmark_neon.sh
cp benchmark_results/benchmark_*_scalar.csv ../scripts/armv7_scalar.csv
cp benchmark_results/benchmark_*_neon.csv ../scripts/armv7_neon.csv

# Later, on AArch64 (e.g., Raspberry Pi 4+)
cd /home/user/leansdr/test
./run_benchmark_neon.sh
cp benchmark_results/benchmark_*_scalar.csv ../scripts/aarch64_scalar.csv
cp benchmark_results/benchmark_*_neon.csv ../scripts/aarch64_neon.csv

# Compare architectures
cd ../scripts
./generate_performance_report.py \
    armv7_scalar.csv armv7_neon.csv \
    aarch64_scalar.csv aarch64_neon.csv \
    --output architecture_comparison.html
```

### Example 10: Regression Testing

Monitor performance across commits:

```bash
#!/bin/bash
# compare_performance.sh

REPORT_DIR="performance_reports"
mkdir -p "$REPORT_DIR"

cd /home/user/leansdr/test

# Get two different commits
COMMIT1="aca89f2"  # Latest
COMMIT2="e6416aa"  # Previous

# Checkout and benchmark first commit
git show $COMMIT1:test/benchmark_neon.cc > benchmark_v1.cc
g++ -O3 -march=armv8-a -ftree-vectorize -I../src \
    benchmark_v1.cc -o benchmark_v1
./benchmark_v1 > results_v1.csv

# Checkout and benchmark second commit
git show $COMMIT2:test/benchmark_neon.cc > benchmark_v2.cc
g++ -O3 -march=armv8-a -ftree-vectorize -I../src \
    benchmark_v2.cc -o benchmark_v2
./benchmark_v2 > results_v2.csv

# Generate comparison report
cd ../scripts
./generate_performance_report.py \
    ../test/results_v2.csv \
    ../test/results_v1.csv \
    --output "$REPORT_DIR/regression_test.html"

echo "Report: $REPORT_DIR/regression_test.html"
```

## Batch Processing

### Example 11: Automated Daily Reports

Create a cron job for daily performance monitoring:

```bash
#!/bin/bash
# daily_performance_report.sh

set -e

REPO_DIR="/home/user/leansdr"
REPORTS_DIR="$REPO_DIR/reports/performance"
REPORT_DATE=$(date +%Y%m%d)

mkdir -p "$REPORTS_DIR"

echo "[$(date)] Starting daily performance report..."

cd "$REPO_DIR/test"

# Run benchmarks
./run_benchmark_neon.sh

# Generate report
cd "$REPO_DIR/scripts"

LATEST_SCALAR=$(ls -t "$REPO_DIR/test/benchmark_results/"*_scalar.csv | head -1)
LATEST_NEON=$(ls -t "$REPO_DIR/test/benchmark_results/"*_neon.csv | head -1)

./generate_performance_report.py \
    "$LATEST_SCALAR" "$LATEST_NEON" \
    --output "$REPORTS_DIR/report_${REPORT_DATE}.html" \
    --pdf "$REPORTS_DIR/report_${REPORT_DATE}.pdf"

echo "[$(date)] Report saved: $REPORTS_DIR/report_${REPORT_DATE}.html"
```

Add to crontab:

```bash
# Run daily at 3 AM
0 3 * * * /home/user/leansdr/scripts/daily_performance_report.sh >> /var/log/performance_reports.log 2>&1
```

### Example 12: Parallel Analysis

Process multiple benchmark results in parallel:

```bash
#!/bin/bash
# parallel_analysis.sh

SCRIPT_DIR="/home/user/leansdr/scripts"
RESULTS_DIR="$SCRIPT_DIR/../test/benchmark_results"
OUTPUT_DIR="$SCRIPT_DIR/reports"

mkdir -p "$OUTPUT_DIR"

# Process each benchmark file separately
for csv in "$RESULTS_DIR"/*.csv; do
    basename=$(basename "$csv" .csv)
    echo "Processing $basename..."

    "$SCRIPT_DIR/generate_performance_report.py" \
        "$csv" \
        --output "$OUTPUT_DIR/${basename}_report.html" &
done

# Wait for all background jobs
wait

echo "All reports generated in $OUTPUT_DIR"
```

### Example 13: Weekly Summary Report

Combine multiple days of reports into a summary:

```bash
#!/bin/bash
# weekly_summary.sh

REPORTS_DIR="/home/user/leansdr/reports/performance"
OUTPUT_FILE="$REPORTS_DIR/weekly_summary.html"

# Get all reports from the past 7 days
find "$REPORTS_DIR" -name "report_*.csv" -mtime -7 | \
xargs /home/user/leansdr/scripts/generate_performance_report.py \
    --output "$OUTPUT_FILE"

echo "Weekly summary: $OUTPUT_FILE"
```

## Troubleshooting

### Example 14: Debugging CSV Format Issues

If the script fails to parse your CSV:

```bash
# Check CSV format
head benchmark_results.csv

# Expected output:
# operation,param1,param2,scalar_cycles,neon_cycles,speedup
# fir_filter,64,128,82534,21456,3.85
```

If columns are in different order:

```bash
# Fix column order with awk
awk -F',' '{print $1","$2","$3","$4","$5","$6}' \
    wrong_format.csv > correct_format.csv

./generate_performance_report.py correct_format.csv
```

### Example 15: Memory Issues with Large Files

For very large benchmark sets (>10,000 rows):

```bash
# Split into chunks
split -l 5000 large_benchmark.csv chunk_

# Process each chunk
for chunk in chunk_*; do
    ./generate_performance_report.py "$chunk" \
        --output "report_${chunk}.html"
done
```

### Example 16: Testing with Sample Data

Create minimal test data:

```bash
cat > test_data.csv << 'EOF'
operation,param1,param2,scalar_cycles,neon_cycles,speedup
fir_filter,64,128,1000,250,4.0
agc_power,0,256,500,150,3.33
complex_multiply,0,512,800,200,4.0
mpeg_sync,0,4096,5000,700,7.14
EOF

./generate_performance_report.py test_data.csv --verbose
```

## Integration with CI/CD

### Example 17: GitHub Actions Integration

```yaml
name: Performance Report

on:
  push:
    branches: [main, develop]
  schedule:
    - cron: '0 3 * * *'  # Daily at 3 AM

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: pip install numpy pandas matplotlib seaborn weasyprint

      - name: Build benchmarks
        run: |
          cd test
          ./run_benchmark_neon.sh

      - name: Generate report
        run: |
          cd scripts
          ./generate_performance_report.py \
              ../test/benchmark_results/*.csv \
              --output performance_report.html \
              --pdf performance_report.pdf

      - name: Upload artifacts
        uses: actions/upload-artifact@v2
        with:
          name: performance-reports
          path: scripts/performance_report.*
```

### Example 18: Integration with Jenkins

```groovy
pipeline {
    agent any

    stages {
        stage('Benchmark') {
            steps {
                sh '''
                    cd test
                    ./run_benchmark_neon.sh
                '''
            }
        }

        stage('Generate Report') {
            steps {
                sh '''
                    cd scripts
                    python3 generate_performance_report.py \
                        ../test/benchmark_results/*.csv \
                        --output performance_report.html \
                        --pdf performance_report.pdf
                '''
            }
        }

        stage('Archive') {
            steps {
                archiveArtifacts artifacts: 'scripts/performance_report.*'
            }
        }
    }
}
```

## Quick Reference

```bash
# Minimal example
./generate_performance_report.py results.csv

# Full example with all options
./generate_performance_report.py \
    results_scalar.csv \
    results_neon.csv \
    --output my_report.html \
    --pdf my_report.pdf \
    --title "Performance Analysis" \
    --verbose

# Multiple files
./generate_performance_report.py *.csv \
    --output combined_report.html
```

## See Also

- [PERFORMANCE_REPORT_README.md](./PERFORMANCE_REPORT_README.md) - Full documentation
- [/home/user/leansdr/test/BENCHMARK_README.md](../test/BENCHMARK_README.md) - Benchmark suite docs
- [/home/user/leansdr/test/run_benchmark_neon.sh](../test/run_benchmark_neon.sh) - Benchmark script
