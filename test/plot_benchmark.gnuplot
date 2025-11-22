# Gnuplot script for visualizing LeanSDR NEON benchmark results
# Usage: gnuplot -c plot_benchmark.gnuplot <csv_file>
#
# Example:
#   gnuplot -c plot_benchmark.gnuplot benchmark_results/benchmark_abc123_neon.csv

# Check if input file was provided
if (!exists("ARG1")) {
    print "Error: No input file specified"
    print "Usage: gnuplot -c plot_benchmark.gnuplot <csv_file>"
    exit
}

datafile = ARG1

# Output settings
set terminal png size 1920,1080 font "Arial,12"
set output 'benchmark_results.png'

# Global settings
set datafile separator ","
set key outside right
set grid

# Multi-plot layout
set multiplot layout 2,2 title sprintf("LeanSDR NEON Benchmark Results\nData: %s", datafile) font "Arial,16"

# ============================================================================
# Plot 1: Speedup by Operation Type
# ============================================================================

set title "NEON Speedup by Operation" font "Arial,14"
set xlabel "Data Size (samples)"
set ylabel "Speedup (NEON vs Scalar)"
set logscale x
set yrange [0:*]
set grid

# Plot speedup for each operation
plot datafile using 3:6:(strcol(1) eq "fir_filter" ? $6 : 1/0) \
        with linespoints pt 7 ps 1.5 lw 2 title "FIR Filter", \
     datafile using 3:6:(strcol(1) eq "agc_power" ? $6 : 1/0) \
        with linespoints pt 9 ps 1.5 lw 2 title "AGC Power", \
     datafile using 3:6:(strcol(1) eq "complex_multiply" ? $6 : 1/0) \
        with linespoints pt 5 ps 1.5 lw 2 title "Complex Multiply", \
     datafile using 3:6:(strcol(1) eq "mpeg_sync" ? $6 : 1/0) \
        with linespoints pt 11 ps 1.5 lw 2 title "MPEG Sync", \
     datafile using 3:6:(strcol(1) eq "horizontal_sum" ? $6 : 1/0) \
        with linespoints pt 13 ps 1.5 lw 2 title "Horizontal Sum", \
     datafile using 3:6:(strcol(1) eq "dot_product" ? $6 : 1/0) \
        with linespoints pt 15 ps 1.5 lw 2 title "Dot Product", \
     datafile using 3:6:(strcol(1) eq "viterbi_acs" ? $6 : 1/0) \
        with linespoints pt 4 ps 1.5 lw 2 title "Viterbi ACS", \
     4.0 with lines dt 2 lc rgb "red" lw 2 notitle, \
     2.0 with lines dt 2 lc rgb "orange" lw 2 notitle, \
     1.0 with lines dt 2 lc rgb "black" lw 1 notitle

# Add annotations
set label "4x speedup" at graph 0.05,0.95 left tc rgb "red"
set label "2x speedup" at graph 0.05,0.50 left tc rgb "orange"

# ============================================================================
# Plot 2: Cycles Comparison (Scalar vs NEON)
# ============================================================================

unset label
set title "CPU Cycles: Scalar vs NEON" font "Arial,14"
set xlabel "Data Size (samples)"
set ylabel "CPU Cycles"
set logscale xy
set grid

plot datafile using 3:4 with points pt 7 ps 0.8 lc rgb "blue" title "Scalar", \
     datafile using 3:5 with points pt 7 ps 0.8 lc rgb "red" title "NEON"

# ============================================================================
# Plot 3: FIR Filter Performance by Tap Count
# ============================================================================

unset logscale
set title "FIR Filter Performance by Tap Count" font "Arial,14"
set xlabel "Number of Filter Taps"
set ylabel "Cycles per Sample"
set logscale y
set grid

# Calculate cycles per sample for FIR filter
plot datafile using ($2):($4/$3):(strcol(1) eq "fir_filter" ? $4/$3 : 1/0) \
        with linespoints pt 7 ps 1.5 lw 2 lc rgb "blue" title "Scalar", \
     datafile using ($2):($5/$3):(strcol(1) eq "fir_filter" && $5 > 0 ? $5/$3 : 1/0) \
        with linespoints pt 9 ps 1.5 lw 2 lc rgb "red" title "NEON"

# ============================================================================
# Plot 4: Speedup Distribution Histogram
# ============================================================================

unset logscale
set title "Speedup Distribution" font "Arial,14"
set xlabel "Speedup Factor"
set ylabel "Count"
set boxwidth 0.4
set style fill solid 0.5
set grid ytics

# Create histogram of speedup values
bin_width = 0.5
bin(x) = bin_width * floor(x / bin_width)

plot datafile using (bin($6)):(1.0) smooth freq with boxes \
     lc rgb "green" title "Frequency"

unset multiplot

# ============================================================================
# Generate Summary Statistics
# ============================================================================

print ""
print "===================================================================="
print "BENCHMARK SUMMARY"
print "===================================================================="
print ""

# Calculate statistics using gnuplot's stats command
stats datafile using 6 nooutput name "speedup"

print sprintf("Overall Statistics:")
print sprintf("  Average speedup:  %.2fx", speedup_mean)
print sprintf("  Median speedup:   %.2fx", speedup_median)
print sprintf("  Min speedup:      %.2fx", speedup_min)
print sprintf("  Max speedup:      %.2fx", speedup_max)
print sprintf("  Std deviation:    %.2f", speedup_stddev)
print ""

# Per-operation statistics
operations = "fir_filter agc_power complex_multiply mpeg_sync horizontal_sum dot_product viterbi_acs"

print "Per-Operation Average Speedup:"

do for [op in operations] {
    # Filter data for this operation
    # Note: This is approximate since gnuplot doesn't have great string filtering in stats
    # The actual values will be shown in the plot
    print sprintf("  %-20s: See plot", op)
}

print ""
print "===================================================================="
print sprintf("Plot saved to: benchmark_results.png")
print "===================================================================="
print ""
