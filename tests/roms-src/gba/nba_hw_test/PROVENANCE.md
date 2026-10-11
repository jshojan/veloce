# Provenance: gba/nba_hw_test

Upstream: [`nba-emu/hw-test`](https://codeberg.org/nba-emu/hw-test) on
Codeberg, pinned at `fbc99140e06f083c0a47612467cfbb02470e56dc`
(2026-08-10, "Add timer/tick-before-reload test"). BSD-3-Clause, copyright
2021 fleroviux; the licence is reproduced verbatim in [`LICENSE`](LICENSE)
because [`patches/`](patches/) carries fragments of upstream's source.

The project moved to Codeberg; the GitHub repository
(`github.com/nba-emu/hw-test`) is archived at `3932a0e5` ("Close GitHub
repository"). The Codeberg pin differs from that GitHub commit only by one
added ROM (`timer/tick-before-reload`, not in Veloce's config) and README
text; every file the 10 ROMs below are built from is identical in both.
GBA-2's toolchain proof (`gba/nba_golden_dma_latch`) keeps its GitHub pin.

## What is built

The 10 ROMs `cores/gba/tests/test_config.json` scores:

| ROM | Checks | What it measures |
|---|---|---|
| `dma/latch` | 4 | per-channel DMA data latch, open bus via DMA |
| `dma/start-delay` | 1 | immediate DMA start delay (TM0) |
| `dma/force-nseq-access` | 2 | first DMA access forced non-sequential (EWRAM, ROM) |
| `dma/burst-into-tears` | 3 | DMA3 from IWRAM end into ROM: two values and the time |
| `irq/irq-delay` | 3 | IRQ entry latency from IWRAM, EWRAM, ROM |
| `timer/start-stop` | 2 | timer start and stop latency |
| `timer/reload` | 7 | reload on overflow, reload overwrites by cycle |
| `haltcnt` | 6 | HALT directly, via CpuSet, via DMA; POSTFLG; HALT with IRQ pending |
| `bus/128kb-boundary` | 18 | LDM / DMA timing across region and 128 KiB ROM page boundaries |
| `ppu/status-irq-dma` | 12 | DISPSTAT flags, VCOUNT, IF and H-blank/V-blank/video DMA timing |

`ppu/bgpd` and `ppu/bgx` are **not** rebuilt; see the section on them below.

Build: `tests/roms-src/build.py gba/nba_hw_test` (clones the pin, builds
the unmodified source once to record `golden_sha256` / `rebuilt_sha256`,
applies the patch, rebuilds, copies the ROMs to `build/roms/gba/nba_hw_test/`
and records sha256 and tool versions in `tests/roms-src/rom_manifest.json`).
The config addresses them as `veloce/nba_hw_test/<rom>.gba`
(`cores/gba/tests/runner.py` links `_roms/veloce` to `build/roms/gba`).

## The patch ([`patches/0001-veloce-result-channel.patch`](patches/0001-veloce-result-channel.patch))

Every ROM reports through nba's shared `lib/source/test.c`, so most of the
change lives there and 7 of the 10 `main.c` files are untouched:

- **Channel.** A constructor in `test.c` runs from `__libc_init_array`
  before `main()`, so before each ROM's `consoleDemoInit()`. It calls
  `mgba_console_open()` (the `0x4FFF780` handshake), then writes
  `VELOCE 1 gba nba-hw-test/<title>`. The title is the ROM header title that
  gbafix wrote, which is the make target cut to 12 characters. libgba's
  `consoleInit()` then rebinds stdout/stderr to the screen. So nba's own
  text stays on screen, and only the lines below reach the result file,
  one `mgba_printf()` each.
- **Grammar.** `test_expect*()` keeps its on-screen output unchanged and
  also emits `CHECK <n> PASS <name>` or
  `CHECK <n> FAIL <name> exp=<hex> got=<hex>`. The same values are compared
  against the same expectation. `=` in a name becomes `:`: a token that
  contains `=` would end the check name early (`HBLANK=0` is written
  `HBLANK:0`). `test_print_metrics()` then emits `END <pass>/<total>`.
  This is the grammar in `docs/testing/VELOCE-RESULT.md` section 2, written
  with `tests/roms-src/include/devkitarm/veloce_test.h` and the vendored
  `mgba.c`. Both are reached through the symlinks `lib/veloce` and
  `lib/source/mgba.c`, so nothing is copied. The recipe adds the vendor
  directory to `CPATH` because `mgba.c` includes `<mgba.h>`, and the nba
  Makefiles pass their include directories with `-iquote`.
- **`bus/128kb-boundary`** never calls `test_print_metrics()`. Upstream
  comments "not enough space" (on screen). It now calls
  `test_veloce_end()`.
- **`ppu/status-irq-dma`** is a menu-driven ROM that waits for B after each
  page. `main()` now runs the four pages once, in menu order and unattended,
  before the unchanged menu. Each page opens a `TEST` group, so check names
  read `DISPSTAT HBLANK:0` and similar. `test_veloce_manual_end()` holds the
  `END` line until all four pages have run, because upstream calls
  `test_reset()` per page.
- **Two measured windows are pinned in asm.** See the next section.

No expected value was changed, and none was derived from Veloce.

## Expected values and why the rebuild still measures the same thing

Every expected value is upstream's, hard-coded in its `main.c`. The project
calls itself "GBA hardware tests"; the values are its author's hardware
results. Veloce has not re-measured them on hardware.

Most checks are cycle counts, and a count is only valid for the exact
instructions that ran between the timer start and the timer read. Only
`dma/latch` rebuilds byte-identically from the pinned source with
`tools/rom-toolchain` (devkitARM 16.1 / GCC 16.1). The other nine shipped
binaries came from older devkitARM releases, and their code generation
differs. So the byte-for-byte golden gate is replaced (`golden_gate: false`
in `recipe.json`) by an instruction-window audit,
[`verify_windows.py`](verify_windows.py):

- For each function that executes inside a measured window
  ([`windows.json`](windows.json), 42 functions), the audit disassembles the
  **patched** build and upstream's **shipped** binary.
- It locates the function in the shipped binary and compares the two
  instruction by instruction. Branch targets, PC-relative literal offsets
  and literal-pool words are masked, because they move with the link
  layout.
- Every remaining difference must be recorded in `windows.json` with its
  exact diff and the reason it cannot change the measured value.
  An unrecorded or changed diff fails the gate.

Result at this pin: **35 functions identical, 7 with reviewed diffs, 0
unexplained.**

- Identical: the measured functions of `start-delay`, `force-nseq-access`,
  `burst-into-tears` (`run_dma` inlined into `main`), all five `reload`
  routines, all three `irq-delay` bodies, and libgba's IRQ dispatcher
  (`IntrMain` .. `IntrRet`). That dispatcher runs inside the `irq-delay`
  and `status-irq-dma` windows.
- Reviewed: register allocation or scheduling that sits outside the timer
  window (`start-stop`, `haltcnt` cpuset, the two `haltcnt` timing polls),
  or an equivalent single-cycle instruction (`irq_handler`:
  `cmp r2, #0` for `ands r3, r3, #255`).
- **Pinned by the patch, because the current compiler moved an
  instruction into the window:**
  - `bus/128kb-boundary` `time()`: GCC 16 schedules `movs r0, r1` between
    the timer start and the call. That adds one cycle to all 18 checks.
    `time()` is now naked Thumb asm, byte-identical to the shipped routine,
    including its local `bx r3` stub.
  - `haltcnt` "HALTCNT CPUSET DMA": GCC 16 loads `test_expect()`'s two
    literal arguments between the `CpuSet()` return and the `TM0CNT_L`
    read. The window now lives in naked ARM asm
    `haltcnt_cpuset_dma_time()`, instruction-identical to the shipped code
    up to and including the read.
- Not audited, because they are outside every window:
  - `status-irq-dma`'s `__test_cycle()` (changed by the autorun patch)
    only sets up an IRQ and then calls a 1024-NOP sled in IWRAM. The
    H-blank IRQ lands inside that sled. The timed read runs in code that
    `emit_get_test()` writes as fixed opcode words.
  - Placement: the ROM-resident measured functions (`test_irq_time_rom`,
    `test_haltcnt_timing_rom`) sit at `0x080002ac`, far from a 128 KiB
    page boundary, and none of these ROMs changes `WAITCNT`.

Run it after a build:

```
tests/roms-src/build.py gba/nba_hw_test
tests/roms-src/gba/nba_hw_test/verify_windows.py        # exit 0 = gate passes
```

`build.py` still records, per ROM, `golden_sha256` (shipped) and
`rebuilt_sha256` (unmodified source, current toolchain) in the manifest, so
the one byte-identical ROM (`latch`) and the nine that are not stay visible.

## `ppu/bgpd` and `ppu/bgx` (not converted)

The plan asked for these two to "assert the latched BG2X/BG2PD values". They
can't, honestly:

- The ROMs contain no expectation at all. Each streams increasing values
  into `BG2PD` or `BG2X` with H-blank DMA every scanline and loops forever.
  Their result is the rendered picture (upstream's `expected.png`).
- `BG2X`/`BG2PD` and the internal reference-point latch are write-only.
- The GBA has no CPU-readable pixel or line-buffer readback.

Any `CHECK` written here would compare a value with itself or with an
emulator-side pixel CRC. The first is meaningless, and the second is ruled
out (plan section 11, Q1). So they stay upstream binaries in the
non-scoring `visual_test_suites.ppu_affine_visual`. The observable side of
affine latching (timing and write-only semantics seen through IRQ/VCOUNT)
is what the planned `gba_ppu_readback_and_latch` ROM (GBA-11) is for.

## Results on the Veloce core at this commit

All 10 produce file verdicts, `source=file`, from their own `CHECK` lines.
Nine end on the ROM's `END` line and fail honestly: `reload` 6/7, the rest
0/n. `status-irq-dma` emits the DISPSTAT, VCOUNT and IF pages (9 checks,
all FAIL). It then stays in the DMA page, whose handler waits for the DMA3
video-capture transfer to finish, so the run ends on its 6500-frame budget
with FAIL (`known_fail` in config). Two runs of each ROM give byte-identical
result files.
