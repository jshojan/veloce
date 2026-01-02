#include "mapper_009.hpp"
#include "../debug.hpp"
#include <cstdio>

namespace nes {

Mapper009::Mapper009(std::vector<uint8_t>& prg_rom,
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

void Mapper009::reset() {
    m_prg_bank = 0;
    m_prg_bank_offset = 0;
    // Last 3 8KB banks are fixed at $A000-$FFFF
    m_prg_fixed_offset = m_prg_rom->size() - (3 * 0x2000);

    m_chr_bank_0_fd = 0;
    m_chr_bank_0_fe = 0;
    m_chr_bank_1_fd = 0;
    m_chr_bank_1_fe = 0;

    m_latch_0 = true;  // Power-on state is $FE
    m_latch_1 = true;

    update_chr_banks();
}

void Mapper009::update_chr_banks() {
    uint8_t bank_0 = m_latch_0 ? m_chr_bank_0_fe : m_chr_bank_0_fd;
    uint8_t bank_1 = m_latch_1 ? m_chr_bank_1_fe : m_chr_bank_1_fd;

    m_chr_bank_0_offset = (bank_0 * 0x1000) % m_chr_rom->size();
    m_chr_bank_1_offset = (bank_1 * 0x1000) % m_chr_rom->size();
}

uint8_t Mapper009::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

    // Switchable 8KB PRG bank: $8000-$9FFF
    if (address >= 0x8000 && address < 0xA000) {
        uint32_t offset = m_prg_bank_offset + (address & 0x1FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    // Fixed 24KB (3 x 8KB banks): $A000-$FFFF
    if (address >= 0xA000) {
        uint32_t offset = m_prg_fixed_offset + (address - 0xA000);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper009::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // Mapper registers: $A000-$FFFF
    if (address >= 0xA000) {
        switch (address & 0xF000) {
            case 0xA000:  // PRG ROM bank select
                m_prg_bank = value & 0x0F;
                m_prg_bank_offset = (m_prg_bank * 0x2000) % m_prg_rom->size();
                if (is_debug_mode()) {
                    fprintf(stderr, "MMC2: PRG bank = %02X\n", m_prg_bank);
                }
                break;

            case 0xB000:  // CHR ROM bank 0 select ($FD)
                m_chr_bank_0_fd = value & 0x1F;
                update_chr_banks();
                if (is_debug_mode()) {
                    fprintf(stderr, "MMC2: CHR bank 0 FD = %02X\n", m_chr_bank_0_fd);
                }
                break;

            case 0xC000:  // CHR ROM bank 0 select ($FE)
                m_chr_bank_0_fe = value & 0x1F;
                update_chr_banks();
                if (is_debug_mode()) {
                    fprintf(stderr, "MMC2: CHR bank 0 FE = %02X\n", m_chr_bank_0_fe);
                }
                break;

            case 0xD000:  // CHR ROM bank 1 select ($FD)
                m_chr_bank_1_fd = value & 0x1F;
                update_chr_banks();
                if (is_debug_mode()) {
                    fprintf(stderr, "MMC2: CHR bank 1 FD = %02X\n", m_chr_bank_1_fd);
                }
                break;

            case 0xE000:  // CHR ROM bank 1 select ($FE)
                m_chr_bank_1_fe = value & 0x1F;
                update_chr_banks();
                if (is_debug_mode()) {
                    fprintf(stderr, "MMC2: CHR bank 1 FE = %02X\n", m_chr_bank_1_fe);
                }
                break;

            case 0xF000:  // Mirroring
                if (value & 0x01) {
                    m_mirror_mode = MirrorMode::Horizontal;
                } else {
                    m_mirror_mode = MirrorMode::Vertical;
                }
                if (is_debug_mode()) {
                    fprintf(stderr, "MMC2: Mirroring = %s\n", (value & 0x01) ? "Horizontal" : "Vertical");
                }
                break;
        }
    }
}

uint8_t Mapper009::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    if (address < 0x2000) {
        uint8_t value;

        if (address < 0x1000) {
            // Read from first 4KB CHR bank
            uint32_t offset = m_chr_bank_0_offset + (address & 0x0FFF);
            value = (*m_chr_rom)[offset % m_chr_rom->size()];

            // MMC2 latch switching for $0000-$0FFF:
            // - $0FD8 triggers latch 0 to select $FD bank
            // - $0FE8 triggers latch 0 to select $FE bank
            // The latch updates AFTER the read (old bank is used for the triggering tile)
            // A3 must be set (address & 0x8), ensuring we're on the second byte row
            if (address == 0x0FD8) {
                m_latch_0 = false;
                update_chr_banks();
            } else if (address == 0x0FE8) {
                m_latch_0 = true;
                update_chr_banks();
            }
        } else {
            // Read from second 4KB CHR bank
            uint32_t offset = m_chr_bank_1_offset + (address & 0x0FFF);
            value = (*m_chr_rom)[offset % m_chr_rom->size()];

            // MMC2 latch switching for $1000-$1FFF:
            // - $1FD8-$1FDF triggers latch 1 to select $FD bank
            // - $1FE8-$1FEF triggers latch 1 to select $FE bank
            // Note: latch 1 responds to a range, not just one address
            uint16_t masked = address & 0x0FF8;
            if (masked == 0x0FD8) {
                m_latch_1 = false;
                update_chr_banks();
            } else if (masked == 0x0FE8) {
                m_latch_1 = true;
                update_chr_banks();
            }
        }

        return value;
    }

    return 0;
}

void Mapper009::ppu_write(uint16_t address, uint8_t value) {
    // MMC2 uses CHR ROM, not RAM, so writes are typically ignored
    // But handle CHR RAM case if present
    if (address < 0x2000 && m_has_chr_ram) {
        if (address < 0x1000) {
            uint32_t offset = m_chr_bank_0_offset + (address & 0x0FFF);
            (*m_chr_rom)[offset % m_chr_rom->size()] = value;
        } else {
            uint32_t offset = m_chr_bank_1_offset + (address & 0x0FFF);
            (*m_chr_rom)[offset % m_chr_rom->size()] = value;
        }
    }
}

void Mapper009::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_prg_bank);
    data.push_back(m_chr_bank_0_fd);
    data.push_back(m_chr_bank_0_fe);
    data.push_back(m_chr_bank_1_fd);
    data.push_back(m_chr_bank_1_fe);
    data.push_back(m_latch_0 ? 1 : 0);
    data.push_back(m_latch_1 ? 1 : 0);
    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void Mapper009::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 8) return;

    m_prg_bank = *data++; remaining--;
    m_chr_bank_0_fd = *data++; remaining--;
    m_chr_bank_0_fe = *data++; remaining--;
    m_chr_bank_1_fd = *data++; remaining--;
    m_chr_bank_1_fe = *data++; remaining--;
    m_latch_0 = (*data++ != 0); remaining--;
    m_latch_1 = (*data++ != 0); remaining--;
    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    m_prg_bank_offset = (m_prg_bank * 0x2000) % m_prg_rom->size();
    update_chr_banks();
}

} // namespace nes
