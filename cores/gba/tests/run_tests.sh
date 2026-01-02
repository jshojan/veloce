#!/bin/bash
#
# GBA Emulator Test Suite
# Runs test ROMs through the Veloce emulator to validate accuracy
#
# Usage:
#   ./run_tests.sh              # Run all GBA tests
#   ./run_tests.sh arm          # Run ARM instruction tests only
#   ./run_tests.sh thumb        # Run Thumb instruction tests only
#   ./run_tests.sh memory       # Run memory tests only
#   ./run_tests.sh ppu          # Run PPU tests only
#   ./run_tests.sh bios         # Run BIOS tests only
#   ./run_tests.sh save         # Run save type tests only
#   ./run_tests.sh --keep       # Keep test ROMs after completion
#   ./run_tests.sh --verbose    # Show detailed output
#
# Test ROM Sources:
#   GBA: https://github.com/jsmolka/gba-tests
#
# Test Result Format:
#   - Tests display "All tests passed" on screen when successful
#   - Tests display "Failed test XXX" with a number when failed
#   - In debug mode, the emulator outputs [GBA] PASSED when test succeeds
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$(dirname "$PLUGIN_DIR")")"
TEST_ROMS_DIR="$SCRIPT_DIR/gba-test-roms"
TEST_ROMS_REPO="https://github.com/jsmolka/gba-tests.git"

# Find the veloce emulator binary
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
TIMEOUT_SECONDS=30
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
CYAN='\033[0;36m'
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
        arm|thumb|memory|ppu|bios|save|unsafe)
            TEST_CATEGORY="$1"
            shift
            ;;
        --help|-h)
            echo "GBA Emulator Test Suite"
            echo ""
            echo "Usage: $0 [OPTIONS] [CATEGORY]"
            echo ""
            echo "Categories:"
            echo "  arm         ARM instruction set tests"
            echo "  thumb       Thumb instruction set tests"
            echo "  memory      Memory access and mirroring tests"
            echo "  ppu         PPU/graphics tests"
            echo "  bios        BIOS function tests"
            echo "  save        Save type detection/handling tests"
            echo "  unsafe      Edge case tests (may not pass on hardware)"
            echo ""
            echo "Options:"
            echo "  --keep      Keep test ROMs after completion"
            echo "  --verbose   Show detailed test output"
            echo "  --help      Show this help message"
            echo ""
            echo "Test ROM Source: https://github.com/jsmolka/gba-tests"
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
        echo -e "${BLUE}Cloning gba-tests repository...${NC}"
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

# Run a single test ROM through Veloce
# Returns: 0 = pass, 1 = fail, 2 = skip
run_test() {
    local rom_path="$1"
    local test_name="$2"
    local notes="${3:-}"

    if [ ! -f "$rom_path" ]; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${YELLOW}SKIP${NC} $test_name (ROM not found)"
        fi
        return 2
    fi

    # Run the emulator with DEBUG=1 and timeout
    local output
    local exit_code=0

    output=$(DEBUG=1 timeout "$TIMEOUT_SECONDS" "$EMULATOR" "$rom_path" 2>&1) || exit_code=$?

    # Check for timeout
    if [ "$exit_code" -eq 124 ]; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${YELLOW}TIMEOUT${NC} $test_name"
        fi
        return 1
    fi

    # Check for debug output indicating pass/fail
    if echo "$output" | grep -q "\[GBA\] PASSED"; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${GREEN}PASS${NC} $test_name"
        fi
        return 0
    fi

    # Check for generic pass patterns
    if echo "$output" | grep -qi "all tests passed\|tests passed"; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${GREEN}PASS${NC} $test_name"
        fi
        return 0
    fi

    # Check for explicit failure
    if echo "$output" | grep -qi "failed test"; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${RED}FAIL${NC} $test_name"
            if [ -n "$notes" ]; then
                echo "       Note: $notes"
            fi
            echo "$output" | grep -iE "failed|error|\[GBA\]" | head -5 | sed 's/^/       /'
        fi
        return 1
    fi

    if echo "$output" | grep -qi "fail\|error"; then
        if [ "$VERBOSE" = true ]; then
            echo -e "  ${RED}FAIL${NC} $test_name"
        fi
        return 1
    fi

    # No clear result - check exit code
    if [ "$exit_code" -eq 0 ]; then
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
}

# Run a test suite
run_suite() {
    local suite_name="$1"
    local suite_dir="$2"
    local description="$3"
    shift 3
    local roms=("$@")

    echo -e "\n${BLUE}=== $suite_name ===${NC}"
    if [ "$VERBOSE" = true ]; then
        echo -e "    ${CYAN}$description${NC}"
    fi

    local suite_passed=0
    local suite_failed=0
    local suite_skipped=0

    for rom_spec in "${roms[@]}"; do
        IFS='|' read -r rom_path expected notes <<< "$rom_spec"
        local test_name="$(basename "$rom_path" .gba)"
        local full_path="$suite_dir/$rom_path"

        run_test "$full_path" "$test_name" "$notes"
        case $? in
            0) ((suite_passed++)) ;;
            1) ((suite_failed++)) ;;
            2) ((suite_skipped++)) ;;
        esac
    done

    echo -e "  ${GREEN}Passed: $suite_passed${NC} | ${RED}Failed: $suite_failed${NC} | ${YELLOW}Skipped: $suite_skipped${NC}"

    ((PASSED += suite_passed))
    ((FAILED += suite_failed))
    ((SKIPPED += suite_skipped))
}

