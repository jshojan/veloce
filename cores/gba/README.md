# Game Boy Advance Emulator Core

Game Boy Advance emulator for Veloce with ARM7TDMI CPU emulation.

## Overview

The GBA core provides Game Boy Advance emulation with an ARM7TDMI CPU supporting both ARM and Thumb instruction sets. The implementation is still in development with several components requiring additional work for commercial game compatibility.

## Specifications

| Component | Details |
|-----------|---------|
| CPU | ARM7TDMI (ARM + Thumb modes) |
| Resolution | 240 x 160 |
| Frame Rate | 59.7275 Hz |
| Clock Speed | 16.777216 MHz |
| Colors | 32768 (15-bit) |
| BIOS | HLE (High-Level Emulation) |

## Completion Status

### CPU - ARM7TDMI (Partial)

| Feature | Status | Notes |
|---------|--------|-------|
| ARM data processing | Implemented | ADD, SUB, AND, ORR, etc. |
| ARM branching | Implemented | B, BL, BX |
| ARM load/store | Implemented | LDR, STR, LDM, STM |
| ARM multiply | Implemented | MUL, MLA, UMULL, SMULL |
| ARM swap | Implemented | SWP, SWPB |
| ARM MRS/MSR | Implemented | Status register access |
| Thumb instructions | Implemented | Full 16-bit instruction set |
| Processor modes | Implemented | User, FIQ, IRQ, SVC, ABT, UND, System |
| Register banking | Implemented | Banked R8-R14 for FIQ, R13-R14 for others |
| Condition codes | Implemented | All 16 conditions |
| IRQ handling | Partial | Return address calculation fixed |
| Prefetch buffer | Not Implemented | Affects timing accuracy |
| Wait states | Partial | Basic implementation only |

### PPU (Partial)

| Feature | Status | Notes |
|---------|--------|-------|
| Mode 0 (4 tile BGs) | Implemented | Screen block calculation fixed |
| Mode 1 (2 tile + 1 affine) | Implemented | |
| Mode 2 (2 affine BGs) | Implemented | |
| Mode 3 (240x160 bitmap) | Implemented | 15bpp direct color |
| Mode 4 (240x160 paletted) | Implemented | 8bpp with page flip, index 0 fixed |
| Mode 5 (160x128 bitmap) | Implemented | 15bpp with page flip |
| Regular sprites | Implemented | 128 OBJ support |
| Affine sprites | Implemented | Rotation/scaling, double-size |
| Alpha blending | Implemented | All 3 blend modes |
| Brightness fade | Implemented | Increase/decrease |
| Windows (WIN0/WIN1) | Implemented | |
| OBJ Window | Implemented | |
| Forced blank | Implemented | DISPCNT bit 7 |
| Mosaic | Not Implemented | |
| Sprite semi-transparency | Implemented | |
| HBlank DMA trigger | Implemented | |
| VBlank DMA trigger | Implemented | |

### APU (Partial)

| Feature | Status | Notes |
|---------|--------|-------|
| Channel 1 (Pulse+Sweep) | Implemented | Legacy GB audio |
| Channel 2 (Pulse) | Implemented | Legacy GB audio |
| Channel 3 (Wave) | Implemented | 4-bit wave samples |
| Channel 4 (Noise) | Implemented | LFSR noise |
| Frame sequencer | Implemented | Length, envelope, sweep clocking |
| Direct Sound A | Not Working | FIFO structure exists |
| Direct Sound B | Not Working | FIFO structure exists |
| Sound mixing | Partial | GB channels only |
| Timer-driven audio | Not Working | Critical for GBA games |

### DMA (Partial)

| Feature | Status | Notes |
|---------|--------|-------|
| DMA channels 0-3 | Implemented | |
| Immediate trigger | Implemented | |
| VBlank trigger | Implemented | |
| HBlank trigger | Implemented | |
| Sound FIFO (DMA1/2) | Partial | 4-word transfers |
| Word/Halfword size | Implemented | |
| Address control | Implemented | Inc/Dec/Fixed/Reload |
| Repeat mode | Implemented | Source address preserved |
| IRQ on complete | Not Verified | |

### Timers (Partial)

