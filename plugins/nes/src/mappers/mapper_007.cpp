#include "mapper_007.hpp"

namespace nes {

Mapper007::Mapper007(std::vector<uint8_t>& prg_rom,
                     std::vector<uint8_t>& chr_rom,
                     std::vector<uint8_t>& prg_ram,
                     MirrorMode mirror,
                     bool has_chr_ram)
{
    m_prg_rom = &prg_rom;
    m_chr_rom = &chr_rom;
    m_prg_ram = &prg_ram;
    m_mirror_mode = MirrorMode::SingleScreen0;  // AxROM uses single-screen mirroring
    m_has_chr_ram = has_chr_ram;

    reset();
}

void Mapper007::reset() {
    // AxROM starts with the last bank selected on power-up
    // This ensures the reset vector at $FFFC-$FFFD points to valid code
    size_t num_banks = m_prg_rom->size() / 0x8000;
    m_prg_bank = (num_banks > 0) ? static_cast<uint8_t>(num_banks - 1) : 0;
    m_prg_bank_offset = m_prg_bank * 0x8000;
    m_mirror_mode = MirrorMode::SingleScreen0;
}

uint8_t Mapper007::cpu_read(uint16_t address) {
    // No PRG RAM on AxROM typically, but handle $6000-$7FFF as open bus
    if (address >= 0x6000 && address < 0x8000) {
        return 0;
    }

    // PRG ROM: $8000-$FFFF (32KB switchable bank)
    if (address >= 0x8000) {
        uint32_t offset = m_prg_bank_offset + (address & 0x7FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper007::cpu_write(uint16_t address, uint8_t value) {
    // Bank select: $8000-$FFFF
    // Bits 0-2: PRG bank select (32KB banks) - up to 256KB
    // Some boards use bit 3 for 512KB support
    // Bit 4: Nametable select (0 = lower, 1 = upper)
    if (address >= 0x8000) {
        // Note: AOROM has bus conflicts but ANROM doesn't
        // Most games work without bus conflict emulation
        // Use lower 4 bits for bank, then modulo by actual bank count
        m_prg_bank = value & 0x0F;
        m_prg_bank_offset = (m_prg_bank * 0x8000) % m_prg_rom->size();

        // Single-screen mirroring: bit 4 selects which nametable
        if (value & 0x10) {
            m_mirror_mode = MirrorMode::SingleScreen1;
        } else {
            m_mirror_mode = MirrorMode::SingleScreen0;
        }
    }
}

uint8_t Mapper007::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    // CHR RAM: $0000-$1FFF
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            return (*m_chr_rom)[address];
        }
    }
    return 0;
}

void Mapper007::ppu_write(uint16_t address, uint8_t value) {
    // CHR RAM: $0000-$1FFF (AxROM always uses CHR RAM)
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            (*m_chr_rom)[address] = value;
        }
    }
}

void Mapper007::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_prg_bank);
    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void Mapper007::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 2) return;
    m_prg_bank = *data++; remaining--;
    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;
    m_prg_bank_offset = (m_prg_bank * 0x8000) % m_prg_rom->size();
}

} // namespace nes
