#pragma once

#include "mapper.hpp"
#include <array>
#include <string>

namespace nes {

// Mapper 16: Bandai FCG (FCG-1, FCG-2, LZ93D50)
// Used by: Dragon Ball Z series, SD Gundam, Famicom Jump, etc.
// Features:
// - PRG ROM: 16KB switchable at $8000-$BFFF, fixed last bank at $C000-$FFFF
// - CHR ROM: 8 x 1KB switchable banks
// - IRQ: 16-bit down counter
// - EEPROM: 24C01 (128 bytes) or 24C02 (256 bytes) for save data
//
// Variants:
// - FCG-1/FCG-2: No EEPROM (mapper 153 behavior)
// - LZ93D50 + 24C01: 128 bytes EEPROM
// - LZ93D50 + 24C02: 256 bytes EEPROM
class Mapper016 : public Mapper {
public:
    // EEPROM type enumeration
    enum class EepromType {
        None,       // FCG-1/FCG-2 - no EEPROM
        EEPROM_24C01,  // 128 bytes (1024 bits)
        EEPROM_24C02   // 256 bytes (2048 bits)
    };

    Mapper016(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram,
              EepromType eeprom_type = EepromType::EEPROM_24C02);

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;

    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;

    MirrorMode get_mirror_mode() const override { return m_mirror_mode; }

    bool irq_pending(uint32_t frame_cycle = 0) override;
    void irq_clear() override;
    void notify_frame_start() override;

    void reset() override;
    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

    // EEPROM data access for save file support
    const std::vector<uint8_t>& get_eeprom_data() const { return m_eeprom_data; }
    void set_eeprom_data(const std::vector<uint8_t>& data);
    bool has_eeprom() const { return m_eeprom_type != EepromType::None; }
    size_t get_eeprom_size() const;

    // Override mapper save data methods for EEPROM support
    bool has_mapper_save_data() const override { return has_eeprom(); }
    std::vector<uint8_t> get_mapper_save_data() const override { return m_eeprom_data; }
    bool set_mapper_save_data(const std::vector<uint8_t>& data) override;

private:
    void update_prg_bank();
    void update_chr_banks();

    // I2C EEPROM state machine
    void eeprom_write(uint8_t value);
    uint8_t eeprom_read();
    void eeprom_clock_rise();
    void eeprom_start_condition();
    void eeprom_stop_condition();
    void eeprom_process_byte();

    // PRG banking
    uint8_t m_prg_bank_reg = 0;
    uint32_t m_prg_bank_offset = 0;

    // CHR banking (8 x 1KB)
    std::array<uint8_t, 8> m_chr_bank_regs = {};
    std::array<uint32_t, 8> m_chr_bank_offsets = {};

    // IRQ counter (clocked by CPU cycles)
    uint16_t m_irq_counter = 0;
    uint16_t m_irq_latch = 0;
    bool m_irq_enabled = false;
    bool m_irq_pending = false;
    uint32_t m_last_frame_cycle = 0;  // For CPU cycle-based IRQ clocking

    // EEPROM configuration
    EepromType m_eeprom_type = EepromType::None;
    std::vector<uint8_t> m_eeprom_data;

    // I2C state machine
    enum class I2CState {
        Idle,
        DeviceAddress,  // Receiving 8-bit device address
        WordAddress,    // Receiving 8-bit word address (memory location)
        Data,           // Receiving/sending data
        Ack             // Acknowledge phase
    };

    I2CState m_i2c_state = I2CState::Idle;
    bool m_i2c_sda_out = true;      // SDA output from mapper to EEPROM
    bool m_i2c_scl = true;          // SCL (clock) line
    bool m_i2c_sda_in = true;       // SDA input from EEPROM to mapper
    bool m_i2c_prev_sda = true;     // Previous SDA state for edge detection
    bool m_i2c_prev_scl = true;     // Previous SCL state for edge detection

    uint8_t m_i2c_shift_reg = 0;    // Shift register for I2C data
    int m_i2c_bit_count = 0;        // Bits received/sent in current byte
    bool m_i2c_read_mode = false;   // true = read from EEPROM, false = write
    uint8_t m_i2c_device_addr = 0;  // Device address byte
    uint8_t m_i2c_word_addr = 0;    // Word address (memory location)
    bool m_i2c_addr_received = false; // Have we received the word address?
    bool m_i2c_ack_pending = false;  // Need to send ACK on next clock
    uint8_t m_i2c_output_byte = 0;   // Byte being shifted out during read
};

} // namespace nes
