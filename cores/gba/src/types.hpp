#pragma once

#include <cstdint>

namespace gba {

// System type detection
enum class SystemType {
    Unknown,
    GameBoy,        // DMG - Original Game Boy
    GameBoyColor,   // CGB - Game Boy Color
    GameBoyAdvance  // GBA - Game Boy Advance
};

// ARM processor modes (GBA)
enum class ProcessorMode : uint8_t {
    User       = 0x10,
    FIQ        = 0x11,
    IRQ        = 0x12,
    Supervisor = 0x13,
    Abort      = 0x17,
    Undefined  = 0x1B,
    System     = 0x1F
};

// ARM condition codes
enum class Condition : uint8_t {
    EQ = 0x0,   // Equal (Z=1)
    NE = 0x1,   // Not equal (Z=0)
    CS = 0x2,   // Carry set / unsigned higher or same (C=1)
    CC = 0x3,   // Carry clear / unsigned lower (C=0)
    MI = 0x4,   // Minus / negative (N=1)
    PL = 0x5,   // Plus / positive or zero (N=0)
    VS = 0x6,   // Overflow set (V=1)
    VC = 0x7,   // Overflow clear (V=0)
    HI = 0x8,   // Unsigned higher (C=1 and Z=0)
    LS = 0x9,   // Unsigned lower or same (C=0 or Z=1)
    GE = 0xA,   // Signed greater than or equal (N=V)
    LT = 0xB,   // Signed less than (N!=V)
    GT = 0xC,   // Signed greater than (Z=0 and N=V)
    LE = 0xD,   // Signed less than or equal (Z=1 or N!=V)
    AL = 0xE,   // Always (unconditional)
    NV = 0xF    // Never (ARMv1-v4), or special (ARMv5+)
};

// GB/GBC interrupt types
enum class GBInterrupt : uint8_t {
    VBlank   = 0x01,
    LCDStat  = 0x02,
    Timer    = 0x04,
    Serial   = 0x08,
    Joypad   = 0x10
};

// GBA interrupt types
enum class GBAInterrupt : uint16_t {
    VBlank   = 0x0001,
    HBlank   = 0x0002,
    VCount   = 0x0004,
    Timer0   = 0x0008,
    Timer1   = 0x0010,
    Timer2   = 0x0020,
    Timer3   = 0x0040,
    Serial   = 0x0080,
    DMA0     = 0x0100,
    DMA1     = 0x0200,
    DMA2     = 0x0400,
    DMA3     = 0x0800,
    Keypad   = 0x1000,
    GamePak  = 0x2000
};

// GBA display modes
enum class DisplayMode : uint8_t {
    Mode0 = 0,  // 4 tiled backgrounds
    Mode1 = 1,  // 2 tiled + 1 affine background
    Mode2 = 2,  // 2 affine backgrounds
    Mode3 = 3,  // Single framebuffer, 240x160, 15-bit color
    Mode4 = 4,  // Double framebuffer, 240x160, 8-bit palette
    Mode5 = 5   // Double framebuffer, 160x128, 15-bit color
};

// Memory region identifiers (GBA)
enum class MemoryRegion : uint8_t {
    BIOS,
    EWRAM,
    IWRAM,
    IO,
    Palette,
    VRAM,
    OAM,
    ROM_WS0,
    ROM_WS1,
    ROM_WS2,
    SRAM,
    Invalid
};

// Inline utility functions
constexpr inline uint16_t make_u16(uint8_t lo, uint8_t hi) {
    return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
}

constexpr inline uint32_t make_u32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    return static_cast<uint32_t>(b0) |
           (static_cast<uint32_t>(b1) << 8) |
           (static_cast<uint32_t>(b2) << 16) |
           (static_cast<uint32_t>(b3) << 24);
}

constexpr inline bool bit_test(uint32_t value, int bit) {
    return (value & (1u << bit)) != 0;
}

constexpr inline uint32_t bit_set(uint32_t value, int bit) {
    return value | (1u << bit);
}

constexpr inline uint32_t bit_clear(uint32_t value, int bit) {
    return value & ~(1u << bit);
}

constexpr inline uint32_t bits(uint32_t value, int high, int low) {
    return (value >> low) & ((1u << (high - low + 1)) - 1);
}

// Sign extension helpers
constexpr inline int32_t sign_extend_8(uint32_t value) {
    return static_cast<int32_t>(static_cast<int8_t>(value & 0xFF));
}

constexpr inline int32_t sign_extend_16(uint32_t value) {
    return static_cast<int32_t>(static_cast<int16_t>(value & 0xFFFF));
}

constexpr inline int32_t sign_extend_24(uint32_t value) {
    if (value & 0x800000) {
        return static_cast<int32_t>(value | 0xFF000000);
    }
    return static_cast<int32_t>(value & 0x00FFFFFF);
}

// Rotate right helper
constexpr inline uint32_t ror(uint32_t value, int amount) {
    amount &= 31;
    if (amount == 0) return value;
    return (value >> amount) | (value << (32 - amount));
}

// Arithmetic shift right helper
constexpr inline int32_t asr(int32_t value, int amount) {
    if (amount >= 32) {
        return (value < 0) ? -1 : 0;
    }
    return value >> amount;
}

} // namespace gba
