#pragma once

#include "mbc.hpp"

namespace gb {

// MBC5 - Enhanced MBC
// Supports up to 8MB ROM and 128KB RAM
// Used by many later Game Boy Color games
class MBC5 : public MBC {
public:
    using MBC::MBC;

    void reset() override;
    void write(uint16_t address, uint8_t value) override;

    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

private:
    uint8_t m_rom_bank_lo = 1;   // Lower 8 bits of ROM bank
    uint8_t m_rom_bank_hi = 0;   // Upper 1 bit of ROM bank (9-bit total)
};

} // namespace gb
