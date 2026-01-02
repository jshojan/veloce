#include "mapper_069.hpp"

namespace nes {

Mapper069::Mapper069(std::vector<uint8_t>& prg_rom,
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

void Mapper069::reset() {
    m_command = 0;

    // Initialize PRG banks - last bank fixed at $E000
    for (int i = 0; i < 4; i++) {
        m_prg_bank[i] = 0;
    }

    // PRG RAM disabled by default
    m_prg_ram_enabled = false;
    m_prg_ram_select = false;
    m_prg_ram_bank = 0;

    // Initialize CHR banks
    for (int i = 0; i < 8; i++) {
        m_chr_bank[i] = i;
    }

    // IRQ disabled
    m_irq_enabled = false;
    m_irq_counter_enabled = false;
    m_irq_pending = false;
    m_irq_counter = 0;

    // Audio registers
    m_audio_command = 0;
    for (int i = 0; i < 16; i++) {
        m_audio_registers[i] = 0;
    }

    // Reset audio state
    for (int i = 0; i < 3; i++) {
        m_ss_channels[i] = SunsoftChannel{};
    }
    m_noise_period = 0;
    m_noise_timer = 0;
    m_noise_shift = 1;
    m_noise_output = false;
    m_env_period = 0;
    m_env_timer = 0;
    m_env_shape = 0;
    m_env_volume = 0;
    m_env_holding = false;
    m_env_attack = false;
    m_env_alternate = false;
    m_env_hold = false;
    m_audio_divider = 0;
    m_audio_output = 0.0f;

    update_prg_banks();
    update_chr_banks();
}

void Mapper069::update_prg_banks() {
    size_t prg_size = m_prg_rom->size();
    size_t num_8k_banks = prg_size / 0x2000;
    if (num_8k_banks == 0) num_8k_banks = 1;

    // Banks 0-2 are switchable ($8000-$DFFF)
    for (int i = 0; i < 3; i++) {
        m_prg_bank_offset[i] = (m_prg_bank[i] % num_8k_banks) * 0x2000;
    }

    // Bank 3 is fixed to last bank ($E000-$FFFF)
    m_prg_bank_offset[3] = (num_8k_banks - 1) * 0x2000;
}

void Mapper069::update_chr_banks() {
    if (m_chr_rom->empty()) return;

    size_t chr_size = m_chr_rom->size();
    size_t num_1k_banks = chr_size / 0x400;
    if (num_1k_banks == 0) num_1k_banks = 1;

    for (int i = 0; i < 8; i++) {
        m_chr_bank_offset[i] = (m_chr_bank[i] % num_1k_banks) * 0x400;
    }
}

void Mapper069::write_register(uint8_t value) {
    switch (m_command) {
        case 0x0: case 0x1: case 0x2: case 0x3:
        case 0x4: case 0x5: case 0x6: case 0x7:
            // CHR bank select (commands 0-7)
            m_chr_bank[m_command] = value;
            update_chr_banks();
            break;

        case 0x8:
            // PRG bank 0 / RAM control at $6000-$7FFF
            // Bit 6: RAM enable (1 = enabled)
            // Bit 7: RAM/ROM select (0 = ROM, 1 = RAM)
            // Bits 0-5: Bank number
            m_prg_ram_enabled = (value & 0x80) != 0;
            m_prg_ram_select = (value & 0x40) != 0;
            m_prg_ram_bank = value & 0x3F;
            break;

        case 0x9:
            // PRG bank at $8000-$9FFF
            m_prg_bank[0] = value & 0x3F;
            update_prg_banks();
            break;

        case 0xA:
            // PRG bank at $A000-$BFFF
            m_prg_bank[1] = value & 0x3F;
            update_prg_banks();
            break;

        case 0xB:
            // PRG bank at $C000-$DFFF
            m_prg_bank[2] = value & 0x3F;
            update_prg_banks();
            break;

        case 0xC:
            // Mirroring
            switch (value & 0x03) {
                case 0: m_mirror_mode = MirrorMode::Vertical; break;
                case 1: m_mirror_mode = MirrorMode::Horizontal; break;
                case 2: m_mirror_mode = MirrorMode::SingleScreen0; break;
                case 3: m_mirror_mode = MirrorMode::SingleScreen1; break;
            }
            break;

        case 0xD:
            // IRQ control
            m_irq_enabled = (value & 0x01) != 0;
            m_irq_counter_enabled = (value & 0x80) != 0;
            // Writing to IRQ control acknowledges the IRQ
            m_irq_pending = false;
            break;

        case 0xE:
            // IRQ counter low byte
            m_irq_counter = (m_irq_counter & 0xFF00) | value;
            break;

        case 0xF:
            // IRQ counter high byte
            m_irq_counter = (m_irq_counter & 0x00FF) | (static_cast<uint16_t>(value) << 8);
            break;
    }
}

uint8_t Mapper069::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (m_prg_ram_select && m_prg_ram_enabled) {
            // RAM mode
            if (!m_prg_ram->empty()) {
                // Support up to 32KB PRG RAM (4 banks)
                size_t offset = (m_prg_ram_bank & 0x03) * 0x2000 + (address & 0x1FFF);
                if (offset < m_prg_ram->size()) {
                    return (*m_prg_ram)[offset];
                }
            }
        } else if (!m_prg_ram_select) {
            // ROM mode at $6000-$7FFF
            size_t prg_size = m_prg_rom->size();
            size_t num_8k_banks = prg_size / 0x2000;
            if (num_8k_banks > 0) {
                size_t offset = (m_prg_ram_bank % num_8k_banks) * 0x2000 + (address & 0x1FFF);
                if (offset < prg_size) {
                    return (*m_prg_rom)[offset];
                }
            }
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

void Mapper069::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (m_prg_ram_select && m_prg_ram_enabled) {
            if (!m_prg_ram->empty()) {
                size_t offset = (m_prg_ram_bank & 0x03) * 0x2000 + (address & 0x1FFF);
                if (offset < m_prg_ram->size()) {
                    (*m_prg_ram)[offset] = value;
                }
            }
        }
        return;
    }

    // Command register: $8000-$9FFF
    if (address >= 0x8000 && address < 0xA000) {
        m_command = value & 0x0F;
        return;
    }

    // Parameter register: $A000-$BFFF
    if (address >= 0xA000 && address < 0xC000) {
        write_register(value);
        return;
    }

    // Audio command: $C000-$DFFF
    if (address >= 0xC000 && address < 0xE000) {
        m_audio_command = value & 0x0F;
        return;
    }

    // Audio parameter: $E000-$FFFF
    if (address >= 0xE000) {
        if (m_audio_command < 16) {
            m_audio_registers[m_audio_command] = value;

            // Update audio state based on register
            switch (m_audio_command) {
                case 0: case 2: case 4:
                    // Channel period low byte
                    {
                        int ch = m_audio_command / 2;
                        m_ss_channels[ch].period = (m_ss_channels[ch].period & 0xF00) | value;
                    }
                    break;
                case 1: case 3: case 5:
                    // Channel period high byte (4 bits)
                    {
                        int ch = m_audio_command / 2;
                        m_ss_channels[ch].period = (m_ss_channels[ch].period & 0x0FF) | ((value & 0x0F) << 8);
                    }
                    break;
                case 6:
                    // Noise period (5 bits)
                    m_noise_period = value & 0x1F;
                    break;
                case 7:
                    // Mixer control - bits 0-2: tone enable, bits 3-5: noise enable
                    m_ss_channels[0].tone_enabled = !(value & 0x01);
                    m_ss_channels[1].tone_enabled = !(value & 0x02);
                    m_ss_channels[2].tone_enabled = !(value & 0x04);
                    m_ss_channels[0].noise_enabled = !(value & 0x08);
                    m_ss_channels[1].noise_enabled = !(value & 0x10);
                    m_ss_channels[2].noise_enabled = !(value & 0x20);
                    break;
                case 8: case 9: case 10:
                    // Channel volume (4 bits) or envelope mode (bit 4)
                    {
                        int ch = m_audio_command - 8;
                        m_ss_channels[ch].volume = value & 0x0F;
                        // Bit 4 set means use envelope instead of fixed volume
                        // (simplified - we just use the fixed volume for now)
                    }
                    break;
                case 11:
                    // Envelope period low byte
                    m_env_period = (m_env_period & 0xFF00) | value;
                    break;
                case 12:
                    // Envelope period high byte
                    m_env_period = (m_env_period & 0x00FF) | (static_cast<uint16_t>(value) << 8);
                    break;
                case 13:
                    // Envelope shape
                    m_env_shape = value & 0x0F;
                    m_env_volume = (value & 0x04) ? 0 : 15;  // Attack starts at 0, else at 15
                    m_env_holding = false;
                    m_env_attack = (value & 0x04) != 0;
                    m_env_alternate = (value & 0x02) != 0;
                    m_env_hold = (value & 0x01) != 0;
                    break;
            }
        }
        return;
    }
}

uint8_t Mapper069::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    if (address < 0x2000) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
    }
    return 0;
}

