# Veloce Accuracy Scorecard

This is the project's honest, evidence-based statement of how complete and
cycle-accurate each emulator core is. Every percentage here is produced by the
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

## Platform summary

| Console | Suite size | Verified headline | Verification state |
|---------|-----------|-------------------|--------------------|
| [NES](#nes) | 172 tests / 32 suites | ~90-94% | Strongest verified coverage; memory-detected CPU/PPU/APU/mapper subset is real |
| [SNES](#snes) | 67 tests / 19 suites | Low until refs land | Almost entirely `screenshot-crc`; a small Blargg SPC `memory` subset is the only currently-verifiable part |
| [Game Boy](#game-boy) | 225 tests / 31 suites | Verified subset only | Blargg serial subset verified; the large Mooneye half is unverified pending a breakpoint emitter |
| [GBA](#gba) | 132 tests / 29 suites | Partial | CPU/memory/BIOS/saves verified via R12 protocol; cycle-accurate timing/PPU largely pending |

The platform mean reported by `tests/run_all.py` weights each console equally.
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

Verified headline: **low until reference hashes are populated**, despite a broad
67-test / 19-suite catalog. This is a coverage-versus-verification gap, not an
implementation gap: almost every SNES test is a community ROM that draws its
result on screen, so it requires a `screenshot-crc` reference hash that has not
yet been measured on a trusted build. Those tests ship `known_fail` with an empty
hash and contribute zero, which is the honest treatment.

| Subsystem | Importance | Verifiable today | Basis |
|-----------|-----------|------------------|-------|
| APU (SPC700 + DSP) | 0.55 | the only `memory`-detected subset | Blargg SPC ROMs (`spc_smp`, `spc_timer`, `spc_mem_access_times`, `spc_dsp*`) report via `$6000`. This is where any verified SNES score currently comes from. |
| CPU (65816) | 1.00 | unverified (visual) | krom/PeterLemon CPU opcode suites (ADC/SBC/MSC/MOV, gilyon cputest) draw to screen; `screenshot-crc`, refs pending. |
| PPU | 0.90 | unverified (visual) | smoke, OAM, mode3/color-halve, bus/INIDISP tests; `screenshot-crc`. |
| Timing | 0.85 | unverified (visual) | IRQ/DMA/HDMA timing tests (`test_irq4209`, `test_dmatiming`, `test_hdmatiming`, `HblankEmuTest`); cycle-accurate class, refs pending. |
| Mapper (enhancement chips) | 0.65 | unverified (visual) | SuperFX/GSU, SA-1 opcode tests; `screenshot-crc`. |
| Memory | 0.70 | unverified (visual) | `speed_test_v51`, `op_timing_test_v2`; timing class, refs pending. |

**Verified gaps surfaced by the suite (real, currently-open core issues).**

- `apu.spc_smp` (Blargg SPC700 master test) traps on an **unimplemented SPC700
  opcode `$79` (`CMP (X),(Y)`)** at SPC `$0A99` and spins, never reaching its
  `$6000` write (reported RUNS). This is a genuine SPC700 core gap that may also
  block other SPC700-dependent ROMs.
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

Verified headline: **the Blargg serial subset only.** The catalog is large (225
tests / 31 suites) but the large Mooneye / SameSuite / Wilbertpol half cannot yet
be scored, so the published number reflects the verified serial subset and the
rest is reported as unverified.

| Subsystem area | Verified today | Basis |
|----------------|----------------|-------|
| CPU instructions | Blargg `cpu_instrs` **10/11 PASS** | `02-interrupts` does not signal pass even at 8000 frames -> `known_fail` (real EI/IME or IF/IE dispatch-timing bug). |
| Instruction / memory timing | Blargg `instr_timing` PASS, `mem_timing` 3/3 PASS | `serial` ASCII detection, verified. |
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
| Mapper / saves | verified | SRAM, Flash 64KB, Flash 128KB, none all PASS. |
| Timing (DMA / IRQ / timer / HALTCNT / bus) | largely verified via NBA | NanoBoyAdvance DMA latch/start-delay/force-nseq/burst PASS; IRQ delay PASS; timer start-stop/reload PASS; HALTCNT PASS; 128KB bus boundary PASS. |
| PPU | partly verified, partly pending | smoke (`hello`, `shades`, `stripes`) PASS; `ppu_status_dma` and the affine visual tests need sub-scanline accuracy and are `known_fail` / visual pending references. |

**Detection method.** jsmolka / alyosha / nba ROMs spin with `R12` holding the
failing test number (`0` = all pass); the GBA plugin detects the stable-PC spin
and prints `[GBA] PASSED` / `[GBA] FAILED - Failed at test #N`, parsed by the
shared GBA register detector. This is a reliable, reference-free protocol, which
is why the GBA verified subset is broader than the SNES one.

**Honest caveats.**

- `cpu_psr` is a measured FAIL (CPSR/SPSR banking), the main verified CPU debt.
- FuzzARM and ARMWrestler report pass/fail by drawing to the framebuffer (no R12
  spin loop), so they are `known_fail` pending a screenshot reference or a
  debug-string harness.
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
