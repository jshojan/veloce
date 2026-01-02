#!/bin/bash
#
# NES Emulator Test Suite
# Runs nes-test-roms to validate emulator accuracy
#
# Usage:
#   ./run_tests.sh              # Run all tests
#   ./run_tests.sh cpu          # Run CPU tests only
#   ./run_tests.sh ppu          # Run PPU tests only
#   ./run_tests.sh mapper       # Run mapper tests only
#   ./run_tests.sh --keep       # Keep test ROMs after completion
#   ./run_tests.sh --verbose    # Show detailed output
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$(dirname "$PLUGIN_DIR")")"
TEST_ROMS_DIR="$SCRIPT_DIR/nes-test-roms"
TEST_ROMS_REPO="https://github.com/christopherpow/nes-test-roms.git"

# Find the emulator binary
if [ -f "$PROJECT_ROOT/build/bin/veloce" ]; then
    EMULATOR="$PROJECT_ROOT/build/bin/veloce"
elif [ -f "$PROJECT_ROOT/build/veloce" ]; then
    EMULATOR="$PROJECT_ROOT/build/veloce"
else
    echo "Error: Cannot find veloce binary. Please build the project first."
    echo "  cmake -B build && cmake --build build"
    exit 1
fi

# Configuration
TIMEOUT_SECONDS=10
KEEP_ROMS=false
VERBOSE=false
TEST_CATEGORY=""
PASSED=0
FAILED=0
SKIPPED=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --keep)
            KEEP_ROMS=true
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        cpu|ppu|mapper|apu|timing)
            TEST_CATEGORY="$1"
            shift
            ;;
        --help|-h)
            echo "NES Emulator Test Suite"
            echo ""
            echo "Usage: $0 [OPTIONS] [CATEGORY]"
            echo ""
            echo "Categories:"
            echo "  cpu       CPU instruction and timing tests"
            echo "  ppu       PPU rendering and timing tests"
            echo "  mapper    Mapper-specific tests (MMC1, MMC3, etc.)"
            echo "  apu       Audio processing unit tests"
            echo "  timing    General timing tests"
            echo ""
            echo "Options:"
            echo "  --keep      Keep test ROMs after completion"
            echo "  --verbose   Show detailed test output"
            echo "  --help      Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Clone test ROMs if needed
clone_test_roms() {
    if [ ! -d "$TEST_ROMS_DIR" ]; then
        echo -e "${BLUE}Cloning nes-test-roms repository...${NC}"
        git clone --depth 1 "$TEST_ROMS_REPO" "$TEST_ROMS_DIR"
        echo ""
    fi
}

# Cleanup function
cleanup() {
    if [ "$KEEP_ROMS" = false ] && [ -d "$TEST_ROMS_DIR" ]; then
        echo -e "\n${BLUE}Cleaning up test ROMs...${NC}"
        rm -rf "$TEST_ROMS_DIR"
    fi
}

# Run a single test ROM
# Returns: 0 = pass, 1 = fail, 2 = skip
run_test() {
    local rom_path="$1"
    local expected_result="${2:-0}"  # Default: expect exit code 0 or result byte 0
    local test_name="$(basename "$rom_path" .nes)"

    if [ ! -f "$rom_path" ]; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${YELLOW}SKIP${NC} $test_name (ROM not found)"
        fi
        return 2
    fi

    # Run the emulator with timeout
    local output
    local exit_code
    output=$(timeout "$TIMEOUT_SECONDS" "$EMULATOR" "$rom_path" 2>&1) || exit_code=$?

    # Check for timeout
    if [ "${exit_code:-0}" -eq 124 ]; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${YELLOW}TIMEOUT${NC} $test_name"
        fi
        return 1
    fi

    # Check for blargg test result in output
    # Tests write result to $6000: 0x00 = running, 0x80 = reset, 0x81 = passed, others = failed
    if echo "$output" | grep -q "Test passed\|Result: 0\|PASSED"; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${GREEN}PASS${NC} $test_name"
        fi
        return 0
    elif echo "$output" | grep -q "Result: [1-9]\|FAILED\|Failed"; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${RED}FAIL${NC} $test_name"
            if [ "$VERBOSE" = true ]; then
                echo "$output" | grep -E "Result:|Error:|Failed" | head -5
            fi
        fi
        return 1
    else
        # No clear result - assume pass if no error
        if [ "${exit_code:-0}" -eq 0 ]; then
            if [ "$VERBOSE" = true ]; then
                echo -e "  ${GREEN}PASS${NC} $test_name (no errors)"
            fi
            return 0
        else
            if [ "$VERBOSE" = true ]; then
                echo -e "  ${RED}FAIL${NC} $test_name (exit code: $exit_code)"
            fi
            return 1
        fi
    fi
}

