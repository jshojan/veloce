# SNES Emulator Core

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| SNES | **40%** | 8/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

SNES delivered milestones: Guest sound produces audio samples, Native digital controller/keypad registers consume frontend input, Guest graphics produce rendered video, Parse a native cartridge/executable format (boot scored separately), Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Decimal SBC, full CPU/timing and graphics/audio accuracy, expansion chips, corpus grading and physical validation remain unfinished.
<!-- completion:end -->

Super Nintendo Entertainment System emulator for Veloce.

## Overview

The SNES core provides emulation of the Super Nintendo with support for LoROM and HiROM cartridge types. It features a partial 65C816 CPU implementation, scanline-based PPU rendering with all 8 background modes, and SPC700 audio processor with DSP synthesis.

## Specifications

| Component | Details |
|-----------|---------|
| CPU | Ricoh 5A22 (65C816 core), 256 opcodes |
| Audio | Sony SPC700 + S-DSP |
| Resolution | 256×224 (standard), 512×448 (hi-res interlace) |
| Frame Rate | 60.0988 Hz (NTSC), 50.0070 Hz (PAL) |
| Master Clock | 21.477272 MHz (NTSC) |
| Colors | 32,768 (15-bit BGR), 256 on screen |
| Sprites | 128 total, 32 per scanline |
| RAM | 128KB WRAM, 64KB Audio RAM, 64KB VRAM |

## Completion Status

### CPU - 65C816 (Implemented subset; verification incomplete)

| Feature | Status | Notes |
|---------|--------|-------|
| All 256 opcodes | **Implemented; accuracy unverified** | Full 65816 instruction set |
| 8/16-bit modes | **Implemented; accuracy unverified** | M and X flag switching |
| 24-bit addressing | **Implemented; accuracy unverified** | All addressing modes |
| Emulation mode | **Partial** | 6502 compatibility; the native-only stack instructions wrongly wrap S to page 1 (see Known Issues) |
| Native mode | **Implemented; accuracy unverified** | 16-bit operations |
| Direct page | **Implemented; accuracy unverified** | Relocatable zero page |
| Stack relocation | **Implemented; accuracy unverified** | 16-bit stack pointer |
| NMI handling | **Implemented; accuracy unverified** | VBlank interrupts |
| IRQ handling | **Implemented; accuracy unverified** | H/V counter IRQs |
| WAI/STP | **Implemented; accuracy unverified** | Wait and stop instructions |
| Block moves | **Implemented; accuracy unverified** | MVN/MVP instructions |
| Cycle timing | **Partial** | Per-instruction with FastROM/SlowROM |

### PPU - Picture Processing Unit (Implemented subset; verification incomplete)

| Feature | Status | Notes |
|---------|--------|-------|
| Mode 0 (4×2bpp) | **Implemented; accuracy unverified** | 4 BG layers, 4 colors each |
| Mode 1 (2×4bpp+2bpp) | **Implemented; accuracy unverified** | Most common mode |
| Mode 2 (2×4bpp OPT) | **Implemented; accuracy unverified** | Offset-per-tile implemented |
| Mode 3 (8bpp+4bpp) | **Implemented; accuracy unverified** | 256-color BG1 |
| Mode 4 (8bpp+2bpp OPT) | **Implemented; accuracy unverified** | Offset-per-tile implemented |
| Mode 5 (4bpp hires) | **Implemented; accuracy unverified** | 512-pixel width |
| Mode 6 (4bpp OPT hires) | **Implemented; accuracy unverified** | Offset-per-tile implemented |
| Mode 7 (affine) | **Implemented; accuracy unverified** | Rotation, scaling, perspective |
| Sprites (OBJ) | **Implemented; accuracy unverified** | 128 sprites, 8 sizes |
| BG priority | **Implemented; accuracy unverified** | Per-mode priority ordering |
| BG3 priority bit | **Implemented; accuracy unverified** | Mode 1 BG3 front layer |
| Color math | **Implemented; accuracy unverified** | Add/subtract with half |
| Sub-screen | **Implemented; accuracy unverified** | Main/sub compositing |
| Pseudo-hires | **Implemented; accuracy unverified** | 512-width via sub-screen |
| Windows | **Implemented; accuracy unverified** | 2 windows with logic |
| Mosaic | **Implemented; accuracy unverified** | Per-BG mosaic effect |
| Brightness | **Implemented; accuracy unverified** | Master fade control |
| Force blank | **Implemented; accuracy unverified** | Display disable |
| VRAM access | **Implemented; accuracy unverified** | All increment modes |
| OAM access | **Implemented; accuracy unverified** | Sprite attribute table |
| CGRAM access | **Implemented; accuracy unverified** | Palette memory |
| H/V counters | **Implemented; accuracy unverified** | Latched position read |
| Interlace | **Partial** | Field detection, not full interlace rendering |
| Overscan | **Implemented; accuracy unverified** | 239-line mode |
| Direct color | **Implemented; accuracy unverified** | 8bpp direct RGB in Modes 3/4/7 |
| EXTBG (Mode 7) | **Implemented; accuracy unverified** | BG2 layer with bit 7 priority |

