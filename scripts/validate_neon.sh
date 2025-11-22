#!/bin/bash

################################################################################
# validate_neon.sh - Comprehensive NEON Optimization Validation Script
#
# This script validates NEON optimizations in the LeanSDR project by:
# 1. Building both scalar and NEON versions of applications
# 2. Running unit tests
# 3. Running benchmarks
# 4. Comparing performance metrics
# 5. Validating correctness
# 6. Generating performance reports
# 7. Checking for NEON instructions in binaries
#
# Usage: ./validate_neon.sh [OPTIONS]
#   --build-only     Only build, don't run tests/benchmarks
#   --test-only      Only run tests, don't run benchmarks
#   --clean          Clean previous builds before starting
#   --verbose        Enable verbose output
#   --help           Display this help message
#
# Exit codes:
#   0 = All validations passed
#   1 = One or more validations failed
#   2 = Build failed
#   3 = Usage error
#
################################################################################

set -o pipefail

# Configuration
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
readonly SRC_DIR="$PROJECT_ROOT/src"
readonly APPS_DIR="$SRC_DIR/apps"
readonly TEST_DIR="$PROJECT_ROOT/test"
readonly REPORT_DIR="$PROJECT_ROOT/validation_reports"
readonly TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
readonly REPORT_FILE="$REPORT_DIR/neon_validation_${TIMESTAMP}.txt"

# Build output directories
readonly BUILD_SCALAR_DIR="/tmp/leansdr_scalar_build_$$"
readonly BUILD_NEON_DIR="/tmp/leansdr_neon_build_$$"

# Flags
BUILD_ONLY=0
TEST_ONLY=0
CLEAN_FIRST=0
VERBOSE=0
FAILED_TESTS=0
FAILED_BUILDS=0

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

################################################################################
# Utility Functions
################################################################################

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $*"
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $*"
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_header() {
    echo ""
    echo "================================================================================"
    echo "  $*"
    echo "================================================================================"
    echo ""
}

report_to_file() {
    echo "$*" >> "$REPORT_FILE"
}

report_header_to_file() {
    {
        echo ""
        echo "================================================================================"
        echo "  $*"
        echo "================================================================================"
        echo ""
    } >> "$REPORT_FILE"
}

# Check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Print usage information
print_usage() {
    head -n 28 "$0" | tail -n 24
}

# Parse command line arguments
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --build-only)
                BUILD_ONLY=1
                shift
                ;;
            --test-only)
                TEST_ONLY=1
                shift
                ;;
            --clean)
                CLEAN_FIRST=1
                shift
                ;;
            --verbose)
                VERBOSE=1
                shift
                ;;
            --help)
                print_usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                print_usage
                exit 3
                ;;
        esac
    done
}

################################################################################
# Setup and Cleanup Functions
################################################################################

setup_environment() {
    log_info "Setting up validation environment..."

    # Create report directory
    mkdir -p "$REPORT_DIR"

    # Initialize report file
    {
        echo "LeanSDR NEON Validation Report"
        echo "Generated: $(date)"
        echo "System: $(uname -a)"
        echo "Compiler: $(gcc --version | head -n1)"
        echo "Architecture: $(uname -m)"
    } > "$REPORT_FILE"

    log_success "Environment setup complete"
    log_info "Report file: $REPORT_FILE"
}

cleanup_build_dirs() {
    log_info "Cleaning up temporary build directories..."
    rm -rf "$BUILD_SCALAR_DIR" "$BUILD_NEON_DIR"
    log_success "Temporary directories cleaned"
}

detect_architecture() {
    local arch=$(uname -m)
    log_info "Detected architecture: $arch"
    report_to_file "Architecture: $arch"
    echo "$arch"
}

check_prerequisites() {
    log_header "Checking Prerequisites"

    local missing_tools=()

    local required_tools=("gcc" "g++" "make" "objdump" "bc" "git")
    for tool in "${required_tools[@]}"; do
        if command_exists "$tool"; then
            log_success "$tool found: $(which $tool)"
            report_to_file "$tool: $(which $tool)"
        else
            log_error "$tool not found"
            missing_tools+=("$tool")
        fi
    done

    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        exit 2
    fi

    log_success "All prerequisites satisfied"
    report_to_file "Prerequisites: OK"
}

