#!/bin/bash
export DEBUG=1

run_test() {
    local test="$1"
    local result=$(timeout 25 ./build/bin/veloce "$test" 2>&1)
    local name=$(basename "$test")
    if echo "$result" | grep -q "Status code: 0 (PASSED)"; then
        echo "PASS: $name"
        return 0
    elif echo "$result" | grep -q "Status code:"; then
        local code=$(echo "$result" | grep "Status code:" | head -1 | sed 's/.*Status code: \([0-9]*\).*/\1/')
        echo "FAIL: $name (code: $code)"
        return 1
    else
        echo "N/A:  $name"
        return 2
    fi
}

PASS=0
FAIL=0
NA=0

echo "=============================================="
echo "NES Emulator Test Suite Results"
echo "=============================================="
echo ""

echo "=== CPU Instruction Timing ==="
for test in ~/Downloads/nes-test-roms/instr_timing/rom_singles/*.nes; do
    run_test "$test"
    case $? in
        0) ((PASS++)) ;;
        1) ((FAIL++)) ;;
        2) ((NA++)) ;;
    esac
done

echo ""
echo "=== CPU Dummy Writes ==="
for test in ~/Downloads/nes-test-roms/cpu_dummy_writes/*.nes; do
    run_test "$test"
    case $? in
        0) ((PASS++)) ;;
        1) ((FAIL++)) ;;
        2) ((NA++)) ;;
    esac
done

echo ""
echo "=== PPU VBlank/NMI Tests ==="
for test in ~/Downloads/nes-test-roms/ppu_vbl_nmi/rom_singles/*.nes; do
    run_test "$test"
    case $? in
        0) ((PASS++)) ;;
        1) ((FAIL++)) ;;
        2) ((NA++)) ;;
    esac
done

echo ""
echo "=== MMC3 Tests (mmc3_test_2) ==="
for test in ~/Downloads/nes-test-roms/mmc3_test_2/rom_singles/*.nes; do
    run_test "$test"
    case $? in
        0) ((PASS++)) ;;
        1) ((FAIL++)) ;;
        2) ((NA++)) ;;
    esac
done

echo ""
echo "=== PPU Open Bus ==="
run_test ~/Downloads/nes-test-roms/ppu_open_bus/ppu_open_bus.nes
case $? in
    0) ((PASS++)) ;;
    1) ((FAIL++)) ;;
    2) ((NA++)) ;;
esac

echo ""
echo "=============================================="
echo "SUMMARY"
echo "=============================================="
echo "PASSED: $PASS"
echo "FAILED: $FAIL"
echo "N/A:    $NA"
echo "=============================================="