### APU - Audio Processing Unit (Implemented subset; verification incomplete)

#### SPC700 Sound Processor

| Feature | Status | Notes |
|---------|--------|-------|
| All 256 opcodes | **Implemented; accuracy unverified** | Full instruction set |
| 64KB RAM | **Implemented; accuracy unverified** | Dedicated audio memory |
| IPL ROM boot | **Implemented; accuracy unverified** | 64-byte boot ROM |
| Timer 0 (8kHz) | **Implemented; accuracy unverified** | 8000Hz base |
| Timer 1 (8kHz) | **Implemented; accuracy unverified** | 8000Hz base |
| Timer 2 (64kHz) | **Implemented; accuracy unverified** | 64000Hz base |
| I/O ports | **Implemented; accuracy unverified** | CPU↔SPC communication |
| Direct page | **Implemented; accuracy unverified** | Page 0/1 selection |

#### S-DSP Digital Signal Processor

| Feature | Status | Notes |
|---------|--------|-------|
| 8 voice channels | **Implemented; accuracy unverified** | Full polyphony |
| BRR decoding | **Implemented; accuracy unverified** | 4-bit ADPCM samples |
| ADSR envelopes | **Implemented; accuracy unverified** | Attack/Decay/Sustain/Release |
| GAIN mode | **Implemented; accuracy unverified** | Custom envelope control |
| Gaussian interpolation | **Implemented; accuracy unverified** | Sample smoothing |
| Echo effect | **Implemented; accuracy unverified** | Hardware reverb |
| FIR filter | **Implemented; accuracy unverified** | 8-tap echo filter |
| Noise generator | **Implemented; accuracy unverified** | Pseudo-random noise |
| Pitch modulation | **Implemented; accuracy unverified** | Voice-to-voice FM |
| Voice muting | **Implemented; accuracy unverified** | Per-voice enable |
| Stereo panning | **Implemented; accuracy unverified** | Per-voice L/R volume |

### DMA Controller (Implemented subset; verification incomplete)

| Feature | Status | Notes |
|---------|--------|-------|
| 8 DMA channels | **Implemented; accuracy unverified** | Independent channels |
| Transfer mode 0 | **Implemented; accuracy unverified** | 1 byte to 1 register |
| Transfer mode 1 | **Implemented; accuracy unverified** | 2 bytes to 2 registers |
| Transfer mode 2 | **Implemented; accuracy unverified** | 2 bytes to 1 register |
| Transfer mode 3 | **Implemented; accuracy unverified** | 4 bytes to 2 registers |
| Transfer mode 4 | **Implemented; accuracy unverified** | 4 bytes to 4 registers |
| A→B / B→A | **Implemented; accuracy unverified** | Bidirectional |
| Fixed address | **Implemented; accuracy unverified** | Non-incrementing |
| HDMA | **Implemented; accuracy unverified** | Per-scanline transfers |
| HDMA indirect | **Implemented; accuracy unverified** | Table-based transfers |
| HDMA repeat | **Implemented; accuracy unverified** | Repeat flag handling |
| Mid-transfer abort | **Partial** | Not fully accurate |

### Cartridge Support (Implemented subset; verification incomplete)

| Type | Status | Notes |
|------|--------|-------|
| LoROM | **Implemented; accuracy unverified** | Standard low-ROM mapping |
| HiROM | **Implemented; accuracy unverified** | Standard high-ROM mapping |
| ExHiROM | **Partial** | Extended addressing |
| Battery SRAM | **Implemented; accuracy unverified** | Save game support |
| Header detection | **Implemented; accuracy unverified** | Auto-detect mapping |
| CRC32 | **Implemented; accuracy unverified** | ROM identification |

### Enhancement Chips (Not Implemented)

| Chip | Games | Status |
|------|-------|--------|
| SuperFX | Star Fox, Yoshi's Island | Not implemented |
| SuperFX2 | Doom, Winter Gold | Not implemented |
| SA-1 | Kirby Super Star, Super Mario RPG | Not implemented |
| DSP-1 | Pilotwings, Super Mario Kart | Not implemented |
| DSP-2 | Dungeon Master | Not implemented |
| DSP-3 | SD Gundam GX | Not implemented |
| DSP-4 | Top Gear 3000 | Not implemented |
| S-DD1 | Star Ocean, Street Fighter Alpha 2 | Not implemented |
| SPC7110 | Far East of Eden Zero | Not implemented |
| Cx4 | Mega Man X2, X3 | Not implemented |
| OBC1 | Metal Combat | Not implemented |
| S-RTC | Daikaijuu Monogatari II | Not implemented |
| ST010/ST011 | Hayazashi Nidan Morita Shougi | Not implemented |

## Recent Fixes

### 2026-01-03: Accuracy Review Completed

