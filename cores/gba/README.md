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
| ARM multiply | Implemented | MUL, MLA, UMULL, SMULL (variable timing) |
| ARM swap | Implemented | SWP, SWPB |
| ARM MRS/MSR | Implemented | Status register access |
| Thumb instructions | Implemented | Full 16-bit instruction set |
| Processor modes | Implemented | User, FIQ, IRQ, SVC, ABT, UND, System |
| Register banking | Implemented | Banked R8-R14 for FIQ, R13-R14 for others |
| Condition codes | Implemented | All 16 conditions |
| IRQ handling | Implemented | 7-cycle delay, edge-triggered detection |
| Prefetch buffer | Implemented | 8-halfword buffer with fill/hit logic |
| Wait states | Implemented | N/S cycles per region (WS0/WS1/WS2) |

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
| Mosaic | Implemented | BG and OBJ horizontal/vertical |
| Sprite semi-transparency | Implemented | |
| HBlank DMA trigger | Implemented | |
| VBlank DMA trigger | Implemented | |

### APU (Mostly Complete)

| Feature | Status | Notes |
|---------|--------|-------|
| Channel 1 (Pulse+Sweep) | Implemented | Legacy GB audio |
| Channel 2 (Pulse) | Implemented | Legacy GB audio |
| Channel 3 (Wave) | Implemented | 4-bit wave samples |
| Channel 4 (Noise) | Implemented | LFSR noise |
| Frame sequencer | Implemented | Length, envelope, sweep clocking |
| Direct Sound A | Implemented | FIFO-based PCM audio |
| Direct Sound B | Implemented | FIFO-based PCM audio |
| Sound mixing | Implemented | DMG + Direct Sound mixing |
| Timer-driven audio | Implemented | Timer0/1 overflow triggers |
| Dynamic rate control | Implemented | Sync with host audio |

### DMA (Mostly Complete)

| Feature | Status | Notes |
|---------|--------|-------|
| DMA channels 0-3 | Implemented | |
| Immediate trigger | Implemented | |
| VBlank trigger | Implemented | |
| HBlank trigger | Implemented | |
| Sound FIFO (DMA1/2) | Implemented | 4-sample transfers on timer overflow |
| Word/Halfword size | Implemented | |
| Address control | Implemented | Inc/Dec/Fixed/Reload |
| Repeat mode | Implemented | Source address preserved |
| IRQ on complete | Implemented | Fires DMA0-3 IRQs when enabled |

### Timers (Partial)

| Feature | Status | Notes |
|---------|--------|-------|
| Timers 0-3 | Implemented | |
| Prescaler (1/64/256/1024) | Implemented | |
| Cascade mode | Partial | May have issues |
| IRQ generation | Not Verified | |
| Overflow handling | Implemented | |

### Interrupts (Functional)

| Feature | Status | Notes |
|---------|--------|-------|
| IE register | Implemented | Interrupt enable |
| IF register | Implemented | Interrupt flags (write to acknowledge) |
| IME register | Implemented | Master enable |
| Edge detection | Implemented | Prevents same IRQ from re-triggering |
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
| SoftReset | 0x00 | Implemented | Full GBATEK spec, clears IWRAM, sets SPs |
| RegisterRamReset | 0x01 | Implemented | EWRAM, IWRAM, Palette, VRAM, OAM |
| Halt | 0x02 | Implemented | |
| Stop | 0x03 | Not Implemented | |
| IntrWait | 0x04 | Implemented | Clears flags at 0x03007FF8 |
| VBlankIntrWait | 0x05 | Implemented | |
| Div | 0x06 | Implemented | |
| DivArm | 0x07 | Implemented | Swaps R0/R1 then calls Div |
| Sqrt | 0x08 | Implemented | |
| ArcTan | 0x09 | Implemented | |
| ArcTan2 | 0x0A | Implemented | |
| CpuSet | 0x0B | Implemented | |
| CpuFastSet | 0x0C | Implemented | |
| GetBiosChecksum | 0x0D | Implemented | Returns 0xBAAE187F |
| BgAffineSet | 0x0E | Implemented | Full matrix calculation |
| ObjAffineSet | 0x0F | Implemented | |
| BitUnPack | 0x10 | Implemented | |
| LZ77UnCompWram | 0x11 | Implemented | |
| LZ77UnCompVram | 0x12 | Implemented | Fixed back-reference bug |
| HuffUnComp | 0x13 | Partial | May have bugs |
| RLUnCompWram | 0x14 | Implemented | |
| RLUnCompVram | 0x15 | Partial | May have bugs |
| Diff8bitUnFilterWram | 0x16 | Implemented | 8-bit differential filter |
| Diff8bitUnFilterVram | 0x17 | Implemented | 16-bit aligned writes |
| Diff16bitUnFilter | 0x18 | Implemented | 16-bit differential filter |
| SoundBias | 0x19 | Not Implemented | |
| MidiKey2Freq | 0x1F | Not Implemented | |

