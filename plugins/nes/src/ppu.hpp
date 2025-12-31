#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace nes {

class Bus;

// NES PPU (Picture Processing Unit) - 2C02
class PPU {
public:
    explicit PPU(Bus& bus);
    ~PPU();

    // Reset
    void reset();

    // Step one PPU cycle
    void step();

    // CPU register access ($2000-$2007)
    uint8_t cpu_read(uint16_t address);
    void cpu_write(uint16_t address, uint8_t value);

    // PPU memory access (pattern tables, nametables, palettes)
    uint8_t ppu_read(uint16_t address);
    void ppu_write(uint16_t address, uint8_t value);

    // OAM access
    void oam_write(int address, uint8_t value);

    // NMI check (called after step)
    bool check_nmi();

    // Frame complete check (returns true once per frame, at start of VBlank)
    bool check_frame_complete();

    // Get framebuffer
    const uint32_t* get_framebuffer() const { return m_framebuffer.data(); }

    // Set mirroring mode (from cartridge)
    void set_mirroring(int mode) { m_mirroring = mode; }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    void render_pixel();
    uint8_t get_background_pixel();
    uint8_t get_sprite_pixel(uint8_t& sprite_priority);
    void evaluate_sprites();
    void evaluate_sprites_for_scanline(int scanline);
    void load_background_shifters();
    void update_shifters();

    // Bus reference
    Bus& m_bus;

    // PPU registers
    uint8_t m_ctrl = 0;     // $2000 PPUCTRL
    uint8_t m_mask = 0;     // $2001 PPUMASK
    uint8_t m_status = 0;   // $2002 PPUSTATUS
    uint8_t m_oam_addr = 0; // $2003 OAMADDR

    // Internal registers
    uint16_t m_v = 0;       // Current VRAM address (15 bits)
    uint16_t m_t = 0;       // Temporary VRAM address
    uint8_t m_x = 0;        // Fine X scroll (3 bits)
    bool m_w = false;       // Write toggle

    // Data buffer for reads
    uint8_t m_data_buffer = 0;

    // Timing
    int m_scanline = 0;
    int m_cycle = 0;
    uint64_t m_frame = 0;
    bool m_odd_frame = false;

    // NMI
    bool m_nmi_occurred = false;
    bool m_nmi_output = false;
    bool m_nmi_triggered = false;

    // Frame completion flag (set when entering VBlank)
    bool m_frame_complete = false;

    // Background rendering
    uint16_t m_bg_shifter_pattern_lo = 0;
    uint16_t m_bg_shifter_pattern_hi = 0;
    uint16_t m_bg_shifter_attrib_lo = 0;
    uint16_t m_bg_shifter_attrib_hi = 0;
    uint8_t m_bg_next_tile_id = 0;
    uint8_t m_bg_next_tile_attrib = 0;
    uint8_t m_bg_next_tile_lo = 0;
    uint8_t m_bg_next_tile_hi = 0;

    // Sprite rendering
    struct Sprite {
        uint8_t y;
        uint8_t tile;
        uint8_t attr;
        uint8_t x;
    };

    std::array<uint8_t, 256> m_oam;  // Object Attribute Memory
    std::array<Sprite, 8> m_scanline_sprites;
    std::array<uint8_t, 8> m_sprite_shifter_lo;
    std::array<uint8_t, 8> m_sprite_shifter_hi;
    int m_sprite_count = 0;
    int m_sprite_zero_index = -1;  // Index of OAM sprite 0 in m_scanline_sprites (-1 if not present)
    bool m_sprite_zero_hit_possible = false;
    bool m_sprite_zero_rendering = false;

    // Memory
    std::array<uint8_t, 2048> m_nametable;  // 2KB nametable RAM
    std::array<uint8_t, 32> m_palette;      // Palette RAM

    // Framebuffer (256x240 RGBA)
    std::array<uint32_t, 256 * 240> m_framebuffer;

    // Mirroring mode
    int m_mirroring = 0;  // 0 = horizontal, 1 = vertical

    // NES color palette (RGB values)
    static const uint32_t s_palette[64];
};

} // namespace nes
