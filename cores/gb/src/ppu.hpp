#pragma once

#include "types.hpp"
#include <cstdint>
#include <array>
#include <vector>

namespace gb {

class Bus;

// Game Boy PPU - 160x144 display
class PPU {
public:
    explicit PPU(Bus& bus);
    ~PPU();

    void reset();
    void step();

    // Set CGB mode
    void set_cgb_mode(bool cgb) { m_cgb_mode = cgb; }
    void set_vram_bank(int bank) { m_vram_bank = bank & 1; }

    // Memory access
    uint8_t read_vram(uint16_t offset);
    void write_vram(uint16_t offset, uint8_t value);
    uint8_t read_oam(uint16_t offset);
    void write_oam(uint16_t offset, uint8_t value);

    // Register access
    uint8_t read_register(uint16_t address);
    void write_register(uint16_t address, uint8_t value);

    // Get framebuffer
    const uint32_t* get_framebuffer() const { return m_framebuffer.data(); }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    enum class Mode {
        HBlank = 0,
        VBlank = 1,
        OAMScan = 2,
        Drawing = 3
    };

    void set_mode(Mode mode);
    void render_scanline();
    void render_background();
    void render_window();
    void render_sprites();

    uint32_t get_dmg_color(uint8_t shade);
    uint32_t get_cgb_color(uint16_t color);

    Bus& m_bus;

    // Memory
    std::array<uint8_t, 0x4000> m_vram;  // 8KB for DMG, 16KB for CGB
    std::array<uint8_t, 160> m_oam;      // 160 bytes OAM

    // Framebuffer (160x144 RGBA)
    std::array<uint32_t, 160 * 144> m_framebuffer;

    // Scanline buffer for priority handling
    std::array<uint8_t, 160> m_bg_priority;  // 0 = transparent, 1+ = opaque

    // Registers
    uint8_t m_lcdc = 0;     // FF40 - LCD Control
    uint8_t m_stat = 0;     // FF41 - LCD Status
    uint8_t m_scy = 0;      // FF42 - Scroll Y
    uint8_t m_scx = 0;      // FF43 - Scroll X
    uint8_t m_ly = 0;       // FF44 - LY (current scanline)
    uint8_t m_lyc = 0;      // FF45 - LY Compare
    uint8_t m_bgp = 0;      // FF47 - BG Palette (DMG)
    uint8_t m_obp0 = 0;     // FF48 - OBJ Palette 0 (DMG)
    uint8_t m_obp1 = 0;     // FF49 - OBJ Palette 1 (DMG)
    uint8_t m_wy = 0;       // FF4A - Window Y
    uint8_t m_wx = 0;       // FF4B - Window X

    // CGB palette data
    uint8_t m_bcps = 0;     // FF68 - BG Palette Index
    uint8_t m_ocps = 0;     // FF6A - OBJ Palette Index
    std::array<uint8_t, 64> m_bg_palette;   // CGB BG palettes
    std::array<uint8_t, 64> m_obj_palette;  // CGB OBJ palettes

    // Timing
    int m_cycle = 0;
    Mode m_mode = Mode::OAMScan;
    int m_window_line = 0;  // Internal window line counter

    // State
    bool m_cgb_mode = false;
    int m_vram_bank = 0;

    // DMG color palette (greenish LCD)
    static const uint32_t s_dmg_palette[4];

    // Timing constants
    static constexpr int OAM_SCAN_CYCLES = 80;
    static constexpr int DRAWING_MIN_CYCLES = 172;
    static constexpr int HBLANK_CYCLES = 204;
    static constexpr int SCANLINE_CYCLES = 456;
    static constexpr int VBLANK_LINES = 10;
    static constexpr int TOTAL_LINES = 154;
};

} // namespace gb
