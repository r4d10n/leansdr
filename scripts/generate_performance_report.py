#!/usr/bin/env python3
"""
LeanSDR Performance Report Generator

Comprehensive performance analysis tool that:
1. Parses benchmark results from CSV files
2. Analyzes end-to-end test performance
3. Generates HTML reports with visualizations
4. Optionally exports to PDF

Author: LeanSDR Development
License: GPL-3.0
"""

import os
import sys
import json
import logging
import argparse
import platform
import subprocess
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
from datetime import datetime

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import matplotlib.pyplot as plt
from matplotlib.figure import Figure
import seaborn as sns

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


# ============================================================================
# DATA STRUCTURES
# ============================================================================

@dataclass
class BenchmarkResult:
    """Single benchmark result"""
    operation: str
    param1: int
    param2: int
    scalar_cycles: float
    neon_cycles: float
    speedup: float

    def to_dict(self) -> Dict:
        return asdict(self)


@dataclass
class SystemInfo:
    """System information"""
    hostname: str
    platform: str
    processor: str
    arch: str
    cpu_count: int
    python_version: str
    timestamp: str

    def to_dict(self) -> Dict:
        return asdict(self)


@dataclass
class PerformanceMetrics:
    """Aggregated performance metrics"""
    avg_speedup: float
    max_speedup: float
    min_speedup: float
    median_speedup: float
    operations_count: int
    neon_available: bool


# ============================================================================
# BENCHMARK PARSER
# ============================================================================

class BenchmarkParser:
    """Parse benchmark CSV results"""

    @staticmethod
    def parse_csv(filepath: str) -> Tuple[List[BenchmarkResult], Dict]:
        """
        Parse benchmark CSV file

        Expected format:
            operation,param1,param2,scalar_cycles,neon_cycles,speedup
        """
        results = []
        metadata = {}

        try:
            df = pd.read_csv(filepath, comment='#')
            logger.info(f"Loaded {len(df)} benchmark results from {filepath}")

            # Check for NEON availability
            metadata['neon_available'] = df['neon_cycles'].sum() > 0

            # Parse each row
            for _, row in df.iterrows():
                try:
                    result = BenchmarkResult(
                        operation=str(row['operation']).strip(),
                        param1=int(row['param1']),
                        param2=int(row['param2']),
                        scalar_cycles=float(row['scalar_cycles']),
                        neon_cycles=float(row['neon_cycles']),
                        speedup=float(row['speedup'])
                    )
                    results.append(result)
                except (ValueError, KeyError) as e:
                    logger.warning(f"Skipped invalid row: {e}")

            return results, metadata

        except FileNotFoundError:
            logger.error(f"File not found: {filepath}")
            raise
        except Exception as e:
            logger.error(f"Error parsing CSV: {e}")
            raise


# ============================================================================
# ANALYSIS ENGINE
# ============================================================================