| Feature | Status | Notes |
|---------|--------|-------|
| Timers 0-3 | Implemented | |
| Prescaler (1/64/256/1024) | Implemented | |
| Cascade mode | Partial | May have issues |
| IRQ generation | Not Verified | |
| Overflow handling | Implemented | |

### Interrupts (Partial)

| Feature | Status | Notes |
|---------|--------|-------|
| IE register | Implemented | Interrupt enable |
| IF register | Implemented | Interrupt flags (write to acknowledge) |
| IME register | Implemented | Master enable |
| VBlank IRQ | Implemented | Generated at scanline 160 |
| HBlank IRQ | Partial | |
| VCount IRQ | Partial | |
| Timer IRQ | Not Verified | |
| DMA IRQ | Not Verified | |
| Keypad IRQ | Not Implemented | |
| Serial IRQ | Not Implemented | |

### BIOS - HLE Functions

| Function | SWI | Status | Notes |
|---------|-----|--------|-------|
| SoftReset | 0x00 | Not Implemented | |
| RegisterRamReset | 0x01 | Not Implemented | |
| Halt | 0x02 | Implemented | |
| Stop | 0x03 | Not Implemented | |
| IntrWait | 0x04 | Implemented | Clears flags at 0x03007FF8 |
| VBlankIntrWait | 0x05 | Implemented | |
| Div | 0x06 | Implemented | |
| DivArm | 0x07 | Not Implemented | |
| Sqrt | 0x08 | Implemented | |
| ArcTan | 0x09 | Implemented | |
| ArcTan2 | 0x0A | Implemented | |
| CpuSet | 0x0B | Implemented | |
| CpuFastSet | 0x0C | Implemented | |
| GetBiosChecksum | 0x0D | Not Implemented | |
| BgAffineSet | 0x0E | Not Implemented | |
| ObjAffineSet | 0x0F | Implemented | |
| BitUnPack | 0x10 | Implemented | |
| LZ77UnCompWram | 0x11 | Implemented | |
| LZ77UnCompVram | 0x12 | Implemented | Fixed back-reference bug |
| HuffUnComp | 0x13 | Partial | May have bugs |
| RLUnCompWram | 0x14 | Implemented | |
| RLUnCompVram | 0x15 | Partial | May have bugs |
| Diff8bitUnFilterWram | 0x16 | Not Implemented | |
| Diff8bitUnFilterVram | 0x17 | Not Implemented | |
| Diff16bitUnFilter | 0x18 | Not Implemented | |
| SoundBias | 0x19 | Not Implemented | |
| MidiKey2Freq | 0x1F | Not Implemented | |

### Save Types

| Type | Status | Notes |
|------|--------|-------|
| SRAM 32KB | Implemented | Auto-detected via string search |
| Flash 64KB | Implemented | Full command sequence |
| Flash 128KB | Implemented | Bank switching, chip ID |
| EEPROM 512B | Not Implemented | |
| EEPROM 8KB | Not Implemented | |

### Other Features

| Feature | Status | Notes |
|---------|--------|-------|
| GPIO | Partial | Structure exists |
| RTC | Partial | State machine exists |
| Serial/Link | Not Implemented | |
| Rumble | Not Implemented | |
| Solar Sensor | Not Implemented | |

## Commercial Game Compatibility

### Pokemon Fire Red

**Status: NOT WORKING**

Current issues:
- White screen or freeze during boot
- May be stuck waiting for interrupts
- Graphics decompression may have issues

Attempted fixes:
- IRQ return address calculation
- VBlank interrupt generation
- LZ77 decompression rewrite
- DMA VBlank/HBlank triggers

### What's Needed for Commercial Games

