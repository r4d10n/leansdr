# Performance Report Generator

Comprehensive performance analysis tool for LeanSDR NEON SIMD optimizations.

## Overview

`generate_performance_report.py` is a production-quality Python script that:

1. **Parses benchmark results** from CSV files (scalar vs NEON implementations)
2. **Analyzes performance metrics** including speedup, efficiency, and throughput
3. **Generates interactive HTML reports** with detailed visualizations
4. **Creates publication-quality charts** using matplotlib/seaborn
5. **Optionally exports to PDF** for easy distribution
6. **Provides optimization recommendations** with ROI assessment

## Features

### Comprehensive Analysis

- **Summary Statistics**: Mean, median, min, max, and percentile speedups
- **Per-Operation Analysis**: Detailed breakdown by benchmarked operation
- **Data Size Impact**: Performance categorization by data size (L1/L2/main memory)
- **Efficiency Metrics**: Overall cycles saved and throughput improvements
- **System Information**: Platform details, architecture, CPU info

### Professional Visualizations

1. **Speedup by Operation** - Bar chart showing performance improvement per operation
2. **Speedup Distribution** - Histogram with mean/median indicators
3. **Cycles Comparison** - Scalar vs NEON execution cycles
4. **Performance vs Data Size** - Scatter plot analyzing size impact
5. **Efficiency Gauge** - Polar chart showing overall efficiency percentage

### Intelligent Recommendations

- Categorizes optimizations by performance level (EXCELLENT, GOOD, MODERATE, LOW)
- Calculates throughput gains (0-100%)
- Identifies cycles saved per operation
- Provides actionable optimization strategies

### Error Handling

- Robust CSV parsing with error recovery
- Missing file detection with clear error messages
- Graceful handling of missing dependencies
- Detailed logging for troubleshooting

## Installation

### Requirements

```bash
Python 3.7+
numpy
pandas
matplotlib
seaborn
```

### Setup

```bash
# Install dependencies
pip install numpy pandas matplotlib seaborn

# Optional: For PDF export
pip install weasyprint

# Navigate to scripts directory
cd /home/user/leansdr/scripts
```

## Usage

### Basic Usage

```bash
./generate_performance_report.py results_scalar.csv
```

This generates `performance_report.html` with all visualizations and analysis.

### With NEON Comparison

If you have both scalar and NEON results:

```bash
./generate_performance_report.py results_scalar.csv results_neon.csv
```

The tool merges and analyzes all results together.

### Custom Output Location

```bash
./generate_performance_report.py benchmark_results.csv \
    --output reports/my_report.html
```

### PDF Export

```bash
./generate_performance_report.py benchmark_results.csv \
    --pdf reports/report.pdf
```

### Verbose Logging

```bash
./generate_performance_report.py benchmark_results.csv --verbose
```

## CSV Format

The script expects benchmark results in CSV format:

```
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

### Columns

- **operation**: Name of benchmarked operation
- **param1**: First parameter (e.g., filter coefficients, Viterbi states)
- **param2**: Second parameter (e.g., number of samples)
- **scalar_cycles**: CPU cycles for scalar implementation
- **neon_cycles**: CPU cycles for NEON implementation
- **speedup**: Ratio of scalar_cycles / neon_cycles

## Workflow Example

### 1. Generate Benchmark Results

From the `/home/user/leansdr/test` directory:

```bash
# Build and run benchmarks
./run_benchmark_neon.sh

# This generates:
# - benchmark_results/benchmark_<commit>_<timestamp>_scalar.csv
# - benchmark_results/benchmark_<commit>_<timestamp>_neon.csv
```

### 2. Generate Report

```bash
cd /home/user/leansdr/scripts

./generate_performance_report.py \
    ../test/benchmark_results/benchmark_*_scalar.csv \
    ../test/benchmark_results/benchmark_*_neon.csv \
    --output ../reports/latest_performance.html \
    --pdf ../reports/latest_performance.pdf
```

### 3. View Report

Open `performance_report.html` in your web browser. The report includes:

- Interactive tables (sortable if using browser features)
- High-resolution charts
- System configuration
- Detailed analysis
- Actionable recommendations

## Output Files

The generator creates the following in the output directory:

```
performance_report.html          # Main HTML report
charts/
  ├── speedup_by_operation.png   # Bar chart
  ├── speedup_distribution.png   # Histogram
  ├── cycles_comparison.png       # Dual bar chart
  ├── speedup_by_size.png        # Scatter plot
  └── efficiency_gauge.png        # Polar chart
