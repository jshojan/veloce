#include "mapper_005.hpp"
#include <cstring>

namespace nes {

Mapper005::Mapper005(std::vector<uint8_t>& prg_rom,
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

    // MMC5 typically has 64KB of PRG RAM
    if (m_prg_ram->size() < 0x10000) {
        m_prg_ram->resize(0x10000, 0);
    }

    reset();
}

void Mapper005::reset() {
    m_prg_mode = 3;  // 8KB banks mode
    m_chr_mode = 3;  // 1KB banks mode

    m_prg_ram_protect1 = 0;
    m_prg_ram_protect2 = 0;

    m_exram_mode = 0;
    m_nametable_mapping = 0;

    m_fill_tile = 0;
    m_fill_attribute = 0;

    m_prg_ram_bank = 0;

    // Initialize PRG banks - last bank fixed to last ROM bank
    m_prg_banks[0] = 0;
    m_prg_banks[1] = 0;
    m_prg_banks[2] = 0;
    m_prg_banks[3] = 0xFF;  // ROM, last bank

    // Initialize CHR banks
    m_chr_banks_sprite.fill(0);
    m_chr_banks_bg.fill(0);
    m_chr_upper_bits = 0;
    m_last_chr_write_was_bg = false;

    m_split_mode = 0;
    m_split_scroll = 0;
    m_split_bank = 0;

    m_irq_scanline = 0;
    m_irq_enabled = false;
    m_irq_pending = false;
    m_in_frame = false;

    m_multiplicand = 0xFF;
    m_multiplier = 0xFF;

    m_exram.fill(0);

    m_scanline_counter = 0;
    m_last_ppu_addr = 0;
    m_consecutive_nametable_reads = 0;
    m_last_frame_cycle = 0;

    m_fetching_sprites = false;
    m_current_tile_addr = 0;
    m_exram_attr_latch = 0;

    m_in_split_region = false;
    m_split_tile_count = 0;

    // Reset audio state
    m_mmc5_pulse[0] = MMC5Pulse{};
    m_mmc5_pulse[1] = MMC5Pulse{};
    m_pcm_output = 0;
    m_pcm_irq_enabled = false;
    m_pcm_read_mode = false;
    m_audio_output = 0.0f;
    m_audio_cycles = 0;
}

// ========== PRG Banking ==========

uint32_t Mapper005::get_prg_bank_offset(int bank, bool is_ram) const {
    if (is_ram) {
        // PRG RAM banking - up to 64KB in 8KB banks
        uint32_t ram_bank = bank & 0x07;  // 8 possible 8KB banks
        return (ram_bank * 0x2000) % m_prg_ram->size();
    } else {
        // PRG ROM banking
        uint32_t prg_size = m_prg_rom->size();
        if (prg_size == 0) return 0;
        return (static_cast<uint32_t>(bank) * 0x2000) % prg_size;
    }
}

uint8_t Mapper005::read_prg(uint16_t address) {
    uint32_t prg_size = m_prg_rom->size();
    if (prg_size == 0) return 0;

    int bank_index;
    uint16_t offset;
    bool is_ram = false;
    uint8_t bank_reg;

    switch (m_prg_mode) {
        case 0:  // 32KB mode
            // $5117 selects 32KB bank (ignore bit 0-1)
            bank_reg = m_prg_banks[3];
            is_ram = (bank_reg & 0x80) == 0;
            if (address >= 0x8000) {
                bank_index = (bank_reg & 0x7C) >> 2;  // 32KB aligned
                offset = address & 0x7FFF;
                if (is_ram) {
                    uint32_t ram_offset = (bank_index * 0x8000 + offset) % m_prg_ram->size();
                    return (*m_prg_ram)[ram_offset];
                }
                uint32_t rom_offset = ((bank_reg & 0x7C) * 0x2000 + offset) % prg_size;
                return (*m_prg_rom)[rom_offset];
            }
            break;

        case 1:  // 16KB + 16KB mode
            if (address >= 0xC000) {
                // $C000-$FFFF: $5117
                bank_reg = m_prg_banks[3];
                is_ram = (bank_reg & 0x80) == 0;
                bank_index = (bank_reg & 0x7E) >> 1;  // 16KB aligned
                offset = address & 0x3FFF;
                if (is_ram) {
                    uint32_t ram_offset = (bank_index * 0x4000 + offset) % m_prg_ram->size();
                    return (*m_prg_ram)[ram_offset];
                }
                uint32_t rom_offset = ((bank_reg & 0x7E) * 0x2000 + offset) % prg_size;
                return (*m_prg_rom)[rom_offset];
            } else if (address >= 0x8000) {
                // $8000-$BFFF: $5115
                bank_reg = m_prg_banks[1];
                is_ram = (bank_reg & 0x80) == 0;
                bank_index = (bank_reg & 0x7E) >> 1;
                offset = address & 0x3FFF;
                if (is_ram) {
                    uint32_t ram_offset = (bank_index * 0x4000 + offset) % m_prg_ram->size();
                    return (*m_prg_ram)[ram_offset];
                }
                uint32_t rom_offset = ((bank_reg & 0x7E) * 0x2000 + offset) % prg_size;
                return (*m_prg_rom)[rom_offset];
            }
            break;

        case 2:  // 16KB + 8KB + 8KB mode
            if (address >= 0xE000) {
                // $E000-$FFFF: $5117 (always ROM on real hardware, but we follow register)
                bank_reg = m_prg_banks[3];
                is_ram = (bank_reg & 0x80) == 0;
                offset = address & 0x1FFF;
                if (is_ram) {
                    uint32_t ram_offset = get_prg_bank_offset(bank_reg & 0x7F, true) + offset;
                    return (*m_prg_ram)[ram_offset % m_prg_ram->size()];
                }
                uint32_t rom_offset = get_prg_bank_offset(bank_reg & 0x7F, false) + offset;
                return (*m_prg_rom)[rom_offset];
            } else if (address >= 0xC000) {
                // $C000-$DFFF: $5116
                bank_reg = m_prg_banks[2];
                is_ram = (bank_reg & 0x80) == 0;
                offset = address & 0x1FFF;
                if (is_ram) {
                    uint32_t ram_offset = get_prg_bank_offset(bank_reg & 0x7F, true) + offset;
                    return (*m_prg_ram)[ram_offset % m_prg_ram->size()];
                }
                uint32_t rom_offset = get_prg_bank_offset(bank_reg & 0x7F, false) + offset;
                return (*m_prg_rom)[rom_offset];
            } else if (address >= 0x8000) {
                // $8000-$BFFF: $5115 (16KB)
                bank_reg = m_prg_banks[1];
                is_ram = (bank_reg & 0x80) == 0;
                bank_index = (bank_reg & 0x7E) >> 1;
                offset = address & 0x3FFF;
                if (is_ram) {
                    uint32_t ram_offset = (bank_index * 0x4000 + offset) % m_prg_ram->size();
                    return (*m_prg_ram)[ram_offset];
                }
                uint32_t rom_offset = ((bank_reg & 0x7E) * 0x2000 + offset) % prg_size;
                return (*m_prg_rom)[rom_offset];
            }
            break;

        case 3:  // 8KB + 8KB + 8KB + 8KB mode
        default:
            if (address >= 0xE000) {
                bank_reg = m_prg_banks[3];
                is_ram = (bank_reg & 0x80) == 0;
                offset = address & 0x1FFF;
            } else if (address >= 0xC000) {
                bank_reg = m_prg_banks[2];
                is_ram = (bank_reg & 0x80) == 0;
                offset = address & 0x1FFF;
            } else if (address >= 0xA000) {
                bank_reg = m_prg_banks[1];
                is_ram = (bank_reg & 0x80) == 0;
                offset = address & 0x1FFF;
            } else {  // >= 0x8000
                bank_reg = m_prg_banks[0];
                is_ram = (bank_reg & 0x80) == 0;
                offset = address & 0x1FFF;
            }

            if (is_ram) {
                uint32_t ram_offset = get_prg_bank_offset(bank_reg & 0x7F, true) + offset;
                return (*m_prg_ram)[ram_offset % m_prg_ram->size()];
            }
            uint32_t rom_offset = get_prg_bank_offset(bank_reg & 0x7F, false) + offset;
            return (*m_prg_rom)[rom_offset];
    }

    return 0;
}

void Mapper005::write_prg(uint16_t address, uint8_t value) {
    // Check write protection
    if (m_prg_ram_protect1 != 0x02 || m_prg_ram_protect2 != 0x01) {
        return;  // RAM writes disabled
    }

    uint8_t bank_reg;
    uint16_t offset;
    bool is_ram;

    switch (m_prg_mode) {
        case 0:  // 32KB mode
            bank_reg = m_prg_banks[3];
            is_ram = (bank_reg & 0x80) == 0;
            if (!is_ram) return;  // Can't write to ROM
            if (address >= 0x8000) {
                offset = address & 0x7FFF;
                uint32_t ram_offset = ((bank_reg & 0x7C) * 0x2000 + offset) % m_prg_ram->size();
                (*m_prg_ram)[ram_offset] = value;
            }
            break;

        case 1:  // 16KB + 16KB mode
            if (address >= 0xC000) {
                bank_reg = m_prg_banks[3];
            } else if (address >= 0x8000) {
                bank_reg = m_prg_banks[1];
            } else {
                return;
            }
            is_ram = (bank_reg & 0x80) == 0;
            if (!is_ram) return;
            offset = address & 0x3FFF;
            {
                uint32_t ram_offset = ((bank_reg & 0x7E) * 0x2000 + offset) % m_prg_ram->size();
                (*m_prg_ram)[ram_offset] = value;
            }
            break;

        case 2:  // 16KB + 8KB + 8KB mode
            if (address >= 0xE000) {
                bank_reg = m_prg_banks[3];
                offset = address & 0x1FFF;
            } else if (address >= 0xC000) {
                bank_reg = m_prg_banks[2];
                offset = address & 0x1FFF;
            } else if (address >= 0x8000) {
                bank_reg = m_prg_banks[1];
                offset = address & 0x3FFF;
            } else {
                return;
            }
            is_ram = (bank_reg & 0x80) == 0;
            if (!is_ram) return;
            {
                uint32_t ram_offset;
                if (address >= 0x8000 && address < 0xC000) {
                    ram_offset = ((bank_reg & 0x7E) * 0x2000 + offset) % m_prg_ram->size();
                } else {
                    ram_offset = get_prg_bank_offset(bank_reg & 0x7F, true) + offset;
                    ram_offset %= m_prg_ram->size();
                }
                (*m_prg_ram)[ram_offset] = value;
            }
            break;

        case 3:  // 8KB + 8KB + 8KB + 8KB mode
        default:
            if (address >= 0xE000) {
                bank_reg = m_prg_banks[3];
            } else if (address >= 0xC000) {
                bank_reg = m_prg_banks[2];
            } else if (address >= 0xA000) {
                bank_reg = m_prg_banks[1];
            } else if (address >= 0x8000) {
                bank_reg = m_prg_banks[0];
            } else {
                return;
            }
            is_ram = (bank_reg & 0x80) == 0;
            if (!is_ram) return;
            offset = address & 0x1FFF;
            {
                uint32_t ram_offset = get_prg_bank_offset(bank_reg & 0x7F, true) + offset;
                (*m_prg_ram)[ram_offset % m_prg_ram->size()] = value;
            }
            break;
    }
}

// ========== CHR Banking ==========

uint32_t Mapper005::get_chr_bank_offset(int bank, bool for_sprites) const {
    uint32_t chr_size = m_chr_rom->size();
    if (chr_size == 0) return 0;

    // Use sprite or BG banks depending on context
    // In 8x16 sprite mode or when last write was to sprite regs, use sprite banks
    // The upper bits from $5130 are applied here

    uint16_t full_bank;

    switch (m_chr_mode) {
        case 0:  // 8KB mode
            // All 8 banks are treated as one 8KB bank
            if (for_sprites || !m_last_chr_write_was_bg) {
                full_bank = m_chr_banks_sprite[7];
            } else {
                full_bank = m_chr_banks_bg[3];
            }
            full_bank = (full_bank & 0xFF) | (static_cast<uint16_t>(m_chr_upper_bits) << 8);
            return (static_cast<uint32_t>(full_bank) * 0x2000) % chr_size;

        case 1:  // 4KB mode
            if (bank < 4) {
                if (for_sprites || !m_last_chr_write_was_bg) {
                    full_bank = m_chr_banks_sprite[3];
                } else {
                    full_bank = m_chr_banks_bg[3];
                }
            } else {
                if (for_sprites || !m_last_chr_write_was_bg) {
                    full_bank = m_chr_banks_sprite[7];
                } else {
                    full_bank = m_chr_banks_bg[3];
                }
            }
            full_bank = (full_bank & 0xFF) | (static_cast<uint16_t>(m_chr_upper_bits) << 8);
            return (static_cast<uint32_t>(full_bank) * 0x1000 + (bank & 3) * 0x400) % chr_size;

        case 2:  // 2KB mode
            {
                int reg_index = (bank / 2) & 3;
                if (for_sprites || !m_last_chr_write_was_bg) {
                    if (bank < 4) {
                        full_bank = m_chr_banks_sprite[1 + reg_index * 2];
                    } else {
                        full_bank = m_chr_banks_sprite[5 + (reg_index & 1) * 2];
                    }
                } else {
                    full_bank = m_chr_banks_bg[reg_index];
                }
                full_bank = (full_bank & 0xFF) | (static_cast<uint16_t>(m_chr_upper_bits) << 8);
                return (static_cast<uint32_t>(full_bank) * 0x800 + (bank & 1) * 0x400) % chr_size;
            }

        case 3:  // 1KB mode
        default:
            if (for_sprites || !m_last_chr_write_was_bg) {
                full_bank = m_chr_banks_sprite[bank & 7];
            } else {
                full_bank = m_chr_banks_bg[bank & 3];
            }
            full_bank = (full_bank & 0xFF) | (static_cast<uint16_t>(m_chr_upper_bits) << 8);
            return (static_cast<uint32_t>(full_bank) * 0x400) % chr_size;
    }
}

// ========== CPU Read/Write ==========

uint8_t Mapper005::cpu_read(uint16_t address) {
    // PRG RAM at $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        uint32_t offset = (m_prg_ram_bank & 0x07) * 0x2000 + (address & 0x1FFF);
        if (offset < m_prg_ram->size()) {
            return (*m_prg_ram)[offset];
        }
        return 0;
    }