Based on comparison with mGBA (https://github.com/mgba-emu/mgba):

1. **Event Scheduler** - mGBA uses an event scheduler for accurate timing
2. **IRQ Delay** - mGBA implements a 7-cycle delay before servicing IRQs
3. **Prefetch Buffer** - Affects instruction fetch timing
4. **Accurate Wait States** - Per-region wait state configuration
5. **Direct Sound** - DMA-fed audio is essential for GBA games

## Test Status

### CPU Tests (jsmolka gba-tests)

| Category | Test | Status | Notes |
|----------|------|--------|-------|
| ARM | arm.gba | Needs Verify | ARM instruction tests |
| Thumb | thumb.gba | Needs Verify | Thumb instruction tests |
| Memory | memory.gba | Needs Verify | Memory access tests |

### PPU Tests

| Test | Status | Notes |
|------|--------|-------|
| hello.gba | Needs Verify | Basic rendering |
| stripes.gba | Needs Verify | Mode 0 backgrounds |

### BIOS Tests

| Test | Status | Notes |
|------|--------|-------|
| bios.gba | FAIL | Test #3 - decompression function |

### Save Tests

| Test | Status | Notes |
|------|--------|-------|
| sram.gba | Needs Verify | 32KB SRAM |
| flash64.gba | Needs Verify | 64KB Flash |
| flash128.gba | Needs Verify | 128KB Flash |

## Memory Map

| Address | Size | Region | Wait States |
|---------|------|--------|-------------|
| 0x00000000-0x00003FFF | 16KB | BIOS (HLE) | - |
| 0x02000000-0x0203FFFF | 256KB | EWRAM | 2 |
| 0x03000000-0x03007FFF | 32KB | IWRAM | 0 |
| 0x04000000-0x040003FF | 1KB | I/O Registers | 0 |
| 0x05000000-0x050003FF | 1KB | Palette RAM | 0 |
| 0x06000000-0x06017FFF | 96KB | VRAM | 0 |
| 0x07000000-0x070003FF | 1KB | OAM | 0 |
| 0x08000000-0x09FFFFFF | 32MB | ROM WS0 | Variable |
| 0x0A000000-0x0BFFFFFF | 32MB | ROM WS1 | Variable |
| 0x0C000000-0x0DFFFFFF | 32MB | ROM WS2 | Variable |
| 0x0E000000-0x0E00FFFF | 64KB | SRAM/Flash | 8 |

## Building

Built automatically as part of Veloce:

```bash
cmake -B build
cmake --build build
```

Standalone build:
```bash
cd cores/gba
cmake -B build -DSTANDALONE=ON
cmake --build build
```

Output: `build/bin/cores/gba.so` (Linux), `.dll` (Windows), `.dylib` (macOS)

## Testing

```bash
# Run with debug output
DEBUG=1 ./build/bin/veloce path/to/rom.gba

# Run in headless mode for automated testing
DEBUG=1 HEADLESS=1 FRAMES=300 timeout 10 ./build/bin/veloce path/to/test.gba

# Run test suite
cd cores/gba/tests
./run_tests.sh
```

## Source Files

| File | Purpose | Approx Lines |
|------|---------|--------------|
| plugin.cpp | Plugin interface, frame loop | 600 |
| arm7tdmi.cpp/hpp | ARM7TDMI CPU emulation | 2500 |
| ppu.cpp/hpp | PPU/LCD emulation | 1200 |
| apu.cpp/hpp | Audio emulation | 800 |
| bus.cpp/hpp | Memory bus, DMA, timers, I/O | 1100 |
| cartridge.cpp/hpp | ROM loading, save handling | 800 |
| types.hpp | Common types and enums | 200 |

## References

- [GBATEK](https://problemkaputt.de/gbatek.htm) - Comprehensive GBA technical documentation
- [mGBA Source](https://github.com/mgba-emu/mgba) - Reference GBA emulator
- [NanoBoyAdvance](https://github.com/nba-emu/NanoBoyAdvance) - Cycle-accurate GBA emulator
- [Tonc](https://www.coranac.com/tonc/text/) - GBA programming tutorial
- [GBATEK Mirror](https://rust-console.github.io/gbatek-gbaonly/) - Alternative GBATEK host
- [jsmolka gba-tests](https://github.com/jsmolka/gba-tests) - Test ROMs

## TODO

### Priority 1 - Critical for Game Compatibility
- [ ] Implement event scheduler for accurate timing
- [ ] Add 7-cycle IRQ delay (like mGBA)
- [ ] Implement Direct Sound channels A/B
- [ ] Fix timer cascade for audio
- [ ] Debug Pokemon Fire Red boot sequence

### Priority 2 - Important
- [ ] Implement EEPROM save types
- [ ] Add prefetch buffer emulation
- [ ] Implement remaining BIOS functions
- [ ] Verify all interrupt sources

### Priority 3 - Polish
- [ ] Implement mosaic effect
- [ ] Accurate wait state timing
- [ ] Serial/link cable support
- [ ] Comprehensive test suite validation