class PerformanceAnalyzer:
    """Analyze performance metrics"""

    def __init__(self, results: List[BenchmarkResult]):
        self.results = results
        self.df = self._to_dataframe()

    def _to_dataframe(self) -> pd.DataFrame:
        """Convert results to DataFrame"""
        data = [r.to_dict() for r in self.results]
        return pd.DataFrame(data)

    def get_summary_stats(self) -> Dict:
        """Calculate summary statistics"""
        speedups = self.df['speedup'].values

        return {
            'total_benchmarks': len(self.results),
            'avg_speedup': float(np.mean(speedups)),
            'median_speedup': float(np.median(speedups)),
            'min_speedup': float(np.min(speedups)),
            'max_speedup': float(np.max(speedups)),
            'std_speedup': float(np.std(speedups)),
            'speedup_25': float(np.percentile(speedups, 25)),
            'speedup_75': float(np.percentile(speedups, 75)),
        }

    def get_operation_summary(self) -> pd.DataFrame:
        """Group results by operation"""
        grouped = self.df.groupby('operation').agg({
            'speedup': ['mean', 'min', 'max', 'count'],
            'scalar_cycles': 'mean',
            'neon_cycles': 'mean'
        }).round(2)

        grouped.columns = ['avg_speedup', 'min_speedup', 'max_speedup',
                          'count', 'avg_scalar_cycles', 'avg_neon_cycles']
        return grouped.sort_values('avg_speedup', ascending=False)

    def get_recommendations(self) -> List[Dict]:
        """Generate optimization recommendations"""
        recommendations = []
        op_summary = self.get_operation_summary()

        for op, row in op_summary.iterrows():
            speedup = row['avg_speedup']
            count = int(row['count'])
            scalar_cycles = row['avg_scalar_cycles']

            if speedup < 1.2:
                level = "LOW"
                suggestion = f"{op}: Minimal speedup ({speedup:.2f}x). Consider alternative optimizations or if memory-bound."
            elif speedup < 2.0:
                level = "MODERATE"
                suggestion = f"{op}: Moderate speedup ({speedup:.2f}x). Overhead present, but still beneficial for high-frequency code."
            elif speedup < 3.0:
                level = "GOOD"
                suggestion = f"{op}: Good speedup ({speedup:.2f}x). Well-optimized, solid ROI."
            else:
                level = "EXCELLENT"
                suggestion = f"{op}: Excellent speedup ({speedup:.2f}x). Best practices applied, approaching theoretical limits."

            # Estimate throughput improvement
            throughput_gain = (speedup - 1.0) / speedup * 100

            recommendations.append({
                'operation': op,
                'speedup': speedup,
                'level': level,
                'suggestion': suggestion,
                'throughput_gain_percent': throughput_gain,
                'avg_cycles_saved': scalar_cycles - row['avg_neon_cycles'],
                'test_count': count
            })

        return sorted(recommendations,
                     key=lambda x: x['throughput_gain_percent'],
                     reverse=True)

    def get_efficiency_metrics(self) -> Dict:
        """Calculate efficiency metrics"""
        total_scalar_cycles = self.df['scalar_cycles'].sum()
        total_neon_cycles = self.df['neon_cycles'].sum()

        return {
            'total_scalar_cycles': int(total_scalar_cycles),
            'total_neon_cycles': int(total_neon_cycles),
            'total_cycles_saved': int(total_scalar_cycles - total_neon_cycles),
            'overall_speedup': float(total_scalar_cycles / total_neon_cycles)
                             if total_neon_cycles > 0 else 1.0,
            'efficiency': float((total_scalar_cycles - total_neon_cycles) / total_scalar_cycles * 100)
        }

    def categorize_by_size(self) -> Dict:
        """Categorize benchmarks by data size"""
        categories = {
            'small': (0, 1024),
            'medium': (1024, 8192),
            'large': (8192, float('inf'))
        }

        result = {}
        for cat_name, (min_size, max_size) in categories.items():
            mask = (self.df['param2'] >= min_size) & (self.df['param2'] < max_size)
            cat_data = self.df[mask]
            if len(cat_data) > 0:
                result[cat_name] = {
                    'count': len(cat_data),
                    'avg_speedup': float(cat_data['speedup'].mean()),
                    'samples': int(cat_data['param2'].sum())
                }

        return result


# ============================================================================
# VISUALIZATION GENERATION
# ============================================================================