# ARM Instruction Tests
run_arm_tests() {
    echo -e "\n${BLUE}+--------------------------------------+${NC}"
    echo -e "${BLUE}|     ARM INSTRUCTION SET TESTS        |${NC}"
    echo -e "${BLUE}+--------------------------------------+${NC}"

    local roms=(
        "arm/arm.gba|pass|ARM instruction tests"
    )
    run_suite "ARM Instructions" "$TEST_ROMS_DIR" \
        "Tests ARM mode instructions including data processing, branches, multiply, and memory access" \
        "${roms[@]}"
}

# Thumb Instruction Tests
run_thumb_tests() {
    echo -e "\n${BLUE}+--------------------------------------+${NC}"
    echo -e "${BLUE}|    THUMB INSTRUCTION SET TESTS       |${NC}"
    echo -e "${BLUE}+--------------------------------------+${NC}"

    local roms=(
        "thumb/thumb.gba|pass|Thumb instruction tests"
    )
    run_suite "Thumb Instructions" "$TEST_ROMS_DIR" \
        "Tests Thumb mode instructions" \
        "${roms[@]}"
}

# Memory Tests
run_memory_tests() {
    echo -e "\n${BLUE}+--------------------------------------+${NC}"
    echo -e "${BLUE}|          MEMORY TESTS                |${NC}"
    echo -e "${BLUE}+--------------------------------------+${NC}"

    local roms=(
        "memory/memory.gba|pass|Memory access tests"
    )
    run_suite "Memory" "$TEST_ROMS_DIR" \
        "Tests memory access patterns, mirroring, and video memory operations" \
        "${roms[@]}"
}

# PPU Tests
run_ppu_tests() {
    echo -e "\n${BLUE}+--------------------------------------+${NC}"
    echo -e "${BLUE}|           PPU TESTS                  |${NC}"
    echo -e "${BLUE}+--------------------------------------+${NC}"

    local roms=(
        "ppu/hello.gba|pass|Basic PPU hello world test"
        "ppu/shades.gba|pass|PPU color shading test"
        "ppu/stripes.gba|pass|PPU stripe pattern test"
    )
    run_suite "PPU/Graphics" "$TEST_ROMS_DIR" \
        "Tests PPU rendering" \
        "${roms[@]}"
}

# BIOS Tests
run_bios_tests() {
    echo -e "\n${BLUE}+--------------------------------------+${NC}"
    echo -e "${BLUE}|          BIOS TESTS                  |${NC}"
    echo -e "${BLUE}+--------------------------------------+${NC}"

    local roms=(
        "bios/bios.gba|pass|BIOS function tests"
    )
    run_suite "BIOS Functions" "$TEST_ROMS_DIR" \
        "Tests BIOS SWI functions" \
        "${roms[@]}"
}

# Save Type Tests
run_save_tests() {
    echo -e "\n${BLUE}+--------------------------------------+${NC}"
    echo -e "${BLUE}|         SAVE TYPE TESTS              |${NC}"
    echo -e "${BLUE}+--------------------------------------+${NC}"

    local roms=(
        "save/sram.gba|pass|SRAM save type test"
        "save/flash64.gba|pass|Flash 64K save type test"
        "save/flash128.gba|pass|Flash 128K save type test"
        "save/none.gba|pass|No save type test"
    )
    run_suite "Save Types" "$TEST_ROMS_DIR" \
        "Tests save type detection and functionality" \
        "${roms[@]}"
}

# Unsafe Tests
run_unsafe_tests() {
    echo -e "\n${BLUE}+--------------------------------------+${NC}"
    echo -e "${BLUE}|        UNSAFE/EDGE CASE TESTS        |${NC}"
    echo -e "${BLUE}+--------------------------------------+${NC}"

    echo -e "${YELLOW}WARNING: These tests may not pass on real hardware${NC}"

    local roms=(
        "unsafe/unsafe.gba|known_fail|Edge case tests"
    )
    run_suite "Unsafe Tests" "$TEST_ROMS_DIR" \
        "Edge case tests" \
        "${roms[@]}"
}

# Main execution
main() {
    echo -e "${BLUE}+========================================================+${NC}"
    echo -e "${BLUE}|              GBA EMULATOR TEST SUITE                   |${NC}"
    echo -e "${BLUE}+========================================================+${NC}"
    echo ""
    echo "Emulator:    $EMULATOR"
    echo "Timeout:     ${TIMEOUT_SECONDS}s per test"
    echo "Debug mode:  Enabled (DEBUG=1)"
    echo "Test ROMs:   https://github.com/jsmolka/gba-tests"
    echo ""

    trap cleanup EXIT
    clone_test_roms

    case "$TEST_CATEGORY" in
        arm)
            run_arm_tests
            ;;
        thumb)
            run_thumb_tests
            ;;
        memory)
            run_memory_tests
            ;;
        ppu)
            run_ppu_tests
            ;;
        bios)
            run_bios_tests
            ;;
        save)
            run_save_tests
            ;;
        unsafe)
            run_unsafe_tests
            ;;
        *)
            run_arm_tests
            run_thumb_tests
            run_memory_tests
            run_ppu_tests
            run_bios_tests
            run_save_tests
            ;;
    esac

    # Final summary
    echo -e "\n${BLUE}+========================================================+${NC}"
    echo -e "${BLUE}|                    FINAL RESULTS                       |${NC}"
    echo -e "${BLUE}+========================================================+${NC}"
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

    if [ $FAILED -gt 0 ]; then
        exit 1
    fi
}

main "$@"
