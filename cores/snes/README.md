# SNES Emulator Core

Super Nintendo Entertainment System emulator for Veloce.

## Overview

The SNES core provides emulation of the Super Nintendo with support for LoROM and HiROM cartridge types. It features a complete 65C816 CPU implementation, scanline-based PPU rendering with all 8 background modes, and SPC700 audio processor with DSP synthesis.

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

### CPU - 65C816 (95% Complete)

| Feature | Status | Notes |
|---------|--------|-------|
| All 256 opcodes | **Complete** | Full 65816 instruction set |
| 8/16-bit modes | **Complete** | M and X flag switching |
| 24-bit addressing | **Complete** | All addressing modes |
| Emulation mode | **Complete** | 6502 compatibility |
| Native mode | **Complete** | 16-bit operations |
| Direct page | **Complete** | Relocatable zero page |
| Stack relocation | **Complete** | 16-bit stack pointer |
| NMI handling | **Complete** | VBlank interrupts |
| IRQ handling | **Complete** | H/V counter IRQs |
| WAI/STP | **Complete** | Wait and stop instructions |
| Block moves | **Complete** | MVN/MVP instructions |
| Cycle timing | **Partial** | Per-instruction with FastROM/SlowROM |

### PPU - Picture Processing Unit (95% Complete)

| Feature | Status | Notes |
|---------|--------|-------|
| Mode 0 (4×2bpp) | **Complete** | 4 BG layers, 4 colors each |
| Mode 1 (2×4bpp+2bpp) | **Complete** | Most common mode |
| Mode 2 (2×4bpp OPT) | **Complete** | Offset-per-tile implemented |
| Mode 3 (8bpp+4bpp) | **Complete** | 256-color BG1 |
| Mode 4 (8bpp+2bpp OPT) | **Complete** | Offset-per-tile implemented |
| Mode 5 (4bpp hires) | **Complete** | 512-pixel width |
| Mode 6 (4bpp OPT hires) | **Complete** | Offset-per-tile implemented |
| Mode 7 (affine) | **Complete** | Rotation, scaling, perspective |
| Sprites (OBJ) | **Complete** | 128 sprites, 8 sizes |
| BG priority | **Complete** | Per-mode priority ordering |
| BG3 priority bit | **Complete** | Mode 1 BG3 front layer |
| Color math | **Complete** | Add/subtract with half |
| Sub-screen | **Complete** | Main/sub compositing |
| Pseudo-hires | **Complete** | 512-width via sub-screen |
| Windows | **Complete** | 2 windows with logic |
| Mosaic | **Complete** | Per-BG mosaic effect |
| Brightness | **Complete** | Master fade control |
| Force blank | **Complete** | Display disable |
| VRAM access | **Complete** | All increment modes |
| OAM access | **Complete** | Sprite attribute table |
| CGRAM access | **Complete** | Palette memory |
| H/V counters | **Complete** | Latched position read |
| Interlace | **Partial** | Field detection, not full interlace rendering |
| Overscan | **Complete** | 239-line mode |
| Direct color | **Complete** | 8bpp direct RGB in Modes 3/4/7 |
| EXTBG (Mode 7) | **Complete** | BG2 layer with bit 7 priority |

### APU - Audio Processing Unit (90% Complete)

#### SPC700 Sound Processor

| Feature | Status | Notes |
|---------|--------|-------|
| All 256 opcodes | **Complete** | Full instruction set |
| 64KB RAM | **Complete** | Dedicated audio memory |
| IPL ROM boot | **Complete** | 64-byte boot ROM |
| Timer 0 (8kHz) | **Complete** | 8000Hz base |
| Timer 1 (8kHz) | **Complete** | 8000Hz base |
| Timer 2 (64kHz) | **Complete** | 64000Hz base |
| I/O ports | **Complete** | CPU↔SPC communication |
| Direct page | **Complete** | Page 0/1 selection |

#### S-DSP Digital Signal Processor

| Feature | Status | Notes |
|---------|--------|-------|
| 8 voice channels | **Complete** | Full polyphony |
| BRR decoding | **Complete** | 4-bit ADPCM samples |
| ADSR envelopes | **Complete** | Attack/Decay/Sustain/Release |
| GAIN mode | **Complete** | Custom envelope control |
| Gaussian interpolation | **Complete** | Sample smoothing |
| Echo effect | **Complete** | Hardware reverb |
| FIR filter | **Complete** | 8-tap echo filter |
| Noise generator | **Complete** | Pseudo-random noise |
| Pitch modulation | **Complete** | Voice-to-voice FM |
| Voice muting | **Complete** | Per-voice enable |
| Stereo panning | **Complete** | Per-voice L/R volume |

### DMA Controller (95% Complete)

| Feature | Status | Notes |
|---------|--------|-------|
| 8 DMA channels | **Complete** | Independent channels |
| Transfer mode 0 | **Complete** | 1 byte to 1 register |
| Transfer mode 1 | **Complete** | 2 bytes to 2 registers |
| Transfer mode 2 | **Complete** | 2 bytes to 1 register |
| Transfer mode 3 | **Complete** | 4 bytes to 2 registers |
| Transfer mode 4 | **Complete** | 4 bytes to 4 registers |
| A→B / B→A | **Complete** | Bidirectional |
| Fixed address | **Complete** | Non-incrementing |
| HDMA | **Complete** | Per-scanline transfers |
| HDMA indirect | **Complete** | Table-based transfers |
| HDMA repeat | **Complete** | Repeat flag handling |
| Mid-transfer abort | **Partial** | Not fully accurate |

### Cartridge Support (70% Complete)

| Type | Status | Notes |
|------|--------|-------|
| LoROM | **Complete** | Standard low-ROM mapping |
| HiROM | **Complete** | Standard high-ROM mapping |
| ExHiROM | **Partial** | Extended addressing |
| Battery SRAM | **Complete** | Save game support |
| Header detection | **Complete** | Auto-detect mapping |
| CRC32 | **Complete** | ROM identification |

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

### Automated Test Suite

The SNES core includes an automated test suite using Blargg's SPC700/DSP test ROMs:

```bash
cd cores/snes/tests
python3 test_runner.py
```

**Available Tests:**

| Test ROM | Description | Status |
|----------|-------------|--------|
| `spc_dsp6.sfc` | Comprehensive DSP register and behavior tests | RUNS |
| `spc_mem_access_times.sfc` | Memory access timing validation | RUNS |
| `spc_smp.sfc` | SPC700 instruction set tests | RUNS |
| `spc_timer.sfc` | SPC700 timer functionality tests | RUNS |

**Note:** Blargg's SPC tests communicate results via APU I/O ports rather than the $6000 memory interface used by NES tests. Tests that complete without crashing are marked as "RUNS" to indicate successful execution, even though automated pass/fail detection is not possible.

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
