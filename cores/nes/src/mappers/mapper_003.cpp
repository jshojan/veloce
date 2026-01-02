#include "mapper_003.hpp"
#include "../debug.hpp"
#include <cstdio>

namespace nes {

Mapper003::Mapper003(std::vector<uint8_t>& prg_rom,
                     std::vector<uint8_t>& chr_rom,
                     std::vector<uint8_t>& prg_ram,
                     MirrorMode mirror,
                     bool has_chr_ram)
{
    m_prg_rom = &prg_rom;
    m_chr_rom = &chr_rom;
    m_prg_ram = &prg_ram;
    m_mirror_mode = mirror;
    m_has_chr_ram = has_chr_ram;

    reset();
}

void Mapper003::reset() {
    m_chr_bank = 0;
    m_chr_bank_offset = 0;
}

uint8_t Mapper003::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF (if present)
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

    // PRG ROM: $8000-$FFFF (fixed, no banking)
    if (address >= 0x8000) {
        // Handle both 16KB and 32KB ROMs
        uint32_t offset = address & (m_prg_rom->size() - 1);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper003::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // Bank select: $8000-$FFFF
    if (address >= 0x8000) {
        // Select CHR bank (usually only 2 bits used, but can be more)
        m_chr_bank = value & 0x03;  // Typically 4 banks max
        m_chr_bank_offset = (m_chr_bank * 0x2000) % m_chr_rom->size();
        if (is_debug_mode()) {
            fprintf(stderr, "CNROM: CHR bank = %02X (addr=%04X val=%02X)\n", m_chr_bank, address, value);
        }
    }
}

uint8_t Mapper003::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    // CHR ROM: $0000-$1FFF (banked)
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            uint32_t offset = m_chr_bank_offset + address;
            if (offset < m_chr_rom->size()) {
                return (*m_chr_rom)[offset];
            }
        }
    }
    return 0;
}

void Mapper003::ppu_write(uint16_t address, uint8_t value) {
    // CHR RAM: $0000-$1FFF (if using CHR RAM instead of ROM)
    if (address < 0x2000 && m_has_chr_ram) {
        if (!m_chr_rom->empty()) {
            uint32_t offset = m_chr_bank_offset + address;
            if (offset < m_chr_rom->size()) {
                (*m_chr_rom)[offset] = value;
            }
        }
    }
}

void Mapper003::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_chr_bank);
}

void Mapper003::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 1) return;
    m_chr_bank = *data++; remaining--;
    m_chr_bank_offset = (m_chr_bank * 0x2000) % m_chr_rom->size();
}

} // namespace nes
