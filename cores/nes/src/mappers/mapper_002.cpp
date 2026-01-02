#include "mapper_002.hpp"
#include "../debug.hpp"
#include <cstdio>

namespace nes {

Mapper002::Mapper002(std::vector<uint8_t>& prg_rom,
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

void Mapper002::reset() {
    m_prg_bank = 0;
    m_prg_bank_offset = 0;
    // Last 16KB bank is fixed at $C000
    m_last_bank_offset = m_prg_rom->size() - 0x4000;
}

uint8_t Mapper002::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF (if present)
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

    // PRG ROM bank 0: $8000-$BFFF (switchable)
    if (address >= 0x8000 && address < 0xC000) {
        uint32_t offset = m_prg_bank_offset + (address & 0x3FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    // PRG ROM bank 1: $C000-$FFFF (fixed to last bank)
    if (address >= 0xC000) {
        uint32_t offset = m_last_bank_offset + (address & 0x3FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper002::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // Bank select: $8000-$FFFF
    if (address >= 0x8000) {
        m_prg_bank = value & 0x0F;  // Usually only 4 bits used
        m_prg_bank_offset = (m_prg_bank * 0x4000) % m_prg_rom->size();
        if (is_debug_mode()) {
            fprintf(stderr, "UxROM: PRG bank = %02X (addr=%04X val=%02X)\n", m_prg_bank, address, value);
        }
    }
}

uint8_t Mapper002::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    // CHR RAM: $0000-$1FFF
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            return (*m_chr_rom)[address];
        }
    }
    return 0;
}

void Mapper002::ppu_write(uint16_t address, uint8_t value) {
    // CHR RAM: $0000-$1FFF (UxROM uses CHR RAM)
    if (address < 0x2000 && m_has_chr_ram) {
        if (!m_chr_rom->empty()) {
            (*m_chr_rom)[address] = value;
        }
    }
}

void Mapper002::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_prg_bank);
}

void Mapper002::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 1) return;
    m_prg_bank = *data++; remaining--;
    m_prg_bank_offset = (m_prg_bank * 0x4000) % m_prg_rom->size();
}

} // namespace nes