### Save Types

| Type | Status | Notes |
|------|--------|-------|
| SRAM 32KB | Implemented | Auto-detected via string search |
| Flash 64KB | Implemented | Full command sequence |
| Flash 128KB | Implemented | Bank switching, chip ID |
| EEPROM 512B | Implemented | 6-bit addressing, serial protocol |
| EEPROM 8KB | Implemented | 14-bit addressing, serial protocol |

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

**Status: BOOTS (Intro playable, stable to 6000+ frames)**

The game boots successfully and runs through the intro sequence with stable performance.

Current state:
- ROM loads successfully, CRC32 verified
- Flash 128KB save type auto-detected
- VBlank/Timer IRQs firing correctly
- Direct Sound audio playing
- Game boots through intro, Nintendo logo and Game Freak logo appear
- Intro sequence plays stably (tested to 6000+ frames)
- Prefetch buffer working correctly

Implemented fixes:
- 7-cycle IRQ delay (mGBA-style timing)
- Edge-triggered IRQ detection to prevent nested IRQ loops
- ObjAffineSet BIOS fix (output stride handling)
- LZ77 decompression rewrite
- Direct Sound implementation
- Timer-driven audio DMA
- Prefetch buffer fill/hit logic
- Blending second target detection (proper layer verification)
- Backdrop brightness effects (when only backdrop visible)
- Affine reference point reload timing (VBlank only, not line 0)

### Known Accuracy Issues (from Code Analysis)

Based on comparison with GBATEK documentation and reference emulators (mGBA, NanoBoyAdvance):

#### Rendering
- **Scanline-based PPU** - Renders entire scanlines at HBlank, so mid-scanline raster effects don't work correctly. Games like Golden Sun (water effects) may have visual issues.
- **No cycle-accurate pixel rendering** - Would need per-pixel rendering for proper mid-scanline effects.

#### CPU Timing
- **Simplified pipeline** - 3-stage pipeline behavior is approximated rather than cycle-accurate

#### Audio
- **Missing APU quirks** - Obscure GB sound behaviors (zombie mode, extra length clocking) not implemented

#### BIOS
- **Missing functions** - SWI 0x19 (SoundBias), 0x1F (MidiKey2Freq)
- **Approximate math** - ArcTan/ArcTan2 use floating-point instead of BIOS lookup tables

### What's Needed for Better Compatibility

1. ~~**Prefetch Buffer** - The GBA has an 8-halfword prefetch buffer that affects instruction timing.~~ DONE

2. ~~**Accurate Wait States** - Per-region configurable wait states with proper N/S cycle handling~~ DONE

3. ~~**Variable multiply timing** - Based on operand bit count (1-4 cycles)~~ DONE

4. ~~**IRQ Delay** - mGBA implements a 7-cycle delay before servicing IRQs~~ DONE

5. ~~**Direct Sound** - DMA-fed audio is essential for GBA games~~ DONE

6. **Event Scheduler** - For sub-frame precision timing of DMA, timers, and interrupts

7. ~~**EEPROM Save Support** - Many games use EEPROM (512B or 8KB) instead of SRAM/Flash~~ DONE

8. ~~**Audio interpolation** - For better Direct Sound quality~~ DONE

9. **Cycle-accurate PPU** - For mid-scanline effects (major undertaking)

## Test Status

**Pass Rate: 90.9% (20/22 tests pass, 2 known issues)**

Run the test suite with:
```bash
cd cores/gba/tests
python3 test_runner.py -v
```

### CPU Tests (jsmolka gba-tests)

| Category | Test | Status | Notes |
|----------|------|--------|-------|
| ARM | arm.gba | **PASS** | Full ARM instruction set (conditions, branches, flags, shifts, data processing, PSR transfer, multiply, single/block/halfword transfer, swap) |
| Thumb | thumb.gba | **PASS** | Full Thumb instruction set |
| Memory | memory.gba | **PASS** | Memory access patterns, mirroring, video STRB behavior |

### PPU Tests

| Test | Status | Notes |
|------|--------|-------|
| hello.gba | **PASS** | Basic text rendering (Mode 4) |
| shades.gba | **PASS** | Color palette and shade rendering |
| stripes.gba | **PASS** | Pattern rendering and pixel accuracy |

### BIOS Tests

| Test | Status | Notes |
|------|--------|-------|
| bios.gba | **PASS** | HLE BIOS functions (Div, Sqrt, ArcTan, CpuSet, LZ77, etc.) |

### Save Tests

| Test | Status | Notes |
|------|--------|-------|
| sram.gba | **PASS** | 32KB SRAM read/write |
| flash64.gba | **PASS** | 64KB Flash commands |
| flash128.gba | **PASS** | 128KB Flash with bank switching |
| none.gba | **PASS** | No save type behavior |

### NanoBoyAdvance Hardware Tests

