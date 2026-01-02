#include "mapper_024.hpp"

namespace nes {

Mapper024::Mapper024(std::vector<uint8_t>& prg_rom,
                     std::vector<uint8_t>& chr_rom,
                     std::vector<uint8_t>& prg_ram,
                     MirrorMode mirror,
                     bool has_chr_ram,
                     bool is_vrc6b)
{
    m_prg_rom = &prg_rom;
    m_chr_rom = &chr_rom;
    m_prg_ram = &prg_ram;
    m_mirror_mode = mirror;
    m_has_chr_ram = has_chr_ram;
    m_is_vrc6b = is_vrc6b;

    reset();
}

void Mapper024::reset() {
    m_prg_bank_16k = 0;
    m_prg_bank_8k = 0;

    for (int i = 0; i < 8; i++) {
        m_chr_bank[i] = i;
    }

    m_irq_latch = 0;
    m_irq_counter = 0;
    m_irq_enabled = false;
    m_irq_enabled_after_ack = false;
    m_irq_pending = false;
    m_irq_mode_cycle = false;
    m_irq_prescaler = 0;

    // Audio registers
    for (int i = 0; i < 4; i++) {
        m_pulse1_regs[i] = 0;
        m_pulse2_regs[i] = 0;
    }
    for (int i = 0; i < 3; i++) {
        m_saw_regs[i] = 0;
    }
    m_audio_halt = 0;

    // Audio channel state
    m_vrc6_pulse[0] = VRC6Pulse{};
    m_vrc6_pulse[1] = VRC6Pulse{};
    m_vrc6_saw = VRC6Saw{};
    m_audio_output = 0.0f;

    update_prg_banks();
    update_chr_banks();
}

uint16_t Mapper024::translate_address(uint16_t address) {
    // VRC6a (mapper 24): A0 and A1 directly used
    // VRC6b (mapper 26): A0 and A1 swapped
    if (m_is_vrc6b) {
        uint16_t a0 = (address >> 0) & 1;
        uint16_t a1 = (address >> 1) & 1;
        return (address & 0xFFFC) | (a0 << 1) | (a1 << 0);
    }
    return address;
}

void Mapper024::update_prg_banks() {
    size_t prg_size = m_prg_rom->size();
    size_t num_16k_banks = prg_size / 0x4000;
    size_t num_8k_banks = prg_size / 0x2000;
    if (num_16k_banks == 0) num_16k_banks = 1;
    if (num_8k_banks == 0) num_8k_banks = 1;

    m_prg_bank_16k_offset = (m_prg_bank_16k % num_16k_banks) * 0x4000;
    m_prg_bank_8k_offset = (m_prg_bank_8k % num_8k_banks) * 0x2000;
    m_prg_fixed_offset = (num_8k_banks - 1) * 0x2000;  // Last 8KB bank
}

void Mapper024::update_chr_banks() {
    if (m_chr_rom->empty()) return;

    size_t chr_size = m_chr_rom->size();
    size_t num_1k_banks = chr_size / 0x400;
    if (num_1k_banks == 0) num_1k_banks = 1;

    for (int i = 0; i < 8; i++) {
        m_chr_bank_offset[i] = (m_chr_bank[i] % num_1k_banks) * 0x400;
    }
}

uint8_t Mapper024::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

