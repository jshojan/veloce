#include "mapper_004.hpp"

namespace nes {

Mapper004::Mapper004(std::vector<uint8_t>& prg_rom,
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

void Mapper004::reset() {
    m_bank_select = 0;
    m_prg_mode = false;
    m_chr_mode = false;

    // Initialize registers to 0 (power-on state is undefined on hardware,
    // but many games expect 0)
    for (int i = 0; i < 8; i++) {
        m_registers[i] = 0;
    }

    m_irq_counter = 0;
    m_irq_latch = 0;
    m_irq_enabled = false;
    m_irq_pending = false;
    m_irq_reload = false;

    update_banks();
}

void Mapper004::update_banks() {
    uint32_t prg_size = m_prg_rom->size();
    uint32_t chr_size = m_chr_rom->size();

    // PRG banking (8KB banks)
    // R6 = 8KB bank at $8000 or $C000 (mode dependent)
    // R7 = 8KB bank at $A000
    // Fixed banks: second-to-last at $C000 or $8000, last at $E000

    uint32_t prg_bank_count = prg_size / 0x2000;  // Number of 8KB banks
    if (prg_bank_count == 0) prg_bank_count = 1;

    uint8_t r6 = m_registers[6] % prg_bank_count;
    uint8_t r7 = m_registers[7] % prg_bank_count;
    uint8_t second_last = (prg_bank_count - 2) % prg_bank_count;
    uint8_t last = (prg_bank_count - 1) % prg_bank_count;

    if (m_prg_mode) {
        // Mode 1: $C000 swappable, $8000 fixed to second-to-last
        m_prg_bank[0] = second_last * 0x2000;
        m_prg_bank[1] = r7 * 0x2000;
        m_prg_bank[2] = r6 * 0x2000;
        m_prg_bank[3] = last * 0x2000;
    } else {
        // Mode 0: $8000 swappable, $C000 fixed to second-to-last
        m_prg_bank[0] = r6 * 0x2000;
        m_prg_bank[1] = r7 * 0x2000;
        m_prg_bank[2] = second_last * 0x2000;
        m_prg_bank[3] = last * 0x2000;
    }

    // CHR banking (1KB banks)
    if (chr_size > 0) {
        uint32_t chr_bank_count = chr_size / 0x400;  // Number of 1KB banks
        if (chr_bank_count == 0) chr_bank_count = 1;

        // R0, R1 = 2KB banks (ignore low bit)
        // R2, R3, R4, R5 = 1KB banks

        if (m_chr_mode) {
            // Mode 1: R2-R5 at $0000-$0FFF, R0-R1 at $1000-$1FFF
            m_chr_bank[0] = (m_registers[2] % chr_bank_count) * 0x400;
            m_chr_bank[1] = (m_registers[3] % chr_bank_count) * 0x400;
            m_chr_bank[2] = (m_registers[4] % chr_bank_count) * 0x400;
            m_chr_bank[3] = (m_registers[5] % chr_bank_count) * 0x400;
            m_chr_bank[4] = ((m_registers[0] & 0xFE) % chr_bank_count) * 0x400;
            m_chr_bank[5] = (((m_registers[0] & 0xFE) + 1) % chr_bank_count) * 0x400;
            m_chr_bank[6] = ((m_registers[1] & 0xFE) % chr_bank_count) * 0x400;
            m_chr_bank[7] = (((m_registers[1] & 0xFE) + 1) % chr_bank_count) * 0x400;
        } else {
            // Mode 0: R0-R1 at $0000-$0FFF, R2-R5 at $1000-$1FFF
            m_chr_bank[0] = ((m_registers[0] & 0xFE) % chr_bank_count) * 0x400;
            m_chr_bank[1] = (((m_registers[0] & 0xFE) + 1) % chr_bank_count) * 0x400;
            m_chr_bank[2] = ((m_registers[1] & 0xFE) % chr_bank_count) * 0x400;
            m_chr_bank[3] = (((m_registers[1] & 0xFE) + 1) % chr_bank_count) * 0x400;
            m_chr_bank[4] = (m_registers[2] % chr_bank_count) * 0x400;
            m_chr_bank[5] = (m_registers[3] % chr_bank_count) * 0x400;
            m_chr_bank[6] = (m_registers[4] % chr_bank_count) * 0x400;
            m_chr_bank[7] = (m_registers[5] % chr_bank_count) * 0x400;
        }
    }
}

uint8_t Mapper004::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

    // PRG ROM: $8000-$FFFF (four 8KB banks)
    if (address >= 0x8000) {
        int bank = (address - 0x8000) / 0x2000;
        uint32_t offset = m_prg_bank[bank] + (address & 0x1FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
    }

    return 0;
}

void Mapper004::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // MMC3 registers: $8000-$FFFF
    if (address >= 0x8000) {
        bool even = (address & 1) == 0;

        if (address < 0xA000) {
            // Bank select ($8000-$9FFE even) / Bank data ($8001-$9FFF odd)
            if (even) {
                m_bank_select = value & 0x07;
                m_prg_mode = (value & 0x40) != 0;
                m_chr_mode = (value & 0x80) != 0;
                update_banks();
            } else {
                m_registers[m_bank_select] = value;
                update_banks();
            }
        } else if (address < 0xC000) {
            // Mirroring ($A000-$BFFE even) / PRG RAM protect ($A001-$BFFF odd)
            if (even) {
                m_mirror_mode = (value & 1) ? MirrorMode::Horizontal : MirrorMode::Vertical;
            }
            // PRG RAM protect not fully implemented
        } else if (address < 0xE000) {
            // IRQ latch ($C000-$DFFE even) / IRQ reload ($C001-$DFFF odd)
            if (even) {
                m_irq_latch = value;
            } else {
                m_irq_reload = true;
            }
        } else {
            // IRQ disable ($E000-$FFFE even) / IRQ enable ($E001-$FFFF odd)
            if (even) {
                m_irq_enabled = false;
                m_irq_pending = false;
            } else {
                m_irq_enabled = true;
            }
        }
    }
}

uint8_t Mapper004::ppu_read(uint16_t address) {
    if (address < 0x2000) {
        // CHR ROM/RAM: 8 x 1KB banks
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
    }
    return 0;
}

void Mapper004::ppu_write(uint16_t address, uint8_t value) {
    if (!m_has_chr_ram) return;

    if (address < 0x2000) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
    }
}

