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

### CPU - 65C816 (90% Complete)

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
| Cycle timing | **Partial** | Per-instruction, not per-cycle |

### PPU - Picture Processing Unit (85% Complete)

| Feature | Status | Notes |
|---------|--------|-------|
| Mode 0 (4×2bpp) | **Complete** | 4 BG layers, 4 colors each |
| Mode 1 (2×4bpp+2bpp) | **Complete** | Most common mode |
| Mode 2 (2×4bpp OPT) | **Partial** | Offset-per-tile not implemented |
| Mode 3 (8bpp+4bpp) | **Complete** | 256-color BG1 |
| Mode 4 (8bpp+2bpp OPT) | **Partial** | Offset-per-tile not implemented |
| Mode 5 (4bpp hires) | **Complete** | 512-pixel width |
| Mode 6 (4bpp OPT hires) | **Partial** | Offset-per-tile not implemented |
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
| Direct color | Not implemented | 8bpp direct RGB |
| EXTBG (Mode 7) | Not implemented | Mode 7 external BG |

### APU - Audio Processing Unit (85% Complete)

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

## Known Issues

### Critical

| Issue | Description | Affected Games |
|-------|-------------|----------------|
| Offset-per-tile | Modes 2/4/6 OPT scrolling not implemented | Various parallax games |
| Enhancement chips | No coprocessor support | ~10% of library |

### High Priority

| Issue | Description | Affected Games |
|-------|-------------|----------------|
| APU startup timing | Pre-run APU on reset to avoid handshake issues | Various (workaround in place) |
| Direct color mode | Mode 3/4 direct color not implemented | Some 256-color games |
| EXTBG (Mode 7) | Mode 7 external background layer | Some Mode 7 games |

### Medium Priority

| Issue | Description | Affected Games |
|-------|-------------|----------------|
| Cycle accuracy | Instruction-level, not cycle-level timing | Timing-sensitive code |
| Interlace rendering | Field detection only, not true interlace | Interlaced games |
| Open bus | Not fully implemented | Edge cases |

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
| dsp.cpp | 670 | S-DSP synthesis, BRR, echo |
| cartridge.cpp | 662 | ROM loading, mapping |
| bus.cpp | 565 | Memory bus, I/O routing |
| snes_plugin.cpp | 524 | Plugin interface |
| dma.cpp | 376 | DMA/HDMA controller |
| apu.cpp | 125 | APU coordination |
| **Total** | **8042** | |

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
- [ ] Implement offset-per-tile (Modes 2/4/6)
- [ ] Implement direct color mode
- [ ] Implement EXTBG for Mode 7
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
