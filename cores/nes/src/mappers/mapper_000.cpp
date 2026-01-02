#include "mapper_000.hpp"
#include "../debug.hpp"
#include <cstdio>

namespace nes {

Mapper000::Mapper000(std::vector<uint8_t>& prg_rom,
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

    // Check if we have 16KB or 32KB PRG ROM
    m_prg_16k = (prg_rom.size() <= 16384);
}

uint8_t Mapper000::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

    // PRG ROM: $8000-$FFFF
    if (address >= 0x8000) {
        if (m_prg_16k) {
            // Mirror 16KB across $8000-$FFFF
            return (*m_prg_rom)[address & 0x3FFF];
        } else {
            // Full 32KB
            return (*m_prg_rom)[address & 0x7FFF];
        }
    }

    return 0;
}

void Mapper000::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
            // Debug first few writes (only in debug mode)
            if (is_debug_mode()) {
                static int write_count = 0;
                if (write_count < 5) {
                    fprintf(stderr, "PRG RAM write: $%04X = %02X\n", address, value);
                    write_count++;
                }
            }
        }
    }
    // PRG ROM writes are ignored on NROM
}

uint8_t Mapper000::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    // CHR ROM/RAM: $0000-$1FFF
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            return (*m_chr_rom)[address];
        }
    }
    return 0;
}

void Mapper000::ppu_write(uint16_t address, uint8_t value) {
    // CHR RAM: $0000-$1FFF (only if using CHR RAM)
    if (address < 0x2000 && m_has_chr_ram) {
        if (!m_chr_rom->empty()) {
            (*m_chr_rom)[address] = value;
        }
    }
}

} // namespace nes
