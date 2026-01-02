#include "mapper_016.hpp"
#include <algorithm>
#include <cstring>

namespace nes {

Mapper016::Mapper016(std::vector<uint8_t>& prg_rom,
                     std::vector<uint8_t>& chr_rom,
                     std::vector<uint8_t>& prg_ram,
                     MirrorMode mirror,
                     bool has_chr_ram,
                     EepromType eeprom_type)
{
    m_prg_rom = &prg_rom;
    m_chr_rom = &chr_rom;
    m_prg_ram = &prg_ram;
    m_mirror_mode = mirror;
    m_has_chr_ram = has_chr_ram;
    m_eeprom_type = eeprom_type;

    // Initialize EEPROM data based on type
    size_t eeprom_size = get_eeprom_size();
    if (eeprom_size > 0) {
        m_eeprom_data.resize(eeprom_size, 0xFF);  // EEPROM defaults to all 1s
    }

    reset();
}

void Mapper016::reset() {
    // Reset PRG banking - bank 0 at $8000
    m_prg_bank_reg = 0;
    update_prg_bank();

    // Reset CHR banking - all banks to 0
    m_chr_bank_regs.fill(0);
    update_chr_banks();

    // Reset IRQ state
    m_irq_counter = 0;
    m_irq_latch = 0;
    m_irq_enabled = false;
    m_irq_pending = false;
    m_last_frame_cycle = 0;

    // Reset I2C state (don't reset EEPROM data - that's save data!)
    m_i2c_state = I2CState::Idle;
    m_i2c_sda_out = true;
    m_i2c_scl = true;
    m_i2c_sda_in = true;
    m_i2c_prev_sda = true;
    m_i2c_prev_scl = true;
    m_i2c_shift_reg = 0;
    m_i2c_bit_count = 0;
    m_i2c_read_mode = false;
    m_i2c_device_addr = 0;
    m_i2c_word_addr = 0;
    m_i2c_addr_received = false;
    m_i2c_ack_pending = false;
    m_i2c_output_byte = 0;
}

size_t Mapper016::get_eeprom_size() const {
    switch (m_eeprom_type) {
        case EepromType::EEPROM_24C01:
            return 128;
        case EepromType::EEPROM_24C02:
            return 256;
        default:
            return 0;
    }
}

void Mapper016::set_eeprom_data(const std::vector<uint8_t>& data) {
    size_t eeprom_size = get_eeprom_size();
    if (eeprom_size == 0) return;

    m_eeprom_data.resize(eeprom_size, 0xFF);
    size_t copy_size = std::min(data.size(), eeprom_size);
    std::memcpy(m_eeprom_data.data(), data.data(), copy_size);
}

bool Mapper016::set_mapper_save_data(const std::vector<uint8_t>& data) {
    if (!has_eeprom() || data.empty()) {
        return false;
    }
    set_eeprom_data(data);
    return true;
}

void Mapper016::update_prg_bank() {
    uint32_t prg_size = m_prg_rom->size();
    if (prg_size == 0) return;

    uint32_t bank_count = prg_size / 0x4000;  // 16KB banks
    if (bank_count == 0) bank_count = 1;

    // PRG bank register selects 16KB bank at $8000-$BFFF
    m_prg_bank_offset = (m_prg_bank_reg % bank_count) * 0x4000;
}

void Mapper016::update_chr_banks() {
    uint32_t chr_size = m_chr_rom->size();
    if (chr_size == 0) return;

    uint32_t bank_count = chr_size / 0x400;  // 1KB banks
    if (bank_count == 0) bank_count = 1;

    for (int i = 0; i < 8; i++) {
        m_chr_bank_offsets[i] = (m_chr_bank_regs[i] % bank_count) * 0x400;
    }
}

uint8_t Mapper016::cpu_read(uint16_t address) {
    // EEPROM read at $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        // Return EEPROM SDA input state in bit 4
        return eeprom_read();
    }

