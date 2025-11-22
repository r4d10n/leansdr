#!/bin/bash
#
# Script to build and run NEON benchmarks
# Compares scalar vs NEON performance
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RESULTS_DIR="benchmark_results"
mkdir -p "$RESULTS_DIR"

# Detect architecture
ARCH=$(uname -m)
echo "=== LeanSDR NEON Benchmark Suite ==="
echo "Architecture: $ARCH"
echo "Date: $(date)"
echo ""

# Determine if we're on ARM
IS_ARM=0
if [[ "$ARCH" =~ ^arm || "$ARCH" == "aarch64" ]]; then
    IS_ARM=1
    echo "ARM architecture detected - will build both scalar and NEON versions"
else
    echo "Non-ARM architecture detected - will build scalar version only"
fi

# Get git commit info
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_PREFIX="${RESULTS_DIR}/benchmark_${GIT_COMMIT}_${TIMESTAMP}"

# ============================================================================
# Build scalar version
# ============================================================================

echo ""
echo "=== Building scalar version ==="
g++ -O3 -I../src \
    -Wall -Wno-sign-compare \
    benchmark_neon.cc -o benchmark_neon_scalar
echo "✓ Scalar build complete"

# ============================================================================
# Build NEON version (ARM only)
# ============================================================================

if [ $IS_ARM -eq 1 ]; then
    echo ""
    echo "=== Building NEON version ==="

    # Detect specific ARM variant
    if [ "$ARCH" == "aarch64" ]; then
        # ARMv8 64-bit
        NEON_FLAGS="-march=armv8-a -O3 -ftree-vectorize"
    elif [ "$ARCH" == "armv7l" ] || [ "$ARCH" == "armv7" ]; then
        # ARMv7 32-bit with NEON
        NEON_FLAGS="-march=armv7-a -mfpu=neon -O3 -ftree-vectorize"
    else
        # Generic ARM with NEON
        NEON_FLAGS="-march=armv7-a -mfpu=neon -O3 -ftree-vectorize"
    fi

    echo "NEON flags: $NEON_FLAGS"

    g++ $NEON_FLAGS -I../src \
        -Wall -Wno-sign-compare \
        benchmark_neon.cc -o benchmark_neon_neon
    echo "✓ NEON build complete"
fi

# ============================================================================
# Run benchmarks
# ============================================================================

echo ""
echo "=== Running scalar benchmark ==="
./benchmark_neon_scalar > "${RESULT_PREFIX}_scalar.csv" 2>&1
echo "✓ Scalar benchmark complete"
echo "  Results: ${RESULT_PREFIX}_scalar.csv"

if [ $IS_ARM -eq 1 ]; then
    echo ""
    echo "=== Running NEON benchmark ==="
    ./benchmark_neon_neon > "${RESULT_PREFIX}_neon.csv" 2>&1
    echo "✓ NEON benchmark complete"
    echo "  Results: ${RESULT_PREFIX}_neon.csv"

    # ========================================================================
    # Generate comparison report
    # ========================================================================

    echo ""
    echo "=== Generating comparison report ==="

    REPORT="${RESULT_PREFIX}_report.txt"

    cat > "$REPORT" <<EOF
================================================================================
LeanSDR NEON Benchmark Report
================================================================================

Date:         $(date)
Git Commit:   $GIT_COMMIT
Architecture: $ARCH
CPU Info:     $(grep "model name\|Hardware\|CPU part" /proc/cpuinfo | head -3)

================================================================================
BENCHMARK RESULTS
================================================================================

EOF

    # Extract and compare results
    echo "Operation                    Param1  Param2  Scalar_Cyc  NEON_Cyc    Speedup" >> "$REPORT"
    echo "---------------------------- ------- ------- ----------- ----------- --------" >> "$REPORT"

    # Skip header lines and process data
    tail -n +5 "${RESULT_PREFIX}_neon.csv" | while IFS=',' read -r op p1 p2 sc nc sp; do
        printf "%-28s %7s %7s %11.0f %11.0f %8.2fx\n" "$op" "$p1" "$p2" "$sc" "$nc" "$sp" >> "$REPORT"
    done

    cat >> "$REPORT" <<EOF

================================================================================
SUMMARY
================================================================================

Average speedup by operation type:
EOF

    # Calculate average speedup for each operation type
    for op in fir_filter agc_power complex_multiply mpeg_sync horizontal_sum dot_product viterbi_acs; do
        avg_speedup=$(tail -n +5 "${RESULT_PREFIX}_neon.csv" | grep "^$op," | \
                      awk -F',' '{sum+=$6; count++} END {if(count>0) printf "%.2f", sum/count; else print "N/A"}')
        if [ "$avg_speedup" != "N/A" ]; then
            printf "  %-28s %6sx\n" "$op:" "$avg_speedup" >> "$REPORT"
        fi
    done

    cat >> "$REPORT" <<EOF

