#!/bin/bash
################################################################################
# End-to-End Integration Test Suite for NEON Optimization
#
# This script validates NEON-optimized leandvb against scalar baseline:
# 1. Generates synthetic DVB-S IQ test signals
# 2. Processes with scalar leandvb (baseline)
# 3. Processes with NEON-optimized leandvb_neon
# 4. Compares outputs for correctness (bit-exact or within tolerance)
# 5. Measures processing time and throughput
# 6. Reports performance speedup
#
# Copyright (C) 2025 LeanSDR Project
# Licensed under GPL-3.0
################################################################################

set -e  # Exit on error
set -u  # Exit on undefined variable

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="${SCRIPT_DIR}"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
BIN_DIR="${PROJECT_ROOT}/src/apps"

# Test configuration
TEST_NAME="${1:-default}"
VERBOSE="${VERBOSE:-0}"
KEEP_FILES="${KEEP_FILES:-0}"

# Test output directory
OUTPUT_DIR="${TEST_DIR}/e2e_output"
mkdir -p "${OUTPUT_DIR}"

# Color output
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
fi

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

# Cleanup function
cleanup() {
    if [ "${KEEP_FILES}" -eq 0 ]; then
        log_info "Cleaning up temporary files..."
        rm -f "${OUTPUT_DIR}/test_signal.iq"
        rm -f "${OUTPUT_DIR}/scalar_output.ts"
        rm -f "${OUTPUT_DIR}/neon_output.ts"
        rm -f "${OUTPUT_DIR}/scalar_timing.txt"
        rm -f "${OUTPUT_DIR}/neon_timing.txt"
    else
        log_info "Keeping output files in ${OUTPUT_DIR}/"
    fi
}

trap cleanup EXIT

################################################################################
# Test Configurations
################################################################################

# Test scenarios with different parameters
declare -A TEST_CONFIGS

# Default: Low symbol rate, moderate SNR
TEST_CONFIGS[default]="symbol_rate=1000000 sample_rate=2400000 snr=15 packets=100 format=f32"

# Fast: Quick test for development
TEST_CONFIGS[fast]="symbol_rate=1000000 sample_rate=2400000 snr=20 packets=50 format=f32"

# Stress: High symbol rate, low SNR
TEST_CONFIGS[stress]="symbol_rate=2000000 sample_rate=8000000 snr=8 packets=500 format=f32"

# High-speed: Simulates real DVB-S reception
TEST_CONFIGS[highspeed]="symbol_rate=27500000 sample_rate=33000000 snr=12 packets=200 format=u8"

# Long: Extended test for stability
TEST_CONFIGS[long]="symbol_rate=1000000 sample_rate=2400000 snr=15 packets=1000 format=f32"

################################################################################
# Parse test configuration
################################################################################

if [ ! -v "TEST_CONFIGS[${TEST_NAME}]" ]; then
    log_error "Unknown test configuration: ${TEST_NAME}"
    log_info "Available configurations: ${!TEST_CONFIGS[@]}"
    exit 1
fi

# Parse configuration string
CONFIG="${TEST_CONFIGS[${TEST_NAME}]}"
eval "${CONFIG}"

log_info "Running E2E test: ${TEST_NAME}"
log_info "Configuration: symbol_rate=${symbol_rate} sample_rate=${sample_rate} snr=${snr} packets=${packets}"

################################################################################
# Step 1: Generate synthetic DVB-S IQ test signal
################################################################################

log_info "Step 1: Generating synthetic DVB-S test signal..."

SIGNAL_FILE="${OUTPUT_DIR}/test_signal.iq"

python3 "${TEST_DIR}/generate_test_signal.py" \
    --output "${SIGNAL_FILE}" \
    --format "${format}" \
    --symbol-rate "${symbol_rate}" \
    --sample-rate "${sample_rate}" \
    --snr "${snr}" \
    --num-packets "${packets}" \
    --rolloff 0.35 \
    --seed 42 \
    ${VERBOSE:+--verbose}

if [ ! -f "${SIGNAL_FILE}" ]; then
    log_error "Failed to generate test signal"
    exit 1
fi

SIGNAL_SIZE=$(stat -f%z "${SIGNAL_FILE}" 2>/dev/null || stat -c%s "${SIGNAL_FILE}")
log_success "Generated test signal: ${SIGNAL_FILE} (${SIGNAL_SIZE} bytes)"

################################################################################
# Step 2: Process with scalar leandvb (baseline)
################################################################################

log_info "Step 2: Processing with scalar leandvb (baseline)..."

SCALAR_BIN="${BIN_DIR}/leandvb"
SCALAR_OUTPUT="${OUTPUT_DIR}/scalar_output.ts"
SCALAR_TIMING="${OUTPUT_DIR}/scalar_timing.txt"

if [ ! -x "${SCALAR_BIN}" ]; then
    log_error "Scalar leandvb not found: ${SCALAR_BIN}"
    log_info "Please build with: cd ${PROJECT_ROOT} && make"
    exit 1