    // PRG ROM/RAM at $8000-$FFFF
    if (address >= 0x8000) {
        return read_prg(address);
    }

    // MMC5 internal registers at $5000-$5FFF
    if (address >= 0x5000 && address < 0x6000) {
        // ExRAM at $5C00-$5FFF
        if (address >= 0x5C00) {
            // ExRAM read - only readable in modes 2 and 3
            if (m_exram_mode >= 2) {
                return m_exram[address & 0x3FF];
            }
            return 0;  // Open bus in modes 0 and 1
        }

        switch (address) {
            case 0x5204:
                // IRQ status
                {
                    uint8_t result = 0;
                    if (m_in_frame) result |= 0x40;
                    if (m_irq_pending) result |= 0x80;
                    m_irq_pending = false;  // Reading clears pending flag
                    return result;
                }

            case 0x5205:
                // Multiplier low byte
                return static_cast<uint8_t>((m_multiplicand * m_multiplier) & 0xFF);

            case 0x5206:
                // Multiplier high byte
                return static_cast<uint8_t>((m_multiplicand * m_multiplier) >> 8);

            case 0x5015:
                // Audio status
                {
                    uint8_t status = 0;
                    if (m_mmc5_pulse[0].length_counter > 0) status |= 0x01;
                    if (m_mmc5_pulse[1].length_counter > 0) status |= 0x02;
                    return status;
                }

            default:
                // Other registers return open bus
                return 0;
        }
    }

