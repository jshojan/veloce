#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <string>

namespace nes {

class Mapper;
enum class MirrorMode;

// iNES header format
struct iNESHeader {
    uint8_t signature[4];    // "NES\x1A"
    uint8_t prg_rom_size;    // PRG ROM size in 16KB units
    uint8_t chr_rom_size;    // CHR ROM size in 8KB units
    uint8_t flags6;          // Mapper, mirroring, battery, trainer
    uint8_t flags7;          // Mapper, VS/Playchoice, NES 2.0
    uint8_t flags8;          // PRG RAM size (rarely used extension)
    uint8_t flags9;          // TV system (rarely used extension)
    uint8_t flags10;         // TV system, PRG RAM presence (unofficial)
    uint8_t padding[5];      // Unused padding
};

static_assert(sizeof(iNESHeader) == 16, "iNES header must be 16 bytes");

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    // Load ROM from memory
    bool load(const uint8_t* data, size_t size);

    // Unload current ROM
    void unload();

    // Reset mapper state
    void reset();

    // CPU memory access (via mapper)
    uint8_t cpu_read(uint16_t address);
    void cpu_write(uint16_t address, uint8_t value);

    // PPU memory access (via mapper)
    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0);
    void ppu_write(uint16_t address, uint8_t value);

    // Get mirror mode from mapper
    MirrorMode get_mirror_mode() const;

    // IRQ support
    bool irq_pending(uint32_t frame_cycle = 0);
    void irq_clear();
    void scanline();
    void notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr);
    void notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle);
    void notify_frame_start();

    // ROM info
    uint32_t get_crc32() const { return m_crc32; }
    int get_mapper_number() const { return m_mapper_number; }
    const std::string& get_title() const { return m_title; }
    bool is_loaded() const { return m_loaded; }

    // ROM data access (for save states, etc.)
    const std::vector<uint8_t>& get_prg_rom() const { return m_prg_rom; }
    const std::vector<uint8_t>& get_chr_rom() const { return m_chr_rom; }
    std::vector<uint8_t>& get_prg_ram() { return m_prg_ram; }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    bool parse_header(const iNESHeader& header);
    uint32_t calculate_crc32(const uint8_t* data, size_t size);

    std::unique_ptr<Mapper> m_mapper;
    std::vector<uint8_t> m_prg_rom;
    std::vector<uint8_t> m_chr_rom;
    std::vector<uint8_t> m_prg_ram;

    bool m_loaded = false;
    int m_mapper_number = 0;
    uint32_t m_crc32 = 0;
    std::string m_title;

    // From header
    bool m_has_battery = false;
    bool m_has_trainer = false;
    bool m_has_chr_ram = false;
};

} // namespace nes