class ChartGenerator:
    """Generate matplotlib charts"""

    def __init__(self, analyzer: PerformanceAnalyzer, output_dir: str):
        self.analyzer = analyzer
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.figsize = (12, 6)
        sns.set_style("whitegrid")

    def _save_figure(self, fig: Figure, name: str) -> str:
        """Save figure and return relative path"""
        filepath = self.output_dir / f"{name}.png"
        fig.savefig(filepath, dpi=100, bbox_inches='tight')
        plt.close(fig)
        logger.info(f"Generated chart: {name}")
        return f"charts/{name}.png"

    def speedup_by_operation(self) -> str:
        """Create speedup comparison chart"""
        op_summary = self.analyzer.get_operation_summary()

        fig, ax = plt.subplots(figsize=self.figsize)
        colors = ['#2ecc71' if x > 2.0 else '#f39c12' if x > 1.5 else '#e74c3c'
                 for x in op_summary['avg_speedup']]

        op_summary['avg_speedup'].plot(kind='barh', ax=ax, color=colors)
        ax.axvline(x=1.0, color='red', linestyle='--', linewidth=2, label='Baseline')
        ax.set_xlabel('Speedup (scalar / NEON)', fontsize=12, fontweight='bold')
        ax.set_ylabel('Operation', fontsize=12, fontweight='bold')
        ax.set_title('Performance Speedup by Operation', fontsize=14, fontweight='bold')
        ax.legend()
        ax.grid(axis='x', alpha=0.3)

        # Add value labels
        for i, v in enumerate(op_summary['avg_speedup']):
            ax.text(v + 0.1, i, f'{v:.2f}x', va='center', fontweight='bold')

        return self._save_figure(fig, 'speedup_by_operation')

    def speedup_distribution(self) -> str:
        """Create speedup distribution histogram"""
        fig, ax = plt.subplots(figsize=self.figsize)

        speedups = self.analyzer.df['speedup'].values
        ax.hist(speedups, bins=20, color='#3498db', edgecolor='black', alpha=0.7)
        ax.axvline(x=np.mean(speedups), color='red', linestyle='--',
                  linewidth=2, label=f'Mean: {np.mean(speedups):.2f}x')
        ax.axvline(x=np.median(speedups), color='green', linestyle='--',
                  linewidth=2, label=f'Median: {np.median(speedups):.2f}x')

        ax.set_xlabel('Speedup', fontsize=12, fontweight='bold')
        ax.set_ylabel('Frequency', fontsize=12, fontweight='bold')
        ax.set_title('Distribution of Performance Speedups', fontsize=14, fontweight='bold')
        ax.legend()
        ax.grid(axis='y', alpha=0.3)

        return self._save_figure(fig, 'speedup_distribution')

    def cycles_comparison(self) -> str:
        """Create scalar vs NEON cycles comparison"""
        fig, ax = plt.subplots(figsize=self.figsize)

        operations = self.analyzer.df['operation'].unique()[:10]  # Top 10
        mask = self.analyzer.df['operation'].isin(operations)
        op_data = self.analyzer.df[mask].groupby('operation')[['scalar_cycles', 'neon_cycles']].mean()

        x = np.arange(len(op_data))
        width = 0.35

        ax.bar(x - width/2, op_data['scalar_cycles'], width, label='Scalar', color='#e74c3c')
        ax.bar(x + width/2, op_data['neon_cycles'], width, label='NEON', color='#2ecc71')

        ax.set_xlabel('Operation', fontsize=12, fontweight='bold')
        ax.set_ylabel('CPU Cycles', fontsize=12, fontweight='bold')
        ax.set_title('Scalar vs NEON Execution Cycles', fontsize=14, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(op_data.index, rotation=45, ha='right')
        ax.legend()
        ax.grid(axis='y', alpha=0.3)

        return self._save_figure(fig, 'cycles_comparison')

    def speedup_by_size(self) -> str:
        """Create speedup vs data size scatter plot"""
        fig, ax = plt.subplots(figsize=self.figsize)

        df = self.analyzer.df
        colors_map = {'fir_filter': '#e74c3c', 'agc_power': '#3498db',
                     'complex_multiply': '#2ecc71', 'mpeg_sync': '#f39c12',
                     'horizontal_sum': '#9b59b6', 'dot_product': '#1abc9c',
                     'viterbi_acs': '#34495e'}

        for op in df['operation'].unique():
            op_data = df[df['operation'] == op]
            color = colors_map.get(op, '#95a5a6')
            ax.scatter(op_data['param2'], op_data['speedup'],
                      label=op, s=100, alpha=0.6, color=color)

        ax.axhline(y=1.0, color='red', linestyle='--', linewidth=2, alpha=0.5)
        ax.set_xlabel('Data Size (samples)', fontsize=12, fontweight='bold')
        ax.set_ylabel('Speedup', fontsize=12, fontweight='bold')
        ax.set_title('Performance Speedup vs Data Size', fontsize=14, fontweight='bold')
        ax.set_xscale('log')
        ax.legend(loc='best', fontsize=9)
        ax.grid(True, alpha=0.3)

        return self._save_figure(fig, 'speedup_by_size')

    def efficiency_gauge(self) -> str:
        """Create efficiency gauge chart"""
        metrics = self.analyzer.get_efficiency_metrics()
        efficiency = metrics['efficiency']

        fig, ax = plt.subplots(figsize=(8, 6), subplot_kw=dict(projection='polar'))

        theta = np.linspace(0, 2*np.pi, 100)
        efficiency_norm = efficiency / 100.0
        # Create array of efficiency values matching theta length
        r = np.full_like(theta, efficiency_norm)

        ax.fill_between(theta, 0, r, alpha=0.25, color='#2ecc71')
        ax.plot(theta, r, color='#27ae60', linewidth=2)
        ax.set_ylim(0, 1)
        ax.set_title(f'Overall Efficiency: {efficiency:.1f}%',
                    fontsize=14, fontweight='bold', pad=20)

        return self._save_figure(fig, 'efficiency_gauge')


# ============================================================================
# HTML REPORT GENERATOR
# ============================================================================

class HTMLReportGenerator:
    """Generate comprehensive HTML report"""

    def __init__(self, analyzer: PerformanceAnalyzer, system_info: SystemInfo,
                 chart_paths: Dict[str, str]):
        self.analyzer = analyzer
        self.system_info = system_info
        self.chart_paths = chart_paths

    def generate_html(self, output_file: str) -> None:
        """Generate complete HTML report"""
        html_content = self._build_html()

        with open(output_file, 'w') as f:
            f.write(html_content)

        logger.info(f"Generated HTML report: {output_file}")

    def _build_html(self) -> str:
        """Build HTML content"""
        summary_stats = self.analyzer.get_summary_stats()
        op_summary = self.analyzer.get_operation_summary()
        efficiency = self.analyzer.get_efficiency_metrics()
        recommendations = self.analyzer.get_recommendations()
        size_categories = self.analyzer.categorize_by_size()

        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        html = f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LeanSDR Performance Report</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}

        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
            color: #333;
            padding: 20px;
            line-height: 1.6;
        }}

        .container {{
            max-width: 1400px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.1);
            overflow: hidden;
        }}

        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 40px;
            text-align: center;
        }}

        .header h1 {{
            font-size: 2.5em;
            margin-bottom: 10px;
        }}

        .header p {{
            font-size: 1.1em;
            opacity: 0.9;
        }}

        .content {{
            padding: 40px;
        }}

        .section {{
            margin-bottom: 50px;
        }}

        .section h2 {{
            color: #667eea;
            font-size: 1.8em;
            margin-bottom: 20px;
            border-bottom: 3px solid #667eea;
            padding-bottom: 10px;
        }}

        .section h3 {{
            color: #764ba2;
            font-size: 1.3em;
            margin-top: 20px;
            margin-bottom: 15px;
        }}

        .metrics-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }}

        .metric-card {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 8px;
            text-align: center;
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
        }}

        .metric-card .label {{
            font-size: 0.9em;
            opacity: 0.9;
            margin-bottom: 10px;
        }}

        .metric-card .value {{
            font-size: 2em;
            font-weight: bold;
        }}

        .metric-card.good {{
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
        }}

        .metric-card.warning {{
            background: linear-gradient(135deg, #f39c12 0%, #e74c3c 100%);
        }}

        .chart-container {{
            margin: 30px 0;
            text-align: center;
        }}

        .chart-container img {{
            max-width: 100%;
            height: auto;
            border-radius: 8px;
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.1);
        }}

        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
            border-radius: 8px;
            overflow: hidden;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
        }}

        table thead {{
            background: #667eea;
            color: white;
        }}

        table th {{
            padding: 15px;
            text-align: left;
            font-weight: 600;
        }}

        table td {{
            padding: 12px 15px;
            border-bottom: 1px solid #ecf0f1;
        }}

        table tbody tr:hover {{
            background: #f8f9fa;
        }}

        table tbody tr:nth-child(odd) {{
            background: #f8f9fa;
        }}

        .speedup-badge {{
            display: inline-block;
            padding: 5px 10px;
            border-radius: 5px;
            font-weight: bold;
            font-size: 0.9em;
        }}

        .speedup-excellent {{
            background: #2ecc71;
            color: white;
        }}

        .speedup-good {{
            background: #3498db;
            color: white;
        }}

        .speedup-moderate {{
            background: #f39c12;
            color: white;
        }}

        .speedup-low {{
            background: #e74c3c;
            color: white;
        }}

        .system-info {{
            background: #f8f9fa;
            padding: 20px;
            border-radius: 8px;
            margin: 20px 0;
        }}

        .system-info-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }}

        .system-info-item {{
            padding: 10px;
        }}

        .system-info-item label {{
            font-weight: bold;
            color: #667eea;
            display: block;
            margin-bottom: 5px;
        }}

        .recommendation {{
            background: #ecf0f1;
            border-left: 4px solid #667eea;
            padding: 15px;
            margin: 10px 0;
            border-radius: 4px;
        }}

        .recommendation.high {{
            border-left-color: #2ecc71;
            background: #d5f4e6;
        }}

        .recommendation.medium {{
            border-left-color: #f39c12;
            background: #fef5e7;
        }}

        .recommendation.low {{
            border-left-color: #e74c3c;
            background: #fadbd8;
        }}

        .footer {{
            background: #f8f9fa;
            padding: 20px;
            text-align: center;
            color: #7f8c8d;
            font-size: 0.9em;
            border-top: 1px solid #ecf0f1;
        }}

        .progress-bar {{
            width: 100%;
            height: 30px;
            background: #ecf0f1;
            border-radius: 5px;
            overflow: hidden;
            margin: 10px 0;
        }}

        .progress-fill {{
            height: 100%;
            background: linear-gradient(90deg, #2ecc71 0%, #27ae60 100%);
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
            font-size: 0.9em;
        }}

        .print-note {{
            display: none;
        }}

        @media print {{
            body {{
                background: white;
                padding: 0;
            }}

            .chart-container {{
                page-break-inside: avoid;
                margin: 20px 0;
            }}

            table {{
                page-break-inside: avoid;
            }}

            .print-note {{
                display: block;
                color: #666;
                font-size: 0.85em;
                margin: 20px 0;
            }}
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>LeanSDR Performance Report</h1>
            <p>NEON SIMD Optimization Analysis & Recommendations</p>
        </div>

        <div class="content">
            <!-- Executive Summary -->
            <section class="section">
                <h2>Executive Summary</h2>
                <p>This report analyzes the performance impact of ARM NEON SIMD optimizations in LeanSDR, comparing scalar (non-vectorized) implementations against optimized NEON code.</p>

                <div class="metrics-grid">
                    <div class="metric-card good">
                        <div class="label">Average Speedup</div>
                        <div class="value">{summary_stats['avg_speedup']:.2f}x</div>
                    </div>
                    <div class="metric-card good">
                        <div class="label">Median Speedup</div>
                        <div class="value">{summary_stats['median_speedup']:.2f}x</div>
                    </div>
                    <div class="metric-card good">
                        <div class="label">Maximum Speedup</div>
                        <div class="value">{summary_stats['max_speedup']:.2f}x</div>
                    </div>
                    <div class="metric-card">
                        <div class="label">Total Benchmarks</div>
                        <div class="value">{summary_stats['total_benchmarks']}</div>
                    </div>
                </div>

                <div class="metrics-grid">
                    <div class="metric-card good">
                        <div class="label">Overall Efficiency</div>
                        <div class="value">{efficiency['efficiency']:.1f}%</div>
                    </div>
                    <div class="metric-card good">
                        <div class="label">Overall Speedup</div>
                        <div class="value">{efficiency['overall_speedup']:.2f}x</div>
                    </div>
                    <div class="metric-card good">
                        <div class="label">Cycles Saved</div>
                        <div class="value">{efficiency['total_cycles_saved']:,}</div>
                    </div>
                </div>
            </section>

            <!-- System Information -->
            <section class="section">
                <h2>System Information</h2>
                <div class="system-info">
                    <div class="system-info-grid">
                        <div class="system-info-item">
                            <label>Hostname</label>
                            <span>{self.system_info.hostname}</span>
                        </div>
                        <div class="system-info-item">
                            <label>Platform</label>
                            <span>{self.system_info.platform}</span>
                        </div>
                        <div class="system-info-item">
                            <label>Architecture</label>
                            <span>{self.system_info.arch}</span>
                        </div>
                        <div class="system-info-item">
                            <label>CPU Cores</label>
                            <span>{self.system_info.cpu_count}</span>
                        </div>
                        <div class="system-info-item">
                            <label>Processor</label>
                            <span>{self.system_info.processor}</span>
                        </div>
                        <div class="system-info-item">
                            <label>Timestamp</label>
                            <span>{self.system_info.timestamp}</span>
                        </div>
                    </div>
                </div>
            </section>

            <!-- Performance Visualizations -->
            <section class="section">
                <h2>Performance Analysis</h2>

                <h3>Speedup by Operation</h3>
                <div class="chart-container">
                    <img src="{self.chart_paths.get('speedup_by_operation', '')}" alt="Speedup by Operation">
                </div>

                <h3>Speedup Distribution</h3>
                <div class="chart-container">
                    <img src="{self.chart_paths.get('speedup_distribution', '')}" alt="Speedup Distribution">
                </div>

                <h3>Cycles Comparison (Scalar vs NEON)</h3>
                <div class="chart-container">
                    <img src="{self.chart_paths.get('cycles_comparison', '')}" alt="Cycles Comparison">
                </div>

                <h3>Performance vs Data Size</h3>
                <div class="chart-container">
                    <img src="{self.chart_paths.get('speedup_by_size', '')}" alt="Speedup by Size">
                </div>

                <h3>Overall Efficiency</h3>
                <div class="chart-container">
                    <img src="{self.chart_paths.get('efficiency_gauge', '')}" alt="Efficiency Gauge">
                </div>
            </section>

            <!-- Detailed Results -->
            <section class="section">
                <h2>Detailed Results by Operation</h2>
                <p>Summary statistics for each benchmarked operation:</p>
                <table>
                    <thead>
                        <tr>
                            <th>Operation</th>
                            <th>Avg Speedup</th>
                            <th>Min Speedup</th>
                            <th>Max Speedup</th>
                            <th>Test Count</th>
                            <th>Avg Scalar Cycles</th>
                            <th>Avg NEON Cycles</th>
                        </tr>
                    </thead>
                    <tbody>
"""

        for op, row in op_summary.iterrows():
            speedup = row['avg_speedup']
            if speedup >= 3.0:
                badge_class = 'speedup-excellent'
            elif speedup >= 2.0:
                badge_class = 'speedup-good'
            elif speedup >= 1.5:
                badge_class = 'speedup-moderate'
            else:
                badge_class = 'speedup-low'

            html += f"""
                        <tr>
                            <td><strong>{op}</strong></td>
                            <td><span class="speedup-badge {badge_class}">{speedup:.2f}x</span></td>
                            <td>{row['min_speedup']:.2f}x</td>
                            <td>{row['max_speedup']:.2f}x</td>
                            <td>{int(row['count'])}</td>
                            <td>{row['avg_scalar_cycles']:,.0f}</td>
                            <td>{row['avg_neon_cycles']:,.0f}</td>
                        </tr>
"""

        html += """
                    </tbody>
                </table>
            </section>

            <!-- Performance by Data Size -->
            <section class="section">
                <h2>Performance by Data Size</h2>
                <table>
                    <thead>
                        <tr>
                            <th>Category</th>
                            <th>Data Size Range</th>
                            <th>Test Count</th>
                            <th>Average Speedup</th>
                        </tr>
                    </thead>
                    <tbody>
"""

        size_ranges = {
            'small': '< 1 KB',
            'medium': '1 KB - 8 KB',
            'large': '> 8 KB'
        }

        for cat, range_str in size_ranges.items():
            if cat in size_categories:
                data = size_categories[cat]
                html += f"""
                        <tr>
                            <td><strong>{cat.capitalize()}</strong></td>
                            <td>{range_str}</td>
                            <td>{data['count']}</td>
                            <td><span class="speedup-badge speedup-good">{data['avg_speedup']:.2f}x</span></td>
                        </tr>
"""

        html += """
                    </tbody>
                </table>
            </section>

            <!-- Recommendations -->
            <section class="section">
                <h2>Optimization Recommendations</h2>
                <p>Based on speedup analysis and ROI assessment:</p>
"""

        for rec in recommendations:
            level_class = 'high' if rec['level'] == 'EXCELLENT' else 'medium' if rec['level'] in ['GOOD', 'MODERATE'] else 'low'
            html += f"""
                <div class="recommendation {level_class}">
                    <strong>[{rec['level']}] {rec['operation']}</strong><br>
                    <em>Speedup: {rec['speedup']:.2f}x | Throughput Gain: {rec['throughput_gain_percent']:.1f}%</em><br>
                    {rec['suggestion']}
                </div>
"""

        html += f"""
            </section>

            <!-- Key Insights -->
            <section class="section">
                <h2>Key Insights</h2>
                <ul style="margin-left: 20px; line-height: 1.8;">
"""

        insights = self._generate_insights(summary_stats, recommendations, size_categories)
        for insight in insights:
            html += f"                    <li>{insight}</li>\n"

        html += f"""
                </ul>
            </section>

            <!-- Footer -->
            <div class="footer">
                <p>Report generated on {timestamp}</p>
                <p>LeanSDR Performance Analysis Suite</p>
                <p><small>For more information, visit: https://github.com/f4exb/leansdr</small></p>
            </div>
        </div>
    </div>
</body>
</html>
"""
        return html

    def _generate_insights(self, stats: Dict, recommendations: List[Dict],
                          size_categories: Dict) -> List[str]:
        """Generate key insights from analysis"""
        insights = []

        # Overall performance insight
        avg_speedup = stats['avg_speedup']
        if avg_speedup > 3.0:
            insights.append(
                f"Excellent overall NEON adoption with an average speedup of {avg_speedup:.2f}x. "
                "NEON optimizations are delivering significant performance improvements."
            )
        elif avg_speedup > 2.0:
            insights.append(
                f"Strong NEON performance with {avg_speedup:.2f}x average speedup. "
                "Most critical operations are well-optimized."
            )
        else:
            insights.append(
                f"Moderate NEON speedup of {avg_speedup:.2f}x. "
                "Additional optimization opportunities may exist."
            )

        # Consistency insight
        std = stats['std_speedup']
        if std < 0.5:
            insights.append(
                "Performance is consistent across operations, indicating stable optimization patterns."
            )
        else:
            insights.append(
                "Performance varies across operations. Consider analyzing underperforming "
                "operations for additional optimization opportunities."
            )

        # Data size impact
        if 'large' in size_categories and 'small' in size_categories:
            small_speedup = size_categories['small']['avg_speedup']
            large_speedup = size_categories['large']['avg_speedup']
            diff = small_speedup - large_speedup

            if abs(diff) > 0.3:
                insights.append(
                    f"Data size significantly impacts performance. Small datasets achieve {small_speedup:.2f}x speedup, "
                    f"while large datasets achieve {large_speedup:.2f}x. "
                    f"Consider memory access patterns and cache utilization."
                )

        # Best opportunities
        if recommendations:
            best_ops = [r for r in recommendations if r['level'] == 'EXCELLENT']
            if best_ops:
                top_op = best_ops[0]
                insights.append(
                    f"Highest ROI optimization: {top_op['operation']} with {top_op['speedup']:.2f}x speedup "
                    f"({top_op['throughput_gain_percent']:.1f}% throughput improvement). "
                    f"This operation demonstrates best-practice NEON implementation."
                )

        # Additional recommendation
        insights.append(
            "For maximum performance, prioritize code paths that execute frequently with NEON-optimized "
            "operations showing >2.0x speedup. Monitor power consumption as SIMD operations may increase "
            "CPU utilization."
        )

        return insights


# ============================================================================
# PDF EXPORT (Optional)
# ============================================================================

def export_to_pdf(html_file: str, pdf_file: str) -> bool:
    """
    Export HTML report to PDF

    Requires: weasyprint library
    """
    try:
        from weasyprint import HTML

        HTML(html_file).write_pdf(pdf_file)
        logger.info(f"Generated PDF report: {pdf_file}")
        return True

    except ImportError:
        logger.warning(
            "weasyprint not installed. Install with: pip install weasyprint\n"
            "Skipping PDF generation."
        )
        return False
    except Exception as e:
        logger.error(f"Failed to generate PDF: {e}")
        return False


# ============================================================================
# SYSTEM INFO COLLECTION
# ============================================================================

def collect_system_info() -> SystemInfo:
    """Collect system information"""
    try:
        hostname = platform.node()
        system = platform.system()
        release = platform.release()
        machine = platform.machine()
        processor = platform.processor() or "Unknown"
        cpu_count = os.cpu_count() or 1
        python_version = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        return SystemInfo(
            hostname=hostname,
            platform=f"{system} {release}",
            processor=processor,
            arch=machine,
            cpu_count=cpu_count,
            python_version=python_version,
            timestamp=timestamp
        )
    except Exception as e:
        logger.error(f"Error collecting system info: {e}")
        return SystemInfo(
            hostname="Unknown",
            platform="Unknown",
            processor="Unknown",
            arch="Unknown",
            cpu_count=1,
            python_version=sys.version.split()[0],
            timestamp=datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        )


# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Generate performance reports from LeanSDR benchmarks',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate report from benchmark CSV
  %(prog)s benchmark_results_scalar.csv

  # Generate report with NEON comparison
  %(prog)s benchmark_results_scalar.csv benchmark_results_neon.csv

  # Custom output location
  %(prog)s benchmark_results.csv --output reports/performance.html

  # Generate PDF as well
  %(prog)s benchmark_results.csv --output report.html --pdf report.pdf
"""
    )

    parser.add_argument('input', nargs='+',
                       help='Input benchmark CSV file(s)')
    parser.add_argument('-o', '--output', default='performance_report.html',
                       help='Output HTML file (default: performance_report.html)')
    parser.add_argument('--pdf', default=None,
                       help='Output PDF file (optional, requires weasyprint)')
    parser.add_argument('--title', default='Performance Analysis Report',
                       help='Report title')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Verbose logging')

    args = parser.parse_args()

    if args.verbose:
        logger.setLevel(logging.DEBUG)

    try:
        # Validate input files
        input_files = []
        for input_arg in args.input:
            if not os.path.isfile(input_arg):
                logger.error(f"Input file not found: {input_arg}")
                sys.exit(1)
            input_files.append(input_arg)

        logger.info(f"Processing {len(input_files)} input file(s)...")

        # Parse all benchmark files
        all_results = []
        for input_file in input_files:
            logger.info(f"Parsing: {input_file}")
            results, metadata = BenchmarkParser.parse_csv(input_file)
            all_results.extend(results)

        if not all_results:
            logger.error("No valid benchmark results found!")
            sys.exit(1)

        logger.info(f"Loaded {len(all_results)} total benchmark results")

        # Create output directory
        output_dir = Path(args.output).parent
        output_dir.mkdir(parents=True, exist_ok=True)

        # Create charts directory
        charts_dir = output_dir / 'charts'
        charts_dir.mkdir(parents=True, exist_ok=True)

        # Analyze results
        logger.info("Analyzing performance metrics...")
        analyzer = PerformanceAnalyzer(all_results)

        # Generate charts
        logger.info("Generating charts...")
        chart_gen = ChartGenerator(analyzer, str(charts_dir))
        chart_paths = {
            'speedup_by_operation': chart_gen.speedup_by_operation(),
            'speedup_distribution': chart_gen.speedup_distribution(),
            'cycles_comparison': chart_gen.cycles_comparison(),
            'speedup_by_size': chart_gen.speedup_by_size(),
            'efficiency_gauge': chart_gen.efficiency_gauge(),
        }

        # Collect system info
        logger.info("Collecting system information...")
        system_info = collect_system_info()

        # Generate HTML report
        logger.info("Generating HTML report...")
        html_gen = HTMLReportGenerator(analyzer, system_info, chart_paths)
        html_gen.generate_html(args.output)

        logger.info(f"✓ Report successfully generated: {args.output}")

        # Generate PDF if requested
        if args.pdf:
            logger.info("Generating PDF report...")
            if export_to_pdf(args.output, args.pdf):
                logger.info(f"✓ PDF report successfully generated: {args.pdf}")
            else:
                logger.warning("PDF generation skipped or failed")

        logger.info("Report generation complete!")

    except KeyboardInterrupt:
        logger.info("Interrupted by user")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Fatal error: {e}", exc_info=True)
        sys.exit(1)


if __name__ == '__main__':
    main()