    return 0;
}

void Mapper005::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM at $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        // Check write protection
        if (m_prg_ram_protect1 == 0x02 && m_prg_ram_protect2 == 0x01) {
            uint32_t offset = (m_prg_ram_bank & 0x07) * 0x2000 + (address & 0x1FFF);
            if (offset < m_prg_ram->size()) {
                (*m_prg_ram)[offset] = value;
            }
        }
        return;
    }

    // PRG ROM/RAM at $8000-$FFFF
    if (address >= 0x8000) {
        write_prg(address, value);
        return;
    }

    // MMC5 internal registers at $5000-$5FFF
    if (address >= 0x5000 && address < 0x6000) {
        // ExRAM at $5C00-$5FFF
        if (address >= 0x5C00) {
            // ExRAM write - only writable in mode 2, and during rendering in modes 0-1
            if (m_exram_mode == 2) {
                m_exram[address & 0x3FF] = value;
            } else if (m_exram_mode < 2 && m_in_frame) {
                // In modes 0-1, writes during rendering set the value
                m_exram[address & 0x3FF] = value;
            } else if (m_exram_mode < 2 && !m_in_frame) {
                // In modes 0-1, writes outside rendering set to 0
                m_exram[address & 0x3FF] = 0;
            }
            // Mode 3: read-only, writes ignored
            return;
        }

        switch (address) {
            // ===== Pulse 1 registers =====
            case 0x5000:
                m_mmc5_pulse[0].duty = (value >> 6) & 0x03;
                m_mmc5_pulse[0].length_halt = (value & 0x20) != 0;
                m_mmc5_pulse[0].constant_volume = (value & 0x10) != 0;
                m_mmc5_pulse[0].volume = value & 0x0F;
                break;
            case 0x5001:
                // Sweep (ignored on MMC5 - no sweep hardware)
                break;
            case 0x5002:
                m_mmc5_pulse[0].timer_period = (m_mmc5_pulse[0].timer_period & 0x700) | value;
                break;
            case 0x5003:
                m_mmc5_pulse[0].timer_period = (m_mmc5_pulse[0].timer_period & 0x0FF) | ((value & 0x07) << 8);
                if (m_mmc5_pulse[0].enabled) {
                    // Load length counter from table
                    static const uint8_t length_table[32] = {
                        10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
                        12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
                    };
                    m_mmc5_pulse[0].length_counter = length_table[value >> 3];
                }
                m_mmc5_pulse[0].sequence_pos = 0;
                m_mmc5_pulse[0].envelope_start = true;
                break;

            // ===== Pulse 2 registers =====
            case 0x5004:
                m_mmc5_pulse[1].duty = (value >> 6) & 0x03;
                m_mmc5_pulse[1].length_halt = (value & 0x20) != 0;
                m_mmc5_pulse[1].constant_volume = (value & 0x10) != 0;
                m_mmc5_pulse[1].volume = value & 0x0F;
                break;
            case 0x5005:
                // Sweep (ignored on MMC5)
                break;
            case 0x5006:
                m_mmc5_pulse[1].timer_period = (m_mmc5_pulse[1].timer_period & 0x700) | value;
                break;
            case 0x5007:
                m_mmc5_pulse[1].timer_period = (m_mmc5_pulse[1].timer_period & 0x0FF) | ((value & 0x07) << 8);
                if (m_mmc5_pulse[1].enabled) {
                    static const uint8_t length_table[32] = {
                        10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
                        12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
                    };
                    m_mmc5_pulse[1].length_counter = length_table[value >> 3];
                }
                m_mmc5_pulse[1].sequence_pos = 0;
                m_mmc5_pulse[1].envelope_start = true;
                break;

            // ===== PCM/Status registers =====
            case 0x5010:
                m_pcm_read_mode = (value & 0x01) != 0;
                m_pcm_irq_enabled = (value & 0x80) != 0;
                break;
            case 0x5011:
                if (!m_pcm_read_mode) {
                    m_pcm_output = value;
                }
                break;
            case 0x5015:
                // Audio status
                m_mmc5_pulse[0].enabled = (value & 0x01) != 0;
                m_mmc5_pulse[1].enabled = (value & 0x02) != 0;
                if (!m_mmc5_pulse[0].enabled) m_mmc5_pulse[0].length_counter = 0;
                if (!m_mmc5_pulse[1].enabled) m_mmc5_pulse[1].length_counter = 0;
                break;

            // ===== PRG mode =====
            case 0x5100:
                m_prg_mode = value & 0x03;
                break;

            // ===== CHR mode =====
            case 0x5101:
                m_chr_mode = value & 0x03;
                break;

            // ===== PRG RAM protect =====
            case 0x5102:
                m_prg_ram_protect1 = value & 0x03;
                break;
            case 0x5103:
                m_prg_ram_protect2 = value & 0x03;
                break;

            // ===== ExRAM mode =====
            case 0x5104:
                m_exram_mode = value & 0x03;
                break;

            // ===== Nametable mapping =====
            case 0x5105:
                m_nametable_mapping = value;
                break;

            // ===== Fill mode =====
            case 0x5106:
                m_fill_tile = value;
                break;
            case 0x5107:
                m_fill_attribute = value & 0x03;
                break;

            // ===== PRG banking =====
            case 0x5113:
                m_prg_ram_bank = value & 0x07;
                break;
            case 0x5114:
                m_prg_banks[0] = value;
                break;
            case 0x5115:
                m_prg_banks[1] = value;
                break;
            case 0x5116:
                m_prg_banks[2] = value;
                break;
            case 0x5117:
                m_prg_banks[3] = value | 0x80;  // $5117 always maps ROM
                break;

            // ===== CHR banking (sprites) =====
            case 0x5120: case 0x5121: case 0x5122: case 0x5123:
            case 0x5124: case 0x5125: case 0x5126: case 0x5127:
                m_chr_banks_sprite[address - 0x5120] = value;
                m_last_chr_write_was_bg = false;
                break;

            // ===== CHR banking (background) =====
            case 0x5128: case 0x5129: case 0x512A: case 0x512B:
                m_chr_banks_bg[address - 0x5128] = value;
                m_last_chr_write_was_bg = true;
                break;

            // ===== Upper CHR bits =====
            case 0x5130:
                m_chr_upper_bits = value & 0x03;
                break;

            // ===== Split screen =====
            case 0x5200:
                m_split_mode = value;
                break;
            case 0x5201:
                m_split_scroll = value;
                break;
            case 0x5202:
                m_split_bank = value;
                break;

            // ===== IRQ =====
            case 0x5203:
                m_irq_scanline = value;
                break;
            case 0x5204:
                m_irq_enabled = (value & 0x80) != 0;
                break;

            // ===== Multiplier =====
            case 0x5205:
                m_multiplicand = value;
                break;
            case 0x5206:
                m_multiplier = value;
                break;
        }
    }
}