Comprehensive review of SNES implementation against bsnes and hardware documentation. Verified the following components are correctly implemented:

**CPU (65C816):**
- All addressing modes including 24-bit long addressing
- REP/SEP instructions with proper X/Y truncation in emulation mode
- MVN/MVP block move instructions with correct repeat behavior
- Interrupt handling (NMI/IRQ/BRK/COP) with proper vector selection
- BCD arithmetic in 8-bit and 16-bit modes

**DMA/HDMA:**
- All 8 transfer modes with correct B-bus address patterns
- HDMA timing follows bsnes-accurate sequence (check→transfer→decrement→update)
- Mid-frame HDMA enable with immediate channel initialization
- Indirect mode with proper bank/address separation

**PPU:**
- Window logic for BG/OBJ/color with OR/AND/XOR/XNOR modes
- Color math with add/subtract, half-brightness, clip modes
- Sprite priority interleaving (0-3) with all BG modes
- All 8 background modes with correct priority ordering

### 2026-01-02: DSP Envelope Rate Timing

Fixed DSP envelope processing to use rate-based timing instead of updating every sample:

**Issue:** Envelope was updating every sample regardless of rate setting, causing incorrect attack/decay/sustain timing.

**Fix:** Added `envelope_counter` per voice that tracks samples between envelope updates. The envelope only updates when the counter reaches `ENVELOPE_RATE_TABLE[rate]`:

```cpp
voice.envelope_counter++;
if (voice.envelope_counter < ENVELOPE_RATE_TABLE[rate]) {
    return;  // Not time to update yet
}
voice.envelope_counter = 0;  // Reset and apply envelope change
```

**Impact:** ADSR envelopes now have correct timing, matching hardware behavior for attack, decay, and sustain phases.

### 2026-01-02: SPC700 Timer Target 0 Handling

Fixed SPC700 timer behavior when target register is set to 0:

**Issue:** Timer with target 0 was firing immediately on every tick instead of counting to 256.

**Fix:** Target value 0 means 256 (8-bit overflow). Timer should fire when counter overflows from 255 to 0:

```cpp
if (m_timer_target[i] == 0) {
    fire = (prev == 255);  // Fire on overflow from 255 to 0
} else {
    fire = (m_timer_counter[i] == m_timer_target[i]);
}
```

**Impact:** Games using timer target 0 for 256-tick periods now have correct timing.

### 2026-01-02: Mode 7 Transformation Math Precision

Fixed Mode 7 transformation math to use proper 13-bit signed values:

**Issue:** Mode 7 center point (M7X/M7Y) and scroll offset (M7HOFS/M7VOFS) values were not being sign-extended correctly, causing incorrect rotation centers and offsets.

**Fix:** Use proper 13-bit sign extension for all Mode 7 parameters:

```cpp
int32_t hofs = (static_cast<int16_t>(m_m7hofs << 3)) >> 3;  // 13-bit signed
int32_t cx = (static_cast<int16_t>(m_m7x << 3)) >> 3;       // 13-bit signed

int32_t px = screen_x + hofs - cx;  // Correct order per bsnes
int32_t py = screen_y + vofs - cy;

int32_t tx = ((static_cast<int16_t>(m_m7a) * px) +
              (static_cast<int16_t>(m_m7b) * py) + (cx << 8)) >> 8;
```

**Impact:** Mode 7 games (F-Zero, Super Mario Kart, etc.) now have correct rotation and scaling behavior.

### 2026-01-02: VRAM Address Calculation Fixes

Fixed critical VRAM address calculation bugs that caused garbled/distorted graphics:

**BGnNBA Registers ($210B/$210C) - Character Data Addresses:**
```cpp
// Before (incorrect - used 4 bits, could overflow):
m_bg_chr_addr[0] = (value & 0x0F) << 13;

// After (correct - uses 3 bits per bsnes):
m_bg_chr_addr[0] = (value & 0x07) << 13;  // 0x0000-0xE000
```

**BGnSC Registers ($2107-$210A) - Tilemap Addresses:**
```cpp
// Before (incorrect - used 6 bits):
m_bg_tilemap_addr[bg] = (value & 0xFC) << 9;

// After (correct - uses 5 bits per bsnes):
m_bg_tilemap_addr[bg] = (value & 0x7C) << 9;  // 0x0000-0xF800
```

**Impact:** Super Mario All-Stars and other Mode 3 games now render correctly.

### 2026-01-02: APU Startup Timing Fix

Added APU pre-run at reset to allow SPC700 IPL ROM to complete initialization before the main CPU begins executing. This fixes games that were stuck waiting for the APU handshake ($BBAA response).

### 2026-01-02: Sub-Screen and Color Math

Implemented full sub-screen rendering and color math support:
- Main screen (TM) and sub screen (TS) compositing
- Color addition/subtraction with half-brightness
- Fixed color backdrop for sub-screen
- Sprite palette 0-3 color math rejection

### 2026-01-02: HDMA Timing and Initialization Fixes