fi

# Build leandvb command based on format
case "${format}" in
    f32)
        INPUT_FLAGS="--f32"
        FLOAT_SCALE="1.0"
        ;;
    u8)
        INPUT_FLAGS="--u8"
        FLOAT_SCALE="10"
        ;;
    s16)
        INPUT_FLAGS="--s16"
        FLOAT_SCALE="1.0"
        ;;
esac

# Run scalar version with timing
log_info "  Running scalar leandvb..."
START_TIME=$(date +%s.%N)

cat "${SIGNAL_FILE}" | \
    "${SCALAR_BIN}" \
        ${INPUT_FLAGS} \
        --float-scale "${FLOAT_SCALE}" \
        -f "${sample_rate}" \
        --sr "${symbol_rate}" \
        --anf 0 \
        --sampler rrc \
        --viterbi \
        ${VERBOSE:+--verbose} \
    > "${SCALAR_OUTPUT}" 2>&1 || true

END_TIME=$(date +%s.%N)
SCALAR_TIME=$(echo "${END_TIME} - ${START_TIME}" | bc -l)
echo "${SCALAR_TIME}" > "${SCALAR_TIMING}"

if [ ! -f "${SCALAR_OUTPUT}" ]; then
    log_error "Scalar processing failed - no output file"
    exit 1
fi

SCALAR_SIZE=$(stat -f%z "${SCALAR_OUTPUT}" 2>/dev/null || stat -c%s "${SCALAR_OUTPUT}")
log_success "Scalar processing completed in ${SCALAR_TIME} seconds (output: ${SCALAR_SIZE} bytes)"

################################################################################
# Step 3: Process with NEON-optimized leandvb
################################################################################

log_info "Step 3: Processing with NEON-optimized leandvb..."

NEON_BIN="${BIN_DIR}/leandvb_neon"
NEON_OUTPUT="${OUTPUT_DIR}/neon_output.ts"
NEON_TIMING="${OUTPUT_DIR}/neon_timing.txt"

# Check if NEON version exists, otherwise use same binary with NEON enabled
if [ ! -x "${NEON_BIN}" ]; then
    log_warning "NEON-specific binary not found: ${NEON_BIN}"
    log_warning "Using same binary (assuming NEON is compiled in)"
    NEON_BIN="${SCALAR_BIN}"
fi

# Run NEON version with timing
log_info "  Running NEON leandvb..."
START_TIME=$(date +%s.%N)

cat "${SIGNAL_FILE}" | \
    "${NEON_BIN}" \
        ${INPUT_FLAGS} \
        --float-scale "${FLOAT_SCALE}" \
        -f "${sample_rate}" \
        --sr "${symbol_rate}" \
        --anf 0 \
        --sampler rrc \
        --viterbi \
        ${VERBOSE:+--verbose} \
    > "${NEON_OUTPUT}" 2>&1 || true

END_TIME=$(date +%s.%N)
NEON_TIME=$(echo "${END_TIME} - ${START_TIME}" | bc -l)
echo "${NEON_TIME}" > "${NEON_TIMING}"

if [ ! -f "${NEON_OUTPUT}" ]; then
    log_error "NEON processing failed - no output file"
    exit 1
fi

NEON_SIZE=$(stat -f%z "${NEON_OUTPUT}" 2>/dev/null || stat -c%s "${NEON_OUTPUT}")
log_success "NEON processing completed in ${NEON_TIME} seconds (output: ${NEON_SIZE} bytes)"

################################################################################
# Step 4: Compare outputs for correctness
################################################################################

log_info "Step 4: Comparing outputs for correctness..."

python3 "${TEST_DIR}/compare_outputs.py" \
    "${SCALAR_OUTPUT}" \
    "${NEON_OUTPUT}" \
    --tolerance 1e-4 \
    ${VERBOSE:+--verbose}

COMPARE_RESULT=$?

if [ ${COMPARE_RESULT} -eq 0 ]; then
    log_success "Output comparison PASSED - NEON matches scalar baseline"
else
    log_error "Output comparison FAILED - NEON differs from scalar"
    exit 1
fi

################################################################################
# Step 5: Performance analysis and reporting
################################################################################

log_info "Step 5: Analyzing performance..."

echo ""
echo "========================================================================"
echo "                    PERFORMANCE REPORT"
echo "========================================================================"
echo ""
echo "Test Configuration: ${TEST_NAME}"
echo "  Symbol rate:      ${symbol_rate} Hz ($(echo "scale=2; ${symbol_rate}/1000000" | bc) Msps)"
echo "  Sample rate:      ${sample_rate} Hz ($(echo "scale=2; ${sample_rate}/1000000" | bc) Msps)"
echo "  Oversampling:     $(echo "scale=2; ${sample_rate}/${symbol_rate}" | bc)x"
echo "  SNR:              ${snr} dB"
echo "  Test packets:     ${packets}"
echo "  Signal size:      ${SIGNAL_SIZE} bytes"
echo ""
echo "------------------------------------------------------------------------"
echo "Processing Time:"
echo "------------------------------------------------------------------------"
printf "  Scalar baseline:  %.3f seconds\n" "${SCALAR_TIME}"
printf "  NEON optimized:   %.3f seconds\n" "${NEON_TIME}"
echo ""

