#include "mapper_066.hpp"

namespace nes {

Mapper066::Mapper066(std::vector<uint8_t>& prg_rom,
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

void Mapper066::reset() {
    m_prg_bank = 0;
    m_chr_bank = 0;
    m_prg_bank_offset = 0;
    m_chr_bank_offset = 0;
}

uint8_t Mapper066::cpu_read(uint16_t address) {
    // No PRG RAM on GNROM boards

    // PRG ROM: $8000-$FFFF (32KB switchable)
    if (address >= 0x8000) {
        uint32_t offset = m_prg_bank_offset + (address & 0x7FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper066::cpu_write(uint16_t address, uint8_t value) {
    // Bank select: $8000-$FFFF
    // Bits 0-1: CHR bank select (8KB banks)
    // Bits 4-5: PRG bank select (32KB banks)
    // Note: This is inverted from Color Dreams (Mapper 11)
    if (address >= 0x8000) {
        m_chr_bank = value & 0x03;
        m_prg_bank = (value >> 4) & 0x03;

        m_prg_bank_offset = (m_prg_bank * 0x8000) % m_prg_rom->size();
        if (!m_chr_rom->empty()) {
            m_chr_bank_offset = (m_chr_bank * 0x2000) % m_chr_rom->size();
        }
    }
}

uint8_t Mapper066::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    // CHR ROM: $0000-$1FFF
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            uint32_t offset = m_chr_bank_offset + address;
            return (*m_chr_rom)[offset % m_chr_rom->size()];
        }
    }
    return 0;
}

void Mapper066::ppu_write(uint16_t address, uint8_t value) {
    // CHR RAM: $0000-$1FFF (if present)
    if (address < 0x2000 && m_has_chr_ram) {
        if (!m_chr_rom->empty()) {
            uint32_t offset = m_chr_bank_offset + address;
            (*m_chr_rom)[offset % m_chr_rom->size()] = value;
        }
    }
}

void Mapper066::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_prg_bank);
    data.push_back(m_chr_bank);
}

void Mapper066::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 2) return;

    m_prg_bank = *data++; remaining--;
    m_chr_bank = *data++; remaining--;

    m_prg_bank_offset = (m_prg_bank * 0x8000) % m_prg_rom->size();
    if (!m_chr_rom->empty()) {
        m_chr_bank_offset = (m_chr_bank * 0x2000) % m_chr_rom->size();
    }
}

} // namespace nes