Overall average speedup:
EOF

    overall_avg=$(tail -n +5 "${RESULT_PREFIX}_neon.csv" | \
                  awk -F',' '{sum+=$6; count++} END {printf "%.2fx", sum/count}')
    echo "  $overall_avg" >> "$REPORT"

    cat >> "$REPORT" <<EOF

================================================================================
INTERPRETATION
================================================================================

Speedup >4x:  Excellent NEON optimization
Speedup 2-4x: Good NEON optimization
Speedup 1.5-2x: Moderate NEON benefit
Speedup <1.5x: Limited NEON benefit (overhead, memory bandwidth, etc.)

Notes:
- Horizontal operations (sum, dot product) have lower speedup due to reduction overhead
- Memory-bound operations may show lower speedup with large data sizes
- Viterbi ACS is limited by data dependencies and branch prediction

================================================================================
EOF

    echo "✓ Report generated: $REPORT"

    # Display report
    echo ""
    cat "$REPORT"

else
    echo ""
    echo "Note: NEON benchmarks not run (non-ARM architecture)"
fi

# ============================================================================
# Generate CSV for plotting
# ============================================================================

if [ $IS_ARM -eq 1 ]; then
    echo ""
    echo "=== Generating plot data ==="

    PLOTDATA="${RESULT_PREFIX}_plotdata.csv"

    # Create CSV suitable for plotting
    echo "operation,size,scalar_cycles,neon_cycles,speedup" > "$PLOTDATA"
    tail -n +5 "${RESULT_PREFIX}_neon.csv" | awk -F',' '{
        # Determine size from param2 if available, else param1
        size = ($3 > 0) ? $3 : $2
        if (size == 0) size = 1
        print $1 "," size "," $4 "," $5 "," $6
    }' >> "$PLOTDATA"

    echo "✓ Plot data: $PLOTDATA"

    # Create gnuplot script if gnuplot is available
    if command -v gnuplot &> /dev/null; then
        PLOTSCRIPT="${RESULT_PREFIX}_plot.gnuplot"

        cat > "$PLOTSCRIPT" <<'GNUPLOT_EOF'
set terminal png size 1600,1200
set output 'benchmark_speedup.png'

set multiplot layout 2,2 title "LeanSDR NEON Benchmark Results"

# Plot 1: Speedup by operation
set title "NEON Speedup by Operation"
set ylabel "Speedup (x)"
set xlabel "Data Size"
set logscale x
set grid
set key outside right

plot for [op in "fir_filter agc_power complex_multiply mpeg_sync horizontal_sum dot_product viterbi_acs"] \
     'PLOTDATA' using 2:5:(strcol(1) eq op ? $5 : 1/0) with linespoints title op

# Plot 2: Cycles comparison
set title "Scalar vs NEON Cycles"
set ylabel "CPU Cycles"
set logscale y
set key inside left top

plot 'PLOTDATA' using 2:3 with points pt 7 ps 0.5 title "Scalar", \
     'PLOTDATA' using 2:4 with points pt 7 ps 0.5 title "NEON"

# Plot 3: Speedup distribution
unset logscale
set title "Speedup Distribution"
set ylabel "Frequency"
set xlabel "Speedup (x)"
set boxwidth 0.3
set style fill solid

bin_width = 0.5
bin(x) = bin_width * floor(x / bin_width)

plot 'PLOTDATA' using (bin($5)):(1.0) smooth freq with boxes title "Distribution"

# Plot 4: Operation-specific analysis
set title "Cycles by Operation and Size"
set ylabel "Cycles"
set xlabel "Data Size"
set logscale xy
set key outside right

plot for [op in "fir_filter agc_power complex_multiply"] \
     'PLOTDATA' using 2:4:(strcol(1) eq op ? $4 : 1/0) with linespoints title op

unset multiplot
GNUPLOT_EOF

        # Replace PLOTDATA placeholder with actual file
        sed -i "s|'PLOTDATA'|'$PLOTDATA'|g" "$PLOTSCRIPT"

        echo ""
        echo "To generate plots, run:"
        echo "  cd $RESULTS_DIR"
        echo "  gnuplot $(basename $PLOTSCRIPT)"
    fi
fi

# ============================================================================
# Summary
# ============================================================================

echo ""
echo "=== Benchmark Complete ==="
echo ""
echo "Results saved to:"
echo "  $RESULT_PREFIX"
echo ""
echo "Files generated:"
ls -lh "${RESULT_PREFIX}"* 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'

if [ $IS_ARM -eq 1 ]; then
    echo ""
    echo "Next steps:"
    echo "  1. Review the report: cat ${RESULT_PREFIX}_report.txt"
    echo "  2. Analyze CSV data: ${RESULT_PREFIX}_neon.csv"
    echo "  3. Generate plots with gnuplot (if available)"
else
    echo ""
    echo "To get NEON comparison, run this script on an ARM device (Raspberry Pi, etc.)"
fi

echo ""