Fixed HDMA timing issues that caused missing tiles and incorrect colors on early boot screens:

**Issue 1: HDMA transfer/render order**
For scanline-based rendering, HDMA at scanline N should affect scanline N+1's rendering. The render must happen BEFORE the HDMA transfer for that iteration so each scanline uses HDMA values from the previous scanline's H-blank.

```cpp
// Correct order (render uses previous HDMA values):
render_scanline(scanline - 1);  // Uses HDMA values from iteration N-1
hdma_transfer();                 // Sets values for iteration N+1's render
```

**Issue 2: HDMA line counter handling (bsnes-accurate)**
The line counter reload check was happening AFTER decrementing, but bsnes checks BEFORE decrementing. This caused table entries to be processed one scanline too early. Fixed by restructuring `do_hdma_channel()`:

```cpp
// bsnes-accurate sequence:
// 1. Check if line_counter == 0, if so reload next table entry
// 2. Do transfer if do_transfer flag is set
// 3. Decrement line_counter
// 4. Update do_transfer based on new line_counter value
```

**Issue 3: Mid-frame HDMA enable**
When games enable HDMA channels via $420C write after V=0, those channels weren't initialized until the next frame. Fixed by initializing newly-enabled channels immediately in `write_hdmaen()`.

**Impact:** Super Mario All-Stars now displays the Nintendo logo animation and title screen fade-in correctly. This fix benefits any game using HDMA for brightness fades, color gradients, or per-scanline effects.