    // PRG ROM: $8000-$BFFF (16KB switchable)
    if (address >= 0x8000 && address < 0xC000) {
        uint32_t offset = m_prg_bank_16k_offset + (address & 0x3FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    // PRG ROM: $C000-$DFFF (8KB switchable)
    if (address >= 0xC000 && address < 0xE000) {
        uint32_t offset = m_prg_bank_8k_offset + (address & 0x1FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    // PRG ROM: $E000-$FFFF (8KB fixed to last bank)
    if (address >= 0xE000) {
        uint32_t offset = m_prg_fixed_offset + (address & 0x1FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
        return 0;
    }

    return 0;
}

void Mapper024::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // Translate address for VRC6b
    uint16_t addr = translate_address(address);

    // PRG bank 0: $8000-$8003 (16KB at $8000-$BFFF)
    if ((addr & 0xF003) == 0x8000) {
        m_prg_bank_16k = value & 0x0F;
        update_prg_banks();
        return;
    }

    // Pulse 1 registers: $9000-$9002
    if ((addr & 0xF003) == 0x9000) {
        m_pulse1_regs[0] = value;
        // $9000: duty (bits 4-6), mode (bit 7), volume (bits 0-3)
        m_vrc6_pulse[0].duty = (value >> 4) & 0x07;
        m_vrc6_pulse[0].volume = value & 0x0F;
        // Bit 7 set means duty cycle ignored, constant high output
        if (value & 0x80) {
            m_vrc6_pulse[0].duty = 8;  // Special value for constant high
        }
        return;
    }
    if ((addr & 0xF003) == 0x9001) {
        m_pulse1_regs[1] = value;
        // $9001: period low 8 bits
        m_vrc6_pulse[0].period = (m_vrc6_pulse[0].period & 0xF00) | value;
        return;
    }
    if ((addr & 0xF003) == 0x9002) {
        m_pulse1_regs[2] = value;
        // $9002: period high 4 bits (bits 0-3), enable (bit 7)
        m_vrc6_pulse[0].period = (m_vrc6_pulse[0].period & 0x0FF) | ((value & 0x0F) << 8);
        m_vrc6_pulse[0].enabled = (value & 0x80) != 0;
        return;
    }
    if ((addr & 0xF003) == 0x9003) {
        m_pulse1_regs[3] = value;
        // $9003: audio halt (bit 0), frequency shift (bits 0-3)
        m_audio_halt = value;
        return;
    }

    // Pulse 2 registers: $A000-$A002
    if ((addr & 0xF003) == 0xA000) {
        m_pulse2_regs[0] = value;
        m_vrc6_pulse[1].duty = (value >> 4) & 0x07;
        m_vrc6_pulse[1].volume = value & 0x0F;
        if (value & 0x80) {
            m_vrc6_pulse[1].duty = 8;
        }
        return;
    }
    if ((addr & 0xF003) == 0xA001) {
        m_pulse2_regs[1] = value;
        m_vrc6_pulse[1].period = (m_vrc6_pulse[1].period & 0xF00) | value;
        return;
    }
    if ((addr & 0xF003) == 0xA002) {
        m_pulse2_regs[2] = value;
        m_vrc6_pulse[1].period = (m_vrc6_pulse[1].period & 0x0FF) | ((value & 0x0F) << 8);
        m_vrc6_pulse[1].enabled = (value & 0x80) != 0;
        return;
    }

    // Sawtooth registers: $B000-$B002
    if ((addr & 0xF003) == 0xB000) {
        m_saw_regs[0] = value;
        // $B000: accumulator rate (bits 0-5)
        m_vrc6_saw.rate = value & 0x3F;
        return;
    }
    if ((addr & 0xF003) == 0xB001) {
        m_saw_regs[1] = value;
        // $B001: period low 8 bits
        m_vrc6_saw.period = (m_vrc6_saw.period & 0xF00) | value;
        return;
    }
    if ((addr & 0xF003) == 0xB002) {
        m_saw_regs[2] = value;
        // $B002: period high 4 bits (bits 0-3), enable (bit 7)
        m_vrc6_saw.period = (m_vrc6_saw.period & 0x0FF) | ((value & 0x0F) << 8);
        m_vrc6_saw.enabled = (value & 0x80) != 0;
        return;
    }

    // PRG bank 1: $C000-$C003 (8KB at $C000-$DFFF)
    if ((addr & 0xF003) == 0xC000) {
        m_prg_bank_8k = value & 0x1F;
        update_prg_banks();
        return;
    }

    // CHR banks: $D000-$E003
    if (addr >= 0xD000 && addr < 0xF000) {
        int reg = ((addr & 0xF000) == 0xD000) ? 0 : 4;
        reg += (addr & 0x0003);
        if (reg < 8) {
            m_chr_bank[reg] = value;
            update_chr_banks();
        }
        return;
    }

    // Mirroring: $F000
    if ((addr & 0xF003) == 0xF000) {
        // Bits 2-3: Mirroring mode
        switch ((value >> 2) & 0x03) {
            case 0: m_mirror_mode = MirrorMode::Vertical; break;
            case 1: m_mirror_mode = MirrorMode::Horizontal; break;
            case 2: m_mirror_mode = MirrorMode::SingleScreen0; break;
            case 3: m_mirror_mode = MirrorMode::SingleScreen1; break;
        }
        return;
    }

    // IRQ latch: $F001
    if ((addr & 0xF003) == 0xF001) {
        m_irq_latch = value;
        return;
    }

    // IRQ control: $F002
    if ((addr & 0xF003) == 0xF002) {
        m_irq_enabled_after_ack = (value & 0x01) != 0;
        m_irq_enabled = (value & 0x02) != 0;
        m_irq_mode_cycle = (value & 0x04) != 0;

        if (m_irq_enabled) {
            m_irq_counter = m_irq_latch;
            m_irq_prescaler = 0;
        }

        m_irq_pending = false;
        return;
    }

    // IRQ acknowledge: $F003
    if ((addr & 0xF003) == 0xF003) {
        m_irq_pending = false;
        m_irq_enabled = m_irq_enabled_after_ack;
        return;
    }
}

uint8_t Mapper024::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    if (address < 0x2000) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
    }
    return 0;
}

void Mapper024::ppu_write(uint16_t address, uint8_t value) {
    if (address < 0x2000 && m_has_chr_ram) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
    }
}

void Mapper024::scanline() {
    // Scanline mode IRQ (when not in cycle mode)
    if (!m_irq_mode_cycle && m_irq_enabled) {
        if (m_irq_counter == 0xFF) {
            m_irq_counter = m_irq_latch;
            m_irq_pending = true;
        } else {
            m_irq_counter++;
        }
    }
}

void Mapper024::cpu_cycles(int count) {
    // PERFORMANCE: Process all cycles at once instead of one at a time
    // This reduces function call overhead from ~90,000 calls to ~30,000 per frame

    int remaining = count;

    // Cycle mode IRQ - batch process prescaler countdown
    if (m_irq_mode_cycle && m_irq_enabled) {
        while (remaining > 0) {
            if (m_irq_prescaler >= static_cast<uint16_t>(remaining)) {
                // Prescaler won't underflow this batch
                m_irq_prescaler -= remaining;
                remaining = 0;
            } else {
                // Prescaler will underflow - clock IRQ counter
                remaining -= (m_irq_prescaler + 1);
                m_irq_prescaler = 340;  // ~1 scanline

                if (m_irq_counter == 0xFF) {
                    m_irq_counter = m_irq_latch;
                    m_irq_pending = true;
                } else {
                    m_irq_counter++;
                }
            }
        }
    }

    // Audio divider - batch update
    // Add cycles to divider and call clock_audio for each period elapsed
    m_audio_divider += count;
    while (m_audio_divider >= AUDIO_DIVIDER_PERIOD) {
        m_audio_divider -= AUDIO_DIVIDER_PERIOD;
        clock_audio();
    }
}

void Mapper024::cpu_cycle() {
    // Single-cycle version for compatibility - delegates to batched version
    cpu_cycles(1);
}

void Mapper024::clock_audio() {
    // Check if audio is halted
    if (m_audio_halt & 0x01) {
        m_audio_output = 0.0f;
        return;
    }

    int mix = 0;

    // Process pulse channels with batched timer updates
    for (int p = 0; p < 2; p++) {
        VRC6Pulse& pulse = m_vrc6_pulse[p];

        if (!pulse.enabled || pulse.period < 1) {
            continue;
        }

        // Batch update timer by AUDIO_DIVIDER_PERIOD cycles
        // This is mathematically equivalent to clocking AUDIO_DIVIDER_PERIOD times
        uint16_t period_plus_1 = pulse.period + 1;
        if (pulse.timer >= AUDIO_DIVIDER_PERIOD) {
            pulse.timer -= AUDIO_DIVIDER_PERIOD;
        } else {
            // Timer will underflow - calculate new position
            uint16_t remaining = AUDIO_DIVIDER_PERIOD - pulse.timer - 1;
            uint16_t full_periods = remaining / period_plus_1;
            uint16_t leftover = remaining % period_plus_1;
            pulse.sequence_pos = (pulse.sequence_pos + 1 + full_periods) & 0x0F;
            pulse.timer = pulse.period - leftover;
        }

        // Calculate output based on duty cycle
        bool output_high;
        if (pulse.duty == 8) {
            output_high = true;  // Constant high mode
        } else {
            output_high = (pulse.sequence_pos <= pulse.duty);
        }

        if (output_high) {
            mix += pulse.volume;
        }
    }

    // Process sawtooth channel with batched timer updates
    {
        VRC6Saw& saw = m_vrc6_saw;

        if (saw.enabled && saw.period >= 1) {
            uint16_t period_plus_1 = saw.period + 1;
            if (saw.timer >= AUDIO_DIVIDER_PERIOD) {
                saw.timer -= AUDIO_DIVIDER_PERIOD;
            } else {
                // Timer will underflow - advance through steps
                uint16_t remaining = AUDIO_DIVIDER_PERIOD - saw.timer - 1;
                uint16_t full_periods = remaining / period_plus_1;
                uint16_t leftover = remaining % period_plus_1;

                // Advance steps
                for (uint16_t i = 0; i <= full_periods; i++) {
                    saw.step++;
                    if (saw.step >= 14) {
                        saw.step = 0;
                        saw.accumulator = 0;
                    } else if ((saw.step & 1) == 0) {
                        saw.accumulator += saw.rate;
                    }
                }
                saw.timer = saw.period - leftover;
            }

            // Output is the top 5 bits of the accumulator
            mix += (saw.accumulator >> 3) & 0x1F;
        }
    }

    // Normalize output to -1.0 to 1.0 range
    // Max output: 2 pulses * 15 + saw * 31 = 30 + 31 = 61
    m_audio_output = (mix / 61.0f - 0.5f) * 2.0f;
}

void Mapper024::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_prg_bank_16k);
    data.push_back(m_prg_bank_8k);

    for (int i = 0; i < 8; i++) {
        data.push_back(m_chr_bank[i]);
    }

    data.push_back(m_irq_latch);
    data.push_back(m_irq_counter);
    data.push_back(m_irq_enabled ? 1 : 0);
    data.push_back(m_irq_enabled_after_ack ? 1 : 0);
    data.push_back(m_irq_pending ? 1 : 0);
    data.push_back(m_irq_mode_cycle ? 1 : 0);
    data.push_back(m_irq_prescaler);

    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void Mapper024::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 18) return;

    m_prg_bank_16k = *data++; remaining--;
    m_prg_bank_8k = *data++; remaining--;

    for (int i = 0; i < 8; i++) {
        m_chr_bank[i] = *data++; remaining--;
    }

    m_irq_latch = *data++; remaining--;
    m_irq_counter = *data++; remaining--;
    m_irq_enabled = (*data++ != 0); remaining--;
    m_irq_enabled_after_ack = (*data++ != 0); remaining--;
    m_irq_pending = (*data++ != 0); remaining--;
    m_irq_mode_cycle = (*data++ != 0); remaining--;
    m_irq_prescaler = *data++; remaining--;

    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    update_prg_banks();
    update_chr_banks();
}

} // namespace nes