    // PRG ROM: $8000-$BFFF (switchable), $C000-$FFFF (fixed to last bank)
    if (address >= 0x8000) {
        uint32_t prg_size = m_prg_rom->size();
        if (prg_size == 0) return 0;

        uint32_t offset;
        if (address < 0xC000) {
            // Switchable bank at $8000-$BFFF
            offset = m_prg_bank_offset + (address & 0x3FFF);
        } else {
            // Fixed last 16KB bank at $C000-$FFFF
            uint32_t last_bank_offset = prg_size - 0x4000;
            offset = last_bank_offset + (address & 0x3FFF);
        }

        if (offset < prg_size) {
            return (*m_prg_rom)[offset];
        }
    }

    return 0;
}

void Mapper016::cpu_write(uint16_t address, uint8_t value) {
    // EEPROM control at $6000-$7FFF (some variants)
    if (address >= 0x6000 && address < 0x8000) {
        eeprom_write(value);
        return;
    }

    // Registers at $8000-$FFFF
    // The exact address mapping varies by variant, but the lower nibble determines function
    if (address >= 0x8000) {
        uint8_t reg = address & 0x000F;

        switch (reg) {
            case 0x0: case 0x1: case 0x2: case 0x3:
            case 0x4: case 0x5: case 0x6: case 0x7:
                // CHR bank registers 0-7 (1KB each)
                m_chr_bank_regs[reg] = value;
                update_chr_banks();
                break;

            case 0x8:
                // PRG bank register
                m_prg_bank_reg = value & 0x0F;
                update_prg_bank();
                break;

            case 0x9:
                // Mirroring control
                switch (value & 0x03) {
                    case 0: m_mirror_mode = MirrorMode::Vertical; break;
                    case 1: m_mirror_mode = MirrorMode::Horizontal; break;
                    case 2: m_mirror_mode = MirrorMode::SingleScreen0; break;
                    case 3: m_mirror_mode = MirrorMode::SingleScreen1; break;
                }
                break;

            case 0xA:
                // IRQ control
                // Bit 0: IRQ enable
                // Writing to this register also acknowledges IRQ
                m_irq_enabled = (value & 0x01) != 0;
                m_irq_pending = false;
                // Some variants reload counter on enable
                m_irq_counter = m_irq_latch;
                break;

            case 0xB:
                // IRQ counter low byte
                m_irq_latch = (m_irq_latch & 0xFF00) | value;
                break;

            case 0xC:
                // IRQ counter high byte
                m_irq_latch = (m_irq_latch & 0x00FF) | (static_cast<uint16_t>(value) << 8);
                break;

            case 0xD:
                // EEPROM control (directly controlled I2C lines)
                // Bit 5: SCL (clock)
                // Bit 6: SDA output
                eeprom_write(value);
                break;

            default:
                // Other registers unused
                break;
        }
    }
}

uint8_t Mapper016::ppu_read(uint16_t address, uint32_t frame_cycle) {
    (void)frame_cycle;

    if (address < 0x2000) {
        // CHR ROM/RAM: 8 x 1KB banks
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offsets[bank] + (address & 0x3FF);

        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
    }
    return 0;
}

void Mapper016::ppu_write(uint16_t address, uint8_t value) {
    if (!m_has_chr_ram) return;

    if (address < 0x2000) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offsets[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
    }
}

bool Mapper016::irq_pending(uint32_t frame_cycle) {
    // The Bandai FCG IRQ counter is clocked on every CPU cycle (M2 cycle).
    // frame_cycle is in PPU cycles, and there are 3 PPU cycles per CPU cycle.
    //
    // We calculate how many CPU cycles have passed since the last check
    // and decrement the counter accordingly.

    if (m_irq_enabled && m_irq_counter > 0) {
        // Calculate elapsed PPU cycles (handling wraparound at frame boundary)
        uint32_t elapsed_ppu;
        if (frame_cycle >= m_last_frame_cycle) {
            elapsed_ppu = frame_cycle - m_last_frame_cycle;
        } else {
            // Frame wrapped around (89342 PPU cycles per NTSC frame)
            elapsed_ppu = frame_cycle + (89342 - m_last_frame_cycle);
        }

        // Convert to CPU cycles (3 PPU cycles = 1 CPU cycle)
        uint32_t elapsed_cpu = elapsed_ppu / 3;

        if (elapsed_cpu > 0) {
            // Decrement counter by elapsed CPU cycles
            if (elapsed_cpu >= m_irq_counter) {
                m_irq_counter = 0;
                m_irq_pending = true;
            } else {
                m_irq_counter -= static_cast<uint16_t>(elapsed_cpu);
            }
        }
    }

    m_last_frame_cycle = frame_cycle;
    return m_irq_pending;
}