// ========== PPU Read/Write ==========

uint8_t Mapper005::ppu_read(uint16_t address, uint32_t frame_cycle) {
    address &= 0x3FFF;

    // Pattern tables ($0000-$1FFF)
    if (address < 0x2000) {
        // Detect scanline based on PPU address patterns
        detect_scanline(address, frame_cycle);

        uint32_t chr_size = m_chr_rom->size();
        if (chr_size == 0) return 0;

        // Determine if this is a sprite or background fetch
        // Background fetches happen at cycles 1-256 and 321-340
        // Sprite fetches happen at cycles 257-320
        // The PPU address pattern can tell us:
        // - BG tile fetches come from nametable reads, then pattern reads
        // - Sprite fetches use $1000 or $0000 depending on sprite size/table

        int bank = address / 0x400;  // 1KB bank index
        uint32_t offset = get_chr_bank_offset(bank, m_fetching_sprites);
        offset += (address & 0x3FF);

        if (offset < chr_size) {
            return (*m_chr_rom)[offset];
        }
        return 0;
    }

    // Nametables ($2000-$3EFF)
    if (address < 0x3F00) {
        return read_nametable(address);
    }

    // Palette - shouldn't reach here (handled by PPU)
    return 0;
}

void Mapper005::ppu_write(uint16_t address, uint8_t value) {
    address &= 0x3FFF;

    // CHR RAM writes
    if (address < 0x2000 && m_has_chr_ram) {
        uint32_t chr_size = m_chr_rom->size();
        if (chr_size == 0) return;

        int bank = address / 0x400;
        uint32_t offset = get_chr_bank_offset(bank, m_fetching_sprites);
        offset += (address & 0x3FF);

        if (offset < chr_size) {
            (*m_chr_rom)[offset] = value;
        }
        return;
    }

    // Nametable writes
    if (address >= 0x2000 && address < 0x3F00) {
        write_nametable(address, value);
    }
}

