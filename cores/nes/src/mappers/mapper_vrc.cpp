#include "mapper_vrc.hpp"

namespace nes {

MapperVRC::MapperVRC(std::vector<uint8_t>& prg_rom,
                     std::vector<uint8_t>& chr_rom,
                     std::vector<uint8_t>& prg_ram,
                     MirrorMode mirror,
                     bool has_chr_ram,
                     Variant variant)
{
    m_prg_rom = &prg_rom;
    m_chr_rom = &chr_rom;
    m_prg_ram = &prg_ram;
    m_mirror_mode = mirror;
    m_has_chr_ram = has_chr_ram;
    m_variant = variant;

    // VRC4 variants have IRQ support
    m_is_vrc4 = (variant == Variant::VRC4a || variant == Variant::VRC4b ||
                 variant == Variant::VRC4c || variant == Variant::VRC4d ||
                 variant == Variant::VRC4e || variant == Variant::VRC4f);

    reset();
}

void MapperVRC::reset() {
    m_prg_bank_0 = 0;
    m_prg_bank_1 = 0;
    m_prg_swap_mode = false;

    for (int i = 0; i < 8; i++) {
        m_chr_bank_lo[i] = i;
        m_chr_bank_hi[i] = 0;
    }

    m_irq_latch = 0;
    m_irq_counter = 0;
    m_irq_enabled = false;
    m_irq_enabled_after_ack = false;
    m_irq_pending = false;
    m_irq_mode_cycle = false;
    m_irq_prescaler = 0;

    update_prg_banks();
    update_chr_banks();
}

uint16_t MapperVRC::translate_address(uint16_t address) {
    // Different VRC variants use different address line mappings
    // This maps the address to a normalized form: $x000, $x001, $x002, $x003
    uint16_t base = address & 0xF000;
    uint16_t a0 = 0, a1 = 0;

    switch (m_variant) {
        case Variant::VRC2a:  // Mapper 22: A0->A1, A1->A0
            a0 = (address >> 1) & 1;
            a1 = (address >> 0) & 1;
            break;

        case Variant::VRC2b:  // Mapper 23: A0, A1
        case Variant::VRC4e:
        case Variant::VRC4f:
            a0 = (address >> 0) & 1;
            a1 = (address >> 1) & 1;
            break;

        case Variant::VRC2c:  // Mapper 25: A0->A1, A1->A0
        case Variant::VRC4b:
        case Variant::VRC4d:
            a0 = (address >> 1) & 1;
            a1 = (address >> 0) & 1;
            break;

        case Variant::VRC4a:  // Mapper 21: A1->A6, A0->A2
            a0 = (address >> 1) & 1;
            a1 = (address >> 2) & 1;
            break;

        case Variant::VRC4c:  // Mapper 21: A0->A6, A1->A7
            a0 = (address >> 6) & 1;
            a1 = (address >> 7) & 1;
            break;
    }

    return base | (a1 << 1) | a0;
}

void MapperVRC::update_prg_banks() {
    size_t prg_size = m_prg_rom->size();
    size_t num_8k_banks = prg_size / 0x2000;
    if (num_8k_banks == 0) num_8k_banks = 1;

    uint8_t bank0 = m_prg_bank_0 % num_8k_banks;
    uint8_t bank1 = m_prg_bank_1 % num_8k_banks;
    uint8_t second_last = (num_8k_banks - 2) % num_8k_banks;
    uint8_t last = (num_8k_banks - 1) % num_8k_banks;

    if (m_prg_swap_mode) {
        // $C000 swappable mode
        m_prg_bank_offset[0] = second_last * 0x2000;  // $8000
        m_prg_bank_offset[1] = bank1 * 0x2000;        // $A000
        m_prg_bank_offset[2] = bank0 * 0x2000;        // $C000
        m_prg_bank_offset[3] = last * 0x2000;         // $E000
    } else {
        // $8000 swappable mode (default)
        m_prg_bank_offset[0] = bank0 * 0x2000;        // $8000
        m_prg_bank_offset[1] = bank1 * 0x2000;        // $A000
        m_prg_bank_offset[2] = second_last * 0x2000;  // $C000
        m_prg_bank_offset[3] = last * 0x2000;         // $E000
    }
}

void MapperVRC::update_chr_banks() {
    if (m_chr_rom->empty()) return;

    size_t chr_size = m_chr_rom->size();
    size_t num_1k_banks = chr_size / 0x400;
    if (num_1k_banks == 0) num_1k_banks = 1;

    for (int i = 0; i < 8; i++) {
        uint8_t bank;
        if (m_is_vrc4) {
            // VRC4: 8-bit bank number from high and low nibbles
            bank = m_chr_bank_lo[i] | (m_chr_bank_hi[i] << 4);
        } else {
            // VRC2: 8-bit bank number, but CHR is shifted right by 1
            // (effectively 7-bit with LSB ignored in VRC2a)
            if (m_variant == Variant::VRC2a) {
                bank = m_chr_bank_lo[i] >> 1;
            } else {
                bank = m_chr_bank_lo[i];
            }
        }
        m_chr_bank_offset[i] = (bank % num_1k_banks) * 0x400;
    }
}

void MapperVRC::write_chr_bank(int bank, uint8_t value, bool high_nibble) {
    if (bank < 0 || bank >= 8) return;

    if (high_nibble) {
        m_chr_bank_hi[bank] = value & 0x0F;
    } else {
        m_chr_bank_lo[bank] = value;
    }
    update_chr_banks();
}

uint8_t MapperVRC::cpu_read(uint16_t address) {
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
        uint32_t offset = m_prg_bank_offset[bank] + (address & 0x1FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
    }

    return 0;
}

void MapperVRC::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // Translate address based on variant
    uint16_t addr = translate_address(address);

