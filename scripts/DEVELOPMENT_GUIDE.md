# Performance Report Generator - Development Guide

Guide for developers extending and maintaining the performance report generator.

## Architecture Overview

```
generate_performance_report.py
├── Data Structures
│   ├── BenchmarkResult (dataclass)
│   ├── SystemInfo (dataclass)
│   └── PerformanceMetrics (dataclass)
├── Parsing Layer
│   └── BenchmarkParser
├── Analysis Layer
│   └── PerformanceAnalyzer
├── Visualization Layer
│   └── ChartGenerator
├── Reporting Layer
│   └── HTMLReportGenerator
├── Export Layer
│   └── export_to_pdf()
└── Main Orchestration
    └── main()
```

## Code Structure

### Module Organization

**Lines 1-100**: Imports and setup
- Standard library imports
- Third-party library imports
- Logging configuration

**Lines 100-200**: Data Structures
- `@dataclass BenchmarkResult`
- `@dataclass SystemInfo`
- `@dataclass PerformanceMetrics`

**Lines 200-350**: BenchmarkParser
- CSV parsing with error recovery
- Metadata extraction

**Lines 350-500**: PerformanceAnalyzer
- Statistical calculations
- Grouping operations
- Efficiency metrics
- Recommendations generation
- Size categorization

**Lines 500-650**: ChartGenerator
- Individual chart methods using matplotlib
- Figure saving utilities
- Color schemes and styling

**Lines 650-1100**: HTMLReportGenerator
- HTML template generation
- CSS styling
- Insights generation

**Lines 1100-1150**: PDF Export
- weasyprint integration
- Graceful degradation

**Lines 1150-1300**: System Info & Main
- System information collection
- Argument parsing
- Orchestration logic

## Adding New Features

### Example 1: Add New Chart Type

To add a new chart (e.g., speedup stacked bar chart by category):

```python
def speedup_by_category(self) -> str:
    """Create speedup chart grouped by operation category"""
    fig, ax = plt.subplots(figsize=self.figsize)

    # Group results by operation
    grouped = self.analyzer.df.groupby('operation')['speedup'].mean()

    # Create stacked chart
    categories = grouped.index
    values = grouped.values
    colors = ['#2ecc71', '#3498db', '#f39c12', '#e74c3c'][:len(categories)]

    ax.barh(categories, values, color=colors)
    ax.set_xlabel('Average Speedup', fontweight='bold')
    ax.set_title('Speedup by Operation Category', fontweight='bold')
    ax.grid(axis='x', alpha=0.3)

    return self._save_figure(fig, 'speedup_by_category')
```

Then register in `HTMLReportGenerator._build_html()`:

```python
chart_paths = {
    'speedup_by_operation': chart_gen.speedup_by_operation(),
    'speedup_by_category': chart_gen.speedup_by_category(),  # Add this
    # ... other charts
}
```

And add to HTML template in `_build_html()`:

```html
<h3>Speedup by Category</h3>
<div class="chart-container">
    <img src="{self.chart_paths.get('speedup_by_category', '')}" alt="Speedup by Category">
</div>
```

### Example 2: Add New Metric

To calculate a new metric (e.g., power efficiency):

```python
def get_power_efficiency(self) -> Dict:
    """Calculate power efficiency metrics"""
    # Assume we have power measurements in a column
    if 'power_mw' not in self.df.columns:
        return {}

    efficiency = self.df.groupby('operation').apply(
        lambda x: (x['scalar_cycles'] - x['neon_cycles']) / x['power_mw']
    )

    return {
        'avg_efficiency': float(efficiency.mean()),
        'best_efficiency': float(efficiency.max()),
        'best_operation': str(efficiency.idxmax())
    }
```

Register in analysis:

```python
# In PerformanceAnalyzer
efficiency_metrics = self.get_power_efficiency()
```

And use in report:

```python
# In HTMLReportGenerator._build_html()
if efficiency_metrics:
    html += f"""
    <div class="metric-card good">
        <div class="label">Best Power Efficiency</div>
        <div class="value">{efficiency_metrics['best_operation']}</div>
    </div>
    """
```

