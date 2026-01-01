#include "mapper_034.hpp"

namespace nes {

Mapper034::Mapper034(std::vector<uint8_t>& prg_rom,
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

    // Detect BNROM vs NINA-001 based on CHR ROM size
    // BNROM: No CHR ROM (uses CHR RAM)
    // NINA-001: Has CHR ROM
    m_is_nina001 = !has_chr_ram && !chr_rom.empty();

    reset();
}

void Mapper034::reset() {
    m_prg_bank = 0;
    m_chr_bank_0 = 0;
    m_chr_bank_1 = 0;
    m_prg_bank_offset = 0;
    m_chr_bank_0_offset = 0;
    m_chr_bank_1_offset = 0x1000;  // Second 4KB bank
}

uint8_t Mapper034::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF (NINA-001 has this, BNROM typically doesn't)
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

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

void Mapper034::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }

        // NINA-001 register writes (in PRG RAM space)
        if (m_is_nina001) {
            switch (address) {
                case 0x7FFD:  // PRG bank select
                    m_prg_bank = value & 0x01;
                    m_prg_bank_offset = (m_prg_bank * 0x8000) % m_prg_rom->size();
                    break;

                case 0x7FFE:  // CHR bank 0 select ($0000-$0FFF)
                    m_chr_bank_0 = value & 0x0F;
                    m_chr_bank_0_offset = (m_chr_bank_0 * 0x1000) % m_chr_rom->size();
                    break;

                case 0x7FFF:  // CHR bank 1 select ($1000-$1FFF)
                    m_chr_bank_1 = value & 0x0F;
                    m_chr_bank_1_offset = (m_chr_bank_1 * 0x1000) % m_chr_rom->size();
                    break;
            }
        }
        return;
    }

    // BNROM: Bank select via writes to $8000-$FFFF
    if (address >= 0x8000 && !m_is_nina001) {
        m_prg_bank = value & 0x03;  // 2 bits = 4 banks max (128KB)
        m_prg_bank_offset = (m_prg_bank * 0x8000) % m_prg_rom->size();
    }
}

uint8_t Mapper034::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    if (address < 0x2000) {
        if (m_is_nina001) {
            // NINA-001: Two 4KB CHR banks
            if (address < 0x1000) {
                uint32_t offset = m_chr_bank_0_offset + (address & 0x0FFF);
                return (*m_chr_rom)[offset % m_chr_rom->size()];
            } else {
                uint32_t offset = m_chr_bank_1_offset + (address & 0x0FFF);
                return (*m_chr_rom)[offset % m_chr_rom->size()];
            }
        } else {
            // BNROM: 8KB CHR RAM (no banking)
            if (!m_chr_rom->empty()) {
                return (*m_chr_rom)[address];
            }
        }
    }
    return 0;
}

void Mapper034::ppu_write(uint16_t address, uint8_t value) {
    // CHR RAM write (BNROM only, NINA-001 uses CHR ROM)
    if (address < 0x2000 && m_has_chr_ram) {
        if (!m_chr_rom->empty()) {
            (*m_chr_rom)[address] = value;
        }
    }
}

void Mapper034::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_prg_bank);
    data.push_back(m_chr_bank_0);
    data.push_back(m_chr_bank_1);
}

void Mapper034::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 3) return;

    m_prg_bank = *data++; remaining--;
    m_chr_bank_0 = *data++; remaining--;
    m_chr_bank_1 = *data++; remaining--;

    m_prg_bank_offset = (m_prg_bank * 0x8000) % m_prg_rom->size();
    if (m_is_nina001 && !m_chr_rom->empty()) {
        m_chr_bank_0_offset = (m_chr_bank_0 * 0x1000) % m_chr_rom->size();
        m_chr_bank_1_offset = (m_chr_bank_1 * 0x1000) % m_chr_rom->size();
    }
}

} // namespace nes
