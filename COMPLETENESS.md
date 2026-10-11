# Veloce Accuracy Scorecard

Current N64 evidence: [affine textured pixels and current Mario status](#affine-textured-triangle-pixels). Earlier increment reports are historical.

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
Native hardware completion remains unmeasured. The separate, reproducible
[delivery completion ledger](docs/CORE-COMPLETION.md) gives implementation
milestone percentages in the main and core READMEs; these are not the accuracy
headlines or original-ROM domain pass fractions.

## Platform summary

| Console | Suite size | Verified headline | Verification state |
|---------|-----------|-------------------|--------------------|
| [NES](#nes) | 172 external tests / 32 suites, plus 2 original ROMs | ~90-94% historical external subset | Original zero-page ADC/SBC corpus additionally verifies 524,288 A/P pairs; other hardware domains remain |
| [SNES](#snes) | 67 external tests / 19 suites, plus 2 original ROMs | 68,864 settled math cases and 504,832 timing bytes verified | Original multiplication domain is exhaustive; division covers boundaries; timing covers declared patterns/phases; external catalog remains unscored |
| [Game Boy](#game-boy) | 225 external tests / 31 suites, plus 3 original ROMs | Verified subset only | Blargg serial subset, 2 WRAM checks, 8,388,608 ALU pairs and 73,728 unary pairs verified; Mooneye remains unverified |
| [GBA](#gba) | 132 external tests / 29 suites, plus 1 original ROM | Partial; 65,536 original ARM shift cases verified | Legacy CPU/memory/BIOS/saves subsets verified; timing/PPU and remaining original hardware corpus pending |
| N64 | 35 original ROMs; no external catalog | Declared integer/merge/controller/interrupt/PI/RI/MI/device/cache/SP/AI/FPU-control/data/conversion/arithmetic/comparison/TLB/VI/RSP-scalar/vector-transfer/multiply domains verified | Experimental homebrew/native handoff; retail boot pending; full hardware completion unknown; [limits](cores/n64/README.md) |
| Virtual Boy | 7 original ROMs; no external catalog | 6,512,640 result/PSW pairs plus 4,210,688 multiply/divide and 4,685,824 TRAP/illegal and 4,982,784 address-trap snapshots plus 17,055,744 CAXI/port and 14,680,064 arithmetic bit-string and 100,712,448 immediate-instruction snapshots | Experimental marked homebrew; remaining CPU/NVC, VIP/VSU, controllers and timing pending; [limits](cores/vb/README.md) |
| Nintendo DS | 8 original ROMs; no external catalog | 44,238,336 integer plus 38,338,560 multiply plus 51,426,816 Thumb result/CPSR/source/PC observations matched; 49,694,208 nine-word memory and 9,437,184 eight-word long-call observations matched; 3,720,960 stack/block observations matched; 4,096/4,096 digital keypad combinations matched; 100-frame TCP host/viewer states and eight guest words match baseline/oracle; DS wireless multiplayer pending; ARM7 unpredictable multiply flags excluded | Experimental marked ARM/Thumb integer homebrew; firmware, ARM block transfers, graphics/audio, peripherals pending; DSi has a separate partial plugin; [limits](cores/nds/README.md) |
| Nintendo DSi | 2 original ROMs; no external catalog | 387,072/387,072 RAM captures; 67,109,008 component bus observations; 100-frame TCP host/viewer states and eight guest register words match oracle | Experimental dual-CPU expanded-memory profile; firmware/retail boot, full SCFG/MBK, graphics/audio/peripherals and physical timing pending; [limits](cores/dsi/README.md) |
| Nintendo 3DS | 2 original ROMs; no external catalog | 25,165,824/25,165,824 reversal captures; 36,123,648 component register snapshots; 541,065,300 RAM bus observations; 4,096 HID input combinations; 100-frame TCP host/viewer full states and twelve guest words match baseline/oracle | Experimental supplied ARM11/ARM9 integer profile; native boot, remaining CPU, graphics/audio/peripherals, wireless multiplayer and physical timing pending; full hardware percentage unknown; [limits](cores/3ds/README.md) |
| GameCube | 10 original ROMs; no external catalog | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched | Experimental supplied integer profile; full hardware completion unknown; [limits](cores/ppc/README.md) |
| Wii U | 2 original ROMs; no external catalog | 25,165,824/25,165,824 nine-word immediate captures; 28,311,552 component register snapshots; 8,724,152,428 RAM bus observations and dense/sparse state replay; 4,096 synthetic digital-input latch values; 100-frame production TCP states and twelve guest words match baseline/oracle | Experimental supplied three-context PowerPC integer profile; native boot, remaining CPU, GPU/audio/peripherals, native GamePad, wireless multiplayer and physical timing pending; full hardware percentage unknown; [limits](cores/wiiu/README.md) |
| Wii | 10 original ROMs; no external catalog | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched | Experimental supplied integer profile; full hardware completion unknown; [limits](cores/ppc/README.md) |

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

## GameCube/Wii controller evidence

Each original settled serial-controller ROM matches 4,096/4,096 sixteen-word
captures (65,536 guest values), all twelve digital button combinations. The
component gate additionally matches 262,144 port/input/connection snapshots
and 196,608 transfer-length snapshots. Negative guest command, sixteen altered
fields and seven malformed stream controls are rejected per profile. Analog,
polling/timing/interrupt delivery, accessories, four-port frontend routing and
Wii Remote behavior remain pending. This is 100% of the declared digital domain,
with no whole-hardware accuracy percentage implied. See [the profile](cores/ppc/README.md).

Super Mario 64 was separately tested with the user's local image: the current
N64 plugin loads it but faults at unimplemented VI Current register after 5,897,464 IPL3 step attempts. That is a pending retail target, not a scored
pass. The original-ROM ledger contains 64 repository-owned programs; commercial
ROM bytes are never part of that ledger or git.

## N64 cartridge DMA and IPL3 handoff

The [PI corpus](tests/roms-src/n64/pi_dma/README.md) matches 524,288 cases and
8,388,608 guest register values across all even lengths 2–65,536, four cartridge
offsets, two RDRAM addresses and both PI mask states. The [handoff corpus](tests/roms-src/n64/ipl3_handoff/README.md)
matches twelve startup captures without bundling firmware. Executed guest mutants,
all captured-field mutants and seven invalid streams per corpus are rejected.
All twenty-four N64 CTests pass; the PI component also passes undefined-behavior sanitization.
Settled aligned ROM reads and explicit HLE startup are implemented; RI/RDRAM
initialization, native retail boot, PI busy/misalignment/peripheral behavior,
graphics/audio and physical timing remain pending. The ledger contains 64 original
ROMs. CI/CD remains disabled.

## N64 RI configuration register evidence

The [original RI corpus](tests/roms-src/n64/ri_registers/README.md) matches
524,288 cases and 3,145,728 guest reads. Every 19-bit refresh value and every
mode/configuration/selection combination is covered; latency is correlated.
Executed selection mutation, six field mutants and seven invalid streams reject.
The component checks current-load latching, access/state failure atomicity,
legacy conversion, exact replay, reset and loaded-plugin execution. RI refresh
scheduling, analog calibration, bus-error production, native RAM remapping and
physical timing remain pending. Super Mario 64 advances to an unimplemented register read
(unimplemented VI Current register 803275F4, EPC 803275F4, Cause 1C) after 5,897,464 startup step attempts; it is
not bootable/playable. The corpus ledger contains 64 original ROMs.

## N64 MI control evidence

The [original MI corpus](tests/roms-src/n64/mi_controls/README.md) matches
393,216 rows / 3,145,728 guest captures: mode command words across all eight
flag states and interrupt-mask commands across all 64 masks, with actual SI/PI
DMA requests pending. Executed count mutation, eight field mutants and seven
invalid streams reject. The component checks 16,777,216 mode transitions, all
mask transitions, injected DP delivery/acknowledgment/ERET, exact replay, legacy
conversion, atomic rejection and production-plugin execution; it also passes
undefined-behavior sanitization. All twenty-four N64 gates pass. MI repeat/upper device-register effects exist; RAM-repeat/EBus transactions
and native RAM remapping remain pending.
Super Mario 64 stops at unimplemented VI Current register (EPC 803275F4, Cause 1C) after 5,897,464 startup
step attempts; retail gameplay, graphics/audio and physical timing remain pending.
All 64 original ROMs reproduce. CI/CD remains disabled.

## N64 RDRAM device evidence

The [original device corpus](tests/roms-src/n64/rdram_devices/README.md) matches
14,336 cases / 86,016 guest values across four devices, all writable delay
fields and repeated-byte storage patterns. Executed mutation, six field mutants
and seven invalid streams reject. Components check all device ID encodings,
delay/repeat combinations, selectors/register aliases, discovery, MI upper/repeat,
state replay and rejection, legacy conversion and loaded-plugin execution.
All twenty-four N64 gates pass; device components pass undefined-behavior sanitization.
Native RAM remapping, analog calibration, cache timing/errors, physical bus timing, repeat RAM
and EBus memory remain pending. Super Mario 64 reaches unimplemented VI Current register after
5,897,464 step attempts (EPC 803275F4, Cause 1C); gameplay/video/audio are unverified.
All 64 original ROMs reproduce. CI/CD stays disabled.

## N64 functional primary cache evidence

The [original cache corpus](tests/roms-src/n64/cache_coherence/README.md) matches
2,097,152 cases / 12,582,912 guest values across every D-cache index, byte lane
and initial byte, with cached/uncached alias mutation and dirty writeback. An
executed writeback mutation, six field mutants and seven invalid streams reject.
Components additionally cover all physical tag values, instruction staleness/refill,
dirty eviction, widths/merged stores, PI DMA staleness, CACHE operations/privilege,
exact dirty/stale state replay and legacy conversion. All twenty-four N64 gates
pass; cache components pass undefined-behavior sanitization. Timing/hazards,
TLB/address modes, cache errors, native RAM remapping and device bus transactions
remain incomplete. Super Mario 64 stops at unimplemented VI Current register 803275F4 after 5,897,464 attempts
(EPC 803275F4, Cause 1C); video/audio and gameplay remain unverified. All 64
original ROMs reproduce; CI/CD remains disabled.


## N64 SP interface evidence

The [original SP controls](tests/roms-src/n64/sp_controls/README.md) match
196,608 cases / 1,572,864 guest values over every writable status/interrupt
seed, all twelve command pairs and four command states. The
[original SP DMA](tests/roms-src/n64/sp_dma/README.md) matches 65,536 cases /
393,216 guest values across every raw length, both directions/banks and four
wrap boundaries. Executed mutations, all field mutations and seven malformed
streams reject per corpus. Component evidence checks full DMA bytes/guards,
262,144 count/stride transfers, PC/semaphore masks, CPU interrupt delivery/ERET,
cache staleness, states, reset and the loaded shared plugin. SP components pass
undefined-behavior sanitization; all twenty-four N64 CTests pass. All 64 original
ROMs reproduce. CI/CD stays disabled.

RSP scalar/vector execution, DMA queues/clocks, unaligned stride, open-bus DMA,
IO-full and exact address/length readback physical validation remain pending.
Super Mario 64 reaches unimplemented VI Current register `$803275F4` after 5,897,464 startup
attempts (EPC `$803275F4`, Cause `$1C` hex). Gameplay/video/audio remain
unverified. N64 is 35% on the delivery checklist; no complete CPU/device
milestone is claimed.


## N64 AI audio evidence

The [FIFO ROM](tests/roms-src/n64/ai_fifo/README.md) matches 2,048 cases /
22,528 guest values. The [PCM ROM](tests/roms-src/n64/ai_pcm/README.md)
matches nine guest FIFO/IRQ captures; the production plugin outputs 66,169 stereo
pairs covering all 65,536 signed sample values per channel to register files.
Independent host oracles reject executed mutations and malformed streams.
Components cover all length/address/rate fields, dividers, two-buffer FIFO,
start/promotion IRQs, delayed carry, playback-time RAM/cache visibility, pause,
IRQ/ERET, bounded host output, state replay/rejection and identical buffered/
streaming samples and states. The AI component passes undefined-behavior
sanitization. All twenty-four N64 gates pass; all 64 original ROMs reproduce.

N64 now has functional guest PCM output and **35%** delivery-checklist completion.
Physical CPU/DAC timing, serial/analog behavior, status clock fields, dynamic rate
phase behavior, RSP microcode, graphics and native retail playability remain
unverified. Super Mario 64 reaches unimplemented VI Current register after 5,897,464 attempts
(EPC 803275F4, backing word 8D8D0010, Cause 1C hex). No commercial ROM bytes enter
git. CI/CD stays disabled.

## N64 FPU controls and branch corpus (2026-10-10)

CFC1/CTC1 and BC1F/BC1T/BC1FL/BC1TL now implement the defined VR4300
control-register profile. CU1-disabled instructions raise coprocessor unusable
with CE=1. CTC1 commits FCR31 before raising enabled cause or unimplemented
operation FPE. Read-only FCR0 selects implementation 0x0B/revision zero.

The [original control ROM](tests/roms-src/n64/fpu_controls/README.md)
matches **8,192/8,192** cases / **65,536** guest register values, with actual
guest exception capture. An executed mutation fails every reduced row; eight
field mutations fail and seven incomplete/malformed streams are rejected.
The component covers **1,048,576** defined control combinations,
**524,288** branch offset/condition/kind combinations and **1,024** CU1-disabled
format/control-index cases, plus GPR transfers, delay/BEV/nested exception
context, pending-exception replay and atomic state rejection. S010 appends an
8-byte FCR31 scalar; S009 converts with neutral FCR31 and preserves pending
AI/FIFO/host phase state. Earlier state conversions remain supported.

All **65** repository-owned ROMs reproduce from the ledger. The FPU component
passes undefined-behavior sanitization. Raw register streams and independent
reports are under `build/artifacts/owned/n64_fpu_controls/`.
N64 stays **35%** on the delivery checklist: FPR data transfers, arithmetic,
conversions, comparisons, NaN/denormal behavior and physical pipeline timing
remain pending; no full CPU or hardware-accuracy milestone is awarded.
This original cartridge has not been validated on physical hardware.

Super Mario 64 now passes CFC1 and reaches unimplemented VI Current register after
**5,897,464** attempts (EPC `803275F4`, instruction-cache and backing word
`8D8D0010`, Cause `1C` hex). The production plugin still reports uniform video
and zero retail PCM pairs. Boot and playability remain unverified. No commercial
ROM bytes enter the repository. CI/CD remains disabled.

All **26/26 N64 CTests pass** (345.82 seconds); the complete run includes every
original N64 register ROM, PCM ABI oracle and all nine component tests.

## N64 TLB translation and fault corpus (2026-10-10)

TLBR/TLBWI/TLBWR/TLBP now control a real 32-entry paired-page TLB. The CPU
translates fetch/load/store/merge/CACHE access with seven page sizes,
ASID/global/valid/dirty checks, 32/64-bit region privileges, ERL bypass, xkphys
CCA, masked CP0 fields and DMFC0/DMTC0. Translation faults record
BadVAddr/EntryHi/Context/XContext and use refill/general/extended vectors.
Virtually indexed cache aliases retain separate stale data until maintenance.

The [original ROM](tests/roms-src/n64/tlb_translation/README.md) matches
**114,688/114,688** cases / **1,261,568** guest register values. An executed
ASID mutation fails all 256 reduced rows; eleven field mutations fail; seven
malformed streams are rejected. The component checks **3,670,016** ASID/global/
half cases, **2,097,152** physical frame selections, **79,188** offsets and
**4,096** mask inputs and **229,376** independent permission/CCA cases, plus
controls, CPU faults/fetch/cache aliases and state
continuation. S011 stores 32 entries in 1,024 bytes and accepts S010 with neutral
TLB entries. Old state conversions preserve their existing fields.

Duplicate-hit shutdown/TS behavior, physical timing and CP0/TLB hazards remain
pending. Neutral reset tags, rejection of undefined masks and RI on duplicate
translation matches are explicit emulator conventions. Unsupported physical
bus access produces IBE/DBE in this bus profile; physical open-bus behavior is
unverified. This cartridge has not been tested on a physical console.
N64 remains **35%** on the delivery checklist; full hardware completion is unknown.

Mario 64 now passes TLBWI and reaches unimplemented VI Current at `A4400010`
after **5,897,464** attempts (EPC `803275F4`, actual decoded/backing word
`8D8D0010`, Cause `1C` hex / DBE). The tracer distinguishes actual CPU exception
entry from software EXL writes and permits nonterminal guest-handled exception
classes. The production probe still sees uniform video and zero retail PCM;
boot/playability remain unverified. Commercial bytes are kept out of git.

All **28/28 N64 CTests pass** (328.64 seconds), including all original N64
register ROMs, PCM ABI oracle and ten component tests. The final DMCP0
32-bit-mode restriction and independent permission checks are verified by the
affected TLB gates and the updated component sanitizer run. All **66** original
ROMs reproduce from their source/image ledger. CI/CD remains disabled.

## Video Interface and first framebuffer

The N64 core now exposes functional VI registers, scanline/field progression,
five-field H_LEAP selection, CURRENT acknowledgement and VI → MI → CPU interrupts.
The NTSC crystal advances at 48,681,818 / 93,750,000 per nominal CPU attempt;
physical CPI, DMA arbitration and sub-line sync timing remain unverified.
Odd V_TOTAL preserves field parity; even V_TOTAL alternates parity. Interrupt
comparison includes the documented odd-field early-match and V_INTR=0 behavior.
TYPE 0 holds CURRENT at zero; reserved TYPE 1 uses an emulator blank convention.

The production ABI derives an **unfiltered RGBA16/RGBA32 framebuffer** from RAM,
ORIGIN, WIDTH, visible spans and fixed-point scale/offset fields. Dirty CPU data
cache remains invisible until writeback. Host observation leaves guest state
unchanged. Host alpha is opaque because source low bits represent coverage.
Inactive/invalid visible spans return a 320×240 black framebuffer; unavailable
source pixels are black. Active dimensions follow the programmed visible spans.
Y offset's upper two stored bits do not affect rendering. Rendering is a snapshot,
not accumulated physical scanout. Antialiasing/divot/dither/gamma, coverage filters,
TEST_MODE staged data, pixel advance, extreme-scale errata, mid-frame changes,
RSP/RDP rendering and physical timing remain pending.

The [register ROM](tests/roms-src/n64/vi_registers/README.md) matches **65,536
rows / 983,040 guest values**. The [framebuffer ROM](tests/roms-src/n64/vi_framebuffer/README.md)
matches **24 guest values** and **131,072 actual production ABI pixels** across
two formats. Executed mutations and strict file/channel/framing controls reject
incorrect results. The component checks all **16,777,216 ORIGIN values**, all
**1,048,576 vertical-total/interrupt-target sequences** over multiple fields,
**4,096 horizontal periods**, all **65,536 RGBA16 words**, register bits, leap
selection, fractional scaling, memory/cache bounds, IRQ routing and save replay.
These are declared finite domains, not their full independent product or physical
hardware certification. No physical-console run has validated these cartridges.

`VN64S012` appends 160 validated VI state bytes. Legacy S011 restores reset VI;
earlier conversions retain their documented neutral state. Phase, field, leap
pattern and IRQ continuation replay exactly; malformed states reject atomically.
N64 is now **40% (8/20 delivery milestones)**: guest framebuffer output earns the
graphics milestone. This is not a hardware-accuracy or retail-compatibility score.

The private Super Mario 64 USA trace now reaches **6,212,303 attempts**, then
unsupported `LDC1`: EPC `80325978`, actual instruction-cache word `D7400130`,
Cause `10000028` (RI code 10). The backing word `44844000` differs because the
cache is noncoherent; the diagnostic records both. The trace passed 22 earlier
nonterminal exception entries. The 120-quantum production probe still observes
uniform video and zero audio pairs. Boot and playability remain unverified;
commercial ROM bytes are never committed. Earlier increment reports below/above
retain their historical blocker and percentage at the time of that increment.

Rules follow the [VI register reference](https://n64brew.dev/wiki/Video_Interface),
[original Nintendo VI manual](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm),
and [hardware timing reference](https://github.com/meauxdal/N64-Refresh-Rate-Reference/blob/main/N64_Timing_Reference.md).

Verification: **32/32 N64 CTests passed** in 668.56 seconds, including all original register ROMs, production PCM/video oracles and eleven component gates. The full build and VI undefined-behavior sanitizer passed; all **68 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## FPU register and memory data transfers

The core implements all eight FPU data-transfer instructions:
MFC1/MTC1/DMFC1/DMTC1 and LWC1/LDC1/SWC1/SDC1. FR=0 pairs low words of even
registers with high words addressed by odd register numbers; FR=1 exposes 32
independent 64-bit registers. MFC1 sign-extends its word, MTC1 ignores high GPR
bits, and data movement preserves payload bits without interpreting NaNs or
changing FCR31. Doubleword moves are valid in 32-bit kernel mode and 64-bit
user/supervisor mode. Memory transfers use the CPU's existing translation/cache
path and enforce aligned 4-/8-byte accesses. CU1 unusable takes priority over
operand decoding or data access; failed loads leave FPR state untouched and
faulted stores preserve backing memory. EPC/BD, nested and BEV context are tested.

The [original register ROM](tests/roms-src/n64/fpu_data/README.md) matches
**131,072 cases / 1,179,648 actual 64-bit values**. Its independent host oracle
rejects an executed capture mutant, nine field mutants and seven malformed-stream
controls. The component checks **4,194,304 word-register/payload cases** and
**3,145,728 defined doubleword-register/payload cases**, **262,144 signed-offset
cases**, all GPR indices/zero, all 64 data-bit positions in every FR=1 register,
reserved transfer fields, privilege/CU1, alignment/TLB/bus exceptions and state
continuation. These are declared finite domains, not full independent products or
a complete FPU/hardware corpus. No physical-console run has validated this ROM.

`VN64S013` appends 32 raw 64-bit FPRs (256 bytes). S012 and earlier versions
restore zero FPR storage. Existing cache/VI/AI/TLB state and legacy conversions
retain their documented behavior. Undefined FR=1 upper word-write bits are
preserved; undefined odd FR=0 doubleword forms raise RI as deterministic emulator
conventions. Raw storage is retained across FR view changes, with physical
mode-transition behavior ungraded. Transfer pipeline hazards, physical timing,
reverse-endian mode and FPR arithmetic/conversion/comparison remain pending.
Rules follow the [NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf),
sections 7.2, 17.3 and the corresponding instruction entries.

Super Mario 64 USA now passes LDC1 and reaches **6,212,336 startup attempts**,
then unsupported `CVT.S.W` at EPC `80325984`, actual instruction-cache and backing
word `468042A0`, Cause `10000028` (RI code 10). It passed 22 earlier nonterminal
exception entries. The 120-quantum production probe still observes uniform video
and zero audio pairs; boot/playability remain unverified. Commercial ROM bytes
are never committed. Earlier increment records retain their historical blockers.
N64 remains **40% (8/20 delivery milestones)**; the whole CPU milestone is pending.

Verification: **34/34 N64 CTests passed** in 437.02 seconds, including twenty original register-ROM gates, two production audio/video oracles and twelve component gates. The full build and final FPU-data undefined-behavior sanitizer passed; all **69 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## FPU conversion and fixed rounding family

The core now implements CVT.S/D/W/L and ROUND/TRUNC/CEIL/FLOOR W/L using integer
bits, without host floating-point state. It supports nearest-even, toward-zero,
positive and negative directed rounding; fixed-mode instructions preserve FCSR
rounding controls. FCSR causes replace previous causes, sticky flags accumulate
only on nontrapping results, and enabled exceptions/software assist suppress
writes while preserving sources and old flags. FR views, long-target privilege,
CU1 precedence, delay/nested/BEV exceptions and S013 state replay are tested.

The [integer ROM](tests/roms-src/n64/fpu_integer_conversions/README.md) matches
**524,288 rows / 2,621,440 captured words**. The
[edge ROM](tests/roms-src/n64/fpu_conversion_edges/README.md) matches
**289,536 rows / 1,447,680 captured words**, including every S/D exponent field
on four fraction patterns and both signs, all conversion functions, selected full
enable/FS/rounding products, integer bit boundaries and reserved source formats.
Independent exact-rational host grading checks actual CPU/FPR/FCSR captures;
undefined upper result bits are recorded and ungraded. Executed capture mutants,
five field mutations per ROM and strict malformed-stream controls reject incorrect
results. The component checks **4,194,304 exact integer/index cases**, **229,376
exception-control combinations**, **98,304 register-pair/FR/format/function cases**,
fixed modes, CU1/privilege, cause replacement, exception context and state replay.

Rules follow the [NEC manual](https://hack64.net/docs/VR43XX.pdf), chapters 7–8,
conversion instruction entries and Appendix D, with ambiguities resolved by the
hardware-test author's [conversion cases](https://github.com/lemmy-64/n64-systemtest/blob/main/src/tests/cop1/mod.rs)
and [raw constants](https://github.com/lemmy-64/n64-systemtest/blob/main/src/cop1.rs).
Subnormal inputs require assist. Floating precision conversion preserves infinity;
NaNs with top payload bit 1 raise invalid and return a canonical legacy value,
while top-bit-0 NaNs require assist. Integer conversions reject both NaN types,
infinity and out-of-range results with assist. Long inputs support [-2^55, 2^55);
long outputs require magnitude below 2^53. Tiny results use FS/enable/rounding
rules, including rounding to the minimum normal and signed zero.

No physical console has validated these original cartridges. Their selected finite
domains do not exhaust the full independent operand product or whole FPU.
Undefined odd FR=0 arithmetic operands retain the RI convention; nonzero unary
`ft` encodings use an assist convention with physical behavior ungraded. Pipeline
hazards, precise CE timing, reverse-endian mode and FPU arithmetic/comparison
remain pending. State stays `VN64S013`; conversions add no serialized fields.
N64 remains **40% (8/20 delivery milestones)**; the whole CPU milestone is pending.

Super Mario 64 USA now passes CVT.S.W and reaches **6,212,339 startup attempts**,
then unsupported `DIV.S` at EPC `803259A0`, actual instruction-cache/backing word
`460A3483`, Cause `10000028` (RI code 10). It passed 22 earlier nonterminal exception
entries. The 120-quantum production probe still sees uniform video and zero audio
pairs; boot/playability remain unverified. Commercial ROM bytes are never committed.
Earlier increment reports retain their historical blockers and counts.

Verification: **37/37 N64 CTests passed** in 475.97 seconds, including twenty-two original register-ROM gates, two production audio/video oracles and thirteen component gates. The full build and final conversion undefined-behavior sanitizer passed; all **71 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## FPU arithmetic family

The core implements ADD/SUB/MUL/DIV/SQRT/ABS/MOV/NEG in single and double
precision using portable integer significands, guard/sticky rounding, long
division and a restoring integer square root. It uses no native floating-point
state or compiler-specific 128-bit type. All four rounding modes, legacy NaNs,
signed zeros, infinity, invalid, division-by-zero, overflow and FS/enable-dependent
tiny results are handled. Assist takes precedence over invalid operands. MOV
copies bits without interpreting NaNs/subnormals or changing existing FCSR causes.
Traps update causes, preserve sticky flags and suppress destination writes.

The [pair ROM](tests/roms-src/n64/fpu_arithmetic_pairs/README.md) matches
**131,072 rows / 786,432 captured words**. The
[edge ROM](tests/roms-src/n64/fpu_arithmetic_edges/README.md) matches
**215,152 rows / 1,290,912 captured words**. Both stream six actual destination,
FCSR, handler, CPU exception and unchanged-source observations per row. An
independent exact-rational oracle grades arithmetic; square roots use integer
square comparisons, without native FP. Each ROM rejects an executed capture
mutant, six individual field mutations and seven malformed-stream controls.

The component passes **540,672 register/FR/format/function cases** and **327,680
rounding/enable/sticky/FS/condition combinations**, including aliased operands,
MOV preservation of enabled old causes, minimum-normal rounding rescue, CU1
precedence, reserved formats,
delay/nested/BEV exceptions and exact pending-arithmetic state replay. A separate
30,000-case seeded raw-bit differential check matches the rational oracle.
State stays `VN64S013`; this family adds no serialized fields.

Rules follow the [NEC manual](https://hack64.net/docs/VR43XX.pdf), chapters 7–8
and arithmetic instruction entries, supplemented by the hardware-test author's
[raw-bit arithmetic cases](https://github.com/lemmy-64/n64-systemtest/blob/main/src/tests/cop1/mod.rs).
These original cartridges have not been validated on a physical console. Their
finite domains do not exhaust all independent operand combinations. Undefined
odd FR=0 operands use the documented RI convention; nonzero unary ft fields use
an assist convention whose physical behavior is ungraded. FPU comparisons,
pipeline/interlock/CPI behavior, precise CE timing and reverse endian remain pending.
N64 stays **40% (8/20 delivery milestones)**; the whole CPU milestone is incomplete.

Super Mario 64 USA passes the former DIV.S blocker and reaches **12,000,000
startup attempts**, the bounded trace limit, without a terminal CPU fault.
It passed 51 nonterminal exception entries and ends at PC `80246DDC`, Status
`FF01`. The 120-quantum production probe observes uniform video and 480 audio
sample pairs; audible game sound, boot and playability remain unverified.
Commercial ROM bytes are never committed. Earlier increment reports retain
their historical counts and blockers.

Verification: **40/40 N64 CTests passed** in 246.85 seconds, including twenty-four original register-ROM gates, two production audio/video oracles and fourteen component gates. The full build and final arithmetic undefined-behavior sanitizer passed; all **73 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## RSP scalar execution

The RSP now executes 32-bit scalar arithmetic, logic, comparisons, immediate and
variable shifts, conditional branches/link/jumps with delay slots, byte/halfword/
word DMEM loads/stores and shared MFC0/MTC0 SP fields. IMEM PC/targets wrap at
4 KiB; unaligned data accesses wrap each byte within DMEM. ADD/SUB/ADDI wrap
without CPU overflow traps. BREAK sets HALT/BROKE and the configured SP interrupt;
MI delivers that real interrupt to the CPU. Semaphore and settled DMA use the
same SP interface as CPU accesses. Single-step NOPs settle HALT.

RSP instructions advance at two per three nominal CPU interpreter attempts;
this is a functional scheduling approximation, not physical CPI, dual issue,
interlocks, bus timing or debugger pipeline behavior. Missing vector/DP operations
halt with an explicit blocked instruction and retained PC, without manufacturing
BROKE, a task-completion IRQ or a CPU hardware exception. Illegal/undefined
encodings and branches within delay slots are not physically graded.

The [ALU cartridge](tests/roms-src/n64/rsp_scalar_alu/README.md) matches
**704,512 rows / 4,931,584 actual captured words**, including all repeated-byte
pairs for ten functions and all fixed/variable shifts on 256 patterns. The
[immediate cartridge](tests/roms-src/n64/rsp_scalar_immediates/README.md) matches
**1,572,864 rows / 11,010,048 words**: every immediate for eight forms on three
operand patterns. Original RSP code writes actual GPRs to DMEM, and original CPU
code streams those values plus zero, SP PC/status and MI pending. A separate
mathematical host oracle grades every word. Each suite rejects an executed capture
mutant, seven field mutations and seven malformed-stream controls.

The component passes **4,194,304 immediate cases**, **327,680 register triples**,
**9,437,184 byte-address/pattern/load-store cases**, **262,144 branch/BREAK cases**,
all low-byte shift counts and all PC slots for J/JAL/JALR. It covers alias/zero
semantics, SP control/semaphore/real DMA bridges, CPU IRQ acknowledgment, explicit
unsupported-operation diagnostics, reset and state atomicity/replay.
The earlier SP-control cartridge now waits for single-step NOPs to settle and
halts before PC write/read to avoid execution races; all 196,608 rows still grade.

State `VN64S014` appends 304 bytes: 32 scalar GPRs, next PC, retired count, delay
state, blocked flag/word and the fractional clock phase. S013 and older states
preserve implemented fields while starting neutral scalar registers/phase and
sequential execution from the restored SP PC. Existing memory/debug prefix
positions stay stable. RSP state rejection is atomic, including invalid zero GPR,
word widths, alignment, flags and phase. Pending branch/BREAK replays exactly.

Rules follow the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf)
and the hardware-test author's [scalar cases](https://github.com/lemmy-64/n64-systemtest/tree/main/src/tests/rsp).
Published raw cases clarify SLTIU unsigned comparison, LWU as a word load,
unaligned DMEM wrapping, unconditional REGIMM links and BREAK in delay slots.
These original cartridges have not been validated on a physical console.
Selected domains do not exhaust all independent 32-bit operands or the whole RSP.

The local Mario trace reaches 12,000,000 CPU attempts with 51 nonterminal
exception entries and no terminal CPU fault. It retires **52 RSP scalar
instructions**, then blocks at **SP PC `090`, `MFC0 DPC_STATUS`, word `40055800`**.
Status `41` records HALT plus interrupt-on-break, without BROKE. The production
probe still observes uniform video and 480 audio sample pairs, which do not prove
audible game sound. Boot/gameplay remain unverified; commercial bytes are excluded.
N64 remains **40% (8/20 delivery milestones)**. Vector/RDP/game rendering, RSP
audio microcode, physical timing, FPU comparisons, analog and netplay remain pending.

Verification: **43/43 N64 CTests passed** in 325.15 seconds, including twenty-six original register-ROM gates, two production audio/video oracles and fifteen component gates. The corrected RSP ROM/component gates also passed in 78.34 seconds. The full build and final scalar undefined-behavior sanitizer passed; all **75 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and GitHub Actions disabled.

## Display-processor interface and fill engine

CPU MMIO `$04100000..1C` and RSP COP0 slots 8–15 now use one shared DP
module. START/END mask to aligned 24-bit pointers; START holds one pending
value and ignores repeats until END activates it. END updates CURRENT even
while frozen. The functional profile includes queued ranges, extensions,
XBUS DMEM wrapping, status controls and 24-bit nominal counters. It selects
zero for physically undefined reset pointers/counters; reset status is `$A8`.
Read-only writes and simultaneous clear/set pairs have deterministic conventions
and receive no physical-hardware credit.

The bounded RDP engine fetches big-endian 64-bit commands from DRAM or DMEM,
implements NOP, settled sync barriers, SetColorImage/SetScissor/SetOtherMode/
SetFillColor, integer fill-cycle RGBA16/32 rectangles, and actual FullSync.
Scissor ends are exclusive; fill rectangle ends are inclusive. RGBA16 alternates
the two halves of the 32-bit fill word by memory address. Bounds are validated
before any draw mutates RAM. FullSync raises MI DP bit 5 and settles the pipeline;
range-end alone never creates an IRQ. MI_INIT acknowledges the real level and
the existing mask/CPU IP2 path delivers it. CPU caches remain stale across RDP
writes until invalidated; the existing VI displays actual filled guest RAM.

The [register cartridge](tests/roms-src/n64/dp_registers/README.md)
matches **262,144 rows / 2,621,440 captured words**, using an original RSP
program to observe both control views. The [fill cartridge](tests/roms-src/n64/dp_fill_rectangles/README.md)
matches **524,288 rows / 6,815,744 captured words**, covering every 16-bit
color code in both pixel sizes, both command sources and whole/center scissor.
Independent integer oracles grade every captured pixel/guard/control word;
executed capture mutants fail all 32 reduced cases, each field mutation fails
individually and seven damaged stream controls return ERROR in each suite.
The component additionally checks all **16,777,216 START encodings**, **262,144
fill/scissor/guard cases**, shared CPU/RSP register banks, command flags/counters,
freeze/flush stalls, queued ranges, DMEM wrap, unsupported/bounds diagnostics,
cache/VI integration and CPU IRQ acknowledgment.

State `VN64S015` appends 19 validated big-endian scalars (152 bytes): DP latches,
active endpoint, status/counters, pipeline, command diagnostics and fill state.
Replay checks preserve pending commands, diagnostics and IRQs; invalid fields
are rejected atomically. S014 and earlier start reset DP controls/counters while
preserving all previously implemented fields, including the stored MI DP level.

Rules follow SGI's [DP register guide](https://hcs64.com/files/RSPCOP0.pdf),
[RDP command summary](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf),
and the hardware-test author's [current DP latch cases](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rdp/mod.rs).
These original cartridges have not been validated on a physical console.
Counters/issue use nominal RCP clocks, with no DMA FIFO/prefetch or physical
raster timing. Hidden coverage memory, TMEM/textures, triangles, blending, depth,
other cycle modes, fractional rectangles and interlace scissor remain unsupported.
Unknown commands or unsupported draws retain CURRENT and report the actual word
and a reason; they do not fabricate task completion or an interrupt.

The local Super Mario 64 (USA) probe on 2026-10-10 now passes the former
`MFC0 DPC_STATUS` blocker and retires **308 RSP scalar instructions**. It then
blocks at **SP PC `424`, `LQV`, word `C8411800`**. The CPU reaches 12,000,000
attempts with 51 nonterminal exception entries and no terminal CPU fault.
The production probe still observes uniform video and 480 audio sample pairs
across 120 scheduling quanta. Native boot, audible game sound and playability
remain unverified. The private image remains local. N64 stays **40% = 8/20**
delivery milestones; partial DP progress does not complete a whole hardware milestone.

Verification: **46/46 N64 gates passed** across the existing 43-gate regression
(322.32 seconds) and the three new DP gates (79.44 seconds). The final component
including VI scanout also passed. The full build and undefined-behavior sanitizer
passed; all **77 original ROMs** reproduce against the committed image/generator
ledger. Completion renderer and whitespace checks pass. CI/CD remains removed
and GitHub Actions disabled.

## RSP vector storage and memory

RSP scalar execution now owns 32 raw 128-bit vector registers and VCO/VCC/VCE
flags through a separate vector storage/transfer module. It implements all twelve
encoded load/store forms: byte/short/long/double, quad/rest, packed/fractional,
strided, whole and transpose. LWV follows the hardware-observed no-op behavior.
Loads truncate at the register end; stores wrap within the source register.
All memory bytes wrap within DMEM without touching IMEM. Seven-bit offsets are
sign-extended and scaled per opcode. Packed and transpose mappings follow the
hardware-test author's cases rather than treating every instruction as memcpy.

MTC2 discards the upper scalar halfword and truncates the last byte at element 15.
MFC2 reads two bytes with register wrap and sign-extends the halfword. CTC2/CFC2
share VCO/VCC/VCE storage, including all 32 selector aliases; VCE is eight bits,
so its readback never becomes negative. Memory transfers preserve all scalar
registers and flags. Unsupported arithmetic retains SP PC and the actual word,
sets HALT without BROKE, and never fabricates task completion or an interrupt.

Three original programs record actual guest observations, with no guest verdict:

| Original cartridge | Cases | Captured 64-bit words | Declared domain |
|---|---:|---:|---|
| [Vector loads](tests/roms-src/n64/rsp_vector_loads/README.md) | 786,432 | 19,660,800 | Twelve forms × 16 byte elements × 16 alignments × 256 byte-pattern seeds |
| [Vector stores](tests/roms-src/n64/rsp_vector_stores/README.md) | 786,432 | 25,952,256 | Same domain, including every byte of surrounding wrapped memory |
| [Scalar/vector transfers](tests/roms-src/n64/rsp_vector_transfers/README.md) | 1,048,576 | 10,485,760 | Every halfword payload × every byte element, with scalar zero and flag masks/signs |

For memory cartridges, seed parity selects `$900` or `$FF0`; VT is seed modulo
32. Results include the affected eight-register group, neighboring register
guards and actual scalar/SP/MI state. Each memory case uses three ordered REG
chunks within the existing debug line cap. Independent destination-oriented
integer byte mappings grade every field. Executed mutants must fail all reduced
cases; every field has a mutation check and seven damaged stream forms must
return ERROR in each suite. Artifacts are under `build/artifacts/owned/n64_rsp_vector_*/`.

The component additionally checks **3,145,728 memory/address/element cases**
covering every DMEM byte on two patterns, **3,145,728 signed-offset/register
cases** covering all seven-bit offsets and vector registers, **1,048,576
payload/element move pairs**, **2,097,152 flag alias cases**, and every GPR/vector
encoding at every byte element. Whole register, scalar, flag, DMEM and IMEM
guards are checked. State `VN64S016` appends 64 packed vector doublewords and
three validated flags (**536 bytes**). Pending-operation replay is exact;
invalid flags are rejected atomically; S015 and earlier restore neutral vector
storage while preserving previously implemented scalar/DP state.

Rules follow the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf),
and the hardware-test author's [loads](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_loads.rs),
[stores](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_stores.rs),
[moves](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_mfc2_mtc2.rs)
and [flag aliases](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_cfc2_ctc2.rs).
The original cartridges have not been validated on a physical console.
LFV element 1's first packed low bit follows the author's published mapping;
the published sequential-byte test cannot distinguish competing mappings there.
That bit on non-linear inputs receives no physical accuracy credit. Independent
full-width byte combinations, load interlocks, dual issue, physical DMA/pipeline
timing, vector arithmetic, accumulator/reciprocal state and complete graphics/
audio remain pending. These finite profiles do not establish full RSP accuracy.

The local Super Mario 64 (USA) probe on 2026-10-10 now passes the former LQV
blocker, retires **683 RSP instructions**, and blocks on **VMULF at SP PC `E5C`,
word `4BDFDEC0`**. The CPU reaches 12,000,000 attempts with 51 nonterminal
exception entries and no terminal CPU fault. Production output remains uniform
video and 480 audio sample pairs across 120 scheduling quanta. Native boot,
audible game sound and playability remain unverified. The private image remains
local. N64 stays **40% = 8/20 delivery milestones**.

Verification: **50 distinct N64 gates pass** after two harness corrections.
The initial 46-gate regression (394.74 seconds) found stale audio-test state
offsets; the corrected audio gate passed in 20.42 seconds. Vector loads passed
in 386.02 seconds, scalar/vector transfers in 114.31 seconds and the final
component in 3.08 seconds. The initial store run correctly rejected an incomplete
stream at the old execution cap; the full 786,432-case store corpus passed in
408.66 seconds with a larger budget, without reducing the domain. The final
build and undefined-behavior sanitizer passed; all **80 original ROMs** reproduce
from the committed image/generator ledger. Completion renderer and whitespace
checks pass. CI/CD remains removed and GitHub Actions disabled.

## RSP vector multiply and accumulate

The RSP vector module now implements all sixteen multiply-family encodings:
VMULF/U, VMULQ, VMUDL/M/N/H, VMACF/U/Q, VMADL/M/N/H and VRNDP/N.
Eight raw 48-bit accumulator lanes wrap modulo 2^48. Destination selection uses
signed or unsigned saturation, raw low/middle slices and quantized masks as
required by each instruction. VMUDM/VMADM multiply unsigned VS by signed VT;
VMUDN/VMADN multiply signed VS by unsigned VT. Source lanes are captured before
any destination writes, including aliases. All element selectors work, and
VRNDP/N use the VS register index parity for the addend position. VMACQ preserves
the low accumulator slice. These instructions preserve VCO/VCC/VCE.

VSAR reads accumulator high/middle/low with elements 8/9/10, writes zero for the
other elements and leaves the accumulator unchanged. Scalar/vector moves and
memory transfers preserve the accumulator. Other vector operations, including
VADD, still block explicitly at their PC without a fabricated BREAK or IRQ.

Two new original MIT programs emit actual guest register observations with no
guest verdict. Their independent host oracle uses unbounded integer arithmetic,
a literal lane-selection table and separate accumulator slices:

| Original cartridge | Cases | Captured 64-bit words | Declared domain |
|---|---:|---:|---|
| [Multiply pairs](tests/roms-src/n64/rsp_multiply_pairs/README.md) | 917,504 | 16,515,072 | Seven load-multiply functions × 16 selectors × 256 repeated-byte VS values × 32 groups of eight repeated-byte VT values |
| [Accumulate edges](tests/roms-src/n64/rsp_accumulate_edges/README.md) | 294,912 | 7,372,800 | Nine accumulate/round functions × 8,192 groups of eight raw halfword values × four partner/48-bit accumulator patterns; selectors distributed through the groups |

Each record captures result, all accumulator slices, source registers, flags
and SP PC/status/MI. The edge ROM additionally captures the accumulator before
the tested operation and scalar zero. Its six-instruction accumulator setup
uses VMUDH, four VMADH and VMADM and is independently graded. Ordered REG chunks
contain at most twelve words. Executed capture mutants must fail every reduced
case, all fields have individual mutation checks and seven incomplete/malformed
stream forms must return ERROR. Artifacts are under
`build/artifacts/owned/n64_rsp_multiply_pairs/` and `n64_rsp_accumulate_edges/`.

The component checks **16,777,216 raw-halfword/function/selector cases**,
**8,388,608 register/function/selector cases**, **524,288 VSAR encodings** and
all declared accumulator carry/overflow/sign/quantization boundaries. Whole
vector banks, flags, GPRs, DMEM and IMEM are guarded. Numeric published hardware
observations anchor thirteen multiply/accumulate instructions and even/odd VS
rounding. They caught and corrected the mixed-product signedness during review.

State `VN64S017` appends eight validated raw accumulator doublewords (**64
bytes**) after the S016 vector bank/flags. Pending multiply replay is exact;
nonzero upper 16 bits in any accumulator lane are rejected atomically. S016 and
earlier restore neutral accumulators while preserving previously implemented
fields. Plugin 0.18.0 continues ABI v2.

Sources are the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf)
and the hardware-test author's [multiply](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmulf.rs),
[mixed product](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmudm.rs),
[accumulate](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmadn.rs),
[round](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vrndp.rs),
[quantization](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmacq.rs)
and [VSAR](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vsar.rs) tests.
These original ROMs have not run on a physical console. Their finite operand
patterns are not the exhaustive full-width pair corpus. Physical pipeline,
load interlocks/dual issue, remaining arithmetic/flags/reciprocal state, complete
RDP and RSP audio remain pending. The earlier LFV caveat remains ungraded.

Mario now passes VMULF and reaches a real RSP BREAK in the short probe. A longer
trace retires **145,423 RSP instructions** without an unsupported RSP operation,
then encounters unsupported CPU **C.LT.S** (`4608503C`, EPC `80248420`). The DP
also records unsupported **SET_COMBINE** (`FCFFFFFFFFFE793C`). Video remains
uniform; production output is 480 sample pairs at 120 quanta and 96,646 at
2,000 quanta. Native boot, audible game sound and playability stay unverified.
The private image stays local. N64 remains **40% = 8/20 delivery milestones**.

Verification: **53 distinct N64 gates pass** after one harness correction.
The 51-gate regression ran in 895.90 seconds: fifty passed and one still expected
VMULF to be unsupported. Its corrected VADD control passed in 0.42 seconds.
The two new original-ROM suites passed their full 917,504/294,912-case domains,
executed mutants, all 18/25 field mutations and seven stream controls each.
Final vector/multiply components, the full build and undefined-behavior
sanitizer pass. All **82 original ROMs** reproduce from the committed
image/generator ledger; completion renderer, probe budget validation and
whitespace checks pass. CI/CD remains removed and GitHub Actions disabled.

## FPU single and double comparisons

The CPU now executes all sixteen C.cond predicates in single and double format:
F/UN/EQ/UEQ/OLT/ULT/OLE/ULE and SF/NGLE/SEQ/NGL/LT/NGE/LE/NGT.
Integer raw encodings determine less/equal/greater/unordered without host floating
point. Signed zeros compare equal; infinities order normally. **Denormal operands
are compared directly**, with no arithmetic-style unimplemented assist and no
flush-to-zero approximation. The fraction MSB of a VR4300 NaN is set for a
signaling NaN, opposite to the newer IEEE convention. Quiet predicates signal
Invalid only for signaling NaNs; signaling predicates signal Invalid for any NaN.

Comparison replaces FCSR cause bits, commits the condition and sticky Invalid
flag only when the new cause is disabled, and leaves every FPR/GPR untouched.
An enabled Invalid trap preserves the old condition and flags. FPE now clears
stale CP0 Cause.CE, matching the published primary hardware observation. CU1 is
checked before numeric/encoding handling. Odd FR=0 sources retain the documented
RI convention; reserved formats and nonzero unused encoding bits use the E-assist
convention. Those undefined/reserved conventions are not physical accuracy claims.

Two original MIT cartridges capture actual guest register output with no guest
verdict. The independent host oracle uses exact rational values and separate
NaN classes and predicate truth masks:

| Original cartridge | Cases | Captured 64-bit words | Declared domain |
|---|---:|---:|---|
| [Comparison pairs](tests/roms-src/n64/fpu_comparison_pairs/README.md) | 262,144 | 1,835,008 | S/D × all sixteen predicates × all -32..31 integer pairs × both old condition bits |
| [Comparison edges](tests/roms-src/n64/fpu_comparison_edges/README.md) | 348,384 | 2,438,688 | Every exponent with cycling fraction patterns; special/sign pairs, full RM/enable/FS/condition products on ten profiles and reserved formats |

Each captures an untouched destination guard, actual FCSR, FPE indicator/code,
both source registers and branch-marker results for BC1F/T/FL/TL, including
actual delay-slot execution or annulment. Original code handles exceptions and
continues to the strict terminator. Executed mutants must fail every reduced
case; all seven fields have mutation controls and seven damaged/incomplete
stream forms must return ERROR. Artifacts:
`build/artifacts/owned/n64_fpu_comparison_pairs/` and `n64_fpu_comparison_edges/`.

The component checks **76,546,048 raw-fraction/exponent predicate cases**,
including every S/D exponent pair, **65,536 source/FR/predicate encodings** with
whole FPR/GPR guards, and **5,242,880 FCSR products**. It covers CU1 precedence,
reserved fields/formats, enabled/disabled exceptions, stale CE clearing,
BD/BEV/nested context and exact pending-comparison state replay. State remains
`VN64S017`; no new persistent fields are needed. Plugin 0.19.0 keeps ABI v2.

Rules follow the NEC [VR4300 manual](https://hack64.net/docs/VR43XX.pdf), chapters
7/8 and C.cond, and the hardware-test author's primary
[comparison tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/cop1/compares.rs)
and [raw NaN constants](https://github.com/thelemmy/nemu64-test/blob/main/src/cop1.rs).
The author's host-language quiet/signaling names are opposite to the VR4300
classification; raw bits and observed results determine this implementation.
These original ROMs have not run on a physical console. Finite operand patterns
do not exhaust every full-width pair. FPU physical timing/interlocks, complete
CP0/CPU and devices, remaining RSP/RDP and playable retail titles remain pending.

The local Mario probe now passes C.LT.S and reaches its **200,000,000-attempt
CPU trace limit without a terminal fault**. It retires **728,648 RSP
instructions** without an unsupported RSP operation. The RDP still blocks on
**SET_COMBINE** (`FCFFFFFFFFFE793C`, CURRENT `227018`, reason 1). Across 2,000
production scheduling quanta video stays uniform and 96,646 audio sample pairs
are observed. Native boot, audible game sound and playability remain unverified.
The private ROM stays local. N64 remains **40% = 8/20 delivery milestones**.

Verification: **56/56 N64 gates passed in one clean run** (700.37 seconds),
including both complete comparison corpora and their executed mutants, all seven
field controls and seven damaged stream controls each. The final expanded
fraction-pattern component and undefined-behavior sanitizer pass. The full build
passes; all **84 original ROMs** reproduce against the committed image/generator
ledger. Completion renderer and whitespace checks pass. Current executable hashes
match both recorded native probes. CI/CD remains removed and Actions disabled.


## RDP combiner command state

SET_COMBINE (0x3C), SET_PRIM_COLOR (0x3A) and SET_ENV_COLOR (0x3B)
now update persistent RDP command state. Both RGB/alpha selector banks are
independently decoded. Primitive RGBA, minimum LOD and fractional LOD, and
all environment RGBA bits are retained. These commands advance CURRENT without
asserting DP IRQ. FullSync remains the actual IRQ producer. Source layouts are
[SGI RDP Command Summary, tables 19, 21 and 22](https://hcs64.com/files/RDP_COMMANDS.pdf).
Discarding unused color/primitive bits is a deterministic convention; physical
reserved-bit behavior is ungraded.

This increment stores and decodes attributes. **Color-combiner arithmetic,
blending, texture sampling, triangle rasterization and depth operations remain
pending.** Existing bounded fill-cycle rectangles continue using FILL_COLOR.
Unsupported draws retain CURRENT and a real diagnostic, without fabricated
pixels or completion interrupts.

The original `rdp_combiner_commands` ROM grades **131,072 cases / 917,504
actual 64-bit words** over all repeated 16-bit payloads and both RDRAM and
wrapping DMEM lists. CURRENT, STATUS, MI IRQ and a RAM guard are captured before
and after a separately extended FullSync. This proves command progression and
IRQ behavior; internal attributes have no hardware readback in this ROM.
The component independently checks **43,264 selector pairs** and **524,288
command streams**, every reserved input bit, LOD/color retention, unsupported
draw failure, frozen pending-list replay and atomic invalid-state rejection.
An executed capture mutant, all seven field mutations and seven malformed
stream controls must fail. The component also passes undefined-behavior
sanitization. See the original corpus README for the exact finite scope.

State **VN64S018** appends three validated 64-bit attribute slots (**24 bytes**).
S017 and all previously accepted older versions restore neutral attributes and
preserve their earlier fields. Plugin **0.20.0** retains ABI v2. Earlier state
component offsets move by the appended 24 bytes.

The current private Super Mario 64 (USA) probe passes the former SET_COMBINE
blocker. At 2,000 scheduling quanta the CPU reaches **200,000,000 attempts**,
with 444 nonterminal exception entries and no terminal fault; the RSP retires
**728,648 instructions**, without an unsupported operation, and reaches real
BREAK (SP PC 0x144, status 0x243). The DP next blocks at **SET_Z_IMAGE**,
word **FE00000000000400**, CURRENT **0x227090**, reason 1. Production output
remains uniform video and **96,646 audio sample pairs**. Sample counts do not
prove audible game sound. Boot/playability remain unverified; no commercial
bytes are committed. The default 120-quantum probe remains bounded at
12,000,000 CPU attempts, 6,026 RSP instructions and 480 audio sample pairs.

Delivery completion stays **40% (8/20 milestones)**. This command-state
increment does not complete the CPU, RDP or native graphics milestone. The
front-page and core percentages remain synchronized with the fixed ledger.

Verification: **58 distinct N64 gates have passing results**. The full batch
passed 56 gates; its AI assertion used an old state-tail offset, and the new
ROM builder used OR instead of addition for one input address. Both were
corrected and their targeted gates pass, including the complete original-ROM
domain and negative controls. All **85 original ROM archives** reproduce;
completion synchronization and whitespace checks pass. CI/CD remains disabled.


## RDP fill pipeline and depth clears

The fill-cycle engine now accepts bypassed texture, combiner, blender and dither
mode state, including the native clearing path, and writes actual 8-, 16- and
32-bit framebuffer bytes. Fill pattern selection follows the absolute RAM
address. SET_Z_IMAGE stores its 26-bit address; clearing depth redirects the
color image to depth RAM, as documented by Nintendo, rather than performing
depth compares. The original corpus clears separate color and depth buffers.

Fractional rectangle/scissor bounds, field-even/field-odd selection and odd row
strides are supported. Width is a memory stride: X spans can extend beyond it
and overlap the next row. The detailed span model includes the truncated high
X scissor pixel, while Y clipping tests quarter scanlines before writing a row.
This corrects the older inset-scissor oracle, whose high X endpoint was excluded.
The original archived integer fill cartridge is unchanged; its oracle now
matches the expanded span model.

Command fields and depth clearing follow the [SGI RDP command summary](https://hcs64.com/files/RDP_COMMANDS.pdf)
and [Nintendo fill-cycle documentation](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-01.html).
The quarter-span and hazard details are inferred from the RDP author's primary
[edge-walker implementation](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/rasterizer.c).
Nintendo gives a broad four-pixel scissor warning; these gates characterize the
author's detailed span model, **not physical subpixel/scissor accuracy**.
Image reads, depth compares and stepped depth-update modes are explicit blockers.
Hardware crash timing and partial crash writes are ungraded; the functional
engine rejects these profiles before any write. Four-bit fills are blocked.

The new original `rdp_fill_pipeline` corpus enumerates **294,912 input patterns**
and grades **589,824 rows / 6,488,064 actual 64-bit words**. All 12,288 safe low
16-bit modes, CI8/RGBA16/RGBA32, RDRAM/wrapping DMEM and four geometry profiles
are covered. Two ordered 11-word rows per pattern contain color/depth pixels,
three guards and actual CURRENT/STATUS/MI state. An independently executed
capture mutant, 22 field mutations and seven malformed streams must fail.
The two rows fit the result channel's 255-byte line limit.

The component passes **1,572,864 fractional/scissor/field/stride cases**,
**196,608 mode cases**, **196,608 raw fill-code cases**, all **1,024 widths**
and all **67,108,864 depth-image address values**, plus guards, selected-row
bounds, actual depth clears, FullSync IRQ, frozen-list replay, rejected state
fields and legacy conversion. Address enumeration proves command retention;
it is not a depth-comparison or physical-register readback test. The component
also passes undefined-behavior sanitization.

State **VN64S019** appends a validated 8-byte depth-image scalar. S018 and
previously accepted older states restore a neutral depth pointer and preserve
previous fields. Plugin **0.21.0** retains ABI v2. Earlier component state-tail
offsets move by eight bytes. Hidden coverage/depth bits, exact bus/DMA/raster
timing, texture loads/sampling, triangles, blending, combiner arithmetic and
actual depth comparisons remain missing or ungraded. Reserved image formats
and input bits receive no accuracy credit.

The private Mario probe now passes SET_Z_IMAGE and the real buffer clears.
At **2,000 scheduling quanta**, the CPU reaches **200,000,000 attempts** with
**534 nonterminal exception entries** and no terminal fault. The later graphics
list blocks at **SET_TILE**, word **F510000007000000**, CURRENT **227128**,
reason 1. The RSP blocks at **VCH**, word **4AFDE8E5**, SP PC **5F4**, status
**C1**, after **215,369 instructions**. Production video remains uniform with
**93,572 audio sample pairs**; sample counts do not prove audible game sound.
The default 120-quantum probe remains at 12,000,000 CPU attempts, 6,026 RSP
instructions and 480 sample pairs. **Retail boot/playability remain unverified.**
No commercial bytes are committed.

Delivery completion remains **40% (8/20 milestones)**. The complete graphics
milestone and the overall Nintendo-platform goal remain unfinished. Root and
core README percentages are synchronized from the fixed completion ledger.
CI/CD remains disabled.

Verification: the full build and **60/60 N64 gates pass** in one regression
run (**676.76 seconds**). All **86 original ROM archives** reproduce. The new
component passes undefined-behavior sanitization, both private retail reports
match current executable hashes, and completion/whitespace checks pass.

## RSP vector comparison and clipping

The RSP now executes **VLT, VEQ, VNE, VGE, VCL, VCH, VCR and VMRG**.
All 16 element encodings select VT lanes; inputs are captured before destination
writes so overlapping registers work. VCO, VCC and VCE follow each instruction's
rules. The destination also replaces accumulator LOW16 while retaining its
upper 32 bits. VCH's zero/-1 boundaries feed VCL and VMRG. Remaining arithmetic,
logic, reciprocal operations and physical pipeline/interlock timing are pending.
Unsupported instructions still halt with an explicit diagnostic and no fake
BREAK or interrupt.

The functional rules follow the hardware-test author's primary
[nemu64-test comparison tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_arithmetic.rs).
The new component independently checks **134,217,728 lane cases**: every 16-bit
VS value, eight boundary/relationship VT partners, all 32 incoming lane-flag
combinations and all eight functions. Another **81,920 actual vector cases**
cover all selectors and register positions, five alias patterns, nonuniform
flags, untouched registers, accumulator upper slices and scalar/DMEM guards.
Clipping chains, real dispatch/BREAK IRQ, pending-state replay and unsupported
VADD behavior are checked. These finite domains do not prove all full-width
operand pairs or physical hardware timing.

| Original ROM | Cases | Actual 64-bit captures | Domain |
|---|---:|---:|---|
| [Comparison pairs](tests/roms-src/n64/rsp_comparison_pairs/README.md) | 262,144 | 4,718,592 | 256 repeated-byte VS values × 32 groups of eight repeated-byte VT lanes × four incoming flag profiles × eight functions |
| [Comparison edges](tests/roms-src/n64/rsp_comparison_edges/README.md) | 262,144 | 4,718,592 | 8,192 groups of consecutive VS halfwords × boundary VT lanes × four incoming flag profiles × eight functions |

Selectors rotate with input/function/profile; the original corpora do not cover
every selector/flag for every operand. Each actual RSP sequence seeds the
accumulator with VMUDH, executes the comparison, and captures destination,
all accumulator slices, both sources, VCO/VCC/VCE and SP PC/status/MI. The guest
computes no verdict. Independent whole-integer grading checks all 18 fields.
An executed capture mutant, eighteen individual field mutations and seven
malformed streams are required to fail. These original cartridges have not run
on a physical console. Artifacts are under
`build/artifacts/owned/n64_rsp_comparison_pairs/` and `n64_rsp_comparison_edges/`.

Plugin **0.22.0**, ABI v2, retains **VN64S019**: its existing vector flags and
48-bit accumulator contain all comparison state, so no state-layout change is
needed.

The private Super Mario 64 probe now passes **VCH** and advances twelve more
RSP instructions. At 2,000 scheduling quanta, the CPU reaches **200,000,000
attempts**, with **534 nonterminal exception entries** and no terminal fault.
The next RSP blocker is **VADD**, word **4B1FED50**, SP PC **624**, status **C1**,
after **215,381 instructions**. RDP **SET_TILE** remains blocked at word
**F510000007000000**, CURRENT **227128**, reason 1. Video remains uniform;
**93,572 audio sample pairs** do not prove audible game sound. The default
120-quantum probe retains its 12,000,000-attempt bound and 6,026 RSP instructions.
Scheduling quanta are not game frames. **Native boot/playability are unverified.**
The commercial ROM remains local and is not archived.

Delivery completion stays **40% (8/20 milestones)**. The full graphics milestone
and the overall Nintendo-platform goal remain unfinished. README percentages
remain synchronized from the fixed ledger. CI/CD remains disabled.

Verification: the full build and **63 distinct N64 gates pass**. The two new
original corpus gates pass together in **80.43 seconds**; the remaining **61/61
regression gates** pass in **739.91 seconds**, including the comparison component.
All **88 original ROM archives** reproduce. The comparison component passes
undefined-behavior sanitization. Both private retail reports match current
executable hashes, and completion/whitespace checks pass.

## RSP vector integer arithmetic and logic

The RSP now executes **VADD, VSUB, VABS, VADDC, VSUBC, VAND, VNAND, VOR,
VNOR, VXOR and VNXOR**. All element encodings and source/destination overlaps
use captured input lanes. Arithmetic obeys signed saturation, the sign-selected
VABS special case, unsigned carry/borrow and nonzero flags. VADD/VSUB consume
VCO-low and clear VCO; VADDC/VSUBC produce carry/borrow flags. VABS and Boolean
operations retain all comparison flags. VCC/VCE are preserved throughout these
operations. Accumulator LOW16 receives the unclamped arithmetic residue, which
can differ from the saturated destination; the upper 32 bits are retained.

Rules follow the hardware-test author's primary
[nemu64-test arithmetic/Boolean tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_arithmetic.rs)
and [opcode encodings](https://github.com/thelemmy/nemu64-test/blob/main/src/rsp/rsp_assembler.rs).
The component independently grades **184,549,376 lane cases**: every 16-bit
VS value, eight boundary/relationship VT partners, all 32 incoming lane-flag
combinations and all eleven functions. It also checks **112,640 actual vector
cases** across all selectors, 32 register positions, five alias patterns and
nonuniform flags. Whole-integer residues and per-bit Boolean truth tables are
independent of the production lane helper. Literal carry/borrow producer-consumer
chains, VABS -32768 saturation, untouched registers/scalar/DMEM, actual dispatch,
BREAK IRQ and exact pending-instruction save replay are covered.

| Original ROM | Cases | Actual 64-bit captures | Domain |
|---|---:|---:|---|
| [Integer pairs](tests/roms-src/n64/rsp_integer_pairs/README.md) | 2,883,584 | 51,904,512 | Every repeated-byte operand pair × all 32 incoming lane flags × eleven functions; identity selection |
| [Integer edges](tests/roms-src/n64/rsp_integer_edges/README.md) | 2,883,584 | 51,904,512 | Every VS halfword × all 32 incoming lane flags × eleven functions; rotating selectors and selected boundary VT lanes |

Each real RSP sequence seeds the accumulator with VMUDH, executes the operation,
and records destination, every accumulator slice, both sources, VCO/VCC/VCE,
SP PC/status and MI. The guest computes no verdict. Independent whole-integer
and per-bit grading checks all eighteen fields. The flag loop runs in guest
code, keeping each original archive below 1 MiB. An executed capture mutant,
eighteen field mutations and seven malformed streams must fail. The edge
corpus collectively covers selectors; it does not enumerate every full-width
operand pair, selector and VT partner. These originals have not run on a physical
console. Their artifacts are under `build/artifacts/owned/n64_rsp_integer_pairs/`
and `n64_rsp_integer_edges/`.

Plugin **0.23.0**, ABI v2, retains **VN64S019**. Existing vector flags and
48-bit accumulators contain all integer-ALU state. Unsupported reciprocal,
square-root, move and rare vector operations still block with an explicit
word/PC diagnostic, without a fake BREAK/interrupt. Physical pipeline timing
and interlocks remain ungraded. Existing unsupported-instruction controls now
use the genuinely unsupported VRCP encoding instead of implemented VADD.

The private Mario probe now passes **VADD** and advances another twelve RSP
instructions to **VRCPH**, word **4B7D5EF2**, SP PC **0**, status **C1**, after
**215,393 instructions**. At 2,000 scheduling quanta the CPU reaches **200,000,000
attempts**, with **534 nonterminal exception entries** and no terminal fault.
RDP **SET_TILE** remains blocked at **F510000007000000**, CURRENT **227128**,
reason 1. Video remains uniform and **93,572 audio sample pairs** do not establish
audible game sound. Scheduling quanta are not game frames. **Native boot and
playability remain unverified**; commercial ROM bytes remain local.

N64 stays **40% = 8/20 fixed delivery milestones**. The complete graphics
milestone and the full Nintendo-platform goal remain unfinished. Front-page
and core README percentages are synchronized. CI/CD remains disabled.

Verification: the full build and **66 distinct N64 gates pass**. Both new
original corpora pass in **759.33 seconds**, grading **5,767,168 cases /
103,809,024 actual 64-bit words** with zero mismatches and rejecting the executed
capture mutants, eighteen field mutations and seven malformed streams per suite.
The remaining **64/64 N64 regression gates** pass in **1,242.24 seconds**,
including the new integer-ALU component. All **90 original ROM archives**
reproduce. The component passes undefined-behavior sanitization, both private
retail reports match current executable hashes, and completion/whitespace
checks pass.

## RSP reciprocal, square-root and move

The RSP now executes **VRCP, VRCPL, VRCPH, VMOV, VRSQ, VRSQL and VRSQH**,
plus the **VNOP/VNULL** no-ops. Reciprocal and square-root share a high input,
a 32-bit output and an explicit pending flag. High instructions return the
previous output's high half and prime the input; low instructions consume it
once, even if its high half is zero. Ordinary reciprocal/square-root operations
clear the pending flag. VMOV and no-ops leave divider state unchanged.

Only the destination lane selected by the low three VS-field bits is written.
Reciprocal/square-root source selection uses raw `e & 7`; VMOV uses the ordinary
element map at the destination lane. All seven replace accumulator LOW16 with
selected VT inputs and retain upper slices and VCO/VCC/VCE. The result can differ
from accumulator LOW16. Inputs are captured before aliases are written. No-ops
retain the entire vector/accumulator/flag/divider state.

The approximation and shared-latch rules follow the hardware-test author's
primary [reciprocal/square-root/move tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmov_vrcp.rs),
including published E834 high/low expectations. Production tables are independently
derived with rational quantization and a binary integer-inequality search;
there is no host floating point or copied emulator implementation. The independent
component uses exponent/residue algebra and a restoring integer-square-root
algorithm. It checks all **1,024 table entries**, **393,216 value cases**
(including every signed halfword and exponent/mantissa/sign boundaries),
**2,097,152 cross-family high/low chains**, and **917,504 actual vector cases**
across register positions, every VS encoding, selectors, aliases and flag profiles.
Zero-high pending state, one-time consumption, published numeric expectations,
no-op guards, exact save replay and atomic rejection are checked. These domains
do not enumerate every full-width input or physical interlock.

The original [divider/move ROM](tests/roms-src/n64/rsp_divider/README.md)
enumerates **5,898,240 cases / 123,863,040 actual 64-bit captures**. Every halfword
is swept over nine instructions and ten profiles: unprimed/primed low inputs,
and every high half combined with eight boundary low halves. Source VT values
are broadcast; selector/destination encodings rotate. Twenty-one fields contain
vectors/accumulators/sources, flags, SP/MI, the full output high half and a replay
with low/high probes exposing pending behavior. The guest computes no verdict.
Independent rational/Python integer-square-root/latch grading checks each field.
An executed capture mutant, 21 field mutations and seven malformed streams must
fail. This original has not run on a physical console. Artifacts:
`build/artifacts/owned/n64_rsp_divider/`.

Plugin **0.24.0**, ABI v2, introduces **VN64S020**. An appended eight-byte word
stores input[63:48], output[47:16] and pending[0]; bits[15:1] must be zero.
S019 and previously accepted older states restore neutral divider state and
preserve previous fields. Existing EOF-relative component state checks move
eight bytes. Reserved-bit rejection is atomic; a pending zero-high VRCPL resumes
exactly after restore. Rare vector operations such as VSUT still block explicitly
without a fake BREAK or interrupt. Physical RSP timing/interlocks and full
RDP/audio/hardware compatibility remain unfinished or ungraded.

The private Mario probe now passes **VRCPH** and reports **no unsupported RSP
instruction** at the 2,000-quantum bound. It executes **3,497,074 RSP instructions**;
the last task reaches a real **BREAK**, SP PC **144**, status **243**. The CPU
reaches **200,000,000 attempts**, with **620 nonterminal exception entries** and
no terminal fault. RDP **SET_TILE** remains blocked at **F510000007000000**,
CURRENT **227128**, reason 1; END advances to **227C30**. Video is still uniform;
**93,571 audio sample pairs** do not prove audible game sound. Quanta are not
game frames. **Native boot/playability remain unverified.** Commercial bytes
stay local. This trace does not establish the full RSP hardware scope.

N64 remains **40% = 8/20 fixed delivery milestones**. The complete graphics
milestone and full Nintendo-platform goal remain unfinished. Front-page and
core README percentages remain synchronized. CI/CD stays disabled.

Verification: the full build and **68 distinct N64 gates pass**. The new original
suite passes in **1,657.36 seconds**, independently grading **5,898,240 cases /
123,863,040 actual 64-bit words** with zero mismatches and rejecting all executed,
21-field and seven malformed-stream controls. **24/24 component gates** pass in
**50.48 seconds**. The remaining ROM batch passes 42 of 43 in **1,285.47 seconds**;
the video capture helper needed its EOF-relative VI offset moved eight bytes for
S020, and the corrected gate passes in **4.78 seconds**, completing all 43.
All **91 original ROM archives** reproduce. Undefined-behavior sanitization,
current executable hashes for both private retail reports, completion synchronization
and whitespace checks pass.

## RDP tile and texture-image state

The RDP executes **SET_TILE (0x35), SET_TILE_SIZE (0x32) and
SET_TEXTURE_IMAGE (0x3D)** and retains all documented fields. Eight independent
tile descriptors contain format, texel size, line/TMEM word addresses, palette,
S/T clamp/mirror/mask/shift; separate bounds retain raw unsigned 10.2 SL/TL/SH/TH.
The texture image retains format/size, width-minus-one and its 26-bit byte address.
Descriptor updates preserve bounds and other tiles; size updates preserve descriptors.
The image width accessor adds one. This state is available to subsequent texture
operations. There is no fabricated TMEM transfer or textured output.

Layout follows [SGI RDP Command Summary, tables 3, 6 and 9](https://hcs64.com/files/RDP_COMMANDS.pdf).
Reserved command bits are discarded as an explicit deterministic convention;
physical reserved-bit behavior is ungraded. Command-state progression from
RDRAM and XBUS DMEM uses the shared DP interface, including wrap/freeze/END
extension. Only real FullSync asserts DP IRQ. LOAD_BLOCK, LOAD_TILE, LOAD_TLUT,
texture sampling/filtering/coordinates, rasterization and exact timing remain pending.

The [original tile command ROM](tests/roms-src/n64/rdp_tile_commands/README.md)
provides **131,072 cases / 917,504 actual register/RAM words** covering every
16-bit repeated payload through both sources, with all eight tile indices rotating.
It captures CURRENT/STATUS/MI before and after FullSync and an untouched RAM guard.
It computes no verdict. The independent host grader checks the exact finite domain
and rejects an executed capture mutant, seven field mutations and seven malformed
streams. **There is no hardware readback of internal tile/image state in this ROM.**
The component independently grades **10,107,008 descriptor-field pairs**, all encoded
field values at all tile indices, **1,048,576 source/tile state streams**, every
input bit, 131,072 isolated coordinate-field cases, raw bounds and image fields, reset and real unsupported-load blocking.
This is neither the full payload/address corpus nor physical-console verification.

Plugin **0.25.0**, ABI v2, saves **VN64S021**. The appended **136 bytes / 17 words**
store descriptor/bounds per tile then texture image; reserved bits must be zero.
Restore validates every word before updating the machine. S020 and previously
accepted older formats restore neutral texture state and preserve previous fields;
the S020 conversion specifically preserves a pending zero-high divider input and
nonzero output. Frozen pending texture commands replay exactly. Existing EOF-relative
state checks and the ABI video capture move 136 bytes.

The private Mario probe passes the prior **SET_TILE** blocker. At **2,000 scheduling
quanta / 200,000,000 CPU attempts**, the next block is **LOAD_BLOCK**, word
**F3000000073FF100**, CURRENT **227158**, reason 1, END **227C30**. There are **620
nonterminal exception entries**, no terminal fault, **3,497,074 RSP instructions**,
no unsupported RSP at this bound, and a real BREAK (SP PC **0x144**, status **0x243**).
Video remains uniform. **93,571 audio sample pairs** do not prove game sound.
Quanta are not game frames. **Native boot/playability remain unverified.** Commercial
ROM bytes remain local. Repeatable reports are in `build/artifacts/retail/n64-tile.json`
and the default `build/artifacts/retail/n64.json`.

N64 remains **40% = 8/20 fixed delivery milestones**; the complete graphics
milestone and full Nintendo-platform goal remain unfinished. Front-page and core
README percentages stay synchronized. CI/CD remains disabled.

Verification: the full build and **70 distinct N64 gates pass**. The full batch
passes 69 of 70 in **1,803.04 seconds**. The AI component's symbolic EOF offset
needed the same 136-byte shift as other state checks; with both rejection and
legacy-conversion offsets corrected, that gate passes in **17.65 seconds**.
Production code is unchanged by this correction. The new original tile suite
passes in **10.57 seconds**, grading **131,072 cases / 917,504 actual words** with
zero mismatches and rejecting all 16 executed capture mutants, seven field
mutations and seven malformed streams. The divider regression also passes in
**1,599.30 seconds**. All **92 original ROM archives** reproduce; the final
texture component passes undefined-behavior sanitization without compiler warnings.
Both private retail reports match current executable hashes. Completion synchronization,
new evidence links and whitespace checks pass.

## RDP texture-memory transfers

The RDP now executes **LOAD_BLOCK (0x33), LOAD_TILE (0x34) and LOAD_TLUT (0x30)**
into real **4 KiB TMEM**, stored as canonical big-endian bytes. Block transfers
use integer S/T and 1.11 DxT; tile/palette commands use raw 10.2 coordinates.
Loads update the selected tile's bounds while retaining descriptors and neighbors.
Full eight-byte groups include padding beyond a partial final span. DxT controls
odd-row longword swapping and line strides. TMEM addresses wrap; RGBA32 and YUV
split data between banks. Even-address palette fetches replicate a halfword;
odd addresses fetch a full word. Source size controls source/coordinate advance.
Loads preserve signed 16-bit coordinate wrapping and fractional edge behavior. Source reads
use the 24-bit RDP memory interface and zero for absent backing. These rules are
implemented with integer addressing, with no host floating point.

Field and transfer descriptions come from the [SGI RDP Command Summary](https://hcs64.com/files/RDP_COMMANDS.pdf).
Bank/partial-transfer rules were checked against the reference author's
[texture loads](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/tex.c),
[TMEM indexing](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/tmem.c)
and memory-interface source. An optional comparison against commit
`9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc` passes **168,192 full-TMEM
comparisons** across explicit load, coordinate and multirow partner domains.
The [repeatable reference harness](tests/reference/n64-tmem/README.md) records the exact domains.
This is software-reference evidence, **not physical-console verification**.
The independent component uses inverse bank-address algebra and byte scatter,
plus literal normal/odd rows, RGBA32 halves, palette alignment, fractional
edges and signed coordinate wrapping. No third-party implementation is vendored.

The component grades **665,884 byte cases**: every TMEM word, all source alignments,
source sizes 8/16/32, all encoded descriptor formats/sizes, all 4,096 DxT values,
counts 1..4,095, coordinate encodings, neighboring stride/base partners,
65,536 halfword-value patterns, and fractional/multirow tile and palette profiles.
These are explicit sweeps and partner products, **not every Cartesian product**
or all source addresses. The entire 4 KiB result is compared, including untouched
bytes. Load progression from RDRAM/XBUS, DMEM command wrapping, END extension,
nominal transfer-group counters, real FullSync IRQ and no RAM writes are checked.
Four-bit source images and multirow palette loads are blocked before writes with
reason 3. Physical crash side effects and timing remain ungraded; this atomic
blocking is a diagnostic convention. Byte sampling, palette lookup, filtering,
coordinate processing for draws, triangles, blending/depth and VI filtering remain
pending. TMEM presence does not establish rendered texture correctness.

The [original transfer-command ROM](tests/roms-src/n64/rdp_tmem_commands/README.md)
passes **393,216 cases / 2,752,512 actual 64-bit register/RAM captures** in
**58.73 seconds**, with zero mismatches and all 48 executed capture mutants,
seven field mutations and seven malformed streams rejected. It checks consumption,
FullSync IRQ and an untouched RAM guard for all three loads, through both sources.
**Internal TMEM bytes have no CPU readback in this ROM** and are verified separately
by the component. Counts are four texels/entries with zero origins; its geometry
is deliberately stated rather than described as the full hardware corpus.

Plugin **0.26.0**, ABI v2, saves **VN64S022**. The appended **4,096 raw bytes**
store all TMEM contents; all byte values are legal. S021 and previously accepted
older states restore zero TMEM and preserve preceding fields. Pending frozen loads
replay exactly; S021 conversion preserves descriptors, bounds, image and a pending
zero-high divider with nonzero output. Invalid state lengths reject atomically.
Reset clears TMEM. Existing EOF-relative state/capture checks move 4,096 bytes,
including the AI test's symbolic offset. TMEM_BUSY counts settled transfer groups
in this profile; it is not a measurement of physical pipeline clocks.

The private Mario probe passes **LOAD_BLOCK**. At **2,000 scheduling quanta /
200,000,000 CPU attempts**, it next blocks on **shaded/textured/depth-tested
triangle opcode 0x0F**, word **CF000FFA0FFAF006**, CURRENT **227160**, reason 1,
END **227C30**. TMEM's nominal counter is **0x100 groups**. There are **620
nonterminal exception entries**, no terminal fault, **3,497,074 RSP instructions**,
no unsupported RSP at this bound and a real BREAK (SP PC **0x144**, status **0x243**).
Video remains uniform. **93,571 audio sample pairs** do not prove game sound.
Quanta are not game frames. **Native boot/playability remain unverified.** Commercial
bytes remain local. Reports: `build/artifacts/retail/n64-tmem.json` and default
`build/artifacts/retail/n64.json`.

N64 remains **40% = 8/20 fixed delivery milestones**. Full graphics and the full
Nintendo-platform goal remain unfinished. Front-page and core README percentages
stay synchronized. CI/CD remains disabled. **72 distinct N64 gates pass**: the 70 existing regressions passed in
**2,149.11 seconds**, and both TMEM gates passed after the reference-driven
corrections. The full build and undefined-behavior sanitizer pass; all 93
original archives reproduce. This verifies the stated implemented profiles,
not the full native hardware or playable retail games.

## RDP scalar combiner arithmetic

The [internal arithmetic kernel](cores/n64/COMBINER-MATH.md) now implements special nine-bit
A/B/D extension, signed C multiplication, the 128 rounding bias, 17-bit RGB
residue, nine-bit alpha and the RDP clamp. **262,144 A/B pairs**, **268,173,312
reduced difference/C/D cases** and **512 clamp encodings** pass the independent
integer component. The exact finite reduction and limits are documented.
The reduced RGB/alpha cases and clamp values also pass the actual routines in
pinned Angrylion RDP Plus; the repeatable optional harness is linked there.
Undefined-behavior sanitizer and existing command-state/replay checks pass.

This is a kernel for the pending draw pipeline. Guest triangles remain blocked;
source routing, one-/two-cycle integration, keying, coverage/alpha coupling,
dithering, texture sampling, blending/depth and rasterization remain unfinished.
The original command ROM grades command progression, not this arithmetic.
Physical-console validation and hardware timing remain unverified. Native Mario
boot/playability remain unverified. ABI v2/plugin 0.26.0 and S022 are unchanged;
N64 remains **40% = 8/20 fixed delivery milestones**. CI/CD remains disabled.

**73 distinct N64 gates pass cumulatively**: the 72 verified S022/TMEM gates
plus the scalar arithmetic gate. The production N64 plugin built from this
branch is byte-identical to the TMEM build, confirming no change to existing
guest execution or state layout. This kernel is not yet wired to guest draws.

## Four digital ports and production netplay

Plugin **0.27.0**, emulator ABI v2, now exposes `INetplayCapable` for four
virtual digital PIF ports. The production default network plugin owns two
participants: host input reaches port 0, client input reaches port 1, and ports
2/3 remain neutral. The N-player core API routes up to four virtual masks;
this does not supply a four-peer network transport or local multi-pad frontend.
Normal local execution still supplies port 0. All four pads are permanently
present in this profile, with zero stick bytes and no accessories.

The [original guest](tests/roms-src/n64/controller_netplay/README.md)
uses actual SI DMA and reads four replies, completion status and acknowledged
status. **16,384 rows / 98,304 words** pass a separate bit-assignment oracle;
all four inputs receive different permutations of the digital domain. The
executed wrong-capture control fails all 16 rows; six field mutations and seven
malformed streams reject. **65,536 isolated component cases** cover every
port and digital mask; sanitizer checks pass. The existing single-input original
ROM is unchanged; its oracle now expects a neutral second pad. Absent-channel
component coverage uses channel 4, outside the four controller ports.

The production TCP test passes **100 scheduling quanta** in two independent
processes. Host, client and baseline match **600 actual guest words** and the
complete state. Both peers perform rollback with real AI replay callbacks.
The wrong-capture guest is rejected with exactly 100 mismatched words; six field
mutations, four malformed result streams, four fragmented invalid network
packets and ROM mismatch are rejected. Different pre-session histories reload
fresh; core replacement and external execution disconnect. Reports and raw
readbacks are in `build/artifacts/netplay-n64/`. Internet, two-machine and
Windows networking are unverified. Quanta are not measured VI game frames.

State **VN64S023** appends three big-endian eight-byte digital masks to S022,
for **8,478,744 bytes**. Reserved upper halves reject atomically. Older accepted
states restore the additional ports to neutral and preserve earlier fields.
Fast save queries the exact bound, rejects undersized buffers without writes,
and hashes the complete serialized state. Netplay drains output PCM each
quantum with or without a callback, keeping muted and streamed states equal;
normal local polling retains its existing behavior. Actual guest-generated AI
samples exercise this determinism check.

The current private **Super Mario 64 (USA)** probe still blocks on triangle
**CF000FFA0FFAF006**, CURRENT **227160**, after **200,000,000 CPU attempts**.
Video remains uniform; native boot and playability are unverified. The current
production core SHA-256 is
`56dd577178d40bae0b7b47125381f55b27d942db73bba49c752fc1eea42818f5`;
trace SHA-256 is
`b6fc9dd72f1c2390043903d91c3fbdab75ae449122632a1d3de6bdc5c388a650`.
Reports: `build/artifacts/retail/n64-controllers.json` and default `n64.json`.
Commercial ROM bytes remain local.

N64 now has **45% = 9/20 fixed delivery milestones**. This credits production
netplay for the implemented profile. Analog/accessories/presence detection,
local multiplayer frontend routing, complete graphics/audio/CPU/devices,
physical timing and retail playability remain unfinished. CI/CD remains disabled.
**77 distinct N64 gates pass cumulatively**. The broad regression run passed
71/73 in **1,906.17 seconds**; two legacy fixture checks still used old EOF
offsets. Updating those offsets for the 24-byte S023 extension preserves their
original assertions; both pass in a **22.32-second targeted rerun**. The four
new controller/netplay gates pass. All six production core network gates pass
in **57.15 seconds**. The full build, sanitizer, completion renderer, changed
local documentation links and whitespace checks pass. All 94 original archives
reproduce. Production binaries were unchanged by the fixture corrections.

## Switch AArch64 and native executable profile

The new emulator ABI v2 [Switch plugin](cores/switch/README.md) executes an
[original NSO0 guest](tests/roms-src/switch/arithmetic_registers/README.md).
**1,572,864 arithmetic observations / 7,864,320 actual register words** match
an independent unbounded-integer oracle. Its executed mutant fails all 384 short
cases; five field and seven malformed-stream controls reject. The component
passes **13,877,248 integer cases**, all 256 condition/NZCV combinations, SP/XZR,
loader/map guards and complete state replay. Counts cover explicitly stated
finite domains; they do not establish full CPU/hardware accuracy.

The loader accepts a checked uncompressed NSO0 subset, rejects compression/hash
flags, and maps text/RO/data/BSS with permissions into a fixed 4 MiB process
profile and test stack. `VNXS0001` captures **4,194,648 bytes**, pinned to the
image. Production ABI replay checks actual register streams and complete states.
Firmware/HOS, compressed/hash-checked images and other formats, MMU/native RAM,
full CPU/extensions/exceptions, GPU/audio, native devices/controllers, persistent
saves, netplay, timing and retail playability remain pending. Original bytes
are MIT; no firmware/keys/SDK/commercial files are included. Switch is now
**25% = 5/20 delivery milestones**. CI/CD remains disabled; the full Nintendo
implementation goal remains active.


## AArch64 multiply and divide

Plugin **0.2.0**, emulator ABI v2, now implements MADD/MSUB in both widths,
SMADDL/SMSUBL and UMADDL/UMSUBL, SMULH/UMULH, and 32/64-bit SDIV/UDIV.
Aliases with Ra=XZR provide MUL/MNEG and signed/unsigned long multiply/negate.
Operands use XZR, not SP. W results zero-extend, all forms preserve NZCV,
zero divisors produce zero, and signed division truncates toward zero with
MIN/−1 retaining the architectural low-width result. Four 32-bit limbs produce
the upper product without host 128-bit extensions. Signed division uses
unsigned magnitudes to avoid host signed overflow. No state layout changes:
`VNXS0001` and the 4 MiB process profile remain unchanged.

The [new original guest](tests/roms-src/switch/muldiv_registers/README.md)
passes **2,071,552 rows / 12,429,312 actual words**: all ordered pairs of 256
repeated-byte and 16 unique boundary values, two accumulator/NZCV seeds and
fourteen forms. Its independent unbounded-integer oracle grades source operands,
accumulator, result, actual NZCV and context. The executed result-bit mutant
fails all 448 short cases; six field and seven stream controls reject. A
production ABI gate compares real register output and full states after restore
while this guest executes. Artifacts: `build/artifacts/owned/switch_muldiv/`.

The bit-serial full-product/long-division component checks **33,147,520 snapshots**:
**33,144,832** table-pair/form/accumulator cases across all sixteen initial NZCV
values, plus **2,688** register-routing/alias cases. It checks source, SP,
unrelated-register, flag and PC/step preservation; reserved base encodings
reject before architectural writes. All fourteen original opcode forms match
the [pinned LLVM encoding reference](tests/reference/switch-muldiv/README.md).
That is an encoding comparison, not execution on a physical console.

All **six Switch gates pass** in **14.26 seconds**; component UBSAN passes with
warning-free compilation. The full build passes and all 96 original archives
reproduce. Completion renderer, changed local documentation links and whitespace
pass. Switch stays **25% = 5/20 fixed delivery milestones**: the whole CPU,
firmware/HOS/retail boot, native memory/MMU/devices, GPU/audio, controllers,
saves/netplay, hardware timing and exhaustive native corpus remain incomplete.
CI/CD remains disabled. The full Nintendo implementation goal remains active.


## AArch64 conditional select

Plugin **0.3.0**, emulator ABI v2, adds 32/64-bit CSEL, CSINC, CSINV and CSNEG.
All sixteen conditions consume actual NZCV; AL and NV select the first source.
False conditions select the second source unchanged, incremented, inverted or
negated. Arithmetic wraps at the result width, W writes zero-extend, both
sources and the destination use XZR semantics, and flags/SP are preserved.
Reserved S/bit-11 encodings stop with a diagnostic before architectural writes.
`VNXS0001`, the 4 MiB process map and scheduling remain unchanged.

The [original guest](tests/roms-src/switch/select_registers/README.md) passes **524,288 rows / 2,621,440 actual words**:
all ordered pairs of sixteen boundary/mixed-bit operands, all sixteen NZCV
inputs and all sixteen condition codes across eight forms. Real STR/MRS
capture sources, result, flags and context; a separate boolean-table and
unbounded-integer host oracle grades the file. It caught a boundary constant
error in the initial builder, corrected before shipment. The executed result
mutant fails all **8,192** short rows; five field and seven stream controls
reject. The production ABI gate replays real output and complete states while
this guest executes. Artifacts: `build/artifacts/owned/switch_select/`.

The independent truth-mask and bit-level component passes **151,846,912
snapshots**: 151,519,232 repeated-byte/boundary pair/form/flag/condition cases
and 327,680 register-number, alias/XZR cases across all flag/condition inputs.
All eight guest forms match [pinned LLVM opcode fields](tests/reference/switch-select/README.md); physical
execution is unverified. The previous core failed the component and stopped
the guest before any register row, establishing the missing instruction path.

All **nine Switch gates pass** in **22.03 seconds** after the full rebuild,
including the six existing regressions. Component UBSAN passes with warning-free
compilation. Full build, completion renderer and whitespace pass; all **97
original archives** reproduce. Switch stays **25% = 5/20 fixed delivery
milestones**: full CPU/extensions/exceptions, HOS/native boot, native RAM/MMU,
GPU/audio/controllers/devices, saves/netplay, physical timing and the complete
original hardware corpus remain pending. CI/CD remains disabled, no commercial
bytes are included, and the full Nintendo implementation goal remains active.


## AArch64 variable shifts and unary bit operations

Plugin **0.4.0**, emulator ABI v2, implements LSLV/LSRV/ASRV/RORV in both
widths, RBIT, REV16, REV32 (including W REV), X REV64, CLZ and CLS. Shift
counts mask to five/six bits; arithmetic shifts sign-fill, and zero rotations
avoid host shifts by the operand width. Byte reversal is local to the selected
group, CLS excludes the sign bit, W writes zero-extend, and all forms preserve
NZCV. Operands/destinations use XZR semantics and leave SP unchanged. Reserved
unary encodings reject before architectural writes. `VNXS0001`, ABI/state
layout, fixed process map and scheduling remain unchanged.

The [original guest](tests/roms-src/switch/bitops_registers/README.md) passes **1,120,096 rows / 5,600,480 actual words**:
272 repeated-byte/full-width boundary operands, all shift counts 0–255, two
flag seeds and eight variable-shift forms (1,114,112 rows), plus eleven unary
forms (5,984 rows). Actual source/count/result/MRS NZCV/context are emitted by
guest STR instructions. Independent unbounded arithmetic, binary strings and
byte-sequence operations grade these files. The executed result mutant fails
all **172** short rows; five field and seven malformed-stream controls reject.
The production ABI gate replays actual register output and complete states.
Artifacts: `build/artifacts/owned/switch_bitops/`.

The separate per-bit component passes **9,218,304 snapshots**: 9,121,792
variable-shift cases, 47,872 unary cases, and 48,640 alias/XZR/register-routing
cases. It covers all sixteen initial NZCV values and six extra count patterns
with upper bits set. All registers, flags, SP and counters are checked; reserved
opcode/Rm/S and shift controls reject. The previous production core stopped
the original guest before any register row, and the component failed before
implementation. All nineteen original forms match the [pinned LLVM encoding
reference](tests/reference/switch-bitops/README.md); this does not claim physical execution.

All **twelve Switch gates pass** in **24.92 seconds** after a warning-free full
build, including nine existing regressions. Component UBSAN passes with clean
compilation. All **98 original archives** reproduce; actual report identities,
completion renderer, changed local links and whitespace pass. Switch stays
**25% = 5/20 fixed delivery milestones**: full CPU/extensions/exceptions, native
memory/MMU, firmware/HOS/retail boot, GPU/audio/controllers/devices, persistent
saves/netplay, physical timing and the complete native hardware corpus remain
pending. These finite operand domains do not claim complete hardware accuracy.
CI/CD remains disabled. The full Nintendo implementation goal remains active.


## AArch64 bit-field and extract

Plugin **0.5.0**, emulator ABI v2, adds W/X SBFM, BFM, UBFM and EXTR. This
includes their fixed-shift, sign/zero-extension and field insertion/extraction
aliases. BFM preserves bits outside the field; SBFM extends the source end bit;
EXTR reads Rn:Rm in high:low order. W results zero-extend, NZCV/SP remain
unchanged, and register aliases/XZR follow native operand rules. Reserved
opcode/N/width/immediate encodings stop before architectural writes.
`VNXS0001`, the process map and scheduling remain unchanged.

The [original guest](tests/roms-src/switch/bitfield_registers/README.md) passes **494,592 rows / 2,967,552 actual words**:
sixteen boundary/mixed-bit inputs, zero/all-ones prior destinations with NZCV
seeds 9/F, and every legal immediate across both widths. There are 15,360
bit-field plus 96 extract instructions. Actual source/high, complement/low,
prior destination, result, MRS NZCV and context are captured by STR. Separate
contiguous-field arithmetic and double-width host integers grade the file.
The executed result mutant fails all **240** short rows; six field and seven
malformed-stream controls reject. Production ABI replay matches actual output
and complete states. Artifacts: `build/artifacts/owned/switch_bitfield/`.

The independent per-bit component passes **247,414,784 snapshots**: 133,693,440
field cases, 113,639,424 extract cases and 81,920 register/alias/XZR cases.
Every legal immediate, all sixteen initial NZCV values and full EXTR ordered
pairs across the 272 repeated-byte/boundary values are checked. The former
core failed the component and emitted no guest row before implementation.
Every one of the **15,456 generated instruction words** matches eight
[pinned LLVM opcode/width patterns](tests/reference/switch-bitfield/README.md). These are finite operand
domains; physical instruction execution and native timing remain unverified.

All **fifteen Switch gates pass** in **36.35 seconds** after a warning-free full
build, including twelve existing regressions. Component UBSAN passes with clean
compilation. All **99 original archives** reproduce; report identities,
completion renderer, changed local links and whitespace pass. Switch stays
**25% = 5/20 fixed delivery milestones**: the full CPU/extensions/exceptions,
native memory/MMU/devices, firmware/HOS/native boot/retail compatibility,
GPU/audio/controllers, persistent saves, netplay, physical timing and the
complete original native corpus remain pending. CI/CD remains disabled. The
full Nintendo implementation goal remains active.


## AArch64 logical immediates

Plugin **0.6.0**, emulator ABI v2, adds W/X AND, ORR, EOR and ANDS immediate
instructions. Encoded runs of ones rotate and repeat at the selected element
width. All-ones elements, zero selector and W/N mismatches are rejected before
architectural writes. Source register 31 is XZR; destination 31 is SP for the
three non-flag operations and XZR for ANDS. W writes zero-extend. ANDS sets N/Z
and clears C/V; other forms preserve NZCV. `VNXS0001`, ABI, process map and
scheduling remain unchanged.

The [original guest](tests/roms-src/switch/logical_imm_registers/README.md) passes **1,449,984 rows / 7,249,920 actual words**:
sixteen boundary/mixed-bit source values, NZCV seeds 9/F and all **45,312**
legal instruction encodings (four operations × 3,648 W plus 7,680 X encoded
masks). Duplicate encodings of the same mask are included. Actual source,
result, MRS flags, incoming flags and context are captured by STR. Independent
forward run/element descriptors and binary-string logic grade the file. The
executed result mutant fails all **128** short rows; five field and seven
malformed-stream controls reject. Production ABI replay matches actual output
and full states. Artifacts: `build/artifacts/owned/switch_logical_imm/`.

The independent forward-element/per-bit component passes **197,296,128
snapshots**: 197,197,824 legal-mask/source/flag cases and 98,304 register/alias/
SP/XZR cases. All **20,224** reserved encoded masks/operations are rejected
with preserved registers, SP and flags. An initial component reference
assertion was corrected: a 32-bit element repeats in X width, giving
`0x100000001`, not scalar one. The former core failed the component and
emitted no guest register row. All 45,312 words match [pinned LLVM fixed
opcode/width fields](tests/reference/switch-logical-imm/README.md); physical execution remains unverified.

All **eighteen distinct Switch gates pass cumulatively**. The first batch
passed seventeen and exposed that reference assertion; after correction the
rebuilt component and production replay pass in **10.18 seconds**. All fifteen
existing regressions and the strict new guest passed the first batch. UBSAN
passes with clean compilation, full build is warning-free, and all **100
original archives** reproduce. Report identities, completion renderer, changed
local links and whitespace pass. Switch stays **25% = 5/20 delivery milestones**:
full CPU/extensions/exceptions, native RAM/MMU/devices, firmware/HOS/boot/retail
compatibility, GPU/audio/controllers, persistent saves/netplay, native timing
and the complete original/physical corpus remain pending. These finite operand
domains do not establish full hardware accuracy. CI/CD remains disabled and
the full Nintendo implementation goal remains active.


## AArch64 conditional comparison

Plugin **0.7.0**, emulator ABI v2, adds W/X CCMP and CCMN with register or
five-bit immediate operands. It evaluates the old condition flags, then writes
arithmetic NZCV on a true condition or the encoded fallback on false. Incoming
carry is not an arithmetic operand; AL/NV are always true. All GPRs/SP remain
unchanged, register sources use XZR, and reserved S/bit-4/bit-10 encodings
reject before architectural writes. `VNXS0001`, ABI, process map and scheduling
remain unchanged.

The [original guest](tests/roms-src/switch/ccmp_registers/README.md) passes **786,432 rows / 3,932,160 actual words /
12,582,912 individual flag observations**. Each row packs sixteen actual MRS
NZCV nibbles, one for every fallback value, using real UBFM/ORR instructions.
Guest STR captures left/right operands, incoming flags, packed results and
context; no expected values are in the image. Every condition and incoming
NZCV value, sixteen boundary/mixed-bit sources, all register operand pairs and
all 32 five-bit immediates are checked in both widths. The separate oracle
uses boolean conditions and unbounded signed/unsigned arithmetic. The executed
mutant fails all **12,288** short rows; five field, sixteen packed-lane and
seven malformed-stream controls reject. Production ABI replay matches actual
packed output and complete states. Artifacts: `build/artifacts/owned/switch_ccmp/`.

The separate truth-mask and bit-serial full-adder component passes **14,155,776
snapshots**: 4,194,304 register, 8,388,608 immediate and 1,572,864 alias/XZR
cases. Every incoming/fallback NZCV value and condition is covered, including
source aliases and every register number. The previous core fails this new
component and stops the guest before any row. All **2,112 compare base words**
match the [pinned LLVM opcode/width fields](tests/reference/switch-ccmp/README.md). These finite operand
domains do not establish physical execution, full-width pair enumeration or
native hardware timing.

All **twenty-one Switch gates pass** in **76.99 seconds** after a warning-free
full build, including all eighteen existing regressions. Component UBSAN passes
with clean compilation. All **101 original archives** reproduce; report
identities, completion renderer, changed local links and whitespace pass.
Switch stays **25% = 5/20 delivery milestones**: full CPU/extensions/exceptions,
native memory/MMU/devices, firmware/HOS/native boot/retail compatibility,
GPU/audio/controllers, persistent saves/netplay, physical timing and complete
native/physical corpus remain pending. CI/CD remains disabled. The full
Nintendo implementation goal remains active.


## RDP complete command packets

Plugin **0.28.0**, emulator ABI v2, fetches complete native command packets,
including all eight triangle forms (4–22 64-bit words) and two-word texture
rectangles. CURRENT advances with each fetched word. Partial packets survive
an END boundary and continue on extension; coefficient words cannot become
commands. A complete unimplemented draw retains its packet and reports a
blocker without drawing or asserting FullSync. Freeze/flush and nominal issue
counters retain their functional-profile conventions; physical FIFO depth,
prefetch/raster timing and reserved electrical behavior remain ungraded.

The [original cartridge](tests/roms-src/n64/rdp_packets/README.md) passes **200,704 cases / 1,404,928 actual words**:
all repeated-byte payloads, every opcode-byte alias, RDRAM/wrapping XBUS and
every partial split in all triangle/rectangle packets. Actual CURRENT/STATUS/MI
pending before and after END extension and a RAM guard are independently graded.
Every coefficient is headed by FullSync's byte; none escapes as an interrupt.
The executed mutant fails all **72** short cases; seven field and seven stream
controls reject. Artifacts: `build/artifacts/owned/n64_rdp_packets/`.

The component verifies **1,441,792 coefficient words**, **648 partial header
states**, and **10,240 RDRAM/XBUS streams / 110,592 word-boundary captures**.
Every eight-byte DMEM alignment, retained data, freeze/flush, fetch faults and
state rejection/replay are checked. The old DP implementation failed the
incomplete-packet check by blocking on its first word. Component UBSAN passes
with clean compilation. A production ABI gate checkpoints a real partial guest
packet and restores through `INetplayCapable`, reproducing actual output and
complete states, with exact capacity and malformed-state guards.

State **VN64S024** appends **200 bytes** to S023 (start/count/needed and 22
canonical words), totaling **8,478,944 bytes**. Unused words must be zero;
invalid counts/lengths/addresses reject before any machine mutation. Accepted
older formats receive a neutral packet and preserve their represented prefix.
Legacy checks now account for the appended bytes. Existing four digital ports
and production TCP capabilities remain present; timing and analog are pending.

The private **Super Mario 64 (USA)** probe was repeated for **2,000 scheduling
quanta / 200,000,000 CPU attempts**. It has no terminal CPU fault and uniform
video. Packet **CF000FFA0FFAF006** is now retained completely: **22/22 words**,
start **0x227160**, fetched CURRENT **0x227210**, blocker reason 1. Triangle
shading/texturing/depth and native boot/playability remain unimplemented or
unverified. The reader does not fabricate successful graphics. Reports are
local at `build/artifacts/retail/n64-packets.json`, including all 22 retained
packet words for renderer diagnosis; no commercial bytes are
archived. Core SHA-256: **5b97a566a9030d0beb1b94bf1226725dd14c81dc5b93933c5f07759d9da07df4**;
trace SHA-256: **29498832f3274f0a64b72e984b01a80ee89efdc67717e7c7d52a65bfb5324939**.

All **102 original archives** reproduce. **80 distinct N64 gates pass
cumulatively**: the 79-gate batch finished in **2,598.12 seconds**, with 78
passes and one sandbox socket-creation failure; that TCP netplay gate passes
with local networking allowed (**2.66 seconds**, unchanged binaries). The new
production packet-state ABI gate separately passes (**0.15 seconds**). Final
component UBSAN, completion rendering and whitespace checks pass. N64 remains
**45% = 9/20 delivery milestones**, with complete native CPU/devices/RSP/RDP/audio, boot/retail play,
analog/accessories/local frontend routing, physical timing and the exhaustive
native/physical corpus still pending. CI/CD remains disabled; the full Nintendo
implementation goal remains active.

## RDP fill-cycle triangle spans

Plugin **0.29.0**, emulator ABI v2, now executes fill-cycle triangle packets.
The decoder separates all eight edge/shade/texture/depth layouts. Edge walking
uses signed 28-bit X and 30-bit slopes, quantized quarter-scanline increments,
mid-edge replacement, sticky fractional clipping and field selection. Bit 55
set places the major edge on the left in the verified raster path. Fill-cycle
writes use the fill word; shade/texture/Z payloads remain attributes rather than
commands. One/two-cycle triangles remain explicitly blocked.

The [original framebuffer cartridge](tests/roms-src/n64/rdp_triangle_fill/README.md) passes **795,648 cases /
10,343,424 actual 64-bit words**. Its color domain covers all 65,536 halfword
patterns combined with their complements, three pixel sizes, both sources and
both directions. The separate 9,216-row matrix covers eight geometry/scissor
profiles, all 32 triangle opcode bytes, three field selections, both sources
and three pixel sizes. Actual framebuffer words, guards, CURRENT, STATUS and
MI pending are independently graded. The executed mutant fails all **9,288**
short cases; thirteen field and seven stream controls reject. This does not
exhaust all 32-bit fill words or arbitrary native geometry combinations.
Artifacts: `build/artifacts/owned/n64_rdp_triangle_fill/`.

Component gates verify **24,576 coefficient basis packets**, **2,097,152
halfword routing packets**, **1,572,864 signed-Y packets**, **4,096 header
combinations** and **640 atomic rejections**. The modular incremental edge
oracle matches **7,062,117 quarter-edge captures**. Ninety-six packet/field
fill dispatches plus mirrored golden spans verify actual RAM, bounds/kill
rejection and payload versus real FullSync. Final component UBSAN passes with
clean compilation. The earlier DP failure gate now explicitly submits an
unsupported one-cycle triangle; legal empty fill triangles can complete.

**15 affected N64 verification gates pass**, including the original rectangle,
fill-pipeline and packet cartridges, production packet-state replay, controller
ports, state and two-process TCP netplay. The four integration gates pass in
**103.14 seconds**; seven component gates pass in **3.11 seconds** and three
state/controller gates in **0.39 seconds**. All **103 original archives**
reproduce. State remains **VN64S024 / 8,478,944 bytes**; these raster calculations
add no serialized machine fields. Four digital ports and TCP capabilities
remain present; native analog/accessories and timing remain pending.

The private Mario probe was repeated for **2,000 scheduling quanta /
200,000,000 CPU attempts**. No terminal CPU fault is observed, video is uniform,
and the same one-cycle packet **CF000FFA0FFAF006** remains blocked: **22/22**
words, start **0x227160**, fetched CURRENT **0x227210**, reason 1. Report:
`build/artifacts/retail/n64-triangle-fill.json`. Core SHA-256:
**67f08620c2e60314900b3b3ece19c65765343413f322f1c002f866d4e561b47b**;
trace SHA-256: **339076695f22f7a0255e8352f5c670dc9e029dcd9a935767e54603b0075155f8**.
No commercial bytes are archived.

N64 remains **45% = 9/20 delivery milestones**. Shading, texture sampling,
coverage/blending/depth, hidden RDRAM bits, native FIFO/raster timing, crash
partial writes, complete CPU/devices/RSP/audio, native boot/retail play and the
complete native/physical corpus remain unfinished. Unsafe profiles/bounds
reject before writes under the functional convention. CI/CD remains disabled;
the full Nintendo implementation goal remains active.

## RDP subpixel coverage helpers

The triangle renderer now shares clipped quarter-row geometry with an eight-sample
coverage helper. Component checks compare the packed mask to an independent
point-coordinate oracle: **134,217,728 boundary-pair observations**,
**655,360 mixed-row/range observations**, all **256 coverage bytes** and
**576 coverage destination states**. Fixed triangle masks also check edge
samples. The component passes undefined-behavior sanitization.

This helper is preparation for one-cycle rendering. It is not yet connected
to framebuffer coverage storage, blending or depth testing, and has no guest
coverage-output corpus. The original fill renderer uses the shared geometry;
shade interpolation and shaded/textured/depth-tested triangles remain pending.
N64 remains **45% = 9/20 delivery milestones**. CI/CD remains disabled.

The private Mario probe was repeated for **2,000 scheduling quanta /
200,000,000 CPU attempts**. No terminal CPU fault is observed; video is uniform.
The same packet **CF000FFA0FFAF006** remains blocked (reason 1), with **22/22
words** retained, start **0x227160** and fetched CURRENT **0x227210**.
Boot/playability remain unverified. Local report:
`build/artifacts/retail/n64-coverage.json`. Core SHA-256:
**435f192a075e654bab2055115497fd0da284e6ddc43dde5e422a3d29cbfaa944**;
trace SHA-256: **6679232a8169c2169dc180f0e532286be90bcd16f6ff6d58491373752d3d5477**.
No commercial ROM bytes are archived.

The shared geometry also passes the original framebuffer cartridge: **795,648
cases / 10,343,424 actual 64-bit words** independently graded. The executed
mutant fails all **9,288** short cases; all thirteen field and seven malformed
stream controls reject. The existing DP and triangle fill component gates pass
in **3.03 seconds**; the registered coverage gate passes in **1.18 seconds**.
Artifacts: `build/artifacts/owned/n64_triangle_coverage_regression/`.

## Triangle row interpolation

A functional interpolation module now prepares **RGBA, S/T/W and Z** from
decoded triangle coefficients. It handles modular row accumulation, the
major-edge load subrow, fractional-X correction, clipped-start wrap, quantized
pixel increments, coverage-position shade/depth correction and depth-gradient
normalization. Arithmetic follows the
[RDP author's rasterizer model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/rasterizer.c);
these software oracles do not establish physical raster timing.

The separate incremental host oracle passes **196,608 row captures** and
**2,949,120 pixel captures**, including all eight interpolants. Additional
checks cover **327,680 depth-normalization inputs**, **4,194,304 shade-gradient
corrections** (all signed 13-bit gradients, both axes and every coverage mask),
**12,582,912 depth corrections** (all 22-bit depth codes under three selected
masks/gradients), and **131,072 constant-shade/mask combinations**. Invalid
row/pixel bounds reject atomically. The final component passes UBSAN with
clean warning-as-error compilation.

Five related component gates pass in **2.98 seconds**; the fault-runner gate
passes in **6.90 seconds**. The repository's
repeatable fault runner first verifies the unmodified module, then requires
all **eight deliberately faulty module variants** to fail the independent
oracle; compilation errors do not count as rejection. Artifacts:
`build/artifacts/component/n64_triangle_attributes/`. From the repository root:

```sh
cmake --build build --target n64_rdp_triangle_attributes_test
ctest --test-dir build -R '^n64_rdp_triangle_(attributes_test|attribute_mutants)$' --output-on-failure
```

This module is not yet connected to one/two-cycle framebuffer rendering. It
has component evidence, with no guest interpolation-output corpus yet.
Perspective division, texture sampling, blending, hidden memory bits and depth
testing remain pending. Production core/trace executable hashes still match
the preceding private Mario report (`build/artifacts/retail/n64-coverage.json`):
Mario remains blocked at its shaded/textured/depth-tested triangle, with
uniform video and unverified boot/playability. No commercial bytes are
archived. Plugin **0.29.0**, ABI v2 and **VN64S024** remain unchanged.
N64 stays **45% = 9/20 delivery milestones**. CI/CD remains disabled; the full
Nintendo implementation goal remains active.

## One-cycle RGBA32 triangle pixels

Plugin **0.30.0**, ABI v2, connects quarter-row geometry, coverage masks,
RGBA interpolation and combiner source routing to actual one-cycle RGBA32
framebuffer stores. Supported inputs include shade, primitive/environment
colors, ONE/ZERO and primitive LOD. One-cycle uses the second combiner program.
All four coverage destinations and optional RGBA32 memory-coverage reads work;
with AA disabled, the upper-left coverage sample gates stores. Width is a
stride, not an X clip. Unsupported profiles and unsafe bounds reject before
writes under the functional convention. Real FullSync remains the IRQ source.

The original one-cycle cartridge covers **792,576 cases / 10,303,488 actual
64-bit observations**: all 65,536 RG pairs with complemented BA, three programs,
both directions and both RDRAM/wrapping XBUS sources, plus a 6,144-case matrix
of eight geometry profiles, eight programs, fields, sources, coverage
destinations and memory-read settings. Actual framebuffer/guard words and
CURRENT/STATUS/MI are independently graded. The guest contains numeric commands
and capture instructions, with expected pixels only in the separate host
oracle. This does not exhaust arbitrary RGBA colors, gradients or native
pipeline combinations. Artifacts: `build/artifacts/owned/n64_rdp_triangle_shade/`.

Combiner component checks pass **2,170,880 RGB captures** and **4,194,304 alpha
captures**, both cycles, all selector products under declared source patterns
and each RGB selector under all 512 varied nine-bit patterns. The complete
source router accepts explicit combined/texel/key/noise/conversion/LOD inputs;
those inputs are not yet generated by the renderer. Eight fixed framebuffer/
coverage goldens and **47 atomic profile/packet/bounds rejections** pass.
Final routing and pixel-component UBSAN pass with warning-as-error compilation.
Alpha arithmetic has component evidence; RGBA32 framebuffer alpha stores
coverage, not the combiner alpha.

The **12 affected component/state/controller gates** pass in **7.58 seconds**.
The three original rectangle/fill-pipeline/packet regression cartridges and
new shade component pass in **98.82 seconds**. Production two-process TCP
netplay passes in **2.69 seconds**. All **104 original archives** reproduce;
the complete build is warning-free. State remains **VN64S024 / 8,478,944
bytes**; the independent one-cycle profile adds no retained feedback fields.

The private Mario probe was repeated for **2,000 scheduling quanta /
200,000,000 CPU attempts** with no terminal CPU fault. Video remains uniform;
packet **CF000FFA0FFAF006** is still blocked (reason 1), **22/22 words**, start
**0x227160**, fetched CURRENT **0x227210**. Boot/playability remain unverified.
Report: `build/artifacts/retail/n64-one-cycle-shade.json`. Core SHA-256:
**549f2bda0b3de2dc7d7606dec23b54dd539f62b846933967392f894b355e5f1d**;
trace SHA-256: **ca4c14e6dc6d1473b17af09f831fb16092c8ec7b83bc6fb97839a94b6218ad1f**.
No commercial ROM bytes are archived.

N64 remains **45% = 9/20 delivery milestones**. Complete native CPU/devices/
RSP/RDP/audio and physical timing/corpora, retained combiner feedback, keying,
texture fetch/LOD/perspective, blending/AA/dither/alpha comparison, other
framebuffer formats and hidden bits, Z testing and native retail play remain
pending. Four digital ports/TCP remain present; analog/accessories/local
multiplayer frontend routing remain unfinished. CI/CD remains disabled; the
full Nintendo implementation goal remains active.

The full original framebuffer corpus passes all **792,576 cases**. Its
**6,216-case** short baseline passes; the executed capture mutant fails all
6,216 cases, thirteen field controls each detect one mismatch, and seven
malformed-stream controls reject. Together the evidence above accounts for
**18 distinct affected N64 gates**; it is not a fresh whole-core gate batch.
Completion rendering and whitespace checks pass.

## Texture tile coordinate processing

The texture pipeline now has a functional tile-coordinate module for one/
two-cycle sampling. It accepts signed 16-bit S/T with five fractional bits
after division/LOD saturation, applies all sixteen shift encodings, subtracts
fractional tile origins, clamps integer/fractional coordinates, and applies
mask/mirror rules. Mask widths above ten saturate at ten; mask zero enables
clamping. Upper-bound clamping takes precedence for inverted bounds. Coupled
neighbor coordinates preserve the native model's distinct S/T wrap rules and
mirror endpoints. The implementation follows the
[RDP author's coordinate model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tcoord.c)
and [coupled mask model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tex.c).

Independent scalar division/modulo/reflection oracles pass **134,217,728
coordinate/shift/mask/flag captures**: all 65,536 raw coordinates, sixteen shifts,
sixteen masks, clamp/mirror combinations and both axes, at one selected bound
pair. A second **134,217,728 captures** cover every 12-bit origin/upper-bound
pair on both axes, four probes derived from the unshifted bounds and varied
shift/mask/flag settings. Those bound pairs do not cross every possible raw
coordinate and flag combination. **65,536 paired-axis captures** and fixed S/T
wrap goldens also pass. The final component passes UBSAN and warning-as-error
compilation.

The registered coordinate component passes in **3.49 seconds**. The repeatable
fault runner passes in **14.39 seconds**: it compiles and verifies an unchanged
baseline, then requires all eight faulty production-header variants to fail
the independent oracle. Compiler errors never count as rejection. Three
related interpolation/shade/combiner component gates pass in **1.18 seconds**.
Artifacts: `build/artifacts/component/n64_texture_coordinates/`. From the
repository root:

```sh
cmake --build build --target n64_rdp_texture_coordinates_test
ctest --test-dir build -R '^n64_rdp_texture_coordinate' --output-on-failure
```

This helper has component evidence and no guest coordinate-output corpus yet.
It is not connected to TMEM texel fetch or pixel rendering. Texture formats,
TLUT sampling, filtering, perspective/LOD and texture-driven combiner inputs
remain unfinished; physical timing is not established by software oracles.
Current production core/trace hashes still match the preceding private Mario
report at `build/artifacts/retail/n64-one-cycle-shade.json`. Mario remains
graphics-blocked with uniform video and unverified boot/playability. Native
hardware/retail completion remains unproven. Plugin **0.30.0**, ABI v2 and
**VN64S024** remain current. N64 stays **45% = 9/20 delivery milestones**;
CI/CD remains disabled and the full Nintendo implementation goal remains active.

## TMEM texel and palette reads

A functional texel reader now decodes canonical big-endian TMEM bytes with
odd-row interleaving, address wrapping and split RGBA32 banks. RGBA, CI, IA,
I and YUV format/size encodings include the native model's alias behavior.
YUV results retain signed U/V and unconverted Y/A; they are not presented as
converted RGB. TLUT index decoding and explicit palette-bank reads support
RGBA5551 and IA16 entries. Bank selection/remapping in the full sampling
pipeline remains separate. The layout follows the
[RDP author's TMEM reader model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tmem.c).

Independent byte-address/permutation and fixed-code expansion oracles pass
**3,145,728 address captures**, **8,388,608 packed-format captures** and
**1,050,624 palette decodes**. Address tests include signed coordinate patterns
across every format/size code and every tile base/line pair under selected
RGBA32 probes. Packed tests cover all 65,536 halfword patterns with complemented
upper-bank data at four selected S/T positions; they do not exhaust arbitrary
32-bit colors or all YUV triples. Palette tests cover all 16-bit entries at
two indices/all four lanes/both interpretations, plus every index/lane under
one selected TMEM pattern. A further **12,288 actual load/fetch captures**
verify RGBA16/RGBA32 data through the existing TMEM loader, two rows, three
wrap-sensitive bases and 256 byte patterns.

Final UBSAN and warning-as-error compilation pass. The two registered reader/
fault gates pass in **5.71 seconds**; three related tile/TMEM/coordinate gates
pass in **5.78 seconds**. The repeatable fault runner requires the unmodified
baseline to pass and all eight deliberately faulty production-header variants
to fail the independent oracle. Compilation failures never count as rejection.
Artifact header SHA matches the current implementation. Artifacts:
`build/artifacts/component/n64_texel_fetch/`. From the repository root:

```sh
cmake --build build --target n64_rdp_texel_fetch_test
ctest --test-dir build -R '^n64_rdp_texel_fetch_' --output-on-failure
```

This helper has component evidence and no guest texel-output corpus yet. It
is not connected to the pixel renderer. Coupled multi-texel sampling, filtering,
YUV conversion, perspective/LOD, texture-driven combiner routing and complete
blending/depth/hidden-memory behavior remain unfinished; software oracles do
not establish physical timing. Current production core/trace hashes still
match the private Mario report at
`build/artifacts/retail/n64-one-cycle-shade.json`. Mario remains graphics-blocked
with uniform video and unverified boot/playability. Plugin **0.30.0**, ABI v2
and **VN64S024** remain current. N64 stays **45% = 9/20 delivery milestones**.
CI/CD remains disabled; the full Nintendo implementation goal remains active.


## Texture filter and conversion arithmetic

Prepared four-corner texels now support the RDP's triangular filtering,
mid-texel four-corner average, signed nine-bit previous-texel feedback and
four-coefficient color conversion. Chroma fractions account for YUV S parity;
chroma and luma choose their triangles separately. Results retain nine bits
for the combiner. This arithmetic follows the
[RDP author's texture pipeline model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tex.c).

An independent weighted-corner oracle passes **67,174,400 RGB lattice cases**,
**2,097,152 chroma lattice cases**, **1,310,720 previous-texel cases** and
**2,359,296 conversion cases**. RGB probes cover every pair of two eight-bit
pattern seeds at all 32-by-32 fractional positions; these correlated corner
patterns do not exhaust arbitrary four-corner colors. Chroma probes cover
selected signed patterns, S parity and mid-texel modes. Feedback probes cover
all encoded nine-bit R/G pairs at five selected fractional positions.
Conversion probes cover all nine-bit coefficient/chroma pairs under selected
branch modes, plus all K1/K2 pairs under selected previous-texel inputs.
Explicit center goldens distinguish four-corner averaging from triangular
filtering. Warning-as-error compilation and UBSAN pass.

The registered arithmetic gate and repeatable production-fault runner pass.
The runner requires its unmodified baseline to pass and all nine deliberately
faulty production-header variants to fail the independent oracle; compilation
failures never count as rejection. Faults exercise triangle selection,
rounding, chroma parity, center averaging, feedback sign/center correction,
result width, coefficient oddness and mixed chroma/luma selection. Artifacts:
`build/artifacts/component/n64_texture_filter/`. From the repository root:

```sh
cmake --build build --target n64_rdp_texture_filter_test
ctest --test-dir build -R '^n64_rdp_texture_filter_' --output-on-failure
```

This helper has component evidence and no guest filter-output corpus yet.
It accepts prepared corners and explicit options; coupled TMEM/TLUT fetch,
mode/convert command state, persistent previous texels and pixel rendering
are not connected. Point sampling, perspective/LOD, texture-driven combiner
routing and full blending/depth/hidden-memory behavior remain unfinished.
Software oracles do not establish physical timing. Current production
core/trace hashes still match the private Mario report at
`build/artifacts/retail/n64-one-cycle-shade.json`. Mario remains graphics-blocked
with uniform video and unverified boot/playability. Plugin **0.30.0**, ABI v2
and **VN64S024** remain current. N64 stays **45% = 9/20 delivery milestones**.
CI/CD remains disabled; the full Nintendo implementation goal remains active.


## Coupled TMEM and TLUT sampling

Four-corner TMEM reads now preserve neighboring-row carry after the initial
8-bit row normalization. Format aliases retain the scalar reader's decoding;
YUV chroma uses its coupled horizontal stride while luma uses the ordinary
neighbor. YUV4/8 and palette modes remap B/A corners when the chroma and luma
triangles differ. TLUT reads select physical banks by corner and chroma
triangle, supporting both RGBA5551 and IA16 interpretations. Nearest TLUT mode
reads all four banks of one index. These rules follow the
[RDP author's coupled TMEM model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tmem.c).

Independent byte-address, fixed-code expansion and lane-assignment oracles
pass **2,097,152 raw quad captures**, **8,388,608 palette quad captures** and
**3,145,728 row-carry captures**. Quad captures include 65,536 selected signed
S/T and neighbor-difference patterns across every format/size encoding;
palette captures test both interpretations and coupled/nearest modes against
nonidentical physical banks. Row-carry probes cover every tile base/line pair
for RGBA, YUV and intensity at every size, with a selected row-255-to-256
transition. These patterns do not exhaust all arbitrary TMEM contents or all
independent coordinate/neighbor combinations.

A further **1,536 composed load/filter captures** run RGBA16/RGBA32 data through
the actual TMEM loader, tile coordinate processing, coupled fetch and mid-texel
filtering. Four source pixels and their independently averaged colors are
checked across 256 patterns and three wrap-sensitive bases. **1,024 actual
TLUT load/fetch captures** verify full 256-entry palette loads, CI8 sampling,
both palette interpretations and coupled/nearest modes. The palette load tile
uses the zero-size descriptor prescribed by the
[SDK's LoadTLUT macros](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm).

Warning-as-error compilation and UBSAN pass. The registered coupled-fetch
and production-fault gates pass. The unmodified runner baseline passes and
all nine compiled faults are rejected, covering row carry, chroma stride,
palette banks, lane remapping, nearest index selection, YUV nibble/alpha/luma
addressing and palette type. Compilation failures never count as rejection.
The existing scalar fetch, scalar fault, descriptor, coordinate and filter
gates also pass after the shared scalar decoder refactor. Artifacts:
`build/artifacts/component/n64_texel_quad/`. From the repository root:

```sh
cmake --build build --target n64_rdp_texel_quad_test
ctest --test-dir build -R '^n64_rdp_texel_quad_' --output-on-failure
```

This module has component and composed-path evidence, with no guest sampling
output corpus yet. Pixel rendering, mode/convert command state, persistent
previous texels, point sampling without TLUT, perspective/LOD and complete
blending/depth/hidden-memory behavior remain unfinished. Software oracles do
not establish physical timing. Production core/trace hashes still match the
private Mario report at `build/artifacts/retail/n64-one-cycle-shade.json`.
Mario remains graphics-blocked with uniform video and unverified boot/playability.
Plugin **0.30.0**, ABI v2 and **VN64S024** remain current. N64 stays
**45% = 9/20 delivery milestones**. CI/CD remains disabled; the full Nintendo
implementation goal remains active.


## Affine textured triangle pixels

The functional one-cycle RGBA32 renderer now samples TMEM and feeds the
current TEXEL0 into cycle one of the combiner. Signed affine S/T from native
triangle interpolants pass through tile shift/clamp/mask processing. Point
sampling, triangular filtering, mid-texel averaging and coupled/nearest TLUT
sampling use the previously verified fetch/filter modules. TLUT supports
RGBA5551 and IA16. YUV filtering retains unconverted signed chroma; conversion
command state is not implemented in this path. The native cycle-zero texture
and cycle-one combiner ordering follows the
[RDP author's pipeline model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/rasterizer.c).

The new repository-owned
[texture triangle ROM](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_triangle_texture)
passes **786,432 cases / 10,223,616 actual 64-bit framebuffer/guard/register
observations**, with zero mismatches. It loads real source bytes through DP
texture-image/tile/load/bounds commands and emits observed RAM, CURRENT,
STATUS and MI pending. Its host oracle independently decodes logical source
pixels, computes weighted-corner filters and walks a quarter-row point grid.

The source domain covers every 16-bit word and complemented partner under
selected RGBA16/RGBA32/IA16 point probes and both command sources. The sampling
domain covers all 32-by-32 fractional pairs, every format/size encoding and
alias, point/triangular/average modes, both triangle directions and
RDRAM/wrapping XBUS using one selected texture pattern. It does not exhaust
arbitrary RGBA32 colors, independent corners, gradient/tile/geometry
combinations, physical timing or retail graphics. This guest corpus has no
TLUT pixel cases yet; four independent component framebuffer goldens cover
coupled/nearest palette rendering with both palette interpretations.

The short guest control passes; a separately executed wrong-capture ROM is
rejected in every row. All thirteen field corruptions and seven stream
integrity/domain/trailer controls are rejected. Four texture combiner
framebuffer goldens cover passthrough and texture/primitive RGB/alpha factors;
35 unsupported-profile rejections preserve RAM. UBSAN and warning-as-error
compilation pass. All 22 related machine/DP/RDP/state component gates pass in
**20.32 seconds**. Production-plugin texture/TMEM and existing partial-command
checkpoint gates pass in **0.31 seconds**, requiring exact subsequent actual
guest output and complete-state replay through the netplay ABI, exact state
capacity and atomic invalid-state rejection. All **105 repository-owned
archived ROMs** reproduce from their sources and match the manifest.

Artifacts: `build/artifacts/owned/n64_rdp_triangle_texture/`. From the repository
root:

```sh
python3 tests/roms-src/build.py n64/rdp_triangle_texture --original-only
ctest --test-dir build -R '^n64_rdp_(triangle_texture|texture_plugin_replay_test)' --output-on-failure
```

The private Mario probe was repeated with current plugin/trace binaries:
**200 million CPU attempts**, no terminal CPU fault, the same complete
**22/22-word CF000FFA0FFAF006 packet at 0x227160** and fetched CURRENT
**0x227210**. The textured/depth-tested profile still blocks with reason 1;
video remains uniform and native boot/playability remain unverified. Report:
`build/artifacts/retail/n64-textured-affine.json` (ignored, no commercial bytes).
Plugin **0.31.0**, ABI v2 and **VN64S024 / 8,478,944 bytes** are current.

Supported textured pixels require the cycle-zero filter/passthrough flag;
conversion/persistent previous texels, TEXEL1/LOD/key/noise routes, perspective,
full AA/blending/depth/hidden-memory behavior, complete devices/audio and
physical timing remain unfinished. State completeness is limited to the implemented profile; the full Nintendo
implementation goal remains active. N64 stays **45% = 9/20 delivery milestones**;
CI/CD remains disabled.
