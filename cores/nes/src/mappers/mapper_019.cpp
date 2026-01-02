#include "mapper_019.hpp"

namespace nes {

// Audio channel register offsets within internal RAM ($40-$7F)
// Each channel uses 8 bytes, channels are numbered 0-7 but stored 7-0
// Channel 7 registers are at $40-$47, channel 6 at $48-$4F, etc.
// Register layout per channel:
//   +0: Frequency low (bits 0-7)
//   +1: Phase low (bits 0-7)
//   +2: Frequency mid (bits 8-15)
//   +3: Phase mid (bits 0-7)
//   +4: Frequency high (bits 16-17) + wave length (bits 2-7)
//   +5: Phase high (bits 0-7)
//   +6: Wave offset
//   +7: Volume (bits 0-3) + channel count for ch7 (bits 4-6)

static constexpr uint8_t CHANNEL_REG_BASE = 0x40;
static constexpr uint8_t CHANNEL_REG_SIZE = 8;

// For channel N (0-7), registers are at: $40 + (7-N)*8
static inline uint8_t channel_reg_addr(int channel) {
    return CHANNEL_REG_BASE + (7 - channel) * CHANNEL_REG_SIZE;
}

Mapper019::Mapper019(std::vector<uint8_t>& prg_rom,
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

void Mapper019::reset() {
    // Initialize PRG banks
    for (int i = 0; i < 3; i++) {
        m_prg_bank[i] = 0;
    }
    m_prg_ram_write_protect = false;

    // Initialize CHR banks (identity mapping)
    for (int i = 0; i < 8; i++) {
        m_chr_bank[i] = i;
    }

    // Initialize nametable banks to use CIRAM
    for (int i = 0; i < 4; i++) {
        m_nt_bank[i] = 0xE0;  // Default to CIRAM
    }

    // Reset IRQ
    m_irq_counter = 0;
    m_irq_enabled = false;
    m_irq_pending = false;

    // Reset internal RAM
    m_internal_ram.fill(0);

    // Reset RAM address
    m_ram_addr = 0;
    m_ram_auto_increment = false;

    // Reset sound
    m_sound_enabled = false;
    m_active_channels = 1;

    // Reset audio channels
    for (auto& ch : m_channels) {
        ch.frequency = 0;
        ch.phase = 0;
        ch.wave_length = 0;
        ch.wave_offset = 0;
        ch.volume = 0;
    }

    m_audio_output = 0.0f;
    m_audio_cycle = 0;
    m_current_channel = 0;

    update_prg_banks();
    update_chr_banks();
}

void Mapper019::update_prg_banks() {
    size_t prg_size = m_prg_rom->size();
    size_t num_8k_banks = prg_size / 0x2000;
    if (num_8k_banks == 0) num_8k_banks = 1;

    // Banks 0-2 are switchable ($8000-$DFFF)
    for (int i = 0; i < 3; i++) {
        m_prg_bank_offset[i] = (m_prg_bank[i] % num_8k_banks) * 0x2000;
    }

    // Bank 3 is fixed to the last bank ($E000-$FFFF)
    m_prg_bank_offset[3] = (num_8k_banks - 1) * 0x2000;
}

void Mapper019::update_chr_banks() {
    if (m_chr_rom->empty()) return;

    size_t chr_size = m_chr_rom->size();
    size_t num_1k_banks = chr_size / 0x400;
    if (num_1k_banks == 0) num_1k_banks = 1;

    for (int i = 0; i < 8; i++) {
        // CHR banks $E0-$FF map to CIRAM, not CHR ROM
        // But we still calculate the offset for banks that use CHR ROM
        if (m_chr_bank[i] < 0xE0) {
            m_chr_bank_offset[i] = (m_chr_bank[i] % num_1k_banks) * 0x400;
        }
        // For CIRAM banks, offset calculation is handled in ppu_read/ppu_write
    }
}

uint8_t Mapper019::read_internal_ram(uint8_t addr) {
    return m_internal_ram[addr & 0x7F];
}

void Mapper019::write_internal_ram(uint8_t addr, uint8_t value) {
    addr &= 0x7F;
    m_internal_ram[addr] = value;

    // Update audio channel state when registers are written
    if (addr >= 0x40) {
        int channel = 7 - ((addr - 0x40) / 8);
        if (channel >= 0 && channel <= 7) {
            int reg = (addr - 0x40) % 8;
            AudioChannel& ch = m_channels[channel];

            switch (reg) {
                case 0: // Frequency low
                    ch.frequency = (ch.frequency & 0x3FF00) | value;
                    break;
                case 1: // Phase low (write-only effect on phase accumulator)
                    ch.phase = (ch.phase & 0xFFFF00) | value;
                    break;
                case 2: // Frequency mid
                    ch.frequency = (ch.frequency & 0x300FF) | (static_cast<uint32_t>(value) << 8);
                    break;
                case 3: // Phase mid
                    ch.phase = (ch.phase & 0xFF00FF) | (static_cast<uint32_t>(value) << 8);
                    break;
                case 4: // Frequency high (bits 0-1) + wave length (bits 2-7)
                    ch.frequency = (ch.frequency & 0x0FFFF) | (static_cast<uint32_t>(value & 0x03) << 16);
                    ch.wave_length = value >> 2;
                    break;
                case 5: // Phase high
                    ch.phase = (ch.phase & 0x00FFFF) | (static_cast<uint32_t>(value) << 16);
                    break;
                case 6: // Wave offset
                    ch.wave_offset = value;
                    break;
                case 7: // Volume + channel count (for channel 7 only)
                    ch.volume = value & 0x0F;
                    if (channel == 7) {
                        // Bits 4-6 determine number of active channels (1-8)
                        m_active_channels = ((value >> 4) & 0x07) + 1;
                    }
                    break;
            }
        }
    }
}

uint8_t Mapper019::cpu_read(uint16_t address) {
    // Internal RAM data port: $4800
    if (address >= 0x4800 && address < 0x5000) {
        uint8_t value = read_internal_ram(m_ram_addr);
        if (m_ram_auto_increment) {
            m_ram_addr = (m_ram_addr + 1) & 0x7F;
        }
        return value;
    }

    // IRQ counter low: $5000-$57FF
    if (address >= 0x5000 && address < 0x5800) {
        return m_irq_counter & 0xFF;
    }

    // IRQ counter high: $5800-$5FFF
    if (address >= 0x5800 && address < 0x6000) {
        // Reading acknowledges IRQ
        m_irq_pending = false;
        return (m_irq_counter >> 8) & 0x7F;
    }

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

void Mapper019::cpu_write(uint16_t address, uint8_t value) {
    // Internal RAM data port: $4800
    if (address >= 0x4800 && address < 0x5000) {
        write_internal_ram(m_ram_addr, value);
        if (m_ram_auto_increment) {
            m_ram_addr = (m_ram_addr + 1) & 0x7F;
        }
        return;
    }

    // IRQ counter low: $5000-$57FF
    if (address >= 0x5000 && address < 0x5800) {
        m_irq_counter = (m_irq_counter & 0xFF00) | value;
        m_irq_pending = false;  // Acknowledge IRQ
        return;
    }

    // IRQ counter high + enable: $5800-$5FFF
    if (address >= 0x5800 && address < 0x6000) {
        m_irq_counter = (m_irq_counter & 0x00FF) | (static_cast<uint16_t>(value & 0x7F) << 8);
        m_irq_enabled = (value & 0x80) != 0;
        m_irq_pending = false;  // Acknowledge IRQ
        return;
    }

    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty() && !m_prg_ram_write_protect) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // CHR bank 0: $8000-$87FF
    if (address >= 0x8000 && address < 0x8800) {
        m_chr_bank[0] = value;
        update_chr_banks();
        return;
    }

    // CHR bank 1: $8800-$8FFF
    if (address >= 0x8800 && address < 0x9000) {
        m_chr_bank[1] = value;
        update_chr_banks();
        return;
    }

    // CHR bank 2: $9000-$97FF
    if (address >= 0x9000 && address < 0x9800) {
        m_chr_bank[2] = value;
        update_chr_banks();
        return;
    }

    // CHR bank 3: $9800-$9FFF
    if (address >= 0x9800 && address < 0xA000) {
        m_chr_bank[3] = value;
        update_chr_banks();
        return;
    }

    // CHR bank 4: $A000-$A7FF
    if (address >= 0xA000 && address < 0xA800) {
        m_chr_bank[4] = value;
        update_chr_banks();
        return;
    }

    // CHR bank 5: $A800-$AFFF
    if (address >= 0xA800 && address < 0xB000) {
        m_chr_bank[5] = value;
        update_chr_banks();
        return;
    }

    // CHR bank 6: $B000-$B7FF
    if (address >= 0xB000 && address < 0xB800) {
        m_chr_bank[6] = value;
        update_chr_banks();
        return;
    }

    // CHR bank 7: $B800-$BFFF
    if (address >= 0xB800 && address < 0xC000) {
        m_chr_bank[7] = value;
        update_chr_banks();
        return;
    }

    // Nametable 0: $C000-$C7FF
    if (address >= 0xC000 && address < 0xC800) {
        m_nt_bank[0] = value;
        return;
    }

    // Nametable 1: $C800-$CFFF
    if (address >= 0xC800 && address < 0xD000) {
        m_nt_bank[1] = value;
        return;
    }

    // Nametable 2: $D000-$D7FF
    if (address >= 0xD000 && address < 0xD800) {
        m_nt_bank[2] = value;
        return;
    }

    // Nametable 3: $D800-$DFFF
    if (address >= 0xD800 && address < 0xE000) {
        m_nt_bank[3] = value;
        return;
    }

    // PRG bank 0 ($8000-$9FFF): $E000-$E7FF
    if (address >= 0xE000 && address < 0xE800) {
        m_prg_bank[0] = value & 0x3F;
        update_prg_banks();
        return;
    }

    // PRG bank 1 ($A000-$BFFF): $E800-$EFFF
    if (address >= 0xE800 && address < 0xF000) {
        m_prg_bank[1] = value & 0x3F;
        // Bit 6: CHR RAM write protect for banks 4-7
        // Bit 7: CHR RAM write protect for banks 0-3
        // (Not fully implemented - would need additional state)
        update_prg_banks();
        return;
    }

    // PRG bank 2 ($C000-$DFFF): $F000-$F7FF
    if (address >= 0xF000 && address < 0xF800) {
        m_prg_bank[2] = value & 0x3F;
        update_prg_banks();
        return;
    }

    // RAM address + auto-increment + sound enable: $F800-$FFFF
    if (address >= 0xF800) {
        m_ram_addr = value & 0x7F;
        m_ram_auto_increment = (value & 0x80) != 0;
        // Some variants use bit 6 for sound enable
        m_sound_enabled = (value & 0x40) == 0;  // 0 = enabled
        return;
    }
}

uint8_t Mapper019::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    // Pattern tables: $0000-$1FFF
    if (address < 0x2000) {
        int bank = address / 0x400;
        uint8_t bank_num = m_chr_bank[bank];

        // Banks $E0-$FF use CIRAM (nametable RAM)
        if (bank_num >= 0xE0) {
            // Return special value to indicate CIRAM access
            // The bus should handle this by reading from CIRAM
            // We encode the CIRAM address in the low bits
            // CIRAM has 2KB, so we use bit 0 of bank_num to select which 1KB
            // This is a simplified implementation - real hardware is more complex
            return 0;  // Will be handled by bus
        }

        // Regular CHR ROM access
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
        return 0;
    }

    // Nametables: $2000-$2FFF (and mirrors to $3000-$3EFF)
    if (address >= 0x2000 && address < 0x3F00) {
        uint16_t nt_addr = address & 0x0FFF;
        int nt = (nt_addr / 0x400) & 0x03;  // Which nametable (0-3)
        uint8_t bank_num = m_nt_bank[nt];

        // Banks $E0-$FF use CIRAM
        if (bank_num >= 0xE0) {
            // CIRAM is 2KB, select which 1KB based on bit 0
            // Return 0, let bus handle CIRAM
            return 0;
        }

        // Otherwise, map to CHR ROM
        if (!m_chr_rom->empty()) {
            size_t chr_size = m_chr_rom->size();
            size_t num_1k_banks = chr_size / 0x400;
            if (num_1k_banks > 0) {
                uint32_t offset = (bank_num % num_1k_banks) * 0x400 + (nt_addr & 0x3FF);
                if (offset < chr_size) {
                    return (*m_chr_rom)[offset];
                }
            }
        }
    }

    return 0;
}

void Mapper019::ppu_write(uint16_t address, uint8_t value) {
    // Pattern tables: $0000-$1FFF
    if (address < 0x2000) {
        if (!m_has_chr_ram) return;

        int bank = address / 0x400;
        uint8_t bank_num = m_chr_bank[bank];

        // Banks $E0-$FF use CIRAM - let bus handle
        if (bank_num >= 0xE0) {
            return;
        }

        // Regular CHR RAM write
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
        return;
    }

    // Nametables - let bus handle CIRAM writes
    // CHR ROM writes to nametable region are generally not supported
}

void Mapper019::cpu_cycles(int count) {
    // PERFORMANCE: Process all cycles at once instead of one at a time

    // IRQ counter counts up on every CPU cycle when enabled
    if (m_irq_enabled) {
        // Only count if counter is less than $8000
        if (m_irq_counter < 0x8000) {
            uint16_t new_counter = m_irq_counter + count;
            // Check for overflow past $8000
            if (new_counter >= 0x8000 || new_counter < m_irq_counter) {
                m_irq_counter = 0x8000;  // Clamp at trigger point
                m_irq_pending = true;
            } else {
                m_irq_counter = new_counter;
            }
        }
    }

    // Audio synthesis with divider for performance
    // Namco 163 audio updates one channel every 15 CPU cycles
    m_audio_divider += count;
    while (m_audio_divider >= AUDIO_DIVIDER_PERIOD) {
        m_audio_divider -= AUDIO_DIVIDER_PERIOD;
        clock_audio();
    }
}

void Mapper019::cpu_cycle() {
    // Single-cycle version for compatibility - delegates to batched version
    cpu_cycles(1);
}

void Mapper019::clock_audio() {
    // This function is now called every 15 CPU cycles (once per channel clock)

    if (!m_sound_enabled || m_active_channels == 0) {
        m_audio_output = 0.0f;
        return;
    }

    // Update the current channel
    // Only process channels that are active (8 - active_channels to 7)
    int first_active = 8 - m_active_channels;
    if (m_current_channel >= first_active && m_current_channel <= 7) {
        AudioChannel& ch = m_channels[m_current_channel];

        if (ch.volume > 0 && ch.frequency > 0) {
            // Advance phase accumulator
            ch.phase += ch.frequency;
            ch.phase &= 0xFFFFFF;  // 24-bit
        }
    }

    // Move to next channel
    m_current_channel++;
    if (m_current_channel > 7) {
        m_current_channel = 8 - m_active_channels;

        // Calculate final mixed output
        // Mix all active channels
        int mix = 0;
        int first = 8 - m_active_channels;
        for (int i = first; i <= 7; i++) {
            AudioChannel& ch = m_channels[i];
            if (ch.volume > 0) {
                // Calculate sample position
                int wave_samples = 256 - (ch.wave_length * 4);
                if (wave_samples <= 0) wave_samples = 4;

                int sample_pos = (ch.phase >> 16) % wave_samples;
                int ram_addr = (ch.wave_offset + sample_pos) & 0x7F;
                uint8_t sample_byte = m_internal_ram[ram_addr / 2];
                uint8_t sample = (ram_addr & 1) ? (sample_byte >> 4) : (sample_byte & 0x0F);

                // Sample (0-15) * Volume (0-15) = 0-225
                mix += sample * ch.volume;
            }
        }

        // Normalize output
        // Maximum possible: 8 channels * 15 sample * 15 volume = 1800
        if (m_active_channels > 0) {
            float max_value = static_cast<float>(m_active_channels * 15 * 15);
            m_audio_output = (mix / max_value - 0.5f) * 2.0f;
        } else {
            m_audio_output = 0.0f;
        }
    }
}

void Mapper019::save_state(std::vector<uint8_t>& data) {
    // PRG banks
    for (int i = 0; i < 3; i++) {
        data.push_back(m_prg_bank[i]);
    }
    data.push_back(m_prg_ram_write_protect ? 1 : 0);

    // CHR banks
    for (int i = 0; i < 8; i++) {
        data.push_back(m_chr_bank[i]);
    }

    // NT banks
    for (int i = 0; i < 4; i++) {
        data.push_back(m_nt_bank[i]);
    }

    // IRQ
    data.push_back(static_cast<uint8_t>(m_irq_counter & 0xFF));
    data.push_back(static_cast<uint8_t>((m_irq_counter >> 8) & 0xFF));
    data.push_back(m_irq_enabled ? 1 : 0);
    data.push_back(m_irq_pending ? 1 : 0);

    // Internal RAM
    for (size_t i = 0; i < m_internal_ram.size(); i++) {
        data.push_back(m_internal_ram[i]);
    }

    // RAM address
    data.push_back(m_ram_addr);
    data.push_back(m_ram_auto_increment ? 1 : 0);
    data.push_back(m_sound_enabled ? 1 : 0);
    data.push_back(m_active_channels);

    // Audio channels (minimal state for phase accumulators)
    for (int i = 0; i < 8; i++) {
        const AudioChannel& ch = m_channels[i];
        data.push_back(static_cast<uint8_t>(ch.phase & 0xFF));
        data.push_back(static_cast<uint8_t>((ch.phase >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((ch.phase >> 16) & 0xFF));
    }

    // Audio timing
    data.push_back(m_audio_cycle);
    data.push_back(m_current_channel);

    data.push_back(static_cast<uint8_t>(m_mirror_mode));
}

void Mapper019::load_state(const uint8_t*& data, size_t& remaining) {
    // Calculate minimum required size
    size_t min_size = 3 + 1 + 8 + 4 + 2 + 2 + 128 + 4 + 24 + 2 + 1;
    if (remaining < min_size) return;

    // PRG banks
    for (int i = 0; i < 3; i++) {
        m_prg_bank[i] = *data++; remaining--;
    }
    m_prg_ram_write_protect = (*data++ != 0); remaining--;

    // CHR banks
    for (int i = 0; i < 8; i++) {
        m_chr_bank[i] = *data++; remaining--;
    }

    // NT banks
    for (int i = 0; i < 4; i++) {
        m_nt_bank[i] = *data++; remaining--;
    }

    // IRQ
    uint8_t irq_lo = *data++; remaining--;
    uint8_t irq_hi = *data++; remaining--;
    m_irq_counter = irq_lo | (static_cast<uint16_t>(irq_hi) << 8);
    m_irq_enabled = (*data++ != 0); remaining--;
    m_irq_pending = (*data++ != 0); remaining--;

    // Internal RAM
    for (size_t i = 0; i < m_internal_ram.size(); i++) {
        m_internal_ram[i] = *data++; remaining--;
    }

    // RAM address
    m_ram_addr = *data++; remaining--;
    m_ram_auto_increment = (*data++ != 0); remaining--;
    m_sound_enabled = (*data++ != 0); remaining--;
    m_active_channels = *data++; remaining--;

    // Audio channels
    for (int i = 0; i < 8; i++) {
        AudioChannel& ch = m_channels[i];
        uint8_t p0 = *data++; remaining--;
        uint8_t p1 = *data++; remaining--;
        uint8_t p2 = *data++; remaining--;
        ch.phase = p0 | (static_cast<uint32_t>(p1) << 8) | (static_cast<uint32_t>(p2) << 16);
    }

    // Audio timing
    m_audio_cycle = *data++; remaining--;
    m_current_channel = *data++; remaining--;

    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    // Rebuild channel state from internal RAM
    for (int i = 0; i < 8; i++) {
        uint8_t base = channel_reg_addr(i);
        AudioChannel& ch = m_channels[i];
        ch.frequency = m_internal_ram[base + 0];
        ch.frequency |= static_cast<uint32_t>(m_internal_ram[base + 2]) << 8;
        ch.frequency |= static_cast<uint32_t>(m_internal_ram[base + 4] & 0x03) << 16;
        ch.wave_length = m_internal_ram[base + 4] >> 2;
        ch.wave_offset = m_internal_ram[base + 6];
        ch.volume = m_internal_ram[base + 7] & 0x0F;
    }

    update_prg_banks();
    update_chr_banks();
}

} // namespace nes