void Mapper016::irq_clear() {
    m_irq_pending = false;
}

void Mapper016::notify_frame_start() {
    // Reset frame cycle tracking at frame start to prevent timing drift
    m_last_frame_cycle = 0;
}

// ============================================================================
// I2C EEPROM Implementation
// ============================================================================
//
// The 24C01/24C02 EEPROMs use a two-wire I2C protocol:
// - SCL: Serial Clock Line (directly controlled by mapper)
// - SDA: Serial Data Line (bidirectional)
//
// Protocol basics:
// - START condition: SDA goes low while SCL is high
// - STOP condition: SDA goes high while SCL is high
// - Data bit: Sampled on SCL rising edge, changed when SCL is low
// - Bytes are sent MSB first, followed by ACK bit
//
// Write sequence:
// 1. START
// 2. Device address byte (1010xxxW, W=0 for write)
// 3. Word address byte (memory location)
// 4. Data byte(s)
// 5. STOP
//
// Read sequence (random):
// 1. START
// 2. Device address (write mode to set address)
// 3. Word address
// 4. START (repeated start)
// 5. Device address (read mode)
// 6. Read data byte(s) - EEPROM drives SDA
// 7. STOP

void Mapper016::eeprom_write(uint8_t value) {
    if (m_eeprom_type == EepromType::None) return;

    // Extract I2C control bits
    bool new_scl = (value & 0x20) != 0;  // Bit 5: SCL
    bool new_sda = (value & 0x40) != 0;  // Bit 6: SDA output

    // Check for START condition: SDA falls while SCL is high
    if (m_i2c_scl && new_scl && m_i2c_sda_out && !new_sda) {
        eeprom_start_condition();
    }
    // Check for STOP condition: SDA rises while SCL is high
    else if (m_i2c_scl && new_scl && !m_i2c_sda_out && new_sda) {
        eeprom_stop_condition();
    }
    // Check for rising edge of SCL (data is clocked)
    else if (!m_i2c_scl && new_scl) {
        eeprom_clock_rise();
    }

    // Update state
    m_i2c_prev_sda = m_i2c_sda_out;
    m_i2c_prev_scl = m_i2c_scl;
    m_i2c_sda_out = new_sda;
    m_i2c_scl = new_scl;
}

uint8_t Mapper016::eeprom_read() {
    if (m_eeprom_type == EepromType::None) {
        return 0;
    }

    // Return EEPROM SDA state in bit 4
    // When EEPROM is outputting data or ACK, this reflects the EEPROM's SDA
    return m_i2c_sda_in ? 0x10 : 0x00;
}

void Mapper016::eeprom_start_condition() {
    // START condition detected - begin new transaction
    m_i2c_state = I2CState::DeviceAddress;
    m_i2c_bit_count = 0;
    m_i2c_shift_reg = 0;
    m_i2c_read_mode = false;
    m_i2c_ack_pending = false;
    m_i2c_sda_in = true;  // Release SDA line
}

void Mapper016::eeprom_stop_condition() {
    // STOP condition - end transaction
    m_i2c_state = I2CState::Idle;
    m_i2c_bit_count = 0;
    m_i2c_sda_in = true;  // Release SDA line
}