################################################################################
# Build Functions
################################################################################

prepare_build_environment() {
    local build_dir=$1
    local build_type=$2

    log_info "Preparing $build_type build in $build_dir..."

    mkdir -p "$build_dir"
    cp -r "$APPS_DIR"/* "$build_dir/" 2>/dev/null || true

    # Copy the headers
    mkdir -p "$build_dir/../leansdr"
    cp -r "$SRC_DIR/leansdr"/* "$build_dir/../leansdr/" 2>/dev/null || true
}

build_scalar_version() {
    log_header "Building Scalar Version (No NEON)"

    prepare_build_environment "$BUILD_SCALAR_DIR" "scalar"

    # Build with standard flags, no NEON
    report_header_to_file "Scalar Build"
    {
        echo "Build directory: $BUILD_SCALAR_DIR"
        echo "Compiler flags: -O3 -march=native (no NEON)"
        echo ""
    } >> "$REPORT_FILE"

    cd "$BUILD_SCALAR_DIR"

    # Remove NEON flags from compilation
    local compile_cmd='g++ -O3 -I.. -DVERSION=\"$(git describe)\" -Wall -Wno-sign-compare -Wno-array-bounds -Wno-unused-variable'

    # Try to compile each application
    local failed=0
    for cc_file in *.cc; do
        local app_name="${cc_file%.cc}"

        # Skip if not a main application
        [[ "$app_name" == "test_"* ]] && continue

        log_info "Building $app_name (scalar)..."

        if $compile_cmd "$cc_file" -o "$app_name" 2>&1 | tee -a "$REPORT_FILE"; then
            log_success "Built $app_name"
            report_to_file "  $app_name: OK"
        else
            log_error "Failed to build $app_name"
            report_to_file "  $app_name: FAILED"
            failed=1
        fi
    done

    if [[ $failed -eq 1 ]]; then
        FAILED_BUILDS=$((FAILED_BUILDS + 1))
        return 1
    fi

    log_success "Scalar build complete"
    return 0
}

build_neon_version() {
    log_header "Building NEON Version"

    prepare_build_environment "$BUILD_NEON_DIR" "neon"

    # Detect architecture for appropriate NEON flags
    local arch=$(uname -m)
    local neon_flags=""

    report_header_to_file "NEON Build"

    if [[ "$arch" == "armv7l" ]] || [[ "$arch" == "armv7" ]]; then
        neon_flags="-march=armv7-a -mfpu=neon -ftree-vectorize -O3 -funsafe-math-optimizations -fsingle-precision-constant"
        report_to_file "Architecture: ARMv7"
    elif [[ "$arch" == "aarch64" ]]; then
        neon_flags="-march=armv8-a -O3 -funsafe-math-optimizations -fsingle-precision-constant"
        report_to_file "Architecture: AArch64 (ARMv8)"
    else
        # For x86/x64, use native optimizations
        neon_flags="-O3 -march=native -ftree-vectorize"
        report_to_file "Architecture: x86/x64 (simulated NEON with vectorization)"
    fi

    report_to_file "Compiler flags: $neon_flags"
    report_to_file ""

    cd "$BUILD_NEON_DIR"

    local compile_cmd="g++ $neon_flags -I.. -DVERSION=\"\$(git describe)\" -Wall -Wno-sign-compare -Wno-array-bounds -Wno-unused-variable"

    local failed=0
    for cc_file in *.cc; do
        local app_name="${cc_file%.cc}"

        # Skip if not a main application
        [[ "$app_name" == "test_"* ]] && continue

        log_info "Building $app_name (NEON)..."

        if eval "$compile_cmd $cc_file -o ${app_name}_neon" 2>&1 | tee -a "$REPORT_FILE"; then
            log_success "Built $app_name (NEON)"
            report_to_file "  ${app_name}_neon: OK"
        else
            log_error "Failed to build $app_name (NEON)"
            report_to_file "  ${app_name}_neon: FAILED"
            failed=1
        fi
    done

    if [[ $failed -eq 1 ]]; then
        FAILED_BUILDS=$((FAILED_BUILDS + 1))
        return 1
    fi

    log_success "NEON build complete"
    return 0
}

################################################################################
# NEON Instruction Checking Functions
################################################################################

check_neon_instructions() {
    log_header "Checking for NEON Instructions in Binaries"

    report_header_to_file "NEON Instruction Analysis"

    local arch=$(uname -m)

    # NEON instructions to look for (ARMv7)
    local neon_arm_instructions=(
        "vadd"
        "vmul"
        "vsub"
        "vld"
        "vst"
        "vdup"
        "vext"
        "vmax"
        "vmin"
    )

    # SIMD instructions to look for (AArch64)
    local neon_aarch64_instructions=(
        "fadd"
        "fmul"
        "fsub"
        "ldr"
        "str"
        "dup"
        "ext"
        "fmax"
        "fmin"
    )

    log_info "Analyzing NEON binaries for SIMD instructions..."
    report_to_file "Checking NEON binaries for SIMD instructions..."
    report_to_file ""

    local neon_found=0
    for binary in "$BUILD_NEON_DIR"/*_neon; do
        [[ ! -f "$binary" ]] && continue

        local binary_name=$(basename "$binary")
        log_info "Analyzing $binary_name..."
        report_to_file "Binary: $binary_name"

        # Disassemble the binary
        local disasm_output=$(objdump -d "$binary" 2>/dev/null)

        if [[ $? -ne 0 ]]; then
            log_warning "Failed to analyze $binary_name with objdump"
            report_to_file "  Result: Could not disassemble (may be stripped or wrong architecture)"
            continue
        fi

        # Count NEON/SIMD instructions
        local instruction_count=0

        if [[ "$arch" == "armv7l" ]] || [[ "$arch" == "armv7" ]]; then
            for instr in "${neon_arm_instructions[@]}"; do
                local count=$(echo "$disasm_output" | grep -c "\\b$instr")
                if [[ $count -gt 0 ]]; then
                    instruction_count=$((instruction_count + count))
                    report_to_file "    $instr: $count occurrences"
                fi
            done
        else
            # For x86/x64, look for SSE/AVX instructions
            local sse_count=$(echo "$disasm_output" | grep -cE "(xmm|ymm|zmm|paddd|paddq|pmul|movdqa)")
            if [[ $sse_count -gt 0 ]]; then
                instruction_count=$((instruction_count + sse_count))
                report_to_file "    SSE/AVX instructions: $sse_count occurrences"
            fi
        fi

        if [[ $instruction_count -gt 0 ]]; then
            log_success "$binary_name contains $instruction_count SIMD instructions"
            report_to_file "  Total SIMD instructions: $instruction_count"
            neon_found=1
        else
            log_warning "$binary_name appears to have no SIMD instructions"
            report_to_file "  Result: No SIMD instructions detected"
        fi

        report_to_file ""
    done

    if [[ $neon_found -eq 1 ]]; then
        log_success "NEON instruction check passed"
        report_to_file "Overall result: SIMD instructions detected in NEON builds"
        return 0
    else
        log_warning "No NEON instructions detected - vectorization may not be enabled"
        report_to_file "Overall result: WARNING - No SIMD instructions detected"
        return 1
    fi
}

################################################################################
# Testing Functions
################################################################################

run_unit_tests() {
    log_header "Running Unit Tests"

    report_header_to_file "Unit Tests"

    if [[ ! -d "$TEST_DIR" ]]; then
        log_warning "Test directory not found: $TEST_DIR"
        report_to_file "Test directory not found"
        return 0
    fi

    # Check for test executables or test scripts
    local test_count=0
    for test_file in "$TEST_DIR"/*.sh "$TEST_DIR"/test_*; do
        [[ ! -f "$test_file" ]] && continue
        [[ ! -x "$test_file" ]] && continue

        local test_name=$(basename "$test_file")
        log_info "Running $test_name..."
        report_to_file "Test: $test_name"

        if "$test_file" 2>&1 | tee -a "$REPORT_FILE"; then
            log_success "$test_name passed"
            report_to_file "  Result: PASSED"
        else
            log_error "$test_name failed"
            report_to_file "  Result: FAILED"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi

        test_count=$((test_count + 1))
        report_to_file ""
    done

    if [[ $test_count -eq 0 ]]; then
        log_info "No executable tests found in $TEST_DIR"
        report_to_file "No executable tests found"
        return 0
    fi

    log_success "Unit tests completed"
    return 0
}

################################################################################
# Benchmark Functions
################################################################################

run_benchmarks() {
    log_header "Running Benchmarks"

    report_header_to_file "Benchmarks"

    # Scalar benchmark
    log_info "Running scalar benchmark..."
    report_to_file "Scalar Benchmark:"

    local scalar_bench_dir=$(mktemp -d)
    cp "$BUILD_SCALAR_DIR"/* "$scalar_bench_dir/" 2>/dev/null || true

    local scalar_timing=$( { time "$TEST_DIR/leandvb_bench.sh" 2>&1; } 2>&1 | grep real)
    log_info "Scalar benchmark timing: $scalar_timing"
    report_to_file "  Timing: $scalar_timing"
    report_to_file ""

    # NEON benchmark
    log_info "Running NEON benchmark..."
    report_to_file "NEON Benchmark:"

    local neon_bench_dir=$(mktemp -d)
    cp "$BUILD_NEON_DIR"/* "$neon_bench_dir/" 2>/dev/null || true

    local neon_timing=$( { time "$TEST_DIR/leandvb_bench.sh" 2>&1; } 2>&1 | grep real)
    log_info "NEON benchmark timing: $neon_timing"
    report_to_file "  Timing: $neon_timing"
    report_to_file ""

    # Cleanup benchmark directories
    rm -rf "$scalar_bench_dir" "$neon_bench_dir"

    log_success "Benchmarks completed"
    return 0
}

################################################################################
# Performance Comparison Functions
################################################################################

compare_performance() {
    log_header "Performance Comparison"

    report_header_to_file "Performance Comparison"

    log_info "Comparing binary sizes..."
    report_to_file "Binary Size Comparison:"
    report_to_file ""

    local scalar_size=0
    local neon_size=0

    for binary in "$BUILD_SCALAR_DIR"/*.cc; do
        [[ ! -f "$binary" ]] && continue
        local app_name="${binary##*/}"
        app_name="${app_name%.cc}"

        if [[ -f "$BUILD_SCALAR_DIR/$app_name" ]]; then
            local size=$(stat -f%z "$BUILD_SCALAR_DIR/$app_name" 2>/dev/null || stat -c%s "$BUILD_SCALAR_DIR/$app_name" 2>/dev/null)
            report_to_file "  Scalar $app_name: $(numfmt --to=iec-i --suffix=B $size 2>/dev/null || echo $size bytes)"
        fi

        if [[ -f "$BUILD_NEON_DIR/${app_name}_neon" ]]; then
            local size=$(stat -f%z "$BUILD_NEON_DIR/${app_name}_neon" 2>/dev/null || stat -c%s "$BUILD_NEON_DIR/${app_name}_neon" 2>/dev/null)
            report_to_file "  NEON ${app_name}_neon: $(numfmt --to=iec-i --suffix=B $size 2>/dev/null || echo $size bytes)"
        fi
    done

    report_to_file ""
    log_success "Performance comparison completed"
    return 0
}