### Example 3: Add New Export Format

To support a new export format (e.g., JSON):

```python
def export_to_json(html_file: str, json_file: str,
                   analyzer: PerformanceAnalyzer) -> bool:
    """Export analysis results to JSON"""
    try:
        import json

        data = {
            'summary': analyzer.get_summary_stats(),
            'operations': analyzer.get_operation_summary().to_dict(),
            'efficiency': analyzer.get_efficiency_metrics(),
            'recommendations': analyzer.get_recommendations(),
            'timestamp': datetime.now().isoformat()
        }

        with open(json_file, 'w') as f:
            json.dump(data, f, indent=2)

        logger.info(f"Generated JSON export: {json_file}")
        return True

    except Exception as e:
        logger.error(f"Failed to export JSON: {e}")
        return False
```

Add to main():

```python
# In main() after HTML generation
if args.json:
    export_to_json(args.output, args.json, analyzer)
```

And argument parser:

```python
parser.add_argument('--json', default=None,
                   help='Output JSON file (optional)')
```

## Testing

### Unit Testing

Create `test_performance_report.py`:

```python
import unittest
from generate_performance_report import (
    BenchmarkResult, BenchmarkParser, PerformanceAnalyzer
)

class TestBenchmarkParser(unittest.TestCase):
    def test_parse_valid_csv(self):
        """Test parsing valid CSV"""
        results, metadata = BenchmarkParser.parse_csv('test_data.csv')
        self.assertGreater(len(results), 0)
        self.assertIsNotNone(metadata['neon_available'])

    def test_parse_missing_file(self):
        """Test handling of missing file"""
        with self.assertRaises(FileNotFoundError):
            BenchmarkParser.parse_csv('nonexistent.csv')

class TestPerformanceAnalyzer(unittest.TestCase):
    def setUp(self):
        self.results = [
            BenchmarkResult('op1', 0, 100, 1000, 250, 4.0),
            BenchmarkResult('op1', 0, 200, 2000, 600, 3.33),
        ]
        self.analyzer = PerformanceAnalyzer(self.results)

    def test_summary_stats(self):
        """Test summary statistics calculation"""
        stats = self.analyzer.get_summary_stats()
        self.assertAlmostEqual(stats['avg_speedup'], 3.665, places=2)

    def test_operation_summary(self):
        """Test operation grouping"""
        summary = self.analyzer.get_operation_summary()
        self.assertEqual(len(summary), 1)

if __name__ == '__main__':
    unittest.main()
```

Run tests:

```bash
python3 -m pytest test_performance_report.py -v
```

### Integration Testing

Test with real benchmark data:

```bash
# Generate test report
./generate_performance_report.py sample_benchmark.csv \
    --output test_output.html

# Verify output exists
if [ -f test_output.html ] && [ -d test_output/charts ]; then
    echo "✓ Integration test passed"
else
    echo "✗ Integration test failed"
fi
```

## Performance Optimization

### Memory Efficiency

For large datasets (>50MB):

```python
# Use chunkwise processing instead of loading entire CSV
chunks = pd.read_csv(large_file, chunksize=10000)
all_results = []

for chunk in chunks:
    # Process chunk
    results = parse_chunk(chunk)
    all_results.extend(results)
```

### Chart Generation Speed

For many charts:

```python
# Use figure reuse
fig, axes = plt.subplots(2, 2, figsize=(16, 12))

# Plot multiple charts on one figure
axes[0, 0].hist(...)  # Chart 1
axes[0, 1].scatter(...)  # Chart 2
# etc.

fig.savefig('combined_charts.png')
```

## Error Handling

### Current Strategy

1. **File Operations**: Try-except with clear error messages
2. **CSV Parsing**: Skip invalid rows with warnings
3. **Dependencies**: Graceful degradation for optional features
4. **Calculation Errors**: Return defaults or skip calculations

### Adding Error Handling