void Mapper016::eeprom_clock_rise() {
    // Data is sampled on rising edge of SCL

    if (m_i2c_state == I2CState::Idle) {
        return;
    }

    // If we're sending an ACK, the EEPROM pulls SDA low
    if (m_i2c_ack_pending) {
        m_i2c_ack_pending = false;
        m_i2c_sda_in = false;  // ACK (pull low)
        return;
    }

    // If we're in read mode and sending data out
    if (m_i2c_state == I2CState::Data && m_i2c_read_mode) {
        // Shift out the next bit (MSB first)
        m_i2c_sda_in = (m_i2c_output_byte & 0x80) != 0;
        m_i2c_output_byte <<= 1;
        m_i2c_bit_count++;

        if (m_i2c_bit_count >= 8) {
            // Byte sent, wait for master ACK/NACK
            // If master ACKs (SDA low), continue with next byte
            // If master NACKs (SDA high), stop
            m_i2c_bit_count = 0;
            m_i2c_sda_in = true;  // Release SDA for master ACK

            // Prepare next byte (sequential read)
            m_i2c_word_addr++;
            size_t eeprom_size = get_eeprom_size();
            if (eeprom_size > 0) {
                m_i2c_word_addr %= eeprom_size;
                m_i2c_output_byte = m_eeprom_data[m_i2c_word_addr];
            }
        }
        return;
    }

    // Receiving data from master (write mode)
    // Shift in the SDA bit
    m_i2c_shift_reg = (m_i2c_shift_reg << 1) | (m_i2c_sda_out ? 1 : 0);
    m_i2c_bit_count++;

    // After 8 bits, process the byte
    if (m_i2c_bit_count >= 8) {
        eeprom_process_byte();
    } else {
        m_i2c_sda_in = true;  // Release SDA while receiving
    }
}

void Mapper016::eeprom_process_byte() {
    uint8_t byte = m_i2c_shift_reg;
    m_i2c_bit_count = 0;
    m_i2c_shift_reg = 0;

    switch (m_i2c_state) {
        case I2CState::DeviceAddress:
            // Device address format: 1010xxxR
            // Where xxx = device select bits (usually 000)
            // R = 0 for write, 1 for read
            m_i2c_device_addr = byte;
            m_i2c_read_mode = (byte & 0x01) != 0;

            // Check if this is a valid EEPROM address (starts with 1010)
            if ((byte & 0xF0) == 0xA0) {
                // Send ACK
                m_i2c_ack_pending = true;
                m_i2c_sda_in = false;  // ACK

                if (m_i2c_read_mode) {
                    // Read mode - start outputting data from current address
                    m_i2c_state = I2CState::Data;
                    size_t eeprom_size = get_eeprom_size();
                    if (eeprom_size > 0) {
                        m_i2c_word_addr %= eeprom_size;
                        m_i2c_output_byte = m_eeprom_data[m_i2c_word_addr];
                    } else {
                        m_i2c_output_byte = 0xFF;
                    }
                } else {
                    // Write mode - expect word address next
                    if (m_i2c_addr_received) {
                        // If we already have address (repeated start), go to data
                        m_i2c_state = I2CState::Data;
                    } else {
                        m_i2c_state = I2CState::WordAddress;
                    }
                }
            } else {
                // Invalid address - NACK
                m_i2c_sda_in = true;  // NACK (release SDA)
                m_i2c_state = I2CState::Idle;
            }
            break;

        case I2CState::WordAddress:
            // Word address received
            m_i2c_word_addr = byte;
            m_i2c_addr_received = true;

            // For 24C01, only lower 7 bits are used (128 bytes)
            // For 24C02, all 8 bits are used (256 bytes)
            if (m_eeprom_type == EepromType::EEPROM_24C01) {
                m_i2c_word_addr &= 0x7F;
            }

            // Send ACK
            m_i2c_ack_pending = true;
            m_i2c_sda_in = false;

            // Move to data phase
            m_i2c_state = I2CState::Data;
            break;

        case I2CState::Data:
            if (!m_i2c_read_mode) {
                // Write data byte to EEPROM
                size_t eeprom_size = get_eeprom_size();
                if (eeprom_size > 0 && m_i2c_word_addr < eeprom_size) {
                    m_eeprom_data[m_i2c_word_addr] = byte;
                }

                // Increment address (with wraparound)
                m_i2c_word_addr++;
                if (eeprom_size > 0) {
                    // Page write behavior varies by chip, but for simplicity
                    // we allow sequential writes to wrap at page boundaries
                    // 24C01: 8-byte pages, 24C02: 8-byte pages
                    // For random access emulation, we just increment
                    m_i2c_word_addr %= eeprom_size;
                }

                // Send ACK
                m_i2c_ack_pending = true;
                m_i2c_sda_in = false;
            }
            // Read mode is handled in clock_rise()
            break;

        default:
            m_i2c_sda_in = true;  // NACK for unexpected state
            break;
    }
}