void Mapper069::ppu_write(uint16_t address, uint8_t value) {
    if (address < 0x2000 && m_has_chr_ram) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
    }
}

void Mapper069::cpu_cycles(int count) {
    // PERFORMANCE: Process all cycles at once instead of one at a time

    // IRQ counter decrements every CPU cycle when enabled
    if (m_irq_counter_enabled) {
        // Check if counter will underflow during this batch
        if (static_cast<uint32_t>(count) > m_irq_counter) {
            // Counter will underflow - trigger IRQ if enabled
            if (m_irq_enabled) {
                m_irq_pending = true;
            }
            // Calculate new counter value after wraparound
            uint32_t overflow = count - m_irq_counter - 1;
            m_irq_counter = 0xFFFF - (overflow % 0x10000);
        } else {
            m_irq_counter -= count;
        }
    }

    // Clock audio every 16 CPU cycles (5B runs at CPU/16)
    m_audio_divider += count;
    while (m_audio_divider >= 16) {
        m_audio_divider -= 16;
        clock_audio();
    }
}

void Mapper069::cpu_cycle() {
    // Single-cycle version for compatibility - delegates to batched version
    cpu_cycles(1);
}

void Mapper069::clock_audio() {
    int mix = 0;

    // Clock noise generator
    if (m_noise_timer > 0) {
        m_noise_timer--;
    } else {
        m_noise_timer = m_noise_period;

        // 17-bit LFSR (same as AY-3-8910)
        // Taps at bits 0 and 3
        uint32_t bit = ((m_noise_shift >> 0) ^ (m_noise_shift >> 3)) & 1;
        m_noise_shift = (m_noise_shift >> 1) | (bit << 16);
        m_noise_output = (m_noise_shift & 1) != 0;
    }

    // Clock envelope generator
    if (!m_env_holding) {
        if (m_env_timer > 0) {
            m_env_timer--;
        } else {
            m_env_timer = m_env_period;

            if (m_env_attack) {
                // Attack (count up)
                if (m_env_volume < 15) {
                    m_env_volume++;
                } else {
                    // Reached top
                    if (m_env_hold) {
                        m_env_holding = true;
                    } else if (m_env_alternate) {
                        m_env_attack = false;  // Switch to decay
                    } else {
                        m_env_volume = 0;  // Restart
                    }
                }
            } else {
                // Decay (count down)
                if (m_env_volume > 0) {
                    m_env_volume--;
                } else {
                    // Reached bottom
                    if (m_env_hold) {
                        m_env_holding = true;
                    } else if (m_env_alternate) {
                        m_env_attack = true;  // Switch to attack
                    } else {
                        m_env_volume = 15;  // Restart at top
                    }
                }
            }
        }
    }

    // Clock each channel
    for (int ch = 0; ch < 3; ch++) {
        SunsoftChannel& chan = m_ss_channels[ch];

        // Clock timer
        if (chan.timer > 0) {
            chan.timer--;
        } else {
            chan.timer = chan.period;
            chan.output_high = !chan.output_high;
        }

        // Calculate output
        bool tone_out = chan.tone_enabled ? chan.output_high : true;
        bool noise_out = chan.noise_enabled ? m_noise_output : true;

        if (tone_out && noise_out) {
            // Use envelope volume if bit 4 of volume register is set
            uint8_t vol = (m_audio_registers[8 + ch] & 0x10) ? m_env_volume : chan.volume;
            mix += vol;
        }
    }

    // Normalize output to -1.0 to 1.0 range
    // Max output: 3 channels * 15 = 45
    m_audio_output = (mix / 45.0f - 0.5f) * 2.0f;
}

