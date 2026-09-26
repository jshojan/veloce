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
    m_cycle_count = 0;
    m_last_write_cycle = UINT64_MAX;
    update_banks();
}

uint32_t Mapper001::prg_ram_offset(uint16_t address) const {
    // Standard MMC1 boards (SNROM/SCROM/SKROM, PRG <= 256KB) have a single
    // non-banked 8KB PRG RAM chip. The SXROM board used for 512KB MMC1
    // titles (Final Fantasy I&II, Dragon Warrior III/IV, ...) wires
    // PRG-RAM A13/A14 to the CHR bank 0 register (bits 2-3), giving 32KB
    // of banked PRG RAM. Cartridge::load only grows m_prg_ram past 8KB for
    // that class of ROM, so bank only when there is more than one page.
    uint32_t bank = 0;
    if (m_prg_ram->size() > 0x2000) {
        bank = (m_chr_bank_0 >> 2) & 0x03;
    }
    return bank * 0x2000 + (address & 0x1FFF);
}

uint8_t Mapper001::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            uint32_t offset = prg_ram_offset(address);
            if (offset < m_prg_ram->size()) {
                return (*m_prg_ram)[offset];
            }
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
            uint32_t offset = prg_ram_offset(address);
            if (offset < m_prg_ram->size()) {
                (*m_prg_ram)[offset] = value;
            }
        }
        return;
    }

    // MMC1 register write: $8000-$FFFF
    if (address >= 0x8000) {
        write_register(address, value);
    }
}

void Mapper001::write_register(uint16_t address, uint8_t value) {
    // Real MMC1 hardware is too slow to latch two serial writes issued on
    // back-to-back CPU cycles: RMW instructions (INC/DEC on an $8000+
    // address) issue a dummy write of the old value immediately followed
    // by the real write of the new value, one cycle apart. If that old
    // value had bit 7 set (e.g. incrementing a ROM byte read back as
    // $FF), the dummy write would reset the shift register, and the very
    // next write would then shift a bogus bit into the freshly reset
    // register. Hardware ignores the second write of such a pair
    // entirely (nesdev: "MMC1 ignores writes on the CPU cycle immediately
    // after a write"), so detect and drop it here before doing anything
    // else, including the bit-7 reset.
    bool consecutive = (m_last_write_cycle != UINT64_MAX) &&
                        (m_cycle_count == m_last_write_cycle + 1);
    m_last_write_cycle = m_cycle_count;
    if (consecutive) {
        if (is_debug_mode()) {
            fprintf(stderr, "MMC1: ignored consecutive-cycle write (addr=%04X val=%02X)\n",
                    address, value);
        }
        return;
    }

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

    // SXROM boards with a 512KB (or larger) PRG ROM wire CHR bank 0 bit 4
    // to PRG A18, selecting which 256KB outer half the 4-bit m_prg_bank
    // register indexes into. Smaller ROMs (the vast majority of MMC1
    // boards) never exceed one 256KB half, so `outer` is always 0 there
    // and behavior is unchanged.
    uint32_t outer = 0;
    if (prg_size > 0x40000) {
        outer = (m_chr_bank_0 & 0x10) ? 0x40000 : 0;
    }

    switch (prg_mode) {
        case 0:
        case 1:
            // 32KB mode: switch both banks together
            m_prg_bank_0_offset = outer + (m_prg_bank & 0x0E) * 0x4000;
            m_prg_bank_1_offset = m_prg_bank_0_offset + 0x4000;
            break;
        case 2:
            // Fix first bank at $8000, switch $C000
            m_prg_bank_0_offset = outer;
            m_prg_bank_1_offset = outer + m_prg_bank * 0x4000;
            break;
        case 3:
            // Switch $8000, fix last bank at $C000 (last bank of the
            // selected outer half; equals prg_size - 0x4000 when outer is 0)
            m_prg_bank_0_offset = outer + m_prg_bank * 0x4000;
            m_prg_bank_1_offset = outer + 0x3C000;
            break;
    }

    // Ensure offsets are within bounds. Defense in depth: Cartridge::load
    // rejects PRG=0 ROMs before any mapper is constructed, but guard the
    // modulo anyway (integer modulo by zero is undefined behaviour /
    // SIGFPE) in case this mapper is ever built directly with an empty
    // PRG vector.
    if (prg_size > 0) {
        m_prg_bank_0_offset %= prg_size;
        m_prg_bank_1_offset %= prg_size;
    } else {
        m_prg_bank_0_offset = 0;
        m_prg_bank_1_offset = 0;
    }

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
