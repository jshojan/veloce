# SNES Emulator Test Suite

Automated test framework for validating the Veloce SNES emulator accuracy using Blargg's test ROMs.

## Quick Start

```bash
# Run all tests
./run_tests.sh

# Or directly with Python
python3 test_runner.py
```

## Requirements

- Python 3.8+
- Built Veloce emulator (`build/bin/veloce`)
- Internet connection (for downloading test ROMs on first run)

## Test ROMs

This test suite uses **Blargg's SNES test ROMs**, which are considered the gold standard for SNES emulator testing. The tests are automatically downloaded from:

- **Primary**: https://github.com/MiSTer-devel/SNES_MiSTer (blargg_testroms.zip)
- **Fallback**: https://gitlab.com/higan/snes-test-roms (blargg-spc-6 directory)

### Available Tests

| Test ROM | Description |
|----------|-------------|
| `spc_dsp6.sfc` | Comprehensive DSP register and behavior tests |
| `spc_mem_access_times.sfc` | Memory access timing validation |
| `spc_smp.sfc` | SPC700 instruction set tests |
| `spc_timer.sfc` | SPC700 timer functionality tests |

### Known Issues

- **spc_dsp6.sfc**: This test is known to fail on 3-chip SNES consoles and passes only on higan/bsnes. It may also freeze on some hardware configurations.

## Building the Emulator

Before running tests, build the emulator from the project root:

```bash
cd /path/to/emulatorplatform
cmake -B build
cmake --build build
```

## Usage

### Shell Script

```bash
# Run all tests
./run_tests.sh

# Run specific test categories
./run_tests.sh spc          # SPC700/DSP tests

# Options
./run_tests.sh --keep       # Keep test ROMs after completion
./run_tests.sh --verbose    # Show detailed output
./run_tests.sh --json       # JSON output (for CI integration)
./run_tests.sh -v --keep    # Combine options
```

### Python Script

```bash
# All tests
python3 test_runner.py

# Categories
python3 test_runner.py spc
python3 test_runner.py blargg

# Options
python3 test_runner.py --keep           # Keep ROMs
python3 test_runner.py -v               # Verbose
python3 test_runner.py --json           # JSON output
python3 test_runner.py --timeout 60     # Custom timeout (seconds)
```

## Blargg Test Result Detection

Blargg's NES tests write results to specific memory addresses for automated detection:

| Address | Purpose |
|---------|---------|
| `$6000` | Status code (0x00=pass, 0x01-0x7F=fail, 0x80=running, 0x81=needs reset) |
| `$6001-$6003` | Signature bytes (0xDE 0xB0 0x61) |
| `$6004+` | Result text (null-terminated ASCII) |

**Note**: SNES SPC tests primarily communicate results via APU ports and display output on screen rather than using the $6000 memory interface. The test framework monitors for:
- Blargg signature detection at $6001-$6003
- Status code changes at $6000
- Console output patterns (PASSED/FAILED)

If automatic detection fails, tests are marked as "RUNS" (executed successfully but result indeterminate).

## Test Categories

| Category | Description |
|----------|-------------|
| `spc` | SPC700 audio processor tests |
| `apu` | Alias for `spc` |
| `dsp` | Alias for `spc` |
| `blargg` | All Blargg tests |

## Output Formats

### Standard Output

```
============================================================
     SNES EMULATOR TEST SUITE - BLARGG TESTS
============================================================

Emulator: /path/to/build/bin/veloce
Test ROMs: /path/to/blargg-test-roms
Timeout:  60s per test
Total tests: 4

Blargg tests write results to $6000:
  $6000: Status (0x00=pass, 0x01-0x7F=fail code)
  $6004+: Result text

=== Blargg SPC700/DSP Tests ===
  [PASS] spc_timer.sfc
  [RUNS] spc_smp.sfc
  [KNOWN] spc_dsp6.sfc
  Passed: 1 | Runs: 1 | Known: 1

============================================================
                   FINAL RESULTS
============================================================

  Test Suite                          Pass   Fail  Known
  ----------------------------------- ------ ------ ------
  Blargg SPC700/DSP Tests                 1      0      1
  ----------------------------------- ------ ------ ------
  TOTAL                                   1      0      1

  Passed:       1
  Runs:         1
  Failed:       0
  Known Issues: 1
  Timeouts:     0
  Skipped:      0

  Pass Rate: 100.0%
```

### JSON Output (for CI)

```bash
python3 test_runner.py --json
```

```json
{
  "summary": {
    "passed": 1,
    "runs": 1,
    "failed": 0,
    "known_failures": 1,
    "skipped": 0,
    "timeouts": 0,
    "errors": 0,
    "total_run": 3,
    "pass_rate": 66.7
  },
  "suites": [...]
}
```

## How It Works

1. **Download Test ROMs**: On first run, downloads Blargg's test ROMs from GitHub/GitLab
2. **Discovery**: Scans for `.sfc` files in the test ROM directory
3. **Execution**: Runs each ROM with `DEBUG=1 HEADLESS=1` environment variables
4. **Detection**: Monitors for:
   - Blargg signature at $6001-$6003
   - Status code at $6000
   - Console output patterns (BLARGG_STATUS, PASSED, FAILED)
5. **Cleanup**: Removes test ROMs unless `--keep` is specified

## Debug Output

The SNES emulator outputs Blargg test results when `DEBUG=1` is set:

```cpp
// From cores/snes/src/debug.hpp
// Blargg test detection at $6000-$60FF
// Outputs: "BLARGG_STATUS: 0x00" and "Status code: 0 (PASSED)"
```

## Troubleshooting

### "Cannot find veloce binary"

Build the emulator first:
```bash
cmake -B build && cmake --build build
```

### "No test suites found"

Check that test ROMs downloaded correctly:
```bash
ls -la blargg-test-roms/
```

### Tests timing out

Increase the timeout:
```bash
python3 test_runner.py --timeout 120
```

### Tests showing as "RUNS"

This indicates the test executed without crashing but automatic result detection was not possible. This is expected for some SPC tests that display results visually rather than writing to memory.

Run with verbose output to see details:
```bash
python3 test_runner.py -v
```

## Related Resources

- [NESdev Forum - Blargg's SPC Test ROMs](https://forums.nesdev.org/viewtopic.php?t=18005)
- [GitLab - higan/snes-test-roms](https://gitlab.com/higan/snes-test-roms)
- [SNES Dev Wiki](https://wiki.superfamicom.org/) - Hardware documentation
- [SPC700 Reference](https://wiki.superfamicom.org/spc700-reference) - Audio processor docs
- [Anomie's Docs](https://www.romhacking.net/documents/197/) - Classic SNES documentation
- [fullsnes](https://problemkaputt.de/fullsnes.htm) - Comprehensive SNES specs

## References

- [NESdev Forum Discussion](https://forums.nesdev.org/viewtopic.php?t=18005) - Original Blargg SPC test release
- [MiSTer SNES Issue #201](https://github.com/MiSTer-devel/SNES_MiSTer/issues/201) - Test compatibility notes

## License

MIT License - Same as the Veloce Emulation Platform
