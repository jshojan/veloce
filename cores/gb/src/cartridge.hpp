#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <string>

namespace gb {

class MBC;

// Cartridge loader for GB and GBC ROMs
class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    // Load ROM
    bool load(const uint8_t* data, size_t size);
    void unload();

    // Reset
    void reset();

    // ROM access (via MBC)
    uint8_t read_rom(uint16_t address);
    uint8_t read_ram(uint16_t address);
    void write_ram(uint16_t address, uint8_t value);
    void write_mbc(uint16_t address, uint8_t value);

    // Get CRC32
    uint32_t get_crc32() const { return m_crc32; }

    // ROM info
    bool is_loaded() const { return m_loaded; }
    SystemType get_system_type() const { return m_system_type; }
    const std::string& get_title() const { return m_title; }
    bool is_cgb() const { return m_system_type == SystemType::GameBoyColor; }

    // Battery save support
    bool has_battery() const;
    std::vector<uint8_t> get_save_data() const;
    bool set_save_data(const std::vector<uint8_t>& data);

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    void detect_mbc(uint8_t cart_type);
    uint32_t calculate_crc32(const uint8_t* data, size_t size);

    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_ram;  // Cartridge RAM

    std::unique_ptr<MBC> m_mbc;

    bool m_loaded = false;
    uint32_t m_crc32 = 0;
    SystemType m_system_type = SystemType::GameBoy;
    std::string m_title;

    // Cartridge info
    int m_mbc_type = 0;
    bool m_has_battery = false;
    bool m_has_rtc = false;
    int m_rom_banks = 0;
    int m_ram_banks = 0;
};

} // namespace gb
