#include "mapper_001.hpp"
#include "../debug.hpp"
#include <cstdio>

namespace nes {

Mapper001::Mapper001(std::vector<uint8_t>& prg_rom,
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

void Mapper001::reset() {
    m_shift_register = 0x10;
    m_shift_count = 0;
    m_control = 0x0C;  // PRG fixed $C000, CHR 8KB mode
    m_chr_bank_0 = 0;
    m_chr_bank_1 = 0;
    m_prg_bank = 0;
    update_banks();
}

uint8_t Mapper001::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

    // PRG ROM bank 0: $8000-$BFFF
    if (address >= 0x8000 && address < 0xC000) {
        uint32_t offset = m_prg_bank_0_offset + (address & 0x3FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    // PRG ROM bank 1: $C000-$FFFF
    if (address >= 0xC000) {
        uint32_t offset = m_prg_bank_1_offset + (address & 0x3FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper001::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // MMC1 register write: $8000-$FFFF
    if (address >= 0x8000) {
        write_register(address, value);
    }
}

void Mapper001::write_register(uint16_t address, uint8_t value) {
    // Reset shift register if bit 7 is set
    if (value & 0x80) {
        m_shift_register = 0x10;
        m_shift_count = 0;
        m_control |= 0x0C;  // Set PRG ROM mode to 3
        if (is_debug_mode()) {
            fprintf(stderr, "MMC1: Reset (addr=%04X val=%02X)\n", address, value);
        }
        update_banks();
        return;
    }

    // Shift in bit 0
    m_shift_register = ((value & 1) << 4) | (m_shift_register >> 1);
    m_shift_count++;

    // After 5 writes, copy to internal register
    if (m_shift_count == 5) {
        uint8_t reg_value = m_shift_register;

        // Determine which register based on address
        if (address < 0xA000) {
            // Control ($8000-$9FFF)
            m_control = reg_value;

            // Update mirror mode
            switch (m_control & 0x03) {
                case 0: m_mirror_mode = MirrorMode::SingleScreen0; break;
                case 1: m_mirror_mode = MirrorMode::SingleScreen1; break;
                case 2: m_mirror_mode = MirrorMode::Vertical; break;
                case 3: m_mirror_mode = MirrorMode::Horizontal; break;
            }
            if (is_debug_mode()) {
                fprintf(stderr, "MMC1: Control=%02X (mirror=%d, PRG mode=%d, CHR mode=%d)\n",
                        m_control, m_control & 0x03, (m_control >> 2) & 0x03, (m_control >> 4) & 0x01);
            }
        } else if (address < 0xC000) {
            // CHR bank 0 ($A000-$BFFF)
            m_chr_bank_0 = reg_value;
            if (is_debug_mode()) {
                fprintf(stderr, "MMC1: CHR bank 0 = %02X\n", m_chr_bank_0);
            }
        } else if (address < 0xE000) {
            // CHR bank 1 ($C000-$DFFF)
            m_chr_bank_1 = reg_value;
            if (is_debug_mode()) {
                fprintf(stderr, "MMC1: CHR bank 1 = %02X\n", m_chr_bank_1);
            }
        } else {
            // PRG bank ($E000-$FFFF)
            m_prg_bank = reg_value & 0x0F;
            if (is_debug_mode()) {
                fprintf(stderr, "MMC1: PRG bank = %02X\n", m_prg_bank);
            }
        }

        update_banks();

        // Reset shift register
        m_shift_register = 0x10;
        m_shift_count = 0;
    }
}

void Mapper001::update_banks() {
    uint32_t prg_size = m_prg_rom->size();
    uint32_t chr_size = m_chr_rom->size();

    // PRG ROM bank mode (bits 2-3 of control)
    uint8_t prg_mode = (m_control >> 2) & 0x03;

    switch (prg_mode) {
        case 0:
        case 1:
            // 32KB mode: switch both banks together
            m_prg_bank_0_offset = (m_prg_bank & 0x0E) * 0x4000;
            m_prg_bank_1_offset = m_prg_bank_0_offset + 0x4000;
            break;
        case 2:
            // Fix first bank at $8000, switch $C000
            m_prg_bank_0_offset = 0;
            m_prg_bank_1_offset = m_prg_bank * 0x4000;
            break;
        case 3:
            // Switch $8000, fix last bank at $C000
            m_prg_bank_0_offset = m_prg_bank * 0x4000;
            m_prg_bank_1_offset = prg_size - 0x4000;
            break;
    }

    // Ensure offsets are within bounds
    m_prg_bank_0_offset %= prg_size;
    m_prg_bank_1_offset %= prg_size;

    // CHR bank mode (bit 4 of control)
    if (chr_size > 0) {
        if (m_control & 0x10) {
            // 4KB mode: separate banks
            m_chr_bank_0_offset = (m_chr_bank_0 * 0x1000) % chr_size;
            m_chr_bank_1_offset = (m_chr_bank_1 * 0x1000) % chr_size;
        } else {
            // 8KB mode: single bank
            m_chr_bank_0_offset = ((m_chr_bank_0 & 0x1E) * 0x1000) % chr_size;
            m_chr_bank_1_offset = m_chr_bank_0_offset + 0x1000;
            if (m_chr_bank_1_offset >= chr_size) {
                m_chr_bank_1_offset = 0;
            }
        }
    }
}

uint8_t Mapper001::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    if (address < 0x1000) {
        // CHR bank 0: $0000-$0FFF
        uint32_t offset = m_chr_bank_0_offset + address;
        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
    } else if (address < 0x2000) {
        // CHR bank 1: $1000-$1FFF
        uint32_t offset = m_chr_bank_1_offset + (address & 0x0FFF);
        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
    }
    return 0;
}

void Mapper001::ppu_write(uint16_t address, uint8_t value) {
    if (!m_has_chr_ram) return;

    if (address < 0x1000) {
        uint32_t offset = m_chr_bank_0_offset + address;
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
    } else if (address < 0x2000) {
        uint32_t offset = m_chr_bank_1_offset + (address & 0x0FFF);
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
    }
}

void Mapper001::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_shift_register);
    data.push_back(static_cast<uint8_t>(m_shift_count));
    data.push_back(m_control);
    data.push_back(m_chr_bank_0);
    data.push_back(m_chr_bank_1);
    data.push_back(m_prg_bank);
    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void Mapper001::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 7) return;

    m_shift_register = *data++; remaining--;
    m_shift_count = *data++; remaining--;
    m_control = *data++; remaining--;
    m_chr_bank_0 = *data++; remaining--;
    m_chr_bank_1 = *data++; remaining--;
    m_prg_bank = *data++; remaining--;
    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    update_banks();
}

} // namespace nes
