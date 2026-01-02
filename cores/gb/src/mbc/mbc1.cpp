#include "mbc1.hpp"

namespace gb {

void MBC1::reset() {
    MBC::reset();
    m_rom_bank_lo = 1;
    m_bank_hi = 0;
    m_mode = false;
}

uint8_t MBC1::read_rom(uint16_t address) {
    if (address < 0x4000) {
        // Bank 0 (or bank 0x20/0x40/0x60 in mode 1 for large ROMs)
        uint32_t bank = 0;
        if (m_mode && m_rom_banks > 32) {
            bank = m_bank_hi << 5;
        }
        uint32_t offset = (bank * 0x4000) + address;
        offset %= m_rom.size();
        return m_rom[offset];
    } else {
        // Switchable bank
        uint32_t bank = m_rom_bank_lo;
        if (m_rom_banks > 32) {
            bank |= (m_bank_hi << 5);
        }
        bank %= m_rom_banks;

        uint32_t offset = (bank * 0x4000) + (address - 0x4000);
        offset %= m_rom.size();
        return m_rom[offset];
    }
}

void MBC1::write(uint16_t address, uint8_t value) {
    if (address < 0x2000) {
        // RAM Enable
        m_ram_enabled = ((value & 0x0F) == 0x0A);
    } else if (address < 0x4000) {
        // ROM Bank Number (lower 5 bits)
        m_rom_bank_lo = value & 0x1F;
        if (m_rom_bank_lo == 0) m_rom_bank_lo = 1;

        // Update combined bank
        m_rom_bank = m_rom_bank_lo;
        if (m_rom_banks > 32) {
            m_rom_bank |= (m_bank_hi << 5);
        }
    } else if (address < 0x6000) {
        // RAM Bank Number / Upper ROM Bank bits
        m_bank_hi = value & 0x03;

        if (m_mode) {
            // RAM banking mode
            m_ram_bank = m_bank_hi;
        } else {
            // ROM banking mode
            m_rom_bank = m_rom_bank_lo | (m_bank_hi << 5);
        }
    } else {
        // Banking Mode Select
        m_mode = value & 0x01;

        if (m_mode) {
            m_ram_bank = m_bank_hi;
        } else {
            m_ram_bank = 0;
        }
    }
}

void MBC1::save_state(std::vector<uint8_t>& data) {
    MBC::save_state(data);
    data.push_back(m_rom_bank_lo);
    data.push_back(m_bank_hi);
    data.push_back(m_mode ? 1 : 0);
}

void MBC1::load_state(const uint8_t*& data, size_t& remaining) {
    MBC::load_state(data, remaining);
    m_rom_bank_lo = *data++; remaining--;
    m_bank_hi = *data++; remaining--;
    m_mode = (*data++ != 0); remaining--;
}

} // namespace gb