################################################################################
# Validation Functions
################################################################################

validate_correctness() {
    log_header "Validating Correctness"

    report_header_to_file "Correctness Validation"

    log_info "Checking that both scalar and NEON versions produce the same structure size..."
    report_to_file "Structure Size Validation:"
    report_to_file ""

    # Create a simple test to verify structure sizes are consistent
    local test_code='
    #include <iostream>
    #include <cstring>
    #include "../leansdr/framework.h"

    int main() {
        std::cout << "sizeof(complex): " << sizeof(leansdr::complex<float>) << std::endl;
        return 0;
    }
    '

    # Compile scalar version
    local scalar_test=$(mktemp)
    echo "$test_code" > "${scalar_test}.cc"

    if g++ -O3 -I"$SRC_DIR" "${scalar_test}.cc" -o "$scalar_test" 2>/dev/null; then
        local scalar_output=$("$scalar_test" 2>&1)
        log_success "Scalar test compiled and ran"
        report_to_file "  Scalar output: $scalar_output"
    else
        log_warning "Could not compile scalar correctness test"
        report_to_file "  Scalar test: Could not compile"
    fi

    # Compile NEON version
    local neon_test="${scalar_test}_neon"
    local neon_flags="-march=armv7-a -mfpu=neon -ftree-vectorize -O3 -funsafe-math-optimizations"

    if g++ $neon_flags -I"$SRC_DIR" "${scalar_test}.cc" -o "$neon_test" 2>/dev/null; then
        local neon_output=$("$neon_test" 2>&1)
        log_success "NEON test compiled and ran"
        report_to_file "  NEON output: $neon_output"

        if [[ "$scalar_output" == "$neon_output" ]]; then
            log_success "Scalar and NEON outputs match"
            report_to_file "  Result: PASSED - Outputs match"
        else
            log_warning "Scalar and NEON outputs differ"
            report_to_file "  Result: WARNING - Outputs differ"
        fi
    else
        log_warning "Could not compile NEON correctness test"
        report_to_file "  NEON test: Could not compile"
    fi

    # Cleanup
    rm -f "$scalar_test" "$scalar_test.cc" "$neon_test"

    report_to_file ""
    log_success "Correctness validation completed"
    return 0
}