// ========== Nametable Handling ==========

uint8_t Mapper005::read_nametable(uint16_t address) {
    address &= 0x0FFF;  // Mirror to $2000-$2FFF range

    // Determine which nametable (0-3)
    int nt = (address >> 10) & 0x03;
    uint16_t offset = address & 0x3FF;

    // Get source from nametable mapping register
    int source = (m_nametable_mapping >> (nt * 2)) & 0x03;

    bool is_attribute = (offset >= 0x3C0);

    // Extended attribute mode handling
    if (m_exram_mode == 1 && !is_attribute && source < 2) {
        // In extended attribute mode, the tile fetch uses the normal source,
        // but we need to latch the ExRAM value for the attribute fetch
        // The ExRAM byte at the same tile position provides the palette
        uint16_t tile_x = offset & 0x1F;
        uint16_t tile_y = (offset >> 5) & 0x1F;
        m_exram_attr_latch = m_exram[(tile_y * 32 + tile_x) & 0x3FF];
    }

    switch (source) {
        case 0:  // CIRAM page 0
            return 0;  // Will be handled by PPU's internal nametable RAM
        case 1:  // CIRAM page 1
            return 0;  // Will be handled by PPU's internal nametable RAM
        case 2:  // ExRAM
            if (is_attribute && m_exram_mode == 1) {
                // Extended attribute mode - return latched attribute
                return get_exram_attribute(address);
            }
            return m_exram[offset & 0x3FF];
        case 3:  // Fill mode
            if (is_attribute) {
                // Expand 2-bit attribute to all 4 quadrants
                return m_fill_attribute |
                       (m_fill_attribute << 2) |
                       (m_fill_attribute << 4) |
                       (m_fill_attribute << 6);
            }
            return m_fill_tile;
    }

    return 0;
}

