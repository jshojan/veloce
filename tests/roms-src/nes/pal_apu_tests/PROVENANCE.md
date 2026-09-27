# Provenance: nes/pal_apu_tests (01-03)

Upstream: `christopherpow/nes-test-roms`, pinned at `95d8f621ae55cee0d09b91519a8989ae0e64753b`
(sub-tree `pal_apu_tests/`). Test code by Shay Green (blargg), 2005. Its `readme.txt`
has no licence statement, only the author's contact address, and there is no `LICENSE`
file. The plan (section 8) treats blargg's NES test ROMs as freely usable. Nothing from
upstream is committed here: the recipe fetches the sources at the pinned
SHA and translates them at build time. The Veloce-authored parts are shared files under
`tests/roms-src/include/runtime2005/`. The ROMs are built by
`tests/roms-src/build.py nes/pal_apu_tests` and are never committed.

Only 01.len_ctr, 02.len_table and 03.irq_flag are built. They are the three that
`cores/nes/tests/test_config.json` wires. 04-11 time the PAL frame sequencer and
cannot pass on this NTSC-only core, so they stay unwired until a PAL mode exists.

## Why a rebuild (NES-3)

The config listed the three ROMs as `memory` tests. The 2005-era runtime they use
reports only on screen and by beeps (zero-page `$F8` result code). It never writes
`$6000`, so every run ended as `RUNS` ("no result signal").

The plan's NES-3 line ("rebuild against instr_test-v5/source/common") does not fit
these three ROMs, because they are not ca65 sources on blargg's modern shell. They
were written for blargg's own 2005 devcart assembler. `runtime_rom_common.a` also
includes `ppu_util.a`, and upstream never shipped that file in this directory. The
inventory's own detailed plan for this suite ("same port" as the other 2005 suites,
via a shared `runtime2005`) is what was done, scoped to what 01-03 need.

## What the build does

1. **Dialect translation.** `tests/roms-src/include/runtime2005/to_asm6f.sed` rewrites
   01/02/03, `prefix_apu.a`, `sync_apu.a` and `delays.a` from the 2005 dialect into
   asm6f. It makes three textual changes:
   - drops `.code`;
   - turns each `:` anonymous label into an asm6f `-` + `+` label pair at the same
     address, because asm6f's anonymous labels are directional;
   - renames the label `error`, because `ERROR` is an asm6f directive.

   Instruction text is otherwise untouched.
2. **Runtime replacement.** blargg's `validation.a`, `runtime_rom*.a`, `console.a`
   and `debug.a` are not used. Staged in their place:
   - `runtime2005/validation.a`: the same entry points (`result = $f8`,
     `error_if_*`, `tests_passed`, `report_final_result`, the default NMI/IRQ). It
     reports the `$F8` code as `CHECK 1 PASS|FAIL <test_name> ... / END` over
     `$401E`, using `veloce-test-asm6f.inc`.
   - `runtime2005/veloce_rom2005.a` / `_end.a`: an NROM-128 wrapper with 16K PRG at
     `$C000` and 8K CHR of zeros. The reset shim does the devcart runtime's init
     (`sei`/`cld`/stack, PPU off, zero page cleared, two VBL waits), writes the
     `VELOCE 1 nes pal_apu_tests.<rom>` header, and jumps to the test's own `reset:`.
3. The build generates a four-line top file per ROM (in the recipe's commands) that
   includes these pieces in order.

## Expected values: where every verdict comes from

Every verdict is upstream's own: the `$F8` result code the unmodified test logic
stores and reports. 1 means passed, 0 means internal error, and n >= 2 is the failing
sub-test, as listed in upstream's `readme.txt`:

| ROM | `CHECK 1` name (upstream `test_name`) | Codes (readme.txt) |
|---|---|---|
| 01.len_ctr | `APU LENGTH COUNTER` | 2 load/$4015, 3 table/timing, 4 $80->$4017 clocks, 5 $00->$4017 doesn't, 6 $4015 clear, 7 no reload while disabled, 8 halt |
| 02.len_table | `APU LENGTH TABLE` | 2+ = failing table entry + 2 (the 32-entry table is in the source) |
| 03.irq_flag | `APU FRAME IRQ FLAG` | 2-7 per the readme |

A failure is `CHECK 1 FAIL <name> exp=01 got=<code> code=<code>`.

**Region.** The readme says the suite was "tested on a PAL NES and all give a passing
result". Tests 01-03 do not depend on PAL timing:
- the 250 ms and 20 ms delays are loop-counted and NTSC-calibrated (about 1790 clocks
  per "msec");
- 20 ms is longer than both the NTSC (29830 clocks) and the PAL (33254 clocks) mode-0
  frame IRQ period;
- the length table is the same on both regions.

A correct NTSC APU therefore passes them too. The plan's "known_fail until a PAL mode
exists" applies to 04-11, not to these three.

## Golden gate: behavioural-only (not byte-for-byte)

`golden_gate: false`. Upstream's binaries were built by blargg's 2005 assembler with the
devcart runtime that this build deliberately replaces, and one of that runtime's files
is missing at the pinned SHA. A byte-identical rebuild is impossible.

Verified behaviourally instead:

- **Reproducible.** `build.py nes/pal_apu_tests --check` rebuilds all three ROMs
  byte-identical to `rom_manifest.json`.
- **The FAIL path works.** As a negative control, we changed 02.len_table's expected
  table entry 3 from 2 to 3 in a scratch copy. The ROM then reports
  `CHECK 1 FAIL APU LENGTH TABLE exp=01 got=05 code=5`, which is entry 3 + 2, exactly
  as the readme defines.
