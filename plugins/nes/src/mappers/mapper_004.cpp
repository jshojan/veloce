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
    m_irq_pending_at_cycle = 0;

    // A12 tracking for proper scanline detection
    m_last_a12 = false;
    m_last_a12_cycle = 0;
    m_current_frame_cycle = 0;

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

    // R6-R7: MMC3 only has 6 PRG ROM address lines, so mask with 0x3F
    uint8_t r6 = (m_registers[6] & 0x3F) % prg_bank_count;
    uint8_t r7 = (m_registers[7] & 0x3F) % prg_bank_count;
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

// Original version for PPUADDR-triggered calls (with scanline/cycle parameters)
void Mapper004::clock_counter_on_a12(bool a12, uint16_t addr, int scanline, int cycle) {
    (void)addr;
    constexpr uint32_t FILTER_THRESHOLD = 16;

    uint32_t frame_cycle;
    if (scanline < 0 || cycle < 0) {
        frame_cycle = m_last_a12_cycle + FILTER_THRESHOLD + 1;
    } else {
        frame_cycle = static_cast<uint32_t>(scanline * 341 + cycle);
    }

    clock_counter_on_a12_fast(a12, frame_cycle);
}

// Optimized version - called from notify_ppu_address_bus when A12 changes
void Mapper004::clock_counter_on_a12_fast(bool a12, uint32_t frame_cycle) {
    constexpr uint32_t FILTER_THRESHOLD = 16;

    if (!a12) {
        // A12 falling edge - track when it started being low
        if (m_last_a12) {
            m_last_a12_cycle = frame_cycle;
        }
    } else if (!m_last_a12) {
        // A12 rising edge - check if filter is satisfied
        uint32_t cycles_low = (frame_cycle >= m_last_a12_cycle) ?
                              (frame_cycle - m_last_a12_cycle) :
                              (frame_cycle + 89342 - m_last_a12_cycle);

        if (cycles_low >= FILTER_THRESHOLD) {
            // Filter satisfied - clock the scanline counter
            if (m_irq_counter == 0 || m_irq_reload) {
                m_irq_counter = m_irq_latch;
                m_irq_reload = false;
            } else {
                m_irq_counter--;
            }

            if (m_irq_counter == 0 && m_irq_enabled) {
                if (m_irq_pending_at_cycle == 0 && !m_irq_pending) {
                    m_irq_pending_at_cycle = frame_cycle;
                }
            }
        }
    }
    m_last_a12 = a12;
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
                m_irq_pending_at_cycle = 0;
            } else {
                m_irq_enabled = true;
            }
        }
    }
}

uint8_t Mapper004::ppu_read(uint16_t address, uint32_t frame_cycle) {
    (void)frame_cycle;

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

bool Mapper004::irq_pending(uint32_t frame_cycle) {
    if (m_irq_pending) {
        return true;
    }

    // Check if delayed IRQ should fire now
    if (m_irq_pending_at_cycle > 0 && m_irq_enabled) {
        uint32_t cycles_since_trigger;
        if (frame_cycle >= m_irq_pending_at_cycle) {
            cycles_since_trigger = frame_cycle - m_irq_pending_at_cycle;
        } else {
            cycles_since_trigger = frame_cycle + 89342 - m_irq_pending_at_cycle;
        }

        if (cycles_since_trigger >= IRQ_DELAY_CYCLES) {
            m_irq_pending = true;
            m_irq_pending_at_cycle = 0;
            return true;
        }
    }

    return false;
}

void Mapper004::scanline() {
    // Scanline-based clocking is now handled by A12 detection in ppu_read()
    // This function is kept as a fallback but currently does nothing
    // The A12-based clocking provides more accurate timing for most games
}

void Mapper004::notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr) {
    (void)old_addr;

    // Only consider addresses in the CHR range ($0000-$1FFF) for A12 clocking
    uint16_t masked_addr = new_addr & 0x3FFF;
    if (masked_addr >= 0x2000) {
        return;
    }

    bool new_a12 = (new_addr & 0x1000) != 0;
    clock_counter_on_a12(new_a12, new_addr);
}

void Mapper004::notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle) {
    bool a12 = (address & 0x1000) != 0;

    // Fast path: if A12 hasn't changed, nothing to do except possibly updating fall time
    if (a12 == m_last_a12) {
        return;
    }

    // A12 changed - need to process
    m_current_frame_cycle = frame_cycle;
    clock_counter_on_a12_fast(a12, frame_cycle);
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