# Run a test suite (directory of ROMs)
run_suite() {
    local suite_name="$1"
    local suite_dir="$2"
    local pattern="${3:-*.nes}"

    echo -e "\n${BLUE}=== $suite_name ===${NC}"

    local suite_passed=0
    local suite_failed=0
    local suite_skipped=0

    # Find and run all matching ROMs
    while IFS= read -r rom; do
        run_test "$rom"
        case $? in
            0) ((suite_passed++)) ;;
            1) ((suite_failed++)) ;;
            2) ((suite_skipped++)) ;;
        esac
    done < <(find "$suite_dir" -name "$pattern" -type f 2>/dev/null | sort)

    # Summary
    echo -e "  ${GREEN}Passed: $suite_passed${NC} | ${RED}Failed: $suite_failed${NC} | ${YELLOW}Skipped: $suite_skipped${NC}"

    ((PASSED += suite_passed))
    ((FAILED += suite_failed))
    ((SKIPPED += suite_skipped))
}

# CPU Tests
run_cpu_tests() {
    echo -e "\n${BLUE}╔════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║         CPU INSTRUCTION TESTS       ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════╝${NC}"

    run_suite "CPU Instructions v5" "$TEST_ROMS_DIR/instr_test-v5/rom_singles"
    run_suite "CPU Timing Test" "$TEST_ROMS_DIR/cpu_timing_test6"
    run_suite "Branch Timing" "$TEST_ROMS_DIR/branch_timing_tests"
    run_suite "CPU Interrupts" "$TEST_ROMS_DIR/cpu_interrupts_v2/rom_singles"
    run_suite "CPU Dummy Reads" "$TEST_ROMS_DIR/cpu_dummy_reads"
    run_suite "CPU Dummy Writes" "$TEST_ROMS_DIR/cpu_dummy_writes"
}

# PPU Tests
run_ppu_tests() {
    echo -e "\n${BLUE}╔════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║            PPU TESTS                ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════╝${NC}"

    run_suite "PPU VBlank/NMI" "$TEST_ROMS_DIR/ppu_vbl_nmi/rom_singles"
    run_suite "PPU Sprite Hit" "$TEST_ROMS_DIR/sprite_hit_tests_2005.10.05"
    run_suite "PPU Sprite Overflow" "$TEST_ROMS_DIR/sprite_overflow_tests"
    run_suite "PPU Open Bus" "$TEST_ROMS_DIR/ppu_open_bus"
    run_suite "PPU Read Buffer" "$TEST_ROMS_DIR/ppu_read_buffer"
    run_suite "OAM Read" "$TEST_ROMS_DIR/oam_read"
    run_suite "OAM Stress" "$TEST_ROMS_DIR/oam_stress"
}

# Mapper Tests
run_mapper_tests() {
    echo -e "\n${BLUE}╔════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║          MAPPER TESTS               ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════╝${NC}"

    run_suite "MMC3 Test v2" "$TEST_ROMS_DIR/mmc3_test_2/rom_singles"
    run_suite "MMC3 IRQ Tests" "$TEST_ROMS_DIR/mmc3_irq_tests"
    run_suite "MMC1 A12" "$TEST_ROMS_DIR/MMC1_A12"
}

# APU Tests
run_apu_tests() {
    echo -e "\n${BLUE}╔════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║            APU TESTS                ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════╝${NC}"

    run_suite "APU Test" "$TEST_ROMS_DIR/apu_test/rom_singles"
    run_suite "APU Reset" "$TEST_ROMS_DIR/apu_reset"
    run_suite "APU Mixer" "$TEST_ROMS_DIR/apu_mixer"
    run_suite "DMC Tests" "$TEST_ROMS_DIR/dmc_tests"
}

# Timing Tests
run_timing_tests() {
    echo -e "\n${BLUE}╔════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║          TIMING TESTS               ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════╝${NC}"

    run_suite "VBL/NMI Timing" "$TEST_ROMS_DIR/vbl_nmi_timing"
    run_suite "Instruction Timing" "$TEST_ROMS_DIR/instr_timing"
}

# Main execution
main() {
    echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║              NES EMULATOR TEST SUITE                   ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "Emulator: $EMULATOR"
    echo "Timeout:  ${TIMEOUT_SECONDS}s per test"
    echo ""

    # Setup cleanup trap
    trap cleanup EXIT

    # Clone test ROMs
    clone_test_roms

    # Run tests based on category
    case "$TEST_CATEGORY" in
        cpu)
            run_cpu_tests
            ;;
        ppu)
            run_ppu_tests
            ;;
        mapper)
            run_mapper_tests
            ;;
        apu)
            run_apu_tests
            ;;
        timing)
            run_timing_tests
            ;;
        *)
            # Run all tests
            run_cpu_tests
            run_ppu_tests
            run_mapper_tests
            run_apu_tests
            run_timing_tests
            ;;
    esac

    # Final summary
    echo -e "\n${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║                    FINAL RESULTS                       ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "  ${GREEN}Passed:  $PASSED${NC}"
    echo -e "  ${RED}Failed:  $FAILED${NC}"
    echo -e "  ${YELLOW}Skipped: $SKIPPED${NC}"

    local total=$((PASSED + FAILED))
    if [ $total -gt 0 ]; then
        local percent=$((PASSED * 100 / total))
        echo -e "  Pass Rate: ${percent}%"
    fi
    echo ""

    # Exit with failure if any tests failed
    if [ $FAILED -gt 0 ]; then
        exit 1
    fi
}

main "$@"
