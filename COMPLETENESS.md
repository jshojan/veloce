# Veloce Accuracy Scorecard

This is the project's honest, evidence-based statement of how complete and
cycle-accurate each emulator core is. Legacy headline percentages are produced by the
shared scoring methodology described in [TESTING.md](TESTING.md#how-the-completeness-percentage-is-computed)
and is reproducible by running the corresponding suite. Nothing here is a
hand-estimated "feels done" number.

Read this document alongside [TESTING.md](TESTING.md), which explains the harness,
the result-detection protocols, and the exact formula. The rule that governs
every number below: **only verified passes count toward the headline; tests that
run without a readable verdict are reported as "unverified" and contribute zero,
and documented hardware quirks are counted separately, never as passes.**

## How to read these numbers

- **Headline accuracy** is the importance-weighted average of per-subsystem
  scores over the subsystems that have at least one *scored* test. A scored test
  is one with a clean PASS or FAIL verdict; RUNS / SKIP / known_fail are
  excluded from the denominator and reported in their own columns.
- **A high headline with low coverage is caveated, not hidden.** Where a large
  block of tests is currently unverifiable (missing reference hashes, a missing
  trace hook, an undetected breakpoint protocol), that is called out explicitly
  so the headline is never mistaken for full coverage.
- **Subsystem importance weights** (cpu 1.00 > ppu 0.90 > timing 0.85 >
  memory 0.70 > mapper 0.65 > apu 0.55 > misc 0.30) mean the CPU and PPU drive
  the headline; a strong APU cannot mask a weak CPU.

Regenerate any console's authoritative scorecard with
`cd cores/<console>/tests && ./run_tests.sh --json`, or the whole platform with
`python tests/run_all.py`.

Original ROM corpora report the fraction of their explicitly enumerated domain
that was observed and matched, separately from the legacy weighted scorecard.
Run `ctest --test-dir build -L owned --output-on-failure`; read
[HARDWARE-COVERAGE.md](docs/HARDWARE-COVERAGE.md) for the full remaining scope.
No full-core completion percentage has been established.

## Platform summary

| Console | Suite size | Verified headline | Verification state |
|---------|-----------|-------------------|--------------------|
| [NES](#nes) | 172 external tests / 32 suites, plus 2 original ROMs | ~90-94% historical external subset | Original zero-page ADC/SBC corpus additionally verifies 524,288 A/P pairs; other hardware domains remain |
| [SNES](#snes) | 67 external tests / 19 suites, plus 2 original ROMs | 68,864 settled math cases and 504,832 timing bytes verified | Original multiplication domain is exhaustive; division covers boundaries; timing covers declared patterns/phases; external catalog remains unscored |
| [Game Boy](#game-boy) | 225 external tests / 31 suites, plus 3 original ROMs | Verified subset only | Blargg serial subset, 2 WRAM checks, 8,388,608 ALU pairs and 73,728 unary pairs verified; Mooneye remains unverified |
| [GBA](#gba) | 132 external tests / 29 suites, plus 1 original ROM | Partial; 65,536 original ARM shift cases verified | Legacy CPU/memory/BIOS/saves subsets verified; timing/PPU and remaining original hardware corpus pending |
| N64 | 7 original ROMs; no external catalog | Declared integer/merge/controller/interrupt domains verified | Experimental marked homebrew; full hardware completion unknown; [limits](cores/n64/README.md) |
| Virtual Boy | 7 original ROMs; no external catalog | 6,512,640 result/PSW pairs plus 4,210,688 multiply/divide and 4,685,824 TRAP/illegal and 4,982,784 address-trap snapshots plus 17,055,744 CAXI/port and 14,680,064 arithmetic bit-string and 100,712,448 immediate-instruction snapshots | Experimental marked homebrew; remaining CPU/NVC, VIP/VSU, controllers and timing pending; [limits](cores/vb/README.md) |
| Nintendo DS | 8 original ROMs; no external catalog | 44,238,336 integer plus 38,338,560 multiply plus 51,426,816 Thumb result/CPSR/source/PC observations matched; 49,694,208 nine-word memory and 9,437,184 eight-word long-call observations matched; 3,720,960 stack/block observations matched; 4,096/4,096 digital keypad combinations matched; 100-frame TCP host/viewer states and eight guest words match baseline/oracle; DS wireless multiplayer pending; ARM7 unpredictable multiply flags excluded | Experimental marked ARM/Thumb integer homebrew; firmware, ARM block transfers, graphics/audio, peripherals and DSi enhanced mode pending; [limits](cores/nds/README.md) |
| GameCube | 6 original ROMs; no external catalog | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary snapshots matched | Experimental supplied integer profile; full hardware completion unknown; [limits](cores/ppc/README.md) |
| Wii | 6 original ROMs; no external catalog | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary snapshots matched | Experimental supplied integer profile; full hardware completion unknown; [limits](cores/ppc/README.md) |

The legacy four-console platform mean reported by `tests/run_all.py` weights each
console equally; N64, Virtual Boy, DS, GameCube and Wii owned corpora are reported separately.
Because three of the four cores have large currently-unverifiable blocks, the
platform mean understates implemented behavior and overstates nothing; it will
rise as reference hashes and the missing emitters land, without any code becoming
more correct.

---

## NES

Headline accuracy: **approximately 90-94%**, dominated by CPU and PPU. This is
the most thoroughly *verified* core because most of its critical tests use the
Blargg `$6000` `memory` protocol, which the binary reads directly under `DEBUG=1`
(no reference hashes required). Measured 2026-06-02 against `build/bin/veloce`.

Measured 2026-10-09: the original
[ADC/SBC register corpus](tests/roms-src/nes/alu_exhaustive/README.md) matched
524,288/524,288 accumulator/PHP-status pairs (100% of all two-byte operand, carry,
and decimal-flag combinations for these zero-page instructions). The injected
ADC-to-SBC mutation caused 256 mismatches. The raw file and per-group JSON report
are produced in `build/artifacts/owned/nes_alu/`. This proves that specific
arithmetic domain, and is not folded into the historical weighted headline.

| Subsystem | Importance | Verified score | Basis |
|-----------|-----------|----------------|-------|
| CPU | 1.00 | ~96% | All 16 `instr_test-v5` official singles + `official_only` PASS; `instr_misc`, `instr_timing` (instr + branch), dummy reads, dummy writes (RMW), `exec_space_ppuio` PASS. Lone scored fail: `exec_space_apu`. `03-immediate` is `known_fail` (unstable opcodes, chip-dependent). |
| PPU | 0.90 | ~92% | `ppu_vbl_nmi` 9/10 PASS (`05-nmi_timing` known_fail); `oam_read`, `oam_stress`, `ppu_open_bus` PASS. The sprite-hit / overflow / 2005 palette suites are visual and currently unverified, not failing. |
| Mapper | 0.65 | ~88% | MMC3 core verified: `mmc3_test_2` 1/2/3/5/6 PASS, `mmc3_test` v1 1/2/3/5 PASS. `4-scanline_timing` and `6-MMC6` known_fail. MMC1/MMC5 tests are visual/unverified. |
| APU | 0.55 | ~70% | Functional/length/IRQ-flag gates pass (`apu_test` 1/2/3/7/8); the cycle-accurate frame-counter cluster fails (`4-jitter`, `5-len_timing`, `6-irq_flag_timing`). |
| Timing | 0.85 | ~40% (weakest) | `cpu_interrupts_v2/1-cli_latency` PASS but `2/3/4` FAIL; the whole DMA-collisions suite is expected-fail pending cycle-accurate DMC-DMA halt cycles. |
| Misc | 0.30 | unverified | `read_joy3` needs input injection or reference hashes. |

**Verified strengths (cycle-accurate passes, not just functional).** The deep
`ppu_vbl_nmi` cycle-timing chain (VBL set/clear time, suppression, NMI on/off
timing, even/odd frame and dot timing), the MMC3 A12/scanline cycle tests,
`cpu_dummy_writes`, `cpu_exec_space_ppuio`, `oam_stress`, and `ppu_open_bus` all
pass under the `memory` protocol. These are the strongest evidence (rigor weight
3.0) and they are real.

**Verified weaknesses (the real accuracy debt).**

- **Interrupt / DMA cycle timing is the largest deficit.**
  `cpu_interrupts_v2` 2/3/4 fail and the DMA-collision suite is expected-fail.
- **APU frame-counter sub-cycle timing** (`apu_test` 4/5/6) fails.
- `cpu_exec_space_apu` fails (executing from `$4xxx` APU/open-bus space).
- MMC6 and the MMC3 `4-scanline_timing` 104-cycle A12 discrepancy remain.

**Honest coverage caveats (why the headline is "of the verified subset").**

- **nestest golden trace cannot run.** The single strongest per-cycle CPU proof
  needs a `TRACE=1` instruction-stream emitter in the binary *and* the golden
  `nestest.log` (not shipped). It is reported `known_fail` (unverified), never a
  pass. This is the biggest single accuracy-evidence gap.
- **A detection split was found by measurement.** The older 2005-era Blargg
  suites (`sprite_hit_tests_2005`, `sprite_overflow_tests`, `vbl_nmi_timing`,
  `blargg_ppu_tests_2005`, `blargg_nes_cpu_test5`, `cpu_dummy_reads`,
  `dmc_tests`, `blargg_apu_2005.07.30`, `mmc3_irq_tests`) report on-screen only
  and never write `$6000`. They were reclassified to `screenshot-crc` and ship
  `known_fail` until reference hashes are generated. The prior README falsely
  scored several of these as memory passes.
- **Reset-injection tests** (`cpu_reset/*`, `apu_reset/*`,
  `blargg_apu_2005/09.reset_timing`) need a mid-run reset hook the headless
  harness lacks.
- **Mapper breadth is narrow** versus the 20+ implemented mappers: only
  MMC1/MMC3/MMC5 have ROM tests. NROM/UxROM/CNROM/AxROM/MMC2/MMC4/VRC/FME-7/Namco
  have no functional ROM gate (`pinobatch/holy-mapperel` is referenced, not
  auto-fetched).
- **PAL** timing is uncovered as passing (`pal_apu_tests` document the NTSC-only
  gap). `ppu_read_buffer` and `cpu_interrupts_v2/5-branch_delays_irq` stall at
  `0x80` (running) within the frame budget and report unverified.

> The headline reflects the verified `memory`-detected subset. The 62
> `screenshot-crc` tests and nestest contribute **zero** until reference hashes
> or a trace emitter exist, so true coverage breadth is lower than the headline
> implies and is surfaced as unverified rather than counted.

---

## SNES

Verified original corpus: **68,864/68,864 settled math cases matched** on
2026-10-09: all 65,536 unsigned multiplication operand pairs, including the
RDDIV side effect, and 3,328 division cases (13 selected dividends, all 256
divisors). The [original ROM and integer host oracle](tests/roms-src/snes/math_registers/README.md)
use register files, with a deliberately failing stimulus and malformed-file
controls. Multiplication previously left RDDIV unchanged: the new ROM recorded
65,280 mismatches before the core fix, and zero afterward. Math latency and
intermediate outputs remain unverified and currently calculated immediately.
This domain coverage does not establish overall SNES completion.

The external 67-test / 19-suite catalog remains unscored. Most ROMs draw results
on screen and require trusted reference hashes. The following table describes
that external catalog, separate from the original math corpus.

| Subsystem | Importance | Verifiable today | Basis |
|-----------|-----------|------------------|-------|
| APU (SPC700 + DSP) | 0.55 | **0 verified** (`memory`-detected, but no signal) | Blargg SPC ROMs (`spc_smp`, `spc_timer`, `spc_mem_access_times`, `spc_dsp*`) use `result_detection: memory` ($6000), but MEASURED: none of the four blargg-spc-6 ROMs in this mirror ever write $6000, so all four report RUNS / "no result signal" rather than pass or fail (snes-24). No score in this external catalog is currently verified end-to-end. |
| CPU (65816) | 1.00 | unverified (visual) | krom/PeterLemon CPU opcode suites (ADC/SBC/MSC/MOV, gilyon cputest) draw to screen; `screenshot-crc`, refs pending. |
| PPU | 0.90 | unverified (visual) | smoke, OAM, mode3/color-halve, bus/INIDISP tests; `screenshot-crc`. |
| Timing | 0.85 | unverified (visual) | IRQ/DMA/HDMA timing tests (`test_irq4209`, `test_dmatiming`, `test_hdmatiming`, `HblankEmuTest`); cycle-accurate class, refs pending. |
| Mapper (enhancement chips) | 0.65 | unverified (visual) | SuperFX/GSU, SA-1 opcode tests; `screenshot-crc`. |
| Memory | 0.70 | unverified (visual) | `speed_test_v51`, `op_timing_test_v2`; timing class, refs pending. |

**Verified gaps surfaced by the suite (real, currently-open core issues).**

- FIXED (snes-08): `apu.spc_smp` (Blargg SPC700 master test) used to trap on
  the unimplemented SPC700 `(X),(Y)` opcodes -- it hit `$19` (`OR (X),(Y)`)
  first, at SPC `$0A99` (an earlier note here misidentified the first trap as
  `$79`). All four -- `$19`/`$39`/`$59`/`$79` (OR/AND/EOR/CMP `(X),(Y)`) --
  are now implemented. The ROM no longer traps, but it still reports RUNS /
  "no result signal": see the next bullet, which is unrelated to the opcode
  gap and remains open.
- `apu.spc_smp`/`spc_timer`/`spc_mem_access_times`/`spc_dsp6`: none of the
  blargg-spc-6 ROMs in this mirror write the `$6000` Blargg status byte, so
  `result_detection: memory` never gets a signal from them regardless of core
  correctness (snes-24). These external tests remain unverified; the original
  math corpus above provides independent register-file evidence.
- `apu.spc_dsp6` fails on real 3-chip SNES consoles (passes only on
  higan/bsnes); it is a documented hardware quirk, marked `known_fail`.

**Path to a real SNES headline.** Run `--generate-refs` on a build whose output
for each test's all-pass screen has been visually verified, paste the printed
hashes into `test_config.json`, and flip those tests' `expected` to `pass`. At
that point the CPU/PPU/timing/mapper scores become real and the headline rises to
reflect the (already substantial) implementation documented in the SNES core's
component README. Reference hashes are tied to the fixed headless resolution and
PNG encoder; regenerate if either changes.

> The SNES core implements all 256 65816 opcodes, all 8 PPU background modes,
> full DMA/HDMA, and the SPC700 + S-DSP audio path (see
> [cores/snes/README.md](cores/snes/README.md)). The low *verified* headline
> reflects missing test references, not missing features.

---

## Game Boy

Verified headline: **the Blargg serial subset plus one original register probe.** The external catalog is large (225
tests / 31 suites) but the large Mooneye / SameSuite / Wilbertpol half cannot yet
be scored, so the published number reflects the verified serial subset and the
rest is reported as unverified.

| Subsystem area | Verified today | Basis |
|----------------|----------------|-------|
| CPU instructions | Blargg `cpu_instrs` **10/11 PASS** | `02-interrupts` does not signal pass even at 8000 frames -> `known_fail` (real EI/IME or IF/IE dispatch-timing bug). |
| Instruction / memory timing | Blargg `instr_timing` PASS, `mem_timing` 3/3 PASS | `serial` ASCII detection, verified. |
| Original CPU/WRAM probe | 2/2 checks PASS; injected wrong WRAM stimulus FAILs | SB/SC bytes to `VELOCE_TEST_OUT`; `REG` values and `CHECK` verdicts are parsed from the result file. Runs as `gb_register_probe` in CTest; not yet included in the 225-test external scorecard. |
| Timer / PPU / interrupt cycle timing | Mooneye acceptance: **unverified (RUNS)** | Detection gap, see below. |
| APU (cycle-accurate) | SameSuite: **unverified (RUNS)** | Same detection gap. |
| PPU pixel-perfect | dmg-acid2 / cgb-acid2: renders, **not yet validated** | `screenshot-crc`; measured CRC `e643bbc7` / `6ddfa9c7` captured but no golden reference yet. |

**The Mooneye detection gap (the dominant caveat).** Mooneye / SameSuite /
Wilbertpol ROMs do not print ASCII pass/fail. They signal success via the
Fibonacci register fingerprint at an `LD B,B` breakpoint plus non-printable
serial bytes (the bus only captures ASCII `0x20-0x7E`). The shared detector
matches a `MOONEYE: PASS/FAIL` line that the GB plugin does not yet emit.
**Consequence: every Mooneye / SameSuite / Wilbertpol test currently resolves to
RUNS** (zero credit, excluded from the denominator), not PASS. The config records
each test's believed verdict in `expected`, so the harness will score them
automatically once the core emits the breakpoint line. This is a measurement
limitation, not necessarily an accuracy failure, and fixing it requires a `src`
change that is out of scope for the test suite.

**Retired claim.** The previous "111/111, 100%" GB claim was not reproducible
through the harness and has been retired: the Mooneye half was never actually
detected, and `02-interrupts` is a real failure.

**Path to a real GB headline.** Emit a `MOONEYE: PASS/FAIL` line on the `LD B,B`
breakpoint (a `src` change) to unlock the cycle-accurate timer/PPU/interrupt/APU
half, and capture acid2/Mealybug references on known-good hardware to validate
the pixel-perfect PPU tests.

---

## GBA

Original register-file evidence measured 2026-10-09: **65,536/65,536 ARM shift
result/CPSR observations matched**. The
[original ROM](tests/roms-src/gba/arm_shifts/README.md) covers LSL/LSR/ASR/ROR,
every low-byte amount, all C/V inputs, and 16 selected source operands.
The independent host oracle rejects a changed instruction and damaged files;
a channel probe verifies disabled output and the 255-character buffer boundary.
Thumb, other operands/instructions, timing, and peripherals remain unverified
by this corpus. It does not establish a whole-core completion percentage.

Verified headline: **partial.** The CPU, memory, BIOS, and save subsystems are
verified through the R12 spin-loop `serial` protocol; the large cycle-accurate
timing and PPU blocks are partly measured and partly pending. The catalog is 132
tests across 29 suites (23 cpu, 81 timing, 18 ppu, 4 memory, 4 mapper/saves,
2 misc/BIOS), 79 of which are cycle-accurate class.

| Subsystem | Verified state | Basis (representative measured subset, this build) |
|-----------|----------------|---------------------------------------------------|
| CPU (ARM7TDMI) | mostly verified | `cpu_arm` (`arm.gba`) PASS, `cpu_thumb` (`thumb.gba`) PASS; `cpu_psr` (`psr.gba`) **FAIL** (CPSR/SPSR banking gap). |
| Memory | verified | `memory_access` (`memory.gba`) PASS. |
| Misc / BIOS | verified | `bios` (`bios.gba`) PASS (Div, Sqrt, ArcTan, CpuSet, LZ77, etc.). |
| Mapper / saves | mostly verified | SRAM, Flash 64KB, none PASS; Flash 128KB **FAILs** at test #9 (earlier PASS reports were a detector flake). |
| Timing (DMA / IRQ / timer / HALTCNT / bus) | **mostly failing, not verified** | Measured this build (`cd cores/gba/tests && python3 runner.py --json timing`): only 10/80 timing tests pass. Every NanoBoyAdvance DMA/IRQ/timer/HALTCNT test **FAILs** (`latch`, `start_delay`, `force_nseq_access`, `burst_into_tears`, `irq_delay`, `start_stop`, `reload`, `haltcnt`), as does the 128KB bus-boundary test (memory subsystem). NBA `start_delay`/`burst_into_tears`/`128kb-boundary` used to be reported PASS only because the ROMs crashed into Undefined mode on the Thumb `bx pc` veneer (gba-15) before printing anything, leaving R12=0; once they run, their on-screen verdicts are timing FAILs (e.g. start-delay "IMM: FAIL 7, expected 20", burst-into-tears 0/3). `timing_irq`, `timing_dma`, `timing_fifo_dma`, `timing_cpu_mem` and `prefetch` fail almost wholesale. Root cause tracked separately (instruction-granular stepping + atomic bulk-DMA catch-up, per-instruction cycle guesswork, timer remainder/cascade loss - opus-class scheduler work). |
| PPU | partly verified, partly pending | smoke (`hello`, `shades`, `stripes`) PASS; `ppu_status_dma` and the affine visual tests need sub-scanline accuracy and are `known_fail` / visual pending references. |

**Detection method.** jsmolka / alyosha ROMs spin with `R12` holding the
failing test number (`0` = all pass); the GBA plugin detects the stable-PC spin
and prints `[GBA] PASSED` / `[GBA] FAILED - Failed at test #N`, parsed by the
shared GBA register detector. The nba-hw-test ROMs do **not** use this protocol
(they only print results to the screen), so for them the detector's R12 value
is an arbitrary scratch register: non-zero reads as FAIL, and a PASS has only
ever meant the ROM crashed before touching R12.

**Known limitation of the detector itself.** The stable-PC heuristic can also
report a ROM that has hung or crashed into an unrelated stable state as
`PASSED` - it does not confirm the ROM ever reached its own "all tests passed"
spin. This is tracked as a separate finding; until it is fixed, treat every
PASS above as measured-but-provisional, not reference-grade, and re-derive
this table from a fresh `runner.py --json` run rather than trusting hand-copied
numbers - the previous version of this table (NBA DMA/IRQ/timer/HALTCNT all
claimed PASS) had drifted from what the suite actually measures.

**Honest caveats.**

- `cpu_psr` is a measured FAIL (CPSR/SPSR banking), the main verified CPU debt.
- FuzzARM additionally dumps the first failing test to EWRAM (`0x02000000`,
  `'AAAA'`/`'TTTT'` record); the ARM data-processing mismatches it used to
  record were V-flag clears on logical ops (fixed, gba-17) and it now records
  no failure. ARMWrestler draws to the framebuffer only and remains unverified
  pending a debug-string harness.
- Suites needing sub-scanline accuracy (`ppu_status_dma`, `ppu_affine_visual`)
  are pre-marked `known_fail` and excluded from the headline until verified; the
  scanline-based PPU is the core's main accuracy limit for mid-scanline raster
  effects.
- The mGBA suite (`suite.gba`) is interactive with no headless verdict, and the
  AGS aging cartridge is Nintendo-proprietary; both are documented and excluded.

**Retired claim.** The earlier "90.9% pass rate" figure predated the weighted,
unverified-aware methodology and is superseded by the per-subsystem evidence
above.

---

## Reproducing every number

```bash
# One console (authoritative; subsystem subsets run faster)
cd cores/nes/tests  && ./run_tests.sh --json
cd cores/snes/tests && ./run_tests.sh --json
cd cores/gb/tests   && ./run_tests.sh --json
cd cores/gba/tests  && ./run_tests.sh --json   # or: python3 runner.py --json

# The whole platform, with a cross-console roll-up
python tests/run_all.py
```

All percentages above were measured on `build/bin/veloce` on 2026-06-02. They
will change as reference hashes are populated, the nestest trace and Mooneye
breakpoint emitters are added, and the measured failures are fixed; the
methodology that produces them ([TESTING.md](TESTING.md)) will not.