| Category | Test | Status | Notes |
|----------|------|--------|-------|
| DMA | latch.gba | **PASS** | DMA SAD/DAD latch timing |
| DMA | start-delay.gba | **PASS** | DMA enable to first transfer delay |
| DMA | force-nseq-access.gba | **PASS** | DMA non-sequential access |
| DMA | burst-into-tears.gba | **PASS** | DMA burst behavior |
| IRQ | irq-delay.gba | **PASS** | IRQ trigger to handler entry cycles |
| Timer | start-stop.gba | **PASS** | Timer enable/disable behavior |
| Timer | reload.gba | **PASS** | Timer reload on overflow |
| HALTCNT | haltcnt.gba | **PASS** | Halt/stop mode behavior |
| Bus | 128kb-boundary.gba | **PASS** | Memory access across 128KB boundaries |
| PPU | status-irq-dma.gba | KNOWN | Requires sub-scanline cycle accuracy |
| PPU | bgpd.gba | VISUAL | HBlank DMA to BG2PD (screenshot captured) |
| PPU | bgx.gba | VISUAL | HBlank DMA to BG2X (screenshot captured) |

### Edge Case Tests

| Test | Status | Notes |
|------|--------|-------|
| unsafe.gba | KNOWN | SRAM mirrors, unused ROM reads (hardware-undefined behavior) |

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
- [x] Add 7-cycle IRQ delay (like mGBA) - DONE
- [x] Edge-triggered IRQ detection - DONE
- [x] Direct Sound channels A/B - DONE
- [x] Timer-driven audio DMA - DONE
- [x] **Prefetch Buffer Timing** - DONE (8-halfword buffer with fill/hit logic)
- [x] **Accurate Wait State Timing** - DONE (N/S cycles per region WS0/WS1/WS2)
- [x] **Fix IRQ return LR calculation** - DONE (Fixed for both ARM and Thumb modes)
- [x] **Fix sprite priority for same-priority OAM** - DONE (Lower OAM indices win)
- [x] **Add bitmap mode sprite tile restriction** - DONE (Tiles < 512 unavailable in modes 3-5)
- [ ] **Cycle-Accurate Instruction Timing** - Partial (memory access delays partially implemented)

### Priority 2 - Important
- [x] Implement EEPROM save types (512B and 8KB) - DONE
- [ ] Event scheduler for sub-frame timing
- [x] Implement remaining BIOS functions (Diff filters) - DONE
- [ ] Implement SoundBias and MidiKey2Freq BIOS functions
- [x] Verify timer cascade mode accuracy - DONE (NBA tests pass)
- [x] Verify all interrupt source timing - DONE (NBA IRQ tests pass)

### Priority 3 - Polish
- [x] Implement mosaic effect - DONE (BG and OBJ mosaic, regular and affine)
- [x] Audio interpolation for Direct Sound - DONE (linear interpolation)
- [x] Variable multiply timing - DONE (1-4 cycles based on operand)
- [x] Comprehensive test suite validation - DONE (90.9% pass rate)
- [ ] Serial/link cable support
- [ ] GPIO/RTC full implementation

### Reference Emulators to Study
- [mGBA](https://github.com/mgba-emu/mgba) - High-accuracy, well-documented
- [NanoBoyAdvance](https://github.com/nba-emu/NanoBoyAdvance) - Cycle-accurate
- [SkyEmu](https://github.com/skylersaleh/SkyEmu) - Modern, well-structured

## Technical Notes: Prefetch Buffer

The GBA has an 8 x 16-bit prefetch buffer that significantly affects execution timing. This is now fully implemented.

### How It Works
1. **Capacity**: 8 halfwords (16-bit each)
2. **Filling**: Buffer fills during I-cycles or when CPU isn't accessing ROM
3. **Draining**: Provides 0-wait sequential ROM reads if data is prefetched
4. **Branches**: Buffer becomes invalid on non-sequential access (branches)
5. **Control**: WAITCNT bit 14 enables/disables the prefetch buffer

### Current Implementation
```cpp
struct Prefetch {
    uint32_t head_address;  // Start of prefetch region (first valid address)
    uint32_t next_address;  // Next address to prefetch
    int count = 0;          // Current halfwords in buffer (0-8)
    int countdown = 0;      // Cycles until next prefetch completes
    bool active = false;    // Is prefetcher currently filling?
};
```

Key behaviors:
- `prefetch_step(cycles)`: Advances buffer filling during execution cycles
- `prefetch_read(address, size)`: Checks for hit, returns 1 cycle on hit, full wait on miss
- `prefetch_invalidate()`: Clears buffer on branches/non-sequential access

### References
- [mGBA blog on cycle counting](https://mgba.io/2015/06/27/cycle-counting-prefetch/)
- [NanoBoyAdvance](https://github.com/nba-emu/NanoBoyAdvance) - Cycle-accurate reference