void Mapper069::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_command);

    for (int i = 0; i < 4; i++) {
        data.push_back(m_prg_bank[i]);
    }

    data.push_back(m_prg_ram_enabled ? 1 : 0);
    data.push_back(m_prg_ram_select ? 1 : 0);
    data.push_back(m_prg_ram_bank);

    for (int i = 0; i < 8; i++) {
        data.push_back(m_chr_bank[i]);
    }

    data.push_back(m_irq_enabled ? 1 : 0);
    data.push_back(m_irq_counter_enabled ? 1 : 0);
    data.push_back(m_irq_pending ? 1 : 0);
    data.push_back(static_cast<uint8_t>(m_irq_counter & 0xFF));
    data.push_back(static_cast<uint8_t>((m_irq_counter >> 8) & 0xFF));

    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void Mapper069::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 22) return;

    m_command = *data++; remaining--;

    for (int i = 0; i < 4; i++) {
        m_prg_bank[i] = *data++; remaining--;
    }

    m_prg_ram_enabled = (*data++ != 0); remaining--;
    m_prg_ram_select = (*data++ != 0); remaining--;
    m_prg_ram_bank = *data++; remaining--;

    for (int i = 0; i < 8; i++) {
        m_chr_bank[i] = *data++; remaining--;
    }

    m_irq_enabled = (*data++ != 0); remaining--;
    m_irq_counter_enabled = (*data++ != 0); remaining--;
    m_irq_pending = (*data++ != 0); remaining--;

    uint8_t irq_lo = *data++; remaining--;
    uint8_t irq_hi = *data++; remaining--;
    m_irq_counter = irq_lo | (static_cast<uint16_t>(irq_hi) << 8);

    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    update_prg_banks();
    update_chr_banks();
}

} // namespace nes
