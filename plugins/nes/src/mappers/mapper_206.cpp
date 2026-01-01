#include "mapper_206.hpp"

namespace nes {

Mapper206::Mapper206(std::vector<uint8_t>& prg_rom,
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

void Mapper206::reset() {
    m_bank_select = 0;

    for (int i = 0; i < 8; i++) {
        m_registers[i] = 0;
    }

    update_prg_banks();
    update_chr_banks();
}

void Mapper206::update_prg_banks() {
    size_t prg_size = m_prg_rom->size();

    // $8000-$9FFF: Switchable bank (R6)
    m_prg_bank_offsets[0] = (m_registers[6] * 0x2000) % prg_size;

    // $A000-$BFFF: Switchable bank (R7)
    m_prg_bank_offsets[1] = (m_registers[7] * 0x2000) % prg_size;

    // $C000-$DFFF: Fixed to second-to-last bank
    m_prg_bank_offsets[2] = (prg_size - 0x4000);

    // $E000-$FFFF: Fixed to last bank
    m_prg_bank_offsets[3] = (prg_size - 0x2000);
}

void Mapper206::update_chr_banks() {
    if (m_chr_rom->empty()) return;

    size_t chr_size = m_chr_rom->size();

    // 2KB banks at $0000 and $0800 (R0 and R1, ignoring bit 0)
    m_chr_bank_offsets[0] = ((m_registers[0] & 0xFE) * 0x400) % chr_size;
    m_chr_bank_offsets[1] = ((m_registers[0] | 0x01) * 0x400) % chr_size;
    m_chr_bank_offsets[2] = ((m_registers[1] & 0xFE) * 0x400) % chr_size;
    m_chr_bank_offsets[3] = ((m_registers[1] | 0x01) * 0x400) % chr_size;

    // 1KB banks at $1000-$1C00 (R2, R3, R4, R5)
    m_chr_bank_offsets[4] = (m_registers[2] * 0x400) % chr_size;
    m_chr_bank_offsets[5] = (m_registers[3] * 0x400) % chr_size;
    m_chr_bank_offsets[6] = (m_registers[4] * 0x400) % chr_size;
    m_chr_bank_offsets[7] = (m_registers[5] * 0x400) % chr_size;
}

uint8_t Mapper206::cpu_read(uint16_t address) {
    // No PRG RAM on Namcot 118

    // PRG ROM: $8000-$FFFF
    if (address >= 0x8000) {
        int bank = (address - 0x8000) / 0x2000;
        uint32_t offset = m_prg_bank_offsets[bank] + (address & 0x1FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper206::cpu_write(uint16_t address, uint8_t value) {
    // Only even addresses used for bank select, odd for bank data
    if (address >= 0x8000 && address < 0xA000) {
        if (address & 0x0001) {
            // Bank data ($8001, $8003, etc.)
            int reg = m_bank_select & 0x07;

            // Mask values appropriately
            switch (reg) {
                case 0:
                case 1:
                    // CHR 2KB banks - mask to 6 bits (64 banks max)
                    m_registers[reg] = value & 0x3F;
                    break;
                case 2:
                case 3:
                case 4:
                case 5:
                    // CHR 1KB banks - mask to 6 bits
                    m_registers[reg] = value & 0x3F;
                    break;
                case 6:
                case 7:
                    // PRG 8KB banks - mask to 4 bits (16 banks = 128KB max)
                    m_registers[reg] = value & 0x0F;
                    break;
            }

            update_prg_banks();
            update_chr_banks();
        } else {
            // Bank select ($8000, $8002, etc.)
            m_bank_select = value;
            // DxROM ignores CHR A12 inversion (bit 7) and PRG ROM mode (bit 6)
        }
    }
    // DxROM has no mirroring control, no IRQ, no PRG RAM protection
}

uint8_t Mapper206::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            int bank = address / 0x400;
            uint32_t offset = m_chr_bank_offsets[bank] + (address & 0x3FF);
            return (*m_chr_rom)[offset % m_chr_rom->size()];
        }
    }
    return 0;
}

void Mapper206::ppu_write(uint16_t address, uint8_t value) {
    if (address < 0x2000 && m_has_chr_ram) {
        if (!m_chr_rom->empty()) {
            int bank = address / 0x400;
            uint32_t offset = m_chr_bank_offsets[bank] + (address & 0x3FF);
            (*m_chr_rom)[offset % m_chr_rom->size()] = value;
        }
    }
}

void Mapper206::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_bank_select);
    for (int i = 0; i < 8; i++) {
        data.push_back(m_registers[i]);
    }
}

void Mapper206::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 9) return;

    m_bank_select = *data++; remaining--;
    for (int i = 0; i < 8; i++) {
        m_registers[i] = *data++; remaining--;
    }

    update_prg_banks();
    update_chr_banks();
}

} // namespace nes
