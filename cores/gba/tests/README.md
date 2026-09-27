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

# Run by subsystem (cpu, memory, timing, ppu, misc, mapper) or suite id
./run_tests.sh cpu          # all CPU suites
./run_tests.sh cpu_arm      # jsmolka arm.gba only
./run_tests.sh bios save    # BIOS and save-type suites

# Options
./run_tests.sh --json       # scorecard JSON (includes result-file checks)
./run_tests.sh -v           # per-test verdict lines
./run_tests.sh --keep       # Keep test ROMs after completion
```

## Test Result Detection

Verdicts come only from the result file named by `VELOCE_TEST_OUT`
([VELOCE-RESULT/1](../../../docs/testing/VELOCE-RESULT.md)); the emulator prints
no verdict to stdout/stderr. The GBA plugin writes two kinds of lines into it:

- `mgba` channel: whatever the ROM flushes through the mGBA debug registers
  (`0x4FFF600`/`0x4FFF700`), one line per flush, plus a `#VELOCE level=<n>` note
  when the log level changes.
- `r12` adapter: unmodified jsmolka/alyosha ROMs park in a `b .` loop with the
  failing test number in a register (0 = all pass). After 10 stable frames the
  plugin writes `#VELOCE adapter=r12`, `REG pc=.. r12=..` and `END 1/1` or
  `END 0/1 code=<n>`.

Run one ROM by hand:

```bash
VELOCE_TEST_OUT=/tmp/arm.result FRAMES=600 ../../../build/bin/veloce gba-test-roms/arm/arm.gba
cat /tmp/arm.result
```

## Building VELOCE-RESULT/1 ROMs from source

Modified/new GBA test ROMs speak the channel above directly rather than
relying on the `r12` adapter. Two emitter libraries live under
[`tests/roms-src/include/`](../../../tests/roms-src/include/) (repo root),
one per assembler/toolchain the GBA suites use:

- **FASMARM** (jsmolka/alyosha, FuzzARM): `fasmarm/veloce.inc` (macros:
  `m_vt_begin`/`m_vt_test`/`m_vt_pass`/`m_vt_fail`/`m_vt_reg`/`m_vt_log`/
  `m_vt_end`, plus an optional `m_vt_open` handshake probe) +
  `fasmarm/veloce_code.inc` (the actual subroutine code -- `include` it once,
  after the ROM's header, never before, or it will overwrite the boot logo).
- **devkitARM C** (nba-hw-test, new C ROMs): `devkitarm/veloce_test.h`
  (`vt_begin`/`vt_test`/`vt_pass`/`vt_fail`/`vt_reg`/`vt_log`/`vt_end`), built
  on a vendored `mgba_printf()` (`devkitarm/vendor/mgba.c`, MIT, from
  [mgba-emu/suite](https://github.com/mgba-emu/suite); see
  `devkitarm/vendor/PROVENANCE.md`).

`tests/roms-src/gba/veloce_smoke_fasmarm/` and `.../veloce_smoke_devkitarm/`
are Veloce's own smoke ROMs exercising every macro/function above with fixed
values; each has a `recipe.json` built through
[`tests/roms-src/build.py`](../../../tests/roms-src/build.py) against the
pinned `veloce/rom-toolchain` image, and both produce a byte-identical
transcript across repeated runs (the determinism a scored ROM channel
depends on). `tests/roms-src/gba/jsmolka_golden_arm/` and
`.../nba_golden_dma_latch/` golden-gate one representative upstream ROM per
toolchain (FASMARM, devkitARM) -- rebuilt unmodified from its pinned source
and checked byte-for-byte against the shipped binary -- proving the
toolchain reproduces these suites before any of them are patched to speak
this channel (see each suite's `PROVENANCE.md`).

`tests/roms-src/gba/nba_hw_test/` is the first patched upstream suite: nba's
10 scored hw-test ROMs, built from the pinned Codeberg source plus
`patches/0001-veloce-result-channel.patch`, which makes nba's shared
`test_expect*()` also emit `CHECK` lines and `END` over the mGBA registers.
Because their expected values are hardware cycle counts, the suite ships
`verify_windows.py`: it checks that every function inside a measured window
of the rebuilt ROM is instruction-for-instruction the one in nba's shipped
binary (`windows.json` records the few reviewed differences). The config
addresses the rebuilt ROMs as `veloce/nba_hw_test/<rom>.gba`; `runner.py`
links `_roms/veloce` to `build/roms/gba`, so without a build those tests SKIP.

```bash
tests/roms-src/build.py gba/nba_hw_test              # from the repo root
tests/roms-src/gba/nba_hw_test/verify_windows.py
cd cores/gba/tests && python3 runner.py timing_dma_nba
```

## Directory Structure

```
tests/
  run_tests.sh          # Wrapper around runner.py (shared testkit)
  runner.py             # Scorecard runner (shared testkit)
  test_runner.py        # Legacy flat-summary runner (per-repo ROM resolution)
  test_config.json      # Test configuration
  gba-test-roms/        # Cloned test ROMs (gitignored)
  _roms/                # Merged ROM view runner.py builds (gitignored)
    veloce/             # -> build/roms/gba (rom_variant veloce rebuilds)
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