void Mapper005::write_nametable(uint16_t address, uint8_t value) {
    address &= 0x0FFF;

    int nt = (address >> 10) & 0x03;
    uint16_t offset = address & 0x3FF;

    int source = (m_nametable_mapping >> (nt * 2)) & 0x03;

    switch (source) {
        case 0:
        case 1:
            // CIRAM - will be handled by PPU
            break;
        case 2:
            // ExRAM - writable in modes 0-2
            if (m_exram_mode < 3) {
                m_exram[offset & 0x3FF] = value;
            }
            break;
        case 3:
            // Fill mode - writes ignored
            break;
    }
}

uint8_t Mapper005::get_exram_attribute(uint16_t address) {
    // In extended attribute mode, each tile has its own 2-bit palette
    // The ExRAM byte format: bits 7-6 = palette, bits 5-0 = upper CHR bits
    // Return the palette bits expanded for standard attribute format

    // The latched ExRAM value from the tile fetch
    uint8_t palette = (m_exram_attr_latch >> 6) & 0x03;

    // Return expanded attribute (same palette for all quadrants)
    return palette |
           (palette << 2) |
           (palette << 4) |
           (palette << 6);
}

// ========== Mirror Mode ==========

MirrorMode Mapper005::get_mirror_mode() const {
    // MMC5's nametable mapping is too complex for the simple MirrorMode enum
    // The actual mapping is handled in read_nametable/write_nametable
    // For the PPU's internal RAM, we need to return something sensible

    // Check if using vertical or horizontal mirroring pattern
    uint8_t nt0 = m_nametable_mapping & 0x03;
    uint8_t nt1 = (m_nametable_mapping >> 2) & 0x03;
    uint8_t nt2 = (m_nametable_mapping >> 4) & 0x03;
    uint8_t nt3 = (m_nametable_mapping >> 6) & 0x03;

    // Check for standard patterns
    if (nt0 == 0 && nt1 == 0 && nt2 == 1 && nt3 == 1) {
        return MirrorMode::Horizontal;
    }
    if (nt0 == 0 && nt1 == 1 && nt2 == 0 && nt3 == 1) {
        return MirrorMode::Vertical;
    }
    if (nt0 == 0 && nt1 == 0 && nt2 == 0 && nt3 == 0) {
        return MirrorMode::SingleScreen0;
    }
    if (nt0 == 1 && nt1 == 1 && nt2 == 1 && nt3 == 1) {
        return MirrorMode::SingleScreen1;
    }

    // Default to vertical for other configurations
    return MirrorMode::Vertical;
}

// ========== Scanline Detection and IRQ ==========

void Mapper005::detect_scanline(uint16_t address, uint32_t frame_cycle) {
    // MMC5 detects scanlines by watching for consecutive nametable reads
    // During visible rendering, the PPU reads from nametables in a predictable pattern

    // Track if we're in the sprite fetch region (cycles 257-320)
    // During these cycles, the PPU fetches sprite patterns
    // We can detect this by looking at the frame cycle within a scanline

    uint32_t cycle_in_frame = frame_cycle % 341;

    // Sprite fetches occur during cycles 257-320
    m_fetching_sprites = (cycle_in_frame >= 257 && cycle_in_frame <= 320);

    // The MMC5 detects scanlines by counting nametable reads
    // After 3 consecutive reads at $2000-$2FFF, it considers a new scanline

    // For now, we use a simpler approach: detect based on frame cycle wrapping
    // A more accurate implementation would track the actual PPU address patterns

    uint32_t scanline = frame_cycle / 341;

    // Detect frame start (leaving VBlank)
    if (frame_cycle < m_last_frame_cycle && m_last_frame_cycle > 240 * 341) {
        // Frame wrapped
        m_in_frame = false;
        m_scanline_counter = 0;
    }

    // Visible scanlines: 0-239
    if (scanline < 240) {
        if (!m_in_frame && scanline == 0) {
            m_in_frame = true;
            m_scanline_counter = 0;
        }

        // Check for new scanline
        uint8_t expected_scanline = static_cast<uint8_t>(scanline);
        if (expected_scanline != m_scanline_counter && m_in_frame) {
            m_scanline_counter = expected_scanline;

            // Check for IRQ
            if (m_irq_enabled && m_scanline_counter == m_irq_scanline) {
                m_irq_pending = true;
            }
        }
    } else if (scanline >= 241) {
        // VBlank
        m_in_frame = false;
    }

    m_last_frame_cycle = frame_cycle;
}

