# Quick Start Guide - Performance Report Generator

Get started in 2 minutes!

## Installation (1 minute)

```bash
# Install dependencies
pip install numpy pandas matplotlib seaborn

# Optional: For PDF support
pip install weasyprint

# Navigate to scripts
cd /home/user/leansdr/scripts
```

## Generate Your First Report (1 minute)

### From Existing Benchmark Results

```bash
./generate_performance_report.py your_benchmark_results.csv
```

This creates:
- `performance_report.html` - Open in browser
- `charts/` - Directory with 5 visualization PNG files

### From LeanSDR Benchmarks

```bash
# Run benchmarks
cd /home/user/leansdr/test
./run_benchmark_neon.sh

# Generate report
cd /home/user/leansdr/scripts
./generate_performance_report.py ../test/benchmark_results/*.csv --output report.html

# Open report
open report.html  # macOS
xdg-open report.html  # Linux
```

## Common Commands

```bash
# Basic report
./generate_performance_report.py results.csv

# With custom output
./generate_performance_report.py results.csv -o my_report.html

# With PDF export
./generate_performance_report.py results.csv --pdf report.pdf

# Multiple files (comparison)
./generate_performance_report.py scalar.csv neon.csv -o comparison.html

# Verbose (show details)
./generate_performance_report.py results.csv --verbose
```

## What You Get

### Interactive HTML Report
- Executive summary with key metrics
- 5 professional charts
- Detailed performance tables
- System information
- Optimization recommendations
- Key insights and analysis

### Charts Included
1. **Speedup by Operation** - Which operations benefit most
2. **Distribution** - Overall speedup spread
3. **Scalar vs NEON** - Direct cycle comparison
4. **Size Impact** - How data size affects performance
5. **Efficiency** - Overall efficiency percentage

## Expected Output Format

Your benchmark CSV needs these columns:
```
operation,param1,param2,scalar_cycles,neon_cycles,speedup
```

Example:
```csv
operation,param1,param2,scalar_cycles,neon_cycles,speedup
fir_filter,64,128,82534,21456,3.85
agc_power,0,256,1245,342,3.64
```

## Understanding the Report

### Color Coding
- **Green** = Excellent speedup (>3.0x)
- **Blue** = Good speedup (2.0-3.0x)
- **Orange** = Moderate speedup (1.5-2.0x)
- **Red** = Low speedup (<1.5x)

### Key Metrics
- **Average Speedup**: Overall performance improvement
- **Efficiency**: Percentage of cycles saved
- **Speedup Range**: Min to max observed

## Troubleshooting

### Script not found
```bash
chmod +x generate_performance_report.py
```

### Missing modules
```bash
pip install numpy pandas matplotlib seaborn
```

### CSV parsing error
Check your CSV has these columns:
```
operation,param1,param2,scalar_cycles,neon_cycles,speedup
```

### Can't open HTML report
- Try Firefox or Chrome
- Check file path is correct
- Ensure charts/ directory exists next to HTML

## Next Steps

1. **View Report**: Open HTML in web browser
2. **Review Charts**: Check visualization insights
3. **Read Recommendations**: See optimization suggestions
4. **Advanced Usage**: See `USAGE_EXAMPLES.md` for more

## Full Documentation

- **User Guide**: `PERFORMANCE_REPORT_README.md`
- **More Examples**: `USAGE_EXAMPLES.md`
- **Development**: `DEVELOPMENT_GUIDE.md`
- **Implementation**: `IMPLEMENTATION_SUMMARY.md`

## Help

```bash
./generate_performance_report.py --help
```

## All Flags

```
Positional Arguments:
  input                    Benchmark CSV file(s)

Options:
  -h, --help              Show help message
  -o, --output FILE       Output HTML file
  --pdf FILE              Export PDF
  --title TEXT            Report title
  -v, --verbose           Show details
```

## Tips

- Put multiple CSVs to compare them
- Use `--verbose` to see what's happening
- PDF export requires weasyprint library
- Works with any benchmark CSV format
- Charts are PNG, can be used elsewhere

---

Need more? See full documentation files in this directory.