// ============================================================================
// Save State
// ============================================================================

void Mapper016::save_state(std::vector<uint8_t>& data) {
    // PRG banking
    data.push_back(m_prg_bank_reg);

    // CHR banking
    for (int i = 0; i < 8; i++) {
        data.push_back(m_chr_bank_regs[i]);
    }

    // Mirroring
    data.push_back(static_cast<uint8_t>(m_mirror_mode));

    // IRQ state
    data.push_back(m_irq_counter & 0xFF);
    data.push_back((m_irq_counter >> 8) & 0xFF);
    data.push_back(m_irq_latch & 0xFF);
    data.push_back((m_irq_latch >> 8) & 0xFF);
    data.push_back(m_irq_enabled ? 1 : 0);
    data.push_back(m_irq_pending ? 1 : 0);

    // I2C state
    data.push_back(static_cast<uint8_t>(m_i2c_state));
    data.push_back(m_i2c_sda_out ? 1 : 0);
    data.push_back(m_i2c_scl ? 1 : 0);
    data.push_back(m_i2c_sda_in ? 1 : 0);
    data.push_back(m_i2c_shift_reg);
    data.push_back(m_i2c_bit_count);
    data.push_back(m_i2c_read_mode ? 1 : 0);
    data.push_back(m_i2c_device_addr);
    data.push_back(m_i2c_word_addr);
    data.push_back(m_i2c_addr_received ? 1 : 0);
    data.push_back(m_i2c_ack_pending ? 1 : 0);
    data.push_back(m_i2c_output_byte);

    // EEPROM data (important for save data!)
    uint16_t eeprom_size = static_cast<uint16_t>(m_eeprom_data.size());
    data.push_back(eeprom_size & 0xFF);
    data.push_back((eeprom_size >> 8) & 0xFF);
    for (uint8_t byte : m_eeprom_data) {
        data.push_back(byte);
    }
}

void Mapper016::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 24) return;  // Minimum state size

    // PRG banking
    m_prg_bank_reg = *data++; remaining--;
    update_prg_bank();

    // CHR banking
    for (int i = 0; i < 8; i++) {
        m_chr_bank_regs[i] = *data++; remaining--;
    }
    update_chr_banks();

    // Mirroring
    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    // IRQ state
    m_irq_counter = *data++; remaining--;
    m_irq_counter |= static_cast<uint16_t>(*data++) << 8; remaining--;
    m_irq_latch = *data++; remaining--;
    m_irq_latch |= static_cast<uint16_t>(*data++) << 8; remaining--;
    m_irq_enabled = (*data++ != 0); remaining--;
    m_irq_pending = (*data++ != 0); remaining--;

    // I2C state
    m_i2c_state = static_cast<I2CState>(*data++); remaining--;
    m_i2c_sda_out = (*data++ != 0); remaining--;
    m_i2c_scl = (*data++ != 0); remaining--;
    m_i2c_sda_in = (*data++ != 0); remaining--;
    m_i2c_shift_reg = *data++; remaining--;
    m_i2c_bit_count = *data++; remaining--;
    m_i2c_read_mode = (*data++ != 0); remaining--;
    m_i2c_device_addr = *data++; remaining--;
    m_i2c_word_addr = *data++; remaining--;
    m_i2c_addr_received = (*data++ != 0); remaining--;
    m_i2c_ack_pending = (*data++ != 0); remaining--;
    m_i2c_output_byte = *data++; remaining--;

    // EEPROM data
    if (remaining >= 2) {
        uint16_t eeprom_size = *data++; remaining--;
        eeprom_size |= static_cast<uint16_t>(*data++) << 8; remaining--;

        if (remaining >= eeprom_size) {
            m_eeprom_data.resize(eeprom_size);
            for (uint16_t i = 0; i < eeprom_size; i++) {
                m_eeprom_data[i] = *data++; remaining--;
            }
        }
    }
}

} // namespace nes