void Mapper005::scanline() {
    // This is called by the PPU at the end of each visible scanline
    // We use this as a backup scanline detection method

    if (m_in_frame) {
        m_scanline_counter++;

        if (m_irq_enabled && m_scanline_counter == m_irq_scanline) {
            m_irq_pending = true;
        }
    }
}

bool Mapper005::irq_pending(uint32_t frame_cycle) {
    (void)frame_cycle;
    return m_irq_pending;
}

void Mapper005::irq_clear() {
    m_irq_pending = false;
}

void Mapper005::notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr, uint32_t frame_cycle) {
    (void)old_addr;
    (void)new_addr;
    (void)frame_cycle;
    // MMC5 doesn't use A12 clocking like MMC3
}

void Mapper005::notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle) {
    // Track PPU address for scanline detection
    detect_scanline(address, frame_cycle);
}

void Mapper005::notify_frame_start() {
    // Reset frame-related state
    m_in_frame = false;
    m_scanline_counter = 0;
    m_irq_pending = false;  // Clear any stale IRQ
    m_split_tile_count = 0;
    m_in_split_region = false;
}

// ========== Save State ==========

void Mapper005::save_state(std::vector<uint8_t>& data) {
    // Mode registers
    data.push_back(m_prg_mode);
    data.push_back(m_chr_mode);
    data.push_back(m_prg_ram_protect1);
    data.push_back(m_prg_ram_protect2);
    data.push_back(m_exram_mode);
    data.push_back(m_nametable_mapping);
    data.push_back(m_fill_tile);
    data.push_back(m_fill_attribute);

    // PRG banking
    data.push_back(m_prg_ram_bank);
    for (int i = 0; i < 4; i++) {
        data.push_back(m_prg_banks[i]);
    }

    // CHR banking
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(m_chr_banks_sprite[i] & 0xFF));
        data.push_back(static_cast<uint8_t>(m_chr_banks_sprite[i] >> 8));
    }
    for (int i = 0; i < 4; i++) {
        data.push_back(static_cast<uint8_t>(m_chr_banks_bg[i] & 0xFF));
        data.push_back(static_cast<uint8_t>(m_chr_banks_bg[i] >> 8));
    }
    data.push_back(m_chr_upper_bits);
    data.push_back(m_last_chr_write_was_bg ? 1 : 0);

    // Split screen
    data.push_back(m_split_mode);
    data.push_back(m_split_scroll);
    data.push_back(m_split_bank);

    // IRQ
    data.push_back(m_irq_scanline);
    data.push_back(m_irq_enabled ? 1 : 0);
    data.push_back(m_irq_pending ? 1 : 0);
    data.push_back(m_in_frame ? 1 : 0);

    // Multiplier
    data.push_back(m_multiplicand);
    data.push_back(m_multiplier);

    // Scanline counter
    data.push_back(m_scanline_counter);

    // ExRAM
    data.insert(data.end(), m_exram.begin(), m_exram.end());
}

void Mapper005::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 40) return;  // Minimum state size

    // Mode registers
    m_prg_mode = *data++; remaining--;
    m_chr_mode = *data++; remaining--;
    m_prg_ram_protect1 = *data++; remaining--;
    m_prg_ram_protect2 = *data++; remaining--;
    m_exram_mode = *data++; remaining--;
    m_nametable_mapping = *data++; remaining--;
    m_fill_tile = *data++; remaining--;
    m_fill_attribute = *data++; remaining--;

    // PRG banking
    m_prg_ram_bank = *data++; remaining--;
    for (int i = 0; i < 4; i++) {
        m_prg_banks[i] = *data++; remaining--;
    }

    // CHR banking
    for (int i = 0; i < 8; i++) {
        uint8_t lo = *data++; remaining--;
        uint8_t hi = *data++; remaining--;
        m_chr_banks_sprite[i] = lo | (static_cast<uint16_t>(hi) << 8);
    }
    for (int i = 0; i < 4; i++) {
        uint8_t lo = *data++; remaining--;
        uint8_t hi = *data++; remaining--;
        m_chr_banks_bg[i] = lo | (static_cast<uint16_t>(hi) << 8);
    }
    m_chr_upper_bits = *data++; remaining--;
    m_last_chr_write_was_bg = (*data++ != 0); remaining--;

    // Split screen
    m_split_mode = *data++; remaining--;
    m_split_scroll = *data++; remaining--;
    m_split_bank = *data++; remaining--;

    // IRQ
    m_irq_scanline = *data++; remaining--;
    m_irq_enabled = (*data++ != 0); remaining--;
    m_irq_pending = (*data++ != 0); remaining--;
    m_in_frame = (*data++ != 0); remaining--;

    // Multiplier
    m_multiplicand = *data++; remaining--;
    m_multiplier = *data++; remaining--;

    // Scanline counter
    m_scanline_counter = *data++; remaining--;

    // ExRAM
    if (remaining >= 1024) {
        std::memcpy(m_exram.data(), data, 1024);
        data += 1024;
        remaining -= 1024;
    }

    // Reset audio state on load
    m_mmc5_pulse[0] = MMC5Pulse{};
    m_mmc5_pulse[1] = MMC5Pulse{};
    m_pcm_output = 0;
    m_audio_output = 0.0f;
    m_audio_cycles = 0;
}

