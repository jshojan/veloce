# Veloce Accuracy Scorecard

Current N64 evidence: [RDP combiner command state and current Mario evidence](#rdp-combiner-command-state). Earlier increment reports are historical.

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
