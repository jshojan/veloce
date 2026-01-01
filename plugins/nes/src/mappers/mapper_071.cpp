#include "mapper_071.hpp"

namespace nes {

Mapper071::Mapper071(std::vector<uint8_t>& prg_rom,
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

    // Detect if this is a board with mirroring control
    // Fire Hawk uses mirroring control, most others don't
    // We enable it by default as it doesn't hurt games that don't use it
    m_has_mirroring_control = true;

    reset();
}

void Mapper071::reset() {
    m_prg_bank = 0;
    m_prg_bank_offset = 0;
    // Last 16KB bank is fixed at $C000-$FFFF
    m_prg_fixed_offset = m_prg_rom->size() - 0x4000;
}

uint8_t Mapper071::cpu_read(uint16_t address) {
    // No PRG RAM on Camerica boards

    // Switchable 16KB PRG bank: $8000-$BFFF
    if (address >= 0x8000 && address < 0xC000) {
        uint32_t offset = m_prg_bank_offset + (address & 0x3FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    // Fixed 16KB PRG bank: $C000-$FFFF
    if (address >= 0xC000) {
        uint32_t offset = m_prg_fixed_offset + (address & 0x3FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper071::cpu_write(uint16_t address, uint8_t value) {
    // Mirroring control: $9000-$9FFF (on some boards)
    if (address >= 0x9000 && address < 0xA000) {
        if (m_has_mirroring_control) {
            // Bit 4: Single-screen select
            if (value & 0x10) {
                m_mirror_mode = MirrorMode::SingleScreen1;
            } else {
                m_mirror_mode = MirrorMode::SingleScreen0;
            }
        }
        return;
    }

    // Bank select: $C000-$FFFF
    if (address >= 0xC000) {
        m_prg_bank = value & 0x0F;
        m_prg_bank_offset = (m_prg_bank * 0x4000) % m_prg_rom->size();
    }
}

uint8_t Mapper071::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    // CHR RAM: $0000-$1FFF
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            return (*m_chr_rom)[address];
        }
    }
    return 0;
}

void Mapper071::ppu_write(uint16_t address, uint8_t value) {
    // CHR RAM: $0000-$1FFF
    if (address < 0x2000) {
        if (!m_chr_rom->empty()) {
            (*m_chr_rom)[address] = value;
        }
    }
}

void Mapper071::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_prg_bank);
    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void Mapper071::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 2) return;

    m_prg_bank = *data++; remaining--;
    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    m_prg_bank_offset = (m_prg_bank * 0x4000) % m_prg_rom->size();
}

} // namespace nes