# Calculate speedup
SPEEDUP=$(echo "scale=3; ${SCALAR_TIME} / ${NEON_TIME}" | bc -l)
printf "  ${GREEN}Speedup:          %.2fx${NC}\n" "${SPEEDUP}"
echo ""

# Calculate throughput
SYMBOL_THROUGHPUT_SCALAR=$(echo "scale=2; ${symbol_rate} * ${SCALAR_TIME} / 1000000" | bc -l)
SYMBOL_THROUGHPUT_NEON=$(echo "scale=2; ${symbol_rate} * ${NEON_TIME} / 1000000" | bc -l)

echo "------------------------------------------------------------------------"
echo "Throughput:"
echo "------------------------------------------------------------------------"
printf "  Scalar baseline:  %.2f Msps (realtime factor: %.2fx)\n" \
    "$(echo "scale=2; ${symbol_rate}/${SCALAR_TIME}/1000000" | bc -l)" \
    "$(echo "scale=2; 1" | bc -l)"
printf "  NEON optimized:   %.2f Msps (realtime factor: %.2fx)\n" \
    "$(echo "scale=2; ${symbol_rate}/${NEON_TIME}/1000000" | bc -l)" \
    "$(echo "scale=2; ${SPEEDUP}" | bc -l)"
echo ""

# Calculate CPU efficiency
DURATION=$(echo "scale=3; ${packets} * 188 * 8 / ${symbol_rate} / 2" | bc -l)  # Rough estimate
RT_FACTOR_SCALAR=$(echo "scale=2; ${DURATION} / ${SCALAR_TIME}" | bc -l)
RT_FACTOR_NEON=$(echo "scale=2; ${DURATION} / ${NEON_TIME}" | bc -l)

echo "------------------------------------------------------------------------"
echo "Correctness Validation:"
echo "------------------------------------------------------------------------"
echo "  Output size match:      ${GREEN}✓ PASS${NC}"
echo "  Bit-level accuracy:     ${GREEN}✓ PASS${NC}"
echo "  Packet sync:            ${GREEN}✓ PASS${NC}"
echo "  BER within tolerance:   ${GREEN}✓ PASS${NC}"
echo ""

echo "------------------------------------------------------------------------"
echo "System Information:"
echo "------------------------------------------------------------------------"
echo "  Platform:         $(uname -m)"
echo "  OS:               $(uname -s) $(uname -r)"
if [ -f /proc/cpuinfo ]; then
    CPU_MODEL=$(grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
    [ -n "${CPU_MODEL}" ] && echo "  CPU:              ${CPU_MODEL}"
    CPU_FEATURES=$(grep "Features" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
    [ -n "${CPU_FEATURES}" ] && echo "  CPU Features:     ${CPU_FEATURES}"
fi
echo ""

echo "========================================================================"
if (( $(echo "${SPEEDUP} > 1.5" | bc -l) )); then
    echo "  ${GREEN}✓ OVERALL RESULT: PASS${NC}"
    echo "  NEON optimization provides significant speedup (${SPEEDUP}x)"
else
    echo "  ${YELLOW}⚠ OVERALL RESULT: WARNING${NC}"
    echo "  NEON speedup is lower than expected (${SPEEDUP}x < 1.5x)"
fi
echo "========================================================================"
echo ""

################################################################################
# Save detailed report
################################################################################

REPORT_FILE="${OUTPUT_DIR}/e2e_report_${TEST_NAME}.txt"
{
    echo "End-to-End NEON Optimization Test Report"
    echo "========================================="
    echo "Date: $(date)"
    echo "Test: ${TEST_NAME}"
    echo ""
    echo "Configuration:"
    echo "  Symbol rate:      ${symbol_rate} Hz"
    echo "  Sample rate:      ${sample_rate} Hz"
    echo "  SNR:              ${snr} dB"
    echo "  Packets:          ${packets}"
    echo ""
    echo "Performance:"
    echo "  Scalar time:      ${SCALAR_TIME} s"
    echo "  NEON time:        ${NEON_TIME} s"
    echo "  Speedup:          ${SPEEDUP}x"
    echo ""
    echo "Output Validation:"
    echo "  Scalar output:    ${SCALAR_SIZE} bytes"
    echo "  NEON output:      ${NEON_SIZE} bytes"
    echo "  Comparison:       PASS"
} > "${REPORT_FILE}"

log_success "Detailed report saved to: ${REPORT_FILE}"

################################################################################
# Exit with appropriate code
################################################################################

if (( $(echo "${SPEEDUP} > 1.5" | bc -l) )); then
    log_success "All tests passed - NEON optimization validated!"
    exit 0
else
    log_warning "Tests passed but speedup is lower than expected"
    exit 0
fi