    // PRG bank 0: $8000-$8003
    if ((addr & 0xF003) == 0x8000 || (addr & 0xF003) == 0x8001 ||
        (addr & 0xF003) == 0x8002 || (addr & 0xF003) == 0x8003) {
        m_prg_bank_0 = value & 0x1F;
        update_prg_banks();
        return;
    }

    // Mirroring / Swap mode: $9000-$9003
    if ((addr & 0xF000) == 0x9000) {
        switch (addr & 0x0003) {
            case 0:
            case 1:
                // Mirroring
                switch (value & 0x03) {
                    case 0: m_mirror_mode = MirrorMode::Vertical; break;
                    case 1: m_mirror_mode = MirrorMode::Horizontal; break;
                    case 2: m_mirror_mode = MirrorMode::SingleScreen0; break;
                    case 3: m_mirror_mode = MirrorMode::SingleScreen1; break;
                }
                break;
            case 2:
            case 3:
                // PRG swap mode (VRC4 only)
                if (m_is_vrc4) {
                    m_prg_swap_mode = (value & 0x02) != 0;
                    update_prg_banks();
                }
                break;
        }
        return;
    }

    // PRG bank 1: $A000-$A003
    if ((addr & 0xF003) == 0xA000 || (addr & 0xF003) == 0xA001 ||
        (addr & 0xF003) == 0xA002 || (addr & 0xF003) == 0xA003) {
        m_prg_bank_1 = value & 0x1F;
        update_prg_banks();
        return;
    }

    // CHR banks: $B000-$E003
    if (addr >= 0xB000 && addr < 0xF000) {
        // Calculate which CHR bank register and whether it's high or low nibble
        int base_bank = ((addr & 0xF000) - 0xB000) / 0x1000 * 2;
        int sub_reg = (addr & 0x0003);

        int chr_bank = base_bank + (sub_reg / 2);
        bool high_nibble = (sub_reg & 1) != 0;

        if (m_is_vrc4) {
            write_chr_bank(chr_bank, value, high_nibble);
        } else {
            // VRC2 uses full byte writes
            if (!high_nibble) {
                m_chr_bank_lo[chr_bank] = value;
                update_chr_banks();
            }
        }
        return;
    }