performance_report.pdf            # Optional PDF export
```

## Interpreting Results

### Speedup Metrics

- **> 4.0x**: Excellent (approaching theoretical 4x for float32 NEON)
- **2.0 - 4.0x**: Good (typical for well-optimized operations)
- **1.5 - 2.0x**: Moderate (overhead from horizontal operations, memory bandwidth)
- **< 1.5x**: Low (may not justify complexity, memory-bound, or scalar-friendly)

### Color Coding in Report

- **Green**: Excellent performance (> 3.0x speedup)
- **Blue**: Good performance (2.0 - 3.0x speedup)
- **Orange**: Moderate performance (1.5 - 2.0x speedup)
- **Red**: Low performance (< 1.5x speedup)

### Key Sections

1. **Executive Summary**: Quick overview of overall performance
2. **System Information**: Hardware and environment details
3. **Performance Analysis**: Charts and visualizations
4. **Detailed Results**: Per-operation statistics
5. **Performance by Data Size**: L1/L2/memory impact analysis
6. **Optimization Recommendations**: Prioritized improvement suggestions
7. **Key Insights**: High-level takeaways and patterns

## Advanced Usage

### Processing Multiple Benchmark Runs

Compare different compiler flags or architecture variants:

```bash
./generate_performance_report.py \
    benchmark_neon_O2.csv \
    benchmark_neon_O3.csv \
    benchmark_neon_Ofast.csv \
    --output comparison_optimization_flags.html
```

### Filtering Results

For advanced filtering, you can edit CSV files or use command-line tools:

```bash
# Extract only FIR filter results
grep "^fir_filter" results.csv > fir_only.csv
./generate_performance_report.py fir_only.csv
```

## Troubleshooting

### Missing Dependencies

```
ImportError: No module named 'pandas'
```

**Solution**: Install dependencies

```bash
pip install numpy pandas matplotlib seaborn
```

### PDF Export Fails

```
ImportError: No module named 'weasyprint'
```

**Solution**: PDF export is optional. Install if needed:

```bash
pip install weasyprint
```

Note: weasyprint requires additional system dependencies on some platforms.

### No Results Found

```
Error: No valid benchmark results found!
```

**Solution**: Check CSV format matches expected columns

```bash
head benchmark_results.csv
```

Should show header with: operation,param1,param2,scalar_cycles,neon_cycles,speedup

### Permission Denied

```
bash: ./generate_performance_report.py: Permission denied
```

**Solution**: Make script executable

```bash
chmod +x /home/user/leansdr/scripts/generate_performance_report.py
```

## Performance Tips

### For Large Benchmark Sets

- The tool loads entire CSV into memory
- Processing typically takes 5-10 seconds
- Chart generation is the slowest step (matplotlib rendering)
- PDF export adds 10-30 seconds depending on page count

### Batch Processing

Create a wrapper script for automated reporting:

```bash
#!/bin/bash
# run_daily_performance_report.sh

REPORT_DIR="reports/$(date +%Y%m%d)"
mkdir -p "$REPORT_DIR"

cd /home/user/leansdr/scripts

./run_benchmark_neon.sh

./generate_performance_report.py \
    ../test/benchmark_results/*.csv \
    --output "$REPORT_DIR/performance.html" \
    --pdf "$REPORT_DIR/performance.pdf"

echo "Report generated: $REPORT_DIR/performance.html"
```

## Code Architecture

### Key Classes

**BenchmarkResult**: Data structure for individual benchmark results

**SystemInfo**: Captures platform information

**BenchmarkParser**: Parses CSV files with error handling

**PerformanceAnalyzer**: Computes statistics and metrics

**ChartGenerator**: Creates matplotlib visualizations

**HTMLReportGenerator**: Assembles final HTML report

### Error Handling

- All file operations wrapped in try-except
- Graceful degradation for missing dependencies
- Clear logging at each step
- Validation of CSV format before processing

### Extensibility

Easy to extend with:

- Additional chart types (add method to ChartGenerator)
- New metrics (add method to PerformanceAnalyzer)
- Custom HTML templates (modify _build_html)
- Alternative export formats (add export functions)

## Development

### Running with Debug Logging

```bash
python3 generate_performance_report.py results.csv --verbose
```

### Testing with Sample Data

The tool works with any benchmark CSV. Create test data:

```bash
cat > sample_results.csv << 'EOF'
operation,param1,param2,scalar_cycles,neon_cycles,speedup
fir_filter,64,128,1000,250,4.0
fir_filter,128,256,2000,600,3.33
agc_power,0,256,500,150,3.33
complex_multiply,0,512,800,200,4.0
mpeg_sync,0,4096,5000,700,7.14
horizontal_sum,0,1024,1000,400,2.5
dot_product,0,1024,1000,300,3.33
viterbi_acs,64,0,2000,1000,2.0
EOF

./generate_performance_report.py sample_results.csv
```

## License

Part of LeanSDR project - GPL-3.0 License

## Support

For issues or feature requests:

- Check the troubleshooting section above
- Review benchmark CSV format
- Run with `--verbose` flag for detailed diagnostics
- Check matplotlib/pandas documentation for visualization issues

## See Also

- `/home/user/leansdr/test/run_benchmark_neon.sh` - Benchmark execution script
- `/home/user/leansdr/test/benchmark_neon.cc` - Benchmark implementations
- `/home/user/leansdr/NEON_README.md` - NEON optimization documentation
- `/home/user/leansdr/docs/optimization/` - Performance analysis guides
