#pragma once

#include <cstdint>

namespace gb {

// System type detection
enum class SystemType : uint8_t {
    GameBoy = 0,        // DMG - Original Game Boy
    GameBoyColor = 1    // CGB - Game Boy Color
};

// GB/GBC interrupt types
enum class GBInterrupt : uint8_t {
    VBlank   = 0x01,
    LCDStat  = 0x02,
    Timer    = 0x04,
    Serial   = 0x08,
    Joypad   = 0x10
};

// Inline utility functions
constexpr inline uint16_t make_u16(uint8_t lo, uint8_t hi) {
    return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
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

} // namespace gb
