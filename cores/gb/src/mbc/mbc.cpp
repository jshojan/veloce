#include "mbc.hpp"
#include "mbc1.hpp"
#include "mbc3.hpp"
#include "mbc5.hpp"
#include <cstring>

namespace gb {

MBC::MBC(std::vector<uint8_t>& rom, std::vector<uint8_t>& ram, int rom_banks, int ram_banks)
    : m_rom(rom), m_ram(ram), m_rom_banks(rom_banks), m_ram_banks(ram_banks) {
    reset();
}

std::unique_ptr<MBC> MBC::create(int type, std::vector<uint8_t>& rom, std::vector<uint8_t>& ram, int rom_banks, int ram_banks) {
    switch (type) {
        case 0:  return std::make_unique<MBC0>(rom, ram, rom_banks, ram_banks);
        case 1:  return std::make_unique<MBC1>(rom, ram, rom_banks, ram_banks);
        case 2:  return std::make_unique<MBC1>(rom, ram, rom_banks, ram_banks);  // MBC2 similar to MBC1
        case 3:  return std::make_unique<MBC3>(rom, ram, rom_banks, ram_banks);
        case 5:  return std::make_unique<MBC5>(rom, ram, rom_banks, ram_banks);
        default: return std::make_unique<MBC0>(rom, ram, rom_banks, ram_banks);
    }
}

void MBC::reset() {
    m_rom_bank = 1;
    m_ram_bank = 0;
    m_ram_enabled = false;
}

uint8_t MBC::read_rom(uint16_t address) {
    if (address < 0x4000) {
        // Bank 0
        if (address < m_rom.size()) {
            return m_rom[address];
        }
    } else {
        // Switchable bank
        uint32_t offset = (m_rom_bank * 0x4000) + (address - 0x4000);
        offset %= m_rom.size();
        return m_rom[offset];
    }
    return 0xFF;
}

uint8_t MBC::read_ram(uint16_t address) {
    if (!m_ram_enabled || m_ram.empty()) {
        return 0xFF;
    }

    uint32_t offset = (m_ram_bank * 0x2000) + address;
    if (offset < m_ram.size()) {
        return m_ram[offset];
    }
    return 0xFF;
}

void MBC::write_ram(uint16_t address, uint8_t value) {
    if (!m_ram_enabled || m_ram.empty()) {
        return;
    }

    uint32_t offset = (m_ram_bank * 0x2000) + address;
    if (offset < m_ram.size()) {
        m_ram[offset] = value;
    }
}

void MBC::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_rom_bank & 0xFF);
    data.push_back((m_rom_bank >> 8) & 0xFF);
    data.push_back(m_ram_bank);
    data.push_back(m_ram_enabled ? 1 : 0);
}

void MBC::load_state(const uint8_t*& data, size_t& remaining) {
    m_rom_bank = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
    m_ram_bank = *data++; remaining--;
    m_ram_enabled = (*data++ != 0); remaining--;
}

// MBC0 - No banking
void MBC0::write(uint16_t address, uint8_t value) {
    (void)address;
    (void)value;
    // No MBC, writes are ignored
}

} // namespace gb
