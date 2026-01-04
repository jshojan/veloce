# GBA Emulator Test Suite

This directory contains the test infrastructure for validating the GBA emulator plugin's accuracy using [gba-tests](https://github.com/jsmolka/gba-tests) by jsmolka.

## Overview

The GBA test suite validates emulator accuracy across multiple categories:

| Category | Priority | Description |
|----------|----------|-------------|
| ARM | Critical | ARM instruction set tests (data processing, branches, multiply, load/store) |
| Thumb | Critical | Thumb instruction set tests (arithmetic, logical, shifts, memory) |
| Memory | High | Memory access patterns, mirroring, and video memory operations |
| PPU | High | Graphics rendering including mode 4 and color output |
| BIOS | Medium | BIOS SWI function tests (requires BIOS or HLE) |
| Save | Medium | Save type detection and functionality (SRAM, Flash) |
| Unsafe | Low | Edge cases that may not pass on real hardware |

## Test ROM Source

Tests are sourced from: https://github.com/jsmolka/gba-tests

### Runtime Cloning

Test ROMs are **not** included in this repository. Instead, they are automatically cloned at runtime when tests are first run:

1. **On first run**: The test scripts clone the repository into `gba-test-roms/`
2. **On completion**: By default, the cloned repository is automatically deleted
3. **With `--keep`**: Use this flag to retain the test ROMs for subsequent runs

This approach:
- Keeps the main repository clean and small
- Ensures tests always use the latest version of test ROMs
- Avoids distributing third-party test ROMs

The `gba-test-roms/` directory is listed in `.gitignore` in case you use `--keep`.

GBA test ROMs are assembled with [FASMARM](https://arm.flatassembler.net/) and are MIT licensed.

## Quick Start

```bash
# Run all GBA tests
./run_tests.sh

# Run specific test categories
./run_tests.sh arm          # ARM instruction tests
./run_tests.sh thumb        # Thumb instruction tests
./run_tests.sh memory       # Memory tests
./run_tests.sh ppu          # PPU tests
./run_tests.sh bios         # BIOS function tests
./run_tests.sh save         # Save type tests

# Options
./run_tests.sh --verbose    # Show detailed output
./run_tests.sh --keep       # Keep test ROMs after completion
```

## Test Result Detection

The GBA emulator outputs test results in debug mode:
- `[GBA] PASSED` - All tests in the ROM passed
- `[GBA] FAILED` - One or more tests failed

Tests use Mode 4 background to display visual pass/fail results and store a result code in register R12.

## Directory Structure

```
tests/
  run_tests.sh          # Main test runner script
  test_runner.py        # Python test runner (detailed output)
  test_config.json      # Test configuration
  gba-test-roms/        # Cloned test ROMs (gitignored)
```

## Current Test Status

| Test | Status | Notes |
|------|--------|-------|
| arm.gba | Passing | ARM instruction set |
| thumb.gba | Passing | Thumb instruction set |
| memory.gba | Passing | Memory access |
| ppu/hello.gba | Passing | Basic PPU |
| bios.gba | Passing | HLE BIOS with read protection |
| save/sram.gba | Passing | 32KB SRAM |
| save/flash64.gba | Passing | 64KB Flash |
| save/flash128.gba | Passing | 128KB Flash with bank switching |

## Contributing

When adding new tests:
1. Add the test ROM to the appropriate category in `run_tests.sh`
2. Document any special requirements (BIOS, save type, etc.)
3. Update the test status table above

## References

- [gba-tests Repository](https://github.com/jsmolka/gba-tests)
- [GBATEK - GBA Hardware Documentation](https://problemkaputt.de/gbatek.htm)
- [ARM7TDMI Technical Reference Manual](https://developer.arm.com/documentation/ddi0234/latest)
