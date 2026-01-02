#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <array>

namespace snes {

// SNES ROM mapping types
enum class MapperType {
    LoROM,      // Mode $20 - PRG mapped to banks $00-$7D, $80-$FF at $8000-$FFFF
    HiROM,      // Mode $21 - PRG mapped to banks $40-$7D, $C0-$FF at $0000-$FFFF
    ExHiROM,    // Mode $25 - Extended HiROM for ROMs > 4MB
    SA1,        // Mode $23 - SA-1 coprocessor
    SDD1,       // Mode $32 - S-DD1 decompression
    SuperFX,    // Mode $20 with SuperFX
    Unknown
};

// SNES ROM header (located at different offsets depending on mapping)
// LoROM: $007FC0-$007FFF
// HiROM: $00FFC0-$00FFFF
struct SNESHeader {
    char title[21];           // $00-$14: Game title (ASCII, space-padded)
    uint8_t map_mode;         // $15: Mapping mode
    uint8_t rom_type;         // $16: ROM type (with/without RAM, battery, coprocessor)
    uint8_t rom_size;         // $17: ROM size (log2(size in KB))
    uint8_t ram_size;         // $18: RAM size (log2(size in KB))
    uint8_t region;           // $19: Country/region code
    uint8_t developer_id;     // $1A: Developer ID (old format)
    uint8_t version;          // $1B: Version number
    uint16_t checksum_comp;   // $1C-$1D: Checksum complement
    uint16_t checksum;        // $1E-$1F: Checksum

    // Interrupt vectors ($FFE0-$FFFF in native mode, $FFF0-$FFFF in emulation mode)
    // These follow the header in memory but we read them separately
};

// Enhancement chip types
enum class EnhancementChip {
    None,
    DSP1,       // DSP-1 / DSP-1A / DSP-1B
    DSP2,       // DSP-2
    DSP3,       // DSP-3
    DSP4,       // DSP-4
    SuperFX,    // Super FX / Super FX 2
    SA1,        // SA-1
    SDD1,       // S-DD1
    SPC7110,    // SPC7110
    ST010,      // ST010
    ST011,      // ST011
    ST018,      // ST018
    CX4,        // Cx4
    OBC1,       // OBC1
    SRTC        // S-RTC
};

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    // Load ROM from memory
    bool load(const uint8_t* data, size_t size);

    // Unload current ROM
    void unload();

    // Reset cartridge state
    void reset();

    // Memory access (24-bit address space)
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t value);

    // ROM info
    uint32_t get_crc32() const { return m_crc32; }
    const std::string& get_title() const { return m_title; }
    bool is_loaded() const { return m_loaded; }
    MapperType get_mapper_type() const { return m_mapper_type; }
    EnhancementChip get_enhancement_chip() const { return m_enhancement_chip; }

    // ROM size info
    size_t get_rom_size() const { return m_rom.size(); }
    size_t get_ram_size() const { return m_sram.size(); }

    // Battery-backed save support
    bool has_battery() const { return m_has_battery; }

    // Get battery-backed save data (SRAM)
    std::vector<uint8_t> get_save_data() const;

    // Set battery-backed save data
    bool set_save_data(const std::vector<uint8_t>& data);

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

    // Speed info (for FastROM detection)
    bool is_fast_rom() const { return m_fast_rom; }

private:
    // Header detection and parsing
    bool detect_header(const uint8_t* data, size_t size);
    bool parse_header(const uint8_t* header_data);
    int score_header(const uint8_t* header_data, size_t rom_size);
    uint32_t calculate_crc32(const uint8_t* data, size_t size);

    // Memory mapping helpers
    uint8_t read_lorom(uint32_t address);
    void write_lorom(uint32_t address, uint8_t value);
    uint8_t read_hirom(uint32_t address);
    void write_hirom(uint32_t address, uint8_t value);

    // ROM data
    std::vector<uint8_t> m_rom;

    // SRAM (battery-backed or volatile)
    std::vector<uint8_t> m_sram;

    // ROM info
    bool m_loaded = false;
    uint32_t m_crc32 = 0;
    std::string m_title;
    MapperType m_mapper_type = MapperType::Unknown;
    EnhancementChip m_enhancement_chip = EnhancementChip::None;

    // Header info
    bool m_has_battery = false;
    bool m_has_ram = false;
    bool m_fast_rom = false;
    uint8_t m_region = 0;

    // Header offset (for proper vector reading)
    size_t m_header_offset = 0;
};

} // namespace snes