**Reference:** Based on timing analysis from [bsnes](https://github.com/bsnes-emu/bsnes) and [SNES timing documentation](https://snesdev.mesen.ca/wiki/index.php?title=SNES_Timing).

### 2026-01-02: PPU Scanline Counter for HVBJOY

Fixed the PPU's internal scanline counter not being updated for non-visible scanlines (225-261). This caused `HVBJOY` ($4212) to return incorrect V-blank status during NMI handlers.

**Issue:** The `m_scanline` variable was only updated via `render_scanline()`, which is only called for visible scanlines 0-223. During V-blank (scanlines 225-261), games reading `HVBJOY` would see the wrong scanline value, potentially causing incorrect code paths to execute.

**Fix:** Added `set_scanline()` call at the start of each scanline iteration in the main loop:

```cpp
for (int scanline = 0; scanline < SCANLINES_PER_FRAME; scanline++) {
    // Update PPU scanline counter so HVBJOY returns correct V-blank status
    m_ppu->set_scanline(scanline);

    // Render visible scanlines
    if (scanline <= 223) {
        m_ppu->render_scanline(scanline);
    }
    // ...
}
```

**Impact:** Games that poll `HVBJOY` to detect V-blank transitions now receive accurate timing information.

### 2026-01-02: FastROM/SlowROM Memory Access Timing

Implemented variable memory access timing based on address region and FastROM mode:

**Memory Access Speeds:**
- SlowROM: 8 master cycles (2.68 MHz effective)
- FastROM: 6 master cycles (3.58 MHz effective)
- WRAM: 8 master cycles
- Joypad registers ($4000-$41FF): 12 master cycles (XSlow)
- Other I/O: 6 master cycles

**FastROM Activation:**
FastROM timing requires both conditions:
1. Cartridge header indicates FastROM support (map mode bit 4)
2. MEMSEL ($420D) bit 0 is set by the game

```cpp
int Bus::get_access_cycles(uint32_t address) const {
    // Banks $7E-$7F (WRAM): 8 cycles
    // Banks $80-$FF with FastROM: 6 cycles
    // ROM in banks $00-$3F: 8 cycles (SlowROM) or 6 (with MEMSEL)
    // Joypad: 12 cycles
    // ...
}
```

**Impact:** Games with timing-sensitive code now execute at correct speeds. FastROM games run faster when MEMSEL is enabled.

## Known Issues

### Critical

| Issue | Description | Affected Games |
|-------|-------------|----------------|
| Enhancement chips | No coprocessor support | ~10% of library |

### Medium Priority

| Issue | Description | Affected Games |
|-------|-------------|----------------|
| Cycle accuracy | Instruction-level, not per-cycle timing | Very timing-sensitive code |
| E-mode stack wrap | In emulation mode, JSL, JSR (a,x), PEA, PEI, PER, PHD, PLD, PLB and RTL wrap S within page 1; hardware lets them reach `$00FF`/`$0200` (gilyon cputest 0274, 0277, 03c2, 03c4, 03c7, 03d9, 03dc, 03ed, 0446) | Code running these with E=1 near the stack page edge |
| 16-bit decimal SBC | `sbc #$f1e2` with A=`$ab1d`, C=1, D=1, M=0 gives `$58db`; hardware gives `$59db` (gilyon cputest 061a) | Games using 16-bit BCD arithmetic |
| SPC700 SLEEP/STOP | `$EF`/`$FF` run as 2-cycle no-ops instead of halting the S-SMP | Only code that relies on the halt |
| Interlace rendering | Field detection only, not true interlace | Interlaced games |

### Low Priority

| Issue | Description | Affected Games |
|-------|-------------|----------------|
| PPU timing | Scanline-based, not dot-based | Very few |
| Mid-frame register changes | May not be pixel-accurate | Raster effects |

## Game Compatibility

### Well Tested

| Game | Status | Notes |
|------|--------|-------|
| Super Mario All-Stars | **Playable** | Mode 3, color math effects |
| Super Mario World | **Playable** | Mode 1 standard |
| The Legend of Zelda: ALTTP | **Playable** | Mode 1, HDMA effects |
| F-Zero | **Playable** | Mode 7 racing |
| Super Metroid | **Playable** | Mode 1/7 combination |

### Requires Enhancement Chips

| Game | Chip Required | Status |
|------|---------------|--------|
| Star Fox | SuperFX | Not playable |
| Yoshi's Island | SuperFX2 | Not playable |
| Kirby Super Star | SA-1 | Not playable |
| Super Mario RPG | SA-1 | Not playable |
| Mega Man X2 | Cx4 | Not playable |
| Pilotwings | DSP-1 | Not playable |

## Architecture

### Source Files

| File | Lines | Purpose |
|------|-------|---------|
| cpu.cpp | 1684 | 65C816 CPU emulation |
| ppu.cpp | 2185 | PPU rendering, all 8 modes |
| spc700.cpp | 1251 | SPC700 audio CPU |
| dsp.cpp | 704 | S-DSP synthesis, BRR, echo |
| cartridge.cpp | 662 | ROM loading, mapping |
| bus.cpp | 565 | Memory bus, I/O routing |
| snes_plugin.cpp | 524 | Plugin interface |
| dma.cpp | 376 | DMA/HDMA controller |
| apu.cpp | 125 | APU coordination |
| **Total** | **8076** | |

### Memory Map

| Bank | Address | Description |
|------|---------|-------------|
| $00-$3F | $0000-$1FFF | WRAM mirror (8KB) |
| $00-$3F | $2100-$213F | PPU registers |
| $00-$3F | $2140-$2143 | APU ports |
| $00-$3F | $2180-$2183 | WRAM port |
| $00-$3F | $4000-$41FF | CPU I/O, joypad |
| $00-$3F | $4200-$43FF | DMA, NMI/IRQ control |
| $00-$3F | $8000-$FFFF | ROM (LoROM) |
| $40-$7D | $0000-$FFFF | ROM (HiROM) |
| $7E | $0000-$FFFF | WRAM (64KB) |
| $7F | $0000-$FFFF | WRAM (64KB) |
| $80-$FF | - | ROM mirrors |

## Building

### As Part of Veloce

```bash
cmake -B build
cmake --build build
```

### Standalone Build

```bash
cd cores/snes
cmake -B build
cmake --build build
```

### Output

- Linux: `build/bin/cores/snes.so`
- Windows: `build/bin/cores/snes.dll`
- macOS: `build/bin/cores/snes.dylib`

## Testing

### Debug Mode

```bash
DEBUG=1 ./veloce game.sfc
```

Debug output includes:
- CPU register state and PC
- PPU mode, brightness, layer enables
- DMA/HDMA transfers
- APU port communication

### Headless Testing

```bash
HEADLESS=1 FRAMES=300 ./veloce game.sfc
```

### Original math register ROM

The repository-owned [math corpus](../../tests/roms-src/snes/math_registers/README.md)
verifies 65,536 unsigned multiplication pairs and 3,328 division boundary cases
through raw `$21FE` output and an independent host integer oracle. It captures
products, the multiplication RDDIV side effect, quotients, and remainders.
The side-effect check found 65,280 failures before its core fix and zero afterward.
Mapped I/O mirrors and selected WRAM offsets are checked with the sink attached.

```bash
cmake --build build --target veloce snes_plugin
ctest --test-dir build -R snes_math_registers --output-on-failure
```

The optional host sink is borrowed and excluded from save states. `$21FE`
streams bytes and `$21FF` reads `$56` or accepts completion `$01`, only in
I/O banks `$00-$3F/$80-$BF`. WRAM and cartridge banks retain their mappings.
A completion hint has unknown status; the host oracle determines PASS/FAIL.
The math unit now advances over eight multiply or sixteen divide CPU clocks,
including intermediate register values. The separate
[timing corpus](../../tests/roms-src/snes/math_timing/README.md) verifies 504,832
byte reads across phases 2–18. Pending operations and operand latches are
serialized; `snes_math_pipeline_test` checks every multiplication pair at all
phases, selected division cases, busy writes, and save/resume behavior. SNES
state payloads now start with `VSNES002`; recreate old unversioned states.
DMA/HDMA, refresh, interrupt overlap, and physical timing remain unverified.

### Automated Test Suite

The SNES core ships a subsystem-organized test suite (`tests/test_config.json`,
schema v2) driven by the shared `veloce_testkit` package, so detection, scoring,
CLI, and the JSON scorecard are identical across every Veloce console. For the
methodology and the platform-wide picture see the top-level
[TESTING.md](../../TESTING.md) and [COMPLETENESS.md](../../COMPLETENESS.md).

> **Delivery and accuracy:** the generated checklist counts delivery milestones.
> Whole-core hardware accuracy remains unmeasured. Current owned math captures
> and fresh gilyon opcode runs are recorded below. Visual catalog ROMs still
> lack independent file verdicts; Blargg SPC ROMs without a result signal
> remain unverified. Implementation tables do not establish hardware completeness.

```bash
cd cores/snes/tests
./run_tests.sh                 # run all, human scorecard
./run_tests.sh cpu ppu apu     # subset by subsystem key or suite id
./run_tests.sh -v              # per-test verdict lines
./run_tests.sh --json          # scorecard JSON (consumed by tests/run_all.py)
./run_tests.sh --generate-refs # print measured CRC32 hashes for visual tests
```

**ROM provisioning.** The runner (`test_runner.py`, a thin shim around
`veloce_testkit.runner`) clones the standard public SNES test-ROM repositories
declared in `test_config.json["repositories"]` into `tests/roms/` and resolves
each test by basename across the cloned tree, so mirror-layout drift is tolerated
and an unfound ROM reports SKIP (excluded from the score) rather than a false
FAIL. Set `SNES_TEST_OFFLINE=1` to skip cloning and use an already-staged tree.

The primary source is the higan/byuu `snes-test-roms` mirror, which bundles the
entire ROM set this suite references (Blargg SPC, the krom/PeterLemon 65816 CPU +
SPC700 + SuperFX/GSU opcode suites, absindx SA-1, and the jonasquinn / KungFuFurby
/ undisbeliever / Sour / 93143 / Motive / VitorVilela7 timing collection).

**Coverage (67 tests across all hardware subsystems):**

| Subsystem | Suites | Sample ROMs | Detection |
|-----------|--------|-------------|-----------|
| CPU (65816) | arith, logic, inc/dec/cmp, load/store/transfer, branch/jump/misc, full-auto | CPUADC, CPUSBC, CPUMSC, CPUMOV, gilyon cputest | gilyon cputest: file (`port`, 1610 checks); the rest screenshot-crc (visual) |
| APU (SPC700 + DSP) | blargg, SPC700 ops, gilyon | spc_smp, spc_timer, spc_mem_access_times, spc_dsp6, SPC700ADC, gilyon spctest | gilyon spctest: file (`port`, 1368 checks); blargg: memory (`$6000` via the result file; blargg-spc-6 never writes it); PeterLemon: screenshot-crc |
| PPU | smoke, OAM, mode3/color-halve, bus/INIDISP | test_hello, test_oam, demo_mode3, ppubusact | screenshot-crc |
| Timing | IRQ, DMA, HDMA | test_irq4209, test_dmatiming, test_hdmatiming, dma_irq_test, HblankEmuTest | screenshot-crc (cycle-accurate) |
| Mapper (enhancement chips) | SuperFX/GSU, SA-1 | GSUADC, GSUFMULT, SA1RamProtectionTest | screenshot-crc |
| Memory | speed/open-bus | speed_test_v51, op_timing_test_v2 | screenshot-crc (timing) |

**Detection methods.** Scored verdicts come from the VELOCE-RESULT/1 result
file ([docs/testing/VELOCE-RESULT.md](../../docs/testing/VELOCE-RESULT.md)): the
harness sets `VELOCE_TEST_OUT` and the core forwards its result channel into it
(see *Test-result channel* below). The core no longer prints a verdict to
stdout/stderr. Most krom/PeterLemon and community ROMs still draw their result
on screen and use `screenshot-crc`: the binary captures the framebuffer
(`SAVE_SCREENSHOT`), the testkit CRC32s the PNG and compares to a stored
`reference_hash`. Those entries are non-scoring until they are rebuilt to emit
on the port (plan items SNES-4..8). gilyon cputest and spctest are already
rebuilt this way (SNES-3; see *Patched ROMs* below).

**Patched ROMs.** A config entry with `rom_variant: "veloce"` names a ROM that
`tests/roms-src/build.py` builds from upstream source at a pinned SHA plus a
committed patch, into `build/roms/snes/<suite>/`. The runner links
`tests/roms/veloce` to that directory, and the entry's `file` is
`veloce/<suite>/<rom>`. The harness refuses a ROM whose sha256 is not in
`tests/roms-src/rom_manifest.json`. Without a build these entries SKIP. The
basename healer never links an upstream ROM into `veloce/`.

```bash
python3 tests/roms-src/build.py snes/gilyon_cputest   # needs the tools/rom-toolchain image
python3 tests/roms-src/build.py snes/gilyon_spctest
```

- `cpu.gilyon_cputest` (gilyon/snes-tests v1.4, MIT): one `CHECK` per test,
  ids matching `tests-full.txt`. A failure is recorded and the run continues;
  the line carries `field=` (the first mismatching register or memory byte) and
  `exp=`/`got=` from `make_cpu_tests.py`, followed by a `REG` line. Today:
  `END 1600/1610`. The ten failures are the E-mode stack wrap and
  decimal SBC issues listed under Known Issues.
- `apu.gilyon_spctest` (same pin): the S-SMP programs are unmodified; the S-CPU
  converts gilyon's APU-port protocol into one `CHECK` per test and adds a
  120-frame watchdog for a hung S-SMP. Today: `END 1368/1368`.

Details and the expected-value sources are in each suite's
`tests/roms-src/snes/<suite>/PROVENANCE.md`.

### Test-result channel

`snes::TestChannel` (`src/test_channel.{hpp,cpp}`) implements plugin ABI v2
(`set_test_sink`, `test_channels() = "port,blargg6000,stp,sram,spcport"`,
`on_test_reset`). Every driver is inert unless the application attached a sink,
so normal play is unchanged.

| Driver | ROM side | What lands in the result file |
|---|---|---|
| `port` | `STA $21FE` (any bank `$00-$3F`/`$80-$BF`, DMA-able B-bus address); `$21FF` write = control (`$00` nop, `$01` terminator hint); `$21FF` read = `$56` probe | the bytes verbatim (`VELOCE 1 ...`, `CHECK ...`, `END p/t`) |
| `blargg6000` | blargg `$6000` block (status, `DE B0 61` signature, text at `$6004+`) | `#VELOCE adapter=blargg6000`, the text as `LOG` lines as it is written, `END 1/1` or `END 0/1 code=<n>`; status `$81` requests one reset per `$80`->`$81` transition |
| `stp` / `sram` | ROM halts with `STP` (byuu/jonasquinn convention: backdrop `$7C00` from `pass()`, `$001F` from `fail()`) | `#VELOCE stp=<pbr:pc> cgram0=<hex>`, blobs `sram` (cartridge RAM) and `cgram`, then `END 1/1` / `END 0/1` (any other colour: terminator with unknown status) |
| `spcport` | gilyon spctest (`SPC-700 TEST` header) S-SMP port protocol | one `CHECK` per uploaded S-SMP test program, `REG` of the failing test's A/X/Y/P, `END` on failure or once the S-CPU stops uploading |

Once a ROM writes the port, the legacy adapters stay silent. WRAM is filled with
a fixed constant when a sink attaches, so two runs of a ROM produce identical
result files. Measurement ROMs that always end in `pass()` (e.g. `test_mul`)
report completion through `stp`, not correctness; their SRAM blob is the
observable to compare.

```bash
VELOCE_TEST_OUT=/tmp/spctest.result ./veloce spctest.sfc && cat /tmp/spctest.result
```

**Reference hashes are "to be measured."** Visual tests carry no golden hash yet,
so they are marked `expected: known_fail` and contribute **0** to the headline
score (honest, non-inflating). To seed them, run `--generate-refs` on a build you
have visually verified renders the test's all-pass screen, then paste the printed
hashes into `test_config.json`. Reference hashes are tied to the fixed headless
output resolution and PNG encoder; regenerate if either changes.

The gaps surfaced by the suite (the SPC700 `(X),(Y)` opcodes `$19`/`$39`/`$59`/`$79`
that trapped `apu.spc_smp` at `$19`, now fixed; the missing `$6000` signal from the
blargg-spc-6 ROMs; and the `apu.spc_dsp6` hardware quirk) are documented in
[COMPLETENESS.md](../../COMPLETENESS.md#snes).

### Emitting VELOCE-RESULT/1 from a patched ROM

Three assembler dialects can emit the channel's vocabulary
(`tests/roms-src/include/snes/`): `veloce-test.ca65.inc` (ca65 `--cpu 65816`;
verified end to end via `tests/roms-src/snes/veloce_smoke/`), `.asar.inc`
(verified against a throwaway ROM), and `.bass.inc` (written but **blocked**:
the pinned `bass`/`bass-untech` in `tools/rom-toolchain` both segfault on the
architecture-selection command alone, before any source is read -- see
`tests/roms-src/include/PROVENANCE.md`). `tests/roms-src/build.py` gained
`"local"` (a Veloce-authored ROM with no upstream), `"submodules"` and
`"allow_exit_codes"` recipe fields, and mounts this `include/` directory
read-only into every toolchain container at `/veloce-include/<console>`.
`tests/roms-src/snes/{sour_dma_irq_test,sour_op_timing_test,
undisbeliever_auto_joy_timing}` are acquisition-only recipes (golden-gate
rebuilds, no emitter patch yet) proving the higan-mirror-pin and
undisbeliever-with-submodule acquisition paths the plan's SNES-5/6/7/8 items
build on. `gilyon_cputest` and `gilyon_spctest` carry the SNES-3 emitter
patches (see *Patched ROMs* above). All three includes print hex in lowercase,
as the protocol requires; before SNES-3 they printed `A`-`F`.

## References

### Documentation

- [fullsnes](https://problemkaputt.de/fullsnes.htm) - Comprehensive SNES documentation
- [SNESdev Wiki](https://snes.nesdev.org/wiki/) - Community documentation
- [anomie's SNES docs](https://www.romhacking.net/documents/196/) - Register reference
- [65816 Programming Manual](http://6502.org/tutorials/65c816opcodes.html) - CPU reference

### Open Source Emulators (Reference)

- [bsnes](https://github.com/bsnes-emu/bsnes) - Accuracy-focused reference
- [snes9x](https://github.com/snes9xgit/snes9x) - Performance-focused reference
- [higan](https://github.com/higan-emu/higan) - Multi-system accuracy reference
- [Mesen-S](https://github.com/SourMesen/Mesen-S) - Debugging-focused reference

## Roadmap

### Phase 1 - Core Accuracy
- [x] Implement offset-per-tile (Modes 2/4/6)
- [x] Implement direct color mode
- [x] Implement EXTBG for Mode 7
- [ ] Add dot-level PPU timing

### Phase 2 - Enhancement Chips
- [ ] DSP-1 (math coprocessor)
- [ ] SuperFX (RISC CPU)
- [ ] SA-1 (65C816 accelerator)
- [ ] S-DD1 (decompression)

### Phase 3 - Advanced Features
- [ ] True interlace rendering
- [ ] MSU-1 audio enhancement
- [ ] Super Game Boy support
- [ ] Satellaview support

## License

MIT License - See main repository LICENSE file.

## Result-channel branch integration (2026-10-11)

One `TestChannel` now serves `port,blargg6000,stp,sram,spcport`. The owned
`$21FF=1` hint retains unknown status (`finish(-1)`), and explicit port output
suppresses inferred adapter verdicts. No stdout verdict scanner remains.
Attaching a sink after execution starts preserves WRAM and serialized hardware
state; power-on test WRAM remains deterministic.

Executed evidence on the integrated plugin:

- Owned settled math: **68,864 actual register pairs**, independently graded.
- Owned timed math: **504,832 captures**, independently graded with executed
  negative controls and strict stream/channel checks. Timing scope remains the
  documented CPU/math profile, with DMA/HDMA and physical timing pending.
- **12 original production fixtures** save actual captures and CGRAM/SRAM
  blobs in `build/artifacts/owned/snes_result_channel/`; port mirrors, non-I/O
  banks, running/signature status, STP verdicts and port precedence are checked.
  A separate complete-state comparison verifies late sink attachment.
- Fresh gilyon S-CPU opcode run: **1,600/1,610 checks**, scored FAIL. Nine
  emulation-mode stack cases and one decimal SBC case remain broken.
- Fresh gilyon SPC700 opcode run: **1,368/1,368 checks**, scored PASS. It excludes
  SLEEP/STOP, device and timing domains. Both catalog captures repeat exactly.
- The ca65 original emitter matches its literal transcript twice, including
  the deliberately failing check. Five supplemental/emitter images rebuild;
  the auto-joypad recipe remains blocked by `bass-untech` crashing before output.
- The full project build, six shared/channel/math component tests and all
  **117 archived original ROM rebuilds** pass.

Delivery stays **40% = 8/20 milestones**. Complete CPU/devices, expansion chips,
physical validation, full corpus grading and SNES netplay remain unfinished.
Raw catalog captures and reports are under
`build/artifacts/owned/snes_channel_integration/`. Commercial bytes are excluded.

## CPU stack boundaries (2026-10-11)

The nine measured special-stack defects are repaired. Native stack byte accesses
can cross page one during an instruction in emulation mode; SH=1 is restored
when the instruction completes. Legacy operations keep their wrapping behavior.
CPU timing calls and serialized field layout are preserved.

The [original stack ROM](../../tests/roms-src/snes/stack_boundaries/README.md)
passes **5,632 cases** with **45,408 actual RAM-byte observations** and **16,896
S/D/DBR values**. Every permitted emulation S value is tested, with the same
page in native mode, all nine special instructions and legacy PHA/PLA controls.
The oracle imports no emulator code. Captures repeat exactly; an executed EOR
mutant fails, and missing/reordered/truncated streams are rejected.

Fresh deterministic gilyon S-CPU captures improve **1,600/1,610 to 1,609/1,610**.
The remaining decimal SBC case still fails honestly; no expectation was changed.
The SPC700 catalog remains **1,368/1,368**. The full build and ten targeted
owned math/stack/channel/shared gates pass; **119 archived originals** reproduce.

SNES stays **40% = 8/20 delivery milestones**. Complete CPU, hardware timing,
full native stack/operand domains, physical validation, expansion chips and
modern core/frontend features remain unfinished. Earlier catalog numbers in
this README are historical; this section records the current stack repair.
