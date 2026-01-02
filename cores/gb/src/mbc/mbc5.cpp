#include "mbc5.hpp"

namespace gb {

void MBC5::reset() {
    MBC::reset();
    m_rom_bank_lo = 1;
    m_rom_bank_hi = 0;
    m_rom_bank = 1;
}

void MBC5::write(uint16_t address, uint8_t value) {
    if (address < 0x2000) {
        // RAM Enable
        m_ram_enabled = (value == 0x0A);
    } else if (address < 0x3000) {
        // ROM Bank Number (lower 8 bits)
        m_rom_bank_lo = value;
        m_rom_bank = (m_rom_bank_hi << 8) | m_rom_bank_lo;
        m_rom_bank %= m_rom_banks;
    } else if (address < 0x4000) {
        // ROM Bank Number (upper 1 bit)
        m_rom_bank_hi = value & 0x01;
        m_rom_bank = (m_rom_bank_hi << 8) | m_rom_bank_lo;
        m_rom_bank %= m_rom_banks;
    } else if (address < 0x6000) {
        // RAM Bank Number
        m_ram_bank = value & 0x0F;
        if (m_ram_banks > 0) {
            m_ram_bank %= m_ram_banks;
        }
    }
}

void MBC5::save_state(std::vector<uint8_t>& data) {
    MBC::save_state(data);
    data.push_back(m_rom_bank_lo);
    data.push_back(m_rom_bank_hi);
}

void MBC5::load_state(const uint8_t*& data, size_t& remaining) {
    MBC::load_state(data, remaining);
    m_rom_bank_lo = *data++; remaining--;
    m_rom_bank_hi = *data++; remaining--;
    m_rom_bank = (m_rom_bank_hi << 8) | m_rom_bank_lo;
}

} // namespace gb
