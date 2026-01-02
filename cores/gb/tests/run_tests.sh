#!/bin/bash
#
# Game Boy Emulator Test Suite
# Runs Blargg's test ROMs through the Veloce emulator to validate accuracy
#
# Usage:
#   ./run_tests.sh              # Run all GB tests
#   ./run_tests.sh cpu_instrs   # Run CPU instruction tests only
#   ./run_tests.sh dmg_sound    # Run sound tests only
#   ./run_tests.sh --python     # Use Python test runner for detailed output
#
# Test ROM Sources:
#   GB: https://github.com/retrio/gb-test-roms (Blargg's tests)
#
# Test Result Format:
#   Blargg tests output results via the serial link port:
#   - "Passed" indicates test passed
#   - "Failed" indicates test failed
#   In debug mode, the emulator outputs [GB] PASSED or [GB] FAILED.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$(dirname "$PLUGIN_DIR")")"
TEST_ROMS_DIR="$SCRIPT_DIR/gb-test-roms"
TEST_ROMS_REPO="https://github.com/retrio/gb-test-roms.git"

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

# Parse arguments
USE_PYTHON=false
TEST_CATEGORY="all"

while [[ $# -gt 0 ]]; do
    case $1 in
        --python)
            USE_PYTHON=true
            shift
            ;;
        *)
            TEST_CATEGORY="$1"
            shift
            ;;
    esac
done

# Clone test ROMs if needed
if [ ! -d "$TEST_ROMS_DIR" ]; then
    echo "Cloning Game Boy test ROMs..."
    git clone --depth 1 "$TEST_ROMS_REPO" "$TEST_ROMS_DIR"
fi

# If Python flag is set, use the Python test runner
if $USE_PYTHON; then
    if [ -f "$SCRIPT_DIR/test_runner.py" ]; then
        python3 "$SCRIPT_DIR/test_runner.py" "$TEST_CATEGORY"
    else
        echo "Error: test_runner.py not found"
        exit 1
    fi
    exit 0
fi

# Run a single test ROM and check result
run_test() {
    local rom="$1"
    local name=$(basename "$rom" .gb)
    local timeout_seconds=30

    echo -n "Testing: $name... "

    # Run with DEBUG=1 to get serial output
    local output=$(DEBUG=1 timeout $timeout_seconds "$EMULATOR" "$rom" 2>&1) || true

    if echo "$output" | grep -q "\[GB\] PASSED"; then
        echo "PASSED"
        return 0
    elif echo "$output" | grep -q "\[GB\] FAILED"; then
        echo "FAILED"
        return 1
    elif echo "$output" | grep -q "Passed"; then
        echo "PASSED"
        return 0
    elif echo "$output" | grep -q "Failed"; then
        echo "FAILED"
        return 1
    else
        echo "TIMEOUT/UNKNOWN"
        return 2
    fi
}

# Find and run tests
PASSED=0
FAILED=0
TIMEOUT=0

case "$TEST_CATEGORY" in
    cpu_instrs|cpu)
        TESTS=$(find "$TEST_ROMS_DIR/cpu_instrs" -name "*.gb" 2>/dev/null || true)
        ;;
    dmg_sound|sound)
        TESTS=$(find "$TEST_ROMS_DIR/dmg_sound" -name "*.gb" 2>/dev/null || true)
        ;;
    all)
        TESTS=$(find "$TEST_ROMS_DIR" -name "*.gb" 2>/dev/null || true)
        ;;
    *)
        TESTS=$(find "$TEST_ROMS_DIR/$TEST_CATEGORY" -name "*.gb" 2>/dev/null || true)
        ;;
esac

if [ -z "$TESTS" ]; then
    echo "No test ROMs found for category: $TEST_CATEGORY"
    exit 1
fi

echo "Running Game Boy tests..."
echo "========================="

for rom in $TESTS; do
    if run_test "$rom"; then
        ((PASSED++)) || true
    else
        result=$?
        if [ $result -eq 2 ]; then
            ((TIMEOUT++)) || true
        else
            ((FAILED++)) || true
        fi
    fi
done

echo ""
echo "========================="
echo "Results: $PASSED passed, $FAILED failed, $TIMEOUT timeout/unknown"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