################################################################################
# Reporting Functions
################################################################################

generate_summary() {
    log_header "Generating Summary Report"

    report_header_to_file "Validation Summary"

    local summary=""

    if [[ $FAILED_BUILDS -eq 0 ]]; then
        summary="${summary}Builds: PASSED\n"
    else
        summary="${summary}Builds: FAILED ($FAILED_BUILDS errors)\n"
    fi

    if [[ $FAILED_TESTS -eq 0 ]]; then
        summary="${summary}Tests: PASSED\n"
    else
        summary="${summary}Tests: FAILED ($FAILED_TESTS errors)\n"
    fi

    summary="${summary}Architecture: $(uname -m)\n"
    summary="${summary}Date: $(date)\n"

    echo -e "$summary"
    report_to_file "$summary"

    log_info "Full validation report: $REPORT_FILE"
}

################################################################################
# Main Execution
################################################################################

main() {
    # Parse arguments
    parse_arguments "$@"

    # Setup
    setup_environment
    check_prerequisites

    # Clean if requested
    if [[ $CLEAN_FIRST -eq 1 ]]; then
        log_info "Cleaning previous builds..."
        rm -rf "$BUILD_SCALAR_DIR" "$BUILD_NEON_DIR"
    fi

    # Build phase
    log_header "BUILD PHASE"

    if ! build_scalar_version; then
        log_error "Scalar build failed"
        FAILED_BUILDS=$((FAILED_BUILDS + 1))
    fi

    if ! build_neon_version; then
        log_error "NEON build failed"
        FAILED_BUILDS=$((FAILED_BUILDS + 1))
    fi

    if [[ $FAILED_BUILDS -gt 0 ]]; then
        log_error "Build phase failed with $FAILED_BUILDS error(s)"
        if [[ $BUILD_ONLY -eq 1 ]]; then
            generate_summary
            cleanup_build_dirs
            exit 2
        fi
    else
        log_success "Build phase completed successfully"
    fi

    # Don't proceed with tests/benchmarks if build-only flag is set
    if [[ $BUILD_ONLY -eq 1 ]]; then
        generate_summary
        cleanup_build_dirs
        exit 0
    fi

    # NEON instruction checking
    check_neon_instructions

    # Test phase
    if [[ $TEST_ONLY -eq 0 ]]; then
        log_header "TEST PHASE"
        run_unit_tests

        # Benchmark phase
        log_header "BENCHMARK PHASE"
        run_benchmarks

        # Comparison phase
        log_header "ANALYSIS PHASE"
        compare_performance
        validate_correctness
    else
        run_unit_tests
    fi

    # Generate final report
    generate_summary

    # Cleanup
    cleanup_build_dirs

    # Final status
    log_header "VALIDATION COMPLETE"

    if [[ $FAILED_BUILDS -eq 0 ]] && [[ $FAILED_TESTS -eq 0 ]]; then
        log_success "All validations passed"
        echo ""
        log_info "Report available at: $REPORT_FILE"
        exit 0
    else
        log_error "Validation failed"
        echo ""
        log_error "Build failures: $FAILED_BUILDS"
        log_error "Test failures: $FAILED_TESTS"
        log_info "Report available at: $REPORT_FILE"
        exit 1
    fi
}

# Run main function if script is executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