void Mapper005::cpu_cycles(int count) {
    // PERFORMANCE: Process all cycles at once instead of one at a time

    // Add cycles to divider and process audio in batches
    m_audio_divider += count;

    // Process each complete divider period
    while (m_audio_divider >= AUDIO_DIVIDER_PERIOD) {
        m_audio_divider -= AUDIO_DIVIDER_PERIOD;
        m_audio_cycles += AUDIO_DIVIDER_PERIOD;

        // (Audio processing code follows unchanged - called once per AUDIO_DIVIDER_PERIOD cycles)
        process_audio_batch();
    }
}

void Mapper005::cpu_cycle() {
    // Single-cycle version for compatibility - delegates to batched version
    cpu_cycles(1);
}

// Internal helper function for audio processing batch
void Mapper005::process_audio_batch() {

    // MMC5 duty cycle table
    static const uint8_t duty_table[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1},  // 12.5%
        {0, 0, 0, 0, 0, 0, 1, 1},  // 25%
        {0, 0, 0, 0, 1, 1, 1, 1},  // 50%
        {1, 1, 1, 1, 1, 1, 0, 0}   // 75% (inverted 25%)
    };

    // Clock pulse channels - batch update timers (clocked every 2 CPU cycles originally)
    // With divider of 16, we process 8 timer clocks worth of updates
    constexpr uint16_t TIMER_CLOCKS = AUDIO_DIVIDER_PERIOD / 2;
    for (int p = 0; p < 2; p++) {
        MMC5Pulse& pulse = m_mmc5_pulse[p];

        if (pulse.timer_period == 0) continue;

        uint16_t period_plus_1 = pulse.timer_period + 1;
        if (pulse.timer >= TIMER_CLOCKS) {
            pulse.timer -= TIMER_CLOCKS;
        } else {
            // Timer will underflow - calculate how many periods elapsed
            uint16_t remaining = TIMER_CLOCKS - pulse.timer - 1;
            uint16_t full_periods = remaining / period_plus_1;
            uint16_t leftover = remaining % period_plus_1;
            pulse.sequence_pos = (pulse.sequence_pos + 1 + full_periods) & 7;
            pulse.timer = pulse.timer_period - leftover;
        }
    }

    // Frame counter equivalent for envelopes and length counters
    // MMC5 runs these at approximately 240Hz (every ~7457 CPU cycles)
    if (m_audio_cycles >= 7457) {
        m_audio_cycles = 0;

        for (int p = 0; p < 2; p++) {
            MMC5Pulse& pulse = m_mmc5_pulse[p];

            // Clock envelope
            if (pulse.envelope_start) {
                pulse.envelope_start = false;
                pulse.envelope_counter = 15;
                pulse.envelope_divider = pulse.volume;
            } else {
                if (pulse.envelope_divider == 0) {
                    pulse.envelope_divider = pulse.volume;
                    if (pulse.envelope_counter > 0) {
                        pulse.envelope_counter--;
                    } else if (pulse.length_halt) {
                        pulse.envelope_counter = 15;
                    }
                } else {
                    pulse.envelope_divider--;
                }
            }

            // Clock length counter
            if (!pulse.length_halt && pulse.length_counter > 0) {
                pulse.length_counter--;
            }
        }
    }

    // Calculate output
    int mix = 0;

    for (int p = 0; p < 2; p++) {
        MMC5Pulse& pulse = m_mmc5_pulse[p];

        if (pulse.enabled && pulse.length_counter > 0 && pulse.timer_period >= 8) {
            uint8_t volume = pulse.constant_volume ? pulse.volume : pulse.envelope_counter;
            if (duty_table[pulse.duty][pulse.sequence_pos]) {
                mix += volume;
            }
        }
    }

    // Add PCM output (8-bit unsigned, centered)
    int pcm = static_cast<int>(m_pcm_output) - 128;

    // Normalize and combine outputs
    float pulse_output = mix / 30.0f;
    float pcm_output = pcm / 256.0f;

    m_audio_output = (pulse_output + pcm_output - 0.5f) * 2.0f;
}

} // namespace nes