void Mapper004::scanline() {
    // Called during each scanline by the PPU (at cycle 260)
    // This is a simplified implementation - real MMC3 uses A12 rising edge detection
    //
    // MMC3 counter behavior (compatible with both old and new MMC3 revisions):
    // - If counter is 0 OR reload flag is set: reload counter from latch
    // - Then decrement counter (new MMC3 behavior, required by Kirby's Adventure)
    // - If counter becomes 0 AND IRQs enabled: trigger IRQ

    if (m_irq_counter == 0 || m_irq_reload) {
        m_irq_counter = m_irq_latch;
        m_irq_reload = false;
    } else {
        m_irq_counter--;
    }

    // Check for IRQ after the counter operation
    if (m_irq_counter == 0 && m_irq_enabled) {
        m_irq_pending = true;
    }
}

void Mapper004::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_bank_select);
    data.push_back(m_prg_mode ? 1 : 0);
    data.push_back(m_chr_mode ? 1 : 0);

    for (int i = 0; i < 8; i++) {
        data.push_back(m_registers[i]);
    }

    data.push_back(m_irq_counter);
    data.push_back(m_irq_latch);
    data.push_back(m_irq_enabled ? 1 : 0);
    data.push_back(m_irq_pending ? 1 : 0);
    data.push_back(m_irq_reload ? 1 : 0);
    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void Mapper004::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 16) return;

    m_bank_select = *data++; remaining--;
    m_prg_mode = (*data++ != 0); remaining--;
    m_chr_mode = (*data++ != 0); remaining--;

    for (int i = 0; i < 8; i++) {
        m_registers[i] = *data++; remaining--;
    }

    m_irq_counter = *data++; remaining--;
    m_irq_latch = *data++; remaining--;
    m_irq_enabled = (*data++ != 0); remaining--;
    m_irq_pending = (*data++ != 0); remaining--;
    m_irq_reload = (*data++ != 0); remaining--;
    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    update_banks();
}

} // namespace nes
