#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace gba {

// Save types for GBA cartridges
enum class SaveType {
    None,
    SRAM_32K,      // 32KB SRAM
    EEPROM_512,    // 512 bytes EEPROM (4Kbit)
    EEPROM_8K,     // 8KB EEPROM (64Kbit)
    Flash_64K,     // 64KB Flash
    Flash_128K     // 128KB Flash (2 banks)
};

// Flash memory state machine states
enum class FlashState {
    Ready,
    Command1,        // Received 0xAA at 0x5555
    Command2,        // Received 0x55 at 0x2AAA
    Erase,           // Received 0x80, waiting for AA-55-10/30 erase sequence
    EraseCommand1,   // Received AA after Erase state
    EraseCommand2,   // Received 55 after EraseCommand1, waiting for 10/30
    Write,           // Waiting for byte to write
    BankSwitch,      // Waiting for bank number
    ChipID           // Chip identification mode
};

// RTC state machine states
enum class RTCState {
    Idle,
    ReceiveCommand,
    ReceiveData,
    SendData
};

// GBA Cartridge loader with Flash/EEPROM and RTC/GPIO support
class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    // Load ROM
    bool load(const uint8_t* data, size_t size, SystemType system_type);
    void unload();

    // Reset
    void reset();

    // ROM access (includes GPIO handling for RTC games)
    uint8_t read_rom(uint32_t address);
    void write_rom(uint32_t address, uint8_t value);  // For GPIO writes

    uint8_t read_sram(uint32_t address);
    void write_sram(uint32_t address, uint8_t value);

    // Get CRC32
    uint32_t get_crc32() const { return m_crc32; }

    // ROM info
    bool is_loaded() const { return m_loaded; }
    const std::string& get_title() const { return m_title; }
    SaveType get_save_type() const { return m_save_type; }

    // Battery save support
    bool has_battery() const;
    std::vector<uint8_t> get_save_data() const;
    bool set_save_data(const std::vector<uint8_t>& data);

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    uint32_t calculate_crc32(const uint8_t* data, size_t size);
    SaveType detect_save_type(const uint8_t* data, size_t size);

    // Flash memory handling
    uint8_t read_flash(uint32_t address);
    void write_flash(uint32_t address, uint8_t value);
    void reset_flash_state();

    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_save_data;  // SRAM or Flash data

    bool m_loaded = false;
    uint32_t m_crc32 = 0;
    std::string m_title;
    SaveType m_save_type = SaveType::None;

    // Flash memory state
    FlashState m_flash_state = FlashState::Ready;
    uint8_t m_flash_bank = 0;          // Current bank for 128KB Flash
    bool m_flash_id_mode = false;      // Chip ID mode active

    // Flash chip IDs (Sanyo LE26FV10N1TS-10 for 128KB)
    // Manufacturer ID: 0x62 (Sanyo), Device ID: 0x13 (128KB)
    static constexpr uint8_t FLASH_128K_MANUFACTURER = 0x62;
    static constexpr uint8_t FLASH_128K_DEVICE = 0x13;

    // For 64KB Flash (Panasonic MN63F805MNP)
    static constexpr uint8_t FLASH_64K_MANUFACTURER = 0x32;
    static constexpr uint8_t FLASH_64K_DEVICE = 0x1B;

    // GPIO/RTC support
    bool m_has_rtc = false;
    uint8_t m_gpio_data = 0;       // 0x080000C4
    uint8_t m_gpio_direction = 0;  // 0x080000C6 (1 = output from GBA)
    uint8_t m_gpio_control = 0;    // 0x080000C8 (1 = GPIO enabled)

    // RTC state machine
    RTCState m_rtc_state = RTCState::Idle;
    uint8_t m_rtc_command = 0;
    uint8_t m_rtc_data[8] = {0};
    int m_rtc_bit_count = 0;
    int m_rtc_byte_count = 0;
    uint8_t m_rtc_serial_data = 0;
    bool m_rtc_last_sck = false;

    // GPIO pin definitions for RTC
    static constexpr uint8_t GPIO_SCK = 0x01;  // Bit 0: Clock
    static constexpr uint8_t GPIO_SIO = 0x02;  // Bit 1: Data
    static constexpr uint8_t GPIO_CS  = 0x04;  // Bit 2: Chip Select

    // RTC helper functions
    void rtc_clock_edge();
    uint8_t rtc_get_output();
    void rtc_process_command();
    bool detect_rtc(const uint8_t* data, size_t size);
};

} // namespace gba