    // IRQ registers (VRC4 only): $F000-$F003
    if (m_is_vrc4 && (addr & 0xF000) == 0xF000) {
        switch (addr & 0x0003) {
            case 0:
                // IRQ latch low nibble
                m_irq_latch = (m_irq_latch & 0xF0) | (value & 0x0F);
                break;
            case 1:
                // IRQ latch high nibble
                m_irq_latch = (m_irq_latch & 0x0F) | ((value & 0x0F) << 4);
                break;
            case 2:
                // IRQ control
                m_irq_enabled_after_ack = (value & 0x01) != 0;
                m_irq_enabled = (value & 0x02) != 0;
                m_irq_mode_cycle = (value & 0x04) != 0;

                if (m_irq_enabled) {
                    m_irq_counter = m_irq_latch;
                    m_irq_prescaler = 0;
                }

                m_irq_pending = false;
                break;
            case 3:
                // IRQ acknowledge
                m_irq_pending = false;
                m_irq_enabled = m_irq_enabled_after_ack;
                break;
        }
        return;
    }
}

uint8_t MapperVRC::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    if (address < 0x2000) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
    }
    return 0;
}

void MapperVRC::ppu_write(uint16_t address, uint8_t value) {
    if (address < 0x2000 && m_has_chr_ram) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
    }
}

void MapperVRC::scanline() {
    // Scanline mode IRQ (when not in cycle mode)
    if (m_is_vrc4 && !m_irq_mode_cycle && m_irq_enabled) {
        if (m_irq_counter == 0xFF) {
            m_irq_counter = m_irq_latch;
            m_irq_pending = true;
        } else {
            m_irq_counter++;
        }
    }
}

void MapperVRC::cpu_cycles(int count) {
    // PERFORMANCE: Process all cycles at once instead of one at a time

    // Cycle mode IRQ (VRC4 only) - batch process prescaler countdown
    if (m_is_vrc4 && m_irq_mode_cycle && m_irq_enabled) {
        int remaining = count;
        while (remaining > 0) {
            if (m_irq_prescaler >= static_cast<uint16_t>(remaining)) {
                // Prescaler won't underflow this batch
                m_irq_prescaler -= remaining;
                remaining = 0;
            } else {
                // Prescaler will underflow - clock IRQ counter
                remaining -= (m_irq_prescaler + 1);
                m_irq_prescaler = IRQ_PRESCALER_RELOAD - 1;

                if (m_irq_counter == 0xFF) {
                    m_irq_counter = m_irq_latch;
                    m_irq_pending = true;
                } else {
                    m_irq_counter++;
                }
            }
        }
    }
}

void MapperVRC::cpu_cycle() {
    // Single-cycle version for compatibility - delegates to batched version
    cpu_cycles(1);
}

void MapperVRC::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_prg_bank_0);
    data.push_back(m_prg_bank_1);
    data.push_back(m_prg_swap_mode ? 1 : 0);

    for (int i = 0; i < 8; i++) {
        data.push_back(m_chr_bank_lo[i]);
        data.push_back(m_chr_bank_hi[i]);
    }

    data.push_back(m_irq_latch);
    data.push_back(m_irq_counter);
    data.push_back(m_irq_enabled ? 1 : 0);
    data.push_back(m_irq_enabled_after_ack ? 1 : 0);
    data.push_back(m_irq_pending ? 1 : 0);
    data.push_back(m_irq_mode_cycle ? 1 : 0);
    data.push_back(static_cast<uint8_t>(m_irq_prescaler & 0xFF));
    data.push_back(static_cast<uint8_t>((m_irq_prescaler >> 8) & 0xFF));

    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void MapperVRC::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 28) return;

    m_prg_bank_0 = *data++; remaining--;
    m_prg_bank_1 = *data++; remaining--;
    m_prg_swap_mode = (*data++ != 0); remaining--;

    for (int i = 0; i < 8; i++) {
        m_chr_bank_lo[i] = *data++; remaining--;
        m_chr_bank_hi[i] = *data++; remaining--;
    }

    m_irq_latch = *data++; remaining--;
    m_irq_counter = *data++; remaining--;
    m_irq_enabled = (*data++ != 0); remaining--;
    m_irq_enabled_after_ack = (*data++ != 0); remaining--;
    m_irq_pending = (*data++ != 0); remaining--;
    m_irq_mode_cycle = (*data++ != 0); remaining--;

    uint8_t prescaler_lo = *data++; remaining--;
    uint8_t prescaler_hi = *data++; remaining--;
    m_irq_prescaler = prescaler_lo | (static_cast<uint16_t>(prescaler_hi) << 8);

    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    update_prg_banks();
    update_chr_banks();
}

} // namespace nes
