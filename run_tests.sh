#!/bin/bash
export DEBUG=1

run_test() {
    local test="$1"
    local result=$(timeout 20 ./build/bin/veloce "$test" 2>&1)
    local name=$(basename "$test")
    if echo "$result" | grep -q "Status code: 0 (PASSED)"; then
        echo "PASS: $name"
    elif echo "$result" | grep -q "Status code:"; then
        local code=$(echo "$result" | grep "Status code:" | head -1 | sed 's/.*Status code: \([0-9]*\).*/\1/')
        echo "FAIL: $name (code: $code)"
    else
        echo "N/A:  $name"
    fi
}

echo "=== MMC3 Tests ==="
for test in ~/Downloads/nes-test-roms/mmc3_test_2/rom_singles/*.nes; do
    run_test "$test"
done

echo ""
echo "=== MMC3 IRQ Tests ==="
for test in ~/Downloads/nes-test-roms/mmc3_irq_tests/*.nes; do
    run_test "$test"
done

echo ""
echo "=== PPU Open Bus Test ==="
run_test ~/Downloads/nes-test-roms/ppu_open_bus/ppu_open_bus.nes
