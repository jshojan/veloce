#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstdio>
#include <array>
#include <vector>

namespace gba {

class Bus;

// GBA PPU - 240x160 display with multiple modes
class PPU {
public:
    explicit PPU(Bus& bus);
    ~PPU();

    void reset();
    void step(int cycles);

    // Memory access
    uint8_t read_vram(uint32_t offset);
    void write_vram(uint32_t offset, uint8_t value);
    uint8_t read_palette(uint32_t offset);
    void write_palette(uint32_t offset, uint8_t value);
    uint8_t read_oam(uint32_t offset);
    void write_oam(uint32_t offset, uint8_t value);

    // Get framebuffer
    const uint32_t* get_framebuffer() const { return m_framebuffer.data(); }

    // Get current scanline (for VCOUNT)
    uint16_t get_vcount() const { return m_vcount; }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    void render_scanline();
    void render_mode0();
    void render_mode1();
    void render_mode2();
    void render_mode3();
    void render_mode4();
    void render_mode5();
    void render_sprites();
    void render_background(int layer);
    void render_affine_background(int layer);
    void render_affine_sprite(int sprite_idx, uint16_t attr0, uint16_t attr1, uint16_t attr2);

    void compose_scanline();
    bool is_inside_window(int x, int window_id);
    uint8_t get_window_flags(int x);
    void apply_blending(int x, uint16_t& top_color, uint16_t bottom_color);

    uint32_t palette_to_rgba(uint16_t color);

    Bus& m_bus;

    // Memory
    std::array<uint8_t, 0x18000> m_vram;     // 96KB VRAM
    std::array<uint8_t, 0x400> m_palette;    // 1KB Palette RAM
    std::array<uint8_t, 0x400> m_oam;        // 1KB OAM

    // Framebuffer (240x160 RGBA)
    std::array<uint32_t, 240 * 160> m_framebuffer;

    // Scanline buffers for compositing
    std::array<uint16_t, 240> m_bg_buffer[4];     // Background layers
    std::array<uint8_t, 240> m_bg_priority[4];    // Priority for each pixel
    std::array<bool, 240> m_bg_is_target1[4];     // Whether pixel is blend target 1
    std::array<uint16_t, 240> m_sprite_buffer;    // Sprite layer
    std::array<uint8_t, 240> m_sprite_priority;   // Sprite priorities
    std::array<bool, 240> m_sprite_semi_transparent; // Semi-transparent sprite flags
    std::array<bool, 240> m_sprite_is_window;     // OBJ window flags

    // Timing
    uint16_t m_vcount = 0;  // Current scanline (0-227)
    int m_hcount = 0;       // Current cycle within scanline (0-1231)

    // Display mode
    DisplayMode m_mode = DisplayMode::Mode0;
    bool m_frame_select = false;  // For double-buffered modes

    // Register cache (synced from bus)
    uint16_t m_dispcnt = 0;
    uint16_t m_dispstat = 0;

public:
    // Synchronize registers from bus
    void sync_registers(uint16_t dispcnt, uint16_t dispstat_config) {
        m_dispcnt = dispcnt;
        // Only copy the configurable bits (VCount target and IRQ enables)
        // Keep the status bits (VBlank, HBlank, VCount match) from PPU's internal state
        m_dispstat = (m_dispstat & 0x0007) | (dispstat_config & 0xFFF8);
    }

    // Get current DISPSTAT for bus to read (with PPU's status bits)
    uint16_t get_dispstat() const { return m_dispstat; }

private:
    std::array<uint16_t, 4> m_bgcnt;
    std::array<uint16_t, 4> m_bghofs;
    std::array<uint16_t, 4> m_bgvofs;

    // Affine background internal registers (28.4 fixed point)
    std::array<int32_t, 2> m_bgx_internal;
    std::array<int32_t, 2> m_bgy_internal;

    // Affine parameters (from bus)
    std::array<int16_t, 2> m_bgpa;
    std::array<int16_t, 2> m_bgpb;
    std::array<int16_t, 2> m_bgpc;
    std::array<int16_t, 2> m_bgpd;
    std::array<int32_t, 2> m_bgx;
    std::array<int32_t, 2> m_bgy;

    // Window registers
    uint16_t m_win0h = 0, m_win1h = 0;
    uint16_t m_win0v = 0, m_win1v = 0;
    uint16_t m_winin = 0, m_winout = 0;

    // Blending registers
    uint16_t m_bldcnt = 0;
    uint16_t m_bldalpha = 0;
    uint16_t m_bldy = 0;

    // Constants
    static constexpr int HDRAW_CYCLES = 960;    // HBlank starts at cycle 960
    static constexpr int HBLANK_CYCLES = 272;   // HBlank lasts 272 cycles
    static constexpr int SCANLINE_CYCLES = 1232;
    static constexpr int VDRAW_LINES = 160;
    static constexpr int VBLANK_LINES = 68;
    static constexpr int TOTAL_LINES = 228;
};

} // namespace gba
