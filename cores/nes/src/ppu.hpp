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
    // Returns 0 = no NMI, 1 = immediate NMI, 2 = delayed NMI (after next instruction)
    int check_nmi();

    // Frame complete check (returns true once per frame, at start of VBlank)
    bool check_frame_complete();

    // Get framebuffer
    const uint32_t* get_framebuffer() const { return m_framebuffer.data(); }

    // Get current frame cycle for mapper IRQ timing
    uint32_t get_frame_cycle() const { return static_cast<uint32_t>(m_scanline * 341 + m_cycle); }

    // Set mirroring mode (from cartridge)
    void set_mirroring(int mode) { m_mirroring = mode; }

    // Region/variant configuration
    enum class Region { NTSC, PAL, Dendy };
    enum class PPUVariant {
        RP2C02,     // Standard NTSC
        RP2C07,     // Standard PAL
        RP2C03,     // Vs. System RGB (standard palette)
        RP2C04_0001,// Vs. System with scrambled palette
        RP2C04_0002,
        RP2C04_0003,
        RP2C04_0004,
        RC2C05_01,  // Vs. System with different PPU ID
        RC2C05_02,
        RC2C05_03,
        RC2C05_04,
        RC2C05_05,
        Dendy       // Russian Dendy clone
    };

    void set_region(Region region);
    void set_ppu_variant(PPUVariant variant);
    Region get_region() const { return m_region; }
    PPUVariant get_ppu_variant() const { return m_variant; }
    int get_scanlines_per_frame() const { return m_scanlines_per_frame; }
    int get_vblank_scanlines() const { return m_vblank_scanlines; }

    // Emulation options
    void set_sprite_limit_enabled(bool enabled) { m_sprite_limit_enabled = enabled; }
    bool is_sprite_limit_enabled() const { return m_sprite_limit_enabled; }
    void set_crop_overscan(bool enabled) { m_crop_overscan = enabled; }
    bool is_crop_overscan_enabled() const { return m_crop_overscan; }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    void render_pixel();
    uint8_t get_background_pixel();
    uint8_t get_sprite_pixel(uint8_t& sprite_priority);
    void evaluate_sprites();
    void evaluate_sprites_for_scanline(int scanline, uint32_t frame_cycle);
    void evaluate_sprites_for_next_scanline(int scanline);
    uint16_t get_sprite_pattern_addr(int sprite_slot, bool hi_byte);
    uint8_t maybe_flip_sprite_byte(int sprite_slot, uint8_t byte);
    void load_background_shifters();
    void update_shifters();

    // Bus reference
    Bus& m_bus;

    // PPU registers
    uint8_t m_ctrl = 0;     // $2000 PPUCTRL
    uint8_t m_mask = 0;     // $2001 PPUMASK
    uint8_t m_mask_prev = 0;         // Previous PPUMASK value for skip timing
    uint32_t m_mask_write_cycle = 0; // Frame cycle when PPUMASK was last written
    uint8_t m_status = 0;   // $2002 PPUSTATUS
    uint8_t m_oam_addr = 0; // $2003 OAMADDR

    // Internal registers
    uint16_t m_v = 0;       // Current VRAM address (15 bits)
    uint16_t m_t = 0;       // Temporary VRAM address
    uint8_t m_x = 0;        // Fine X scroll (3 bits)
    bool m_w = false;       // Write toggle

    // Data buffer for $2007 reads
    uint8_t m_data_buffer = 0;

    // Open bus (IO latch) - filled by any PPU register write
    // Used for lower 5 bits of PPUSTATUS and reads from write-only registers
    // Per-bit decay: each bit decays independently to 0 after ~600ms if not refreshed with 1
    uint8_t m_io_latch = 0;
    std::array<uint64_t, 8> m_io_latch_decay_frame = {};  // Frame when each bit was last refreshed with 1

    // Timing
    int m_scanline = 0;
    int m_cycle = 0;
    uint64_t m_frame = 0;
    bool m_odd_frame = false;

    // NMI
    bool m_nmi_occurred = false;
    bool m_nmi_output = false;
    bool m_nmi_triggered = false;
    bool m_nmi_triggered_delayed = false;  // NMI should fire after NEXT instruction
    bool m_nmi_pending = false;  // NMI will be triggered after current instruction
    int m_nmi_delay = 0;  // Delay counter for NMI (in PPU cycles)
    bool m_nmi_latched = false;  // NMI edge has been generated and will fire when delay expires

    // VBL suppression - reading $2002 at the exact cycle VBL is set suppresses both flag and NMI
    bool m_vbl_suppress = false;     // Suppress VBL flag from being set
    bool m_suppress_nmi = false;     // Suppress NMI from occurring

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
    // Sprite arrays sized for 64 sprites when sprite limit is disabled
    // Only first 8 are used when sprite limit is enabled (hardware accurate)
    static constexpr int MAX_SPRITES_PER_SCANLINE = 64;
    std::array<Sprite, MAX_SPRITES_PER_SCANLINE> m_scanline_sprites;
    std::array<uint8_t, MAX_SPRITES_PER_SCANLINE> m_sprite_shifter_lo;
    std::array<uint8_t, MAX_SPRITES_PER_SCANLINE> m_sprite_shifter_hi;
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

    // Region and variant configuration
    Region m_region = Region::NTSC;
    PPUVariant m_variant = PPUVariant::RP2C02;

    // Region timing parameters
    int m_scanlines_per_frame = 262;     // NTSC: 262, PAL: 312
    int m_vblank_scanlines = 20;         // NTSC: 20, PAL: 70
    int m_prerender_scanline = 261;      // NTSC: 261, PAL: 311
    int m_postrender_scanline = 240;     // Same for both
    int m_vblank_start_scanline = 241;   // Same for both

    // Vs. System palettes
    static const uint32_t s_palette_rp2c03[64];    // RGB PPU
    static const uint32_t s_palette_rp2c04_0001[64];
    static const uint32_t s_palette_rp2c04_0002[64];
    static const uint32_t s_palette_rp2c04_0003[64];
    static const uint32_t s_palette_rp2c04_0004[64];

    // Current palette pointer (points to one of the above)
    const uint32_t* m_current_palette = s_palette;

    // Emulation options
    bool m_sprite_limit_enabled = true;  // True = accurate 8 sprite limit
    bool m_crop_overscan = false;        // True = hide top/bottom 8 rows
};

} // namespace nes