```python
def my_function():
    """Do something with proper error handling"""
    try:
        # Attempt operation
        result = risky_operation()
        return result
    except ValueError as e:
        logger.error(f"Invalid value: {e}")
        return None  # or default value
    except Exception as e:
        logger.error(f"Unexpected error: {e}", exc_info=True)
        raise  # Re-raise for caller to handle
```

## Code Style

### Conventions

- **Naming**: snake_case for functions/variables, PascalCase for classes
- **Docstrings**: Google-style docstrings with types
- **Type Hints**: All function signatures include types
- **Line Length**: Max 100 characters for code, 80 for docstrings
- **Imports**: Standard library, third-party, local (with blank lines)

### Example:

```python
def calculate_metric(results: List[BenchmarkResult],
                    operation: str) -> Optional[float]:
    """
    Calculate metric for specific operation

    Args:
        results: List of benchmark results
        operation: Operation name to filter on

    Returns:
        Calculated metric value or None if not found

    Raises:
        ValueError: If operation not found in results
    """
    filtered = [r for r in results if r.operation == operation]
    if not filtered:
        raise ValueError(f"Operation '{operation}' not found")
    return np.mean([r.speedup for r in filtered])
```

## Documentation

### Adding Documentation

1. **Docstrings**: Add to all public functions
2. **Type Hints**: Include in function signatures
3. **Comments**: Explain "why", not "what"
4. **README**: Update PERFORMANCE_REPORT_README.md for user-facing changes

### Example:

```python
# Good: Explains the "why"
# Use log scale to show variation across large range of data sizes
ax.set_xscale('log')

# Bad: Just describes what code does
# Set x scale to log
ax.set_xscale('log')
```

## Debugging

### Enable Debug Logging

```bash
python3 -c "
import logging
logging.basicConfig(level=logging.DEBUG)
" python3 generate_performance_report.py results.csv --verbose
```

### Print Intermediate Results

```python
# Add debug output in PerformanceAnalyzer
print(f"DEBUG: Loaded {len(self.df)} results")
print(f"DEBUG: Columns: {self.df.columns.tolist()}")
print(f"DEBUG: Sample data:\n{self.df.head()}")
```

### Profile Execution

```python
import cProfile
import pstats

cProfile.run(
    'generate_performance_report(...)',
    'prof_results'
)

stats = pstats.Stats('prof_results')
stats.sort_stats('cumulative').print_stats(20)
```

## Version Compatibility

### Python Versions

Tested on Python 3.7+. Maintain compatibility:

```python
# Python 3.7+ compatible
from typing import Dict, List, Optional

# Use f-strings (3.6+)
result = f"Value: {value}"

# Use dataclasses (3.7+)
@dataclass
class MyClass:
    field: str
```

### Dependency Versions

Tested with:
- pandas >= 1.0.0
- numpy >= 1.18.0
- matplotlib >= 3.0.0
- seaborn >= 0.10.0

## Future Enhancements

### Planned Features

1. **Interactive Plots**: Plotly integration for web interactivity
2. **Real-time Monitoring**: WebSocket connection to live benchmarks
3. **Trend Analysis**: Historical performance tracking
4. **Comparison Engine**: Automated performance regression detection
5. **Custom Thresholds**: User-defined performance targets
6. **Export Formats**: XML, JSON, LaTeX table exports

### Contribution Guidelines

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Update documentation
5. Submit pull request with clear description

## Performance Testing Checklist

- [ ] All functions have docstrings
- [ ] Type hints on all public functions
- [ ] Error handling for all I/O operations
- [ ] Logging at appropriate levels
- [ ] Unit tests for new classes/functions
- [ ] Integration test with sample data
- [ ] Documentation updated
- [ ] No PEP8 violations (`pylint`)
- [ ] README examples work correctly
- [ ] PDF export tested (if supported)

## See Also

- [PERFORMANCE_REPORT_README.md](./PERFORMANCE_REPORT_README.md) - User documentation
- [USAGE_EXAMPLES.md](./USAGE_EXAMPLES.md) - Usage examples
- Python Matplotlib docs: https://matplotlib.org
- Pandas documentation: https://pandas.pydata.org
- Type hints: https://docs.python.org/3/library/typing.html
