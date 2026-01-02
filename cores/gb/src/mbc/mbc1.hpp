#pragma once

#include "mbc.hpp"

namespace gb {

// MBC1 - Most common mapper
// Supports up to 2MB ROM and 32KB RAM
class MBC1 : public MBC {
public:
    using MBC::MBC;

    void reset() override;
    uint8_t read_rom(uint16_t address) override;
    void write(uint16_t address, uint8_t value) override;

    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

private:
    uint8_t m_rom_bank_lo = 1;   // 5-bit ROM bank number (bits 0-4)
    uint8_t m_bank_hi = 0;       // 2-bit bank number (bits 5-6 for ROM, or RAM bank)
    bool m_mode = false;         // 0 = ROM banking mode, 1 = RAM banking mode
};

} // namespace gb
