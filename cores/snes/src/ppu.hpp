#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace snes {

class Bus;

// SNES PPU (Picture Processing Unit)
// Consists of PPU1 (5C77) and PPU2 (5C78)
// Reference: anomie's SNES docs, fullsnes
class PPU {
public:
    explicit PPU(Bus& bus);
    ~PPU();

    void reset();

    // Step one dot (pixel clock)
    void step();

    // Register access ($2100-$213F)
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    // Frame completion check
    bool check_frame_complete();

    // NMI check (returns true if NMI should be triggered)
    bool check_nmi();

    // NMI enable control (from NMITIMEN $4200 bit 7)
    void set_nmi_enabled(bool enabled) { m_nmi_enabled = enabled; }

    // Get framebuffer (256x224 or 512x448 in hi-res)
    const uint32_t* get_framebuffer() const { return m_framebuffer.data(); }

    // Current scanline/dot for timing
    int get_scanline() const { return m_scanline; }
    int get_dot() const { return m_dot; }
    void set_scanline(int scanline) { m_scanline = scanline; }
    void set_dot(int dot) { m_dot = dot; }
    uint32_t get_frame_cycle() const { return m_scanline * 340 + m_dot; }

    // ========================================================================
    // CATCH-UP RENDERING SYSTEM
    // ========================================================================
    // Reference: Mesen-S, bsnes-hd, other cycle-accurate emulators
    //
    // Instead of rendering entire scanlines at once, we track the current
    // dot position and render pixels on-demand when:
    // 1. CPU cycles advance the PPU clock
    // 2. A PPU register is written (sync before the write takes effect)
    //
    // This allows mid-scanline register changes to affect rendering correctly.
    // ========================================================================

    // Advance the PPU clock by the given number of master cycles
    // This renders any pixels that have become "due" since the last call
    void advance(int master_cycles);

    // Sync rendering up to the current dot position
    // Called before PPU register writes to ensure previous pixels
    // are rendered with the old register values
    void sync_to_current();

    // Pre-evaluate sprites for a visible scanline (called at scanline start)
    // This ensures sprites are evaluated using the register state from
    // the end of the previous scanline (matching hardware timing)
    void prepare_scanline_sprites(int scanline);

    // Set current timing position (for main loop synchronization)
    void set_timing(int scanline, int dot);

    // Check if we're in the visible rendering area
    bool is_rendering() const;

    // Debug getters
    bool is_force_blank() const { return m_force_blank; }
    uint8_t get_brightness() const { return m_brightness; }
    uint8_t get_main_screen_layers() const { return m_tm; }
    uint16_t get_vram_addr() const { return m_vram_addr; }
    uint8_t get_vmain() const { return m_vmain; }

    // Display mode getters (for frontend resolution handling)
    bool is_pseudo_hires() const { return m_pseudo_hires; }
    bool is_interlace() const { return m_interlace; }
    bool is_overscan() const { return m_overscan; }
    // Mode 5/6 always output 512 pixels wide (true hi-res), same as pseudo-hires
    bool is_hires_output() const { return m_pseudo_hires || m_bg_mode == 5 || m_bg_mode == 6; }
    // Always return 512 - framebuffer is always 512 pixels wide to handle mixed modes
    // Non-hi-res scanlines duplicate pixels; hi-res scanlines use full resolution
    int get_screen_width() const { return 512; }
    int get_screen_height() const { return m_overscan ? 239 : 224; }

    // OAM access for DMA
    void oam_write(uint16_t address, uint8_t value);
    uint8_t oam_read(uint16_t address);

    // CGRAM access for DMA
    void cgram_write(uint8_t value);
    uint8_t cgram_read();

    // VRAM access for DMA
    void vram_write(uint16_t address, uint8_t value, bool high_byte);
    uint8_t vram_read(uint16_t address, bool high_byte);

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

    // Render a specific scanline (public for plugin)
    void render_scanline(int scanline);

    // Notify end of frame (called by plugin after all scanlines rendered)
    void end_frame();

private:
    void render_scanline();
    void render_pixel(int x);
    void render_background_pixel(int bg, int x, uint8_t& pixel, uint8_t& priority);
    void render_background_pixel(int bg, int x, uint8_t& pixel, uint8_t& priority, uint8_t& palette);
    void render_background_pixel(int bg, int x, uint8_t& pixel, uint8_t& priority,
                                  bool hires_mode, bool hires_odd_pixel);  // Hi-res Mode 5/6
    void render_mode7_pixel(int x, uint8_t& pixel, uint8_t& priority);
    void render_sprite_pixel(int x, uint8_t& pixel, uint8_t& priority, bool& is_palette_4_7);
    void evaluate_sprites();
    uint16_t get_bg_tile_address(int bg, int tile_x, int tile_y);
    uint16_t get_color(uint8_t palette, uint8_t index, bool sprite = false);
    uint16_t get_direct_color(uint8_t palette, uint8_t color_index);
    uint16_t remap_vram_address(uint16_t addr) const;
    bool get_color_window(int x) const;  // Returns true if pixel is inside color window
    bool get_bg_window(int bg, int x) const;  // Returns true if BG pixel is masked by window
    bool get_obj_window(int x) const;  // Returns true if OBJ pixel is masked by window

    Bus& m_bus;

    // Timing
    int m_scanline = 0;
    int m_dot = 0;
    uint64_t m_frame = 0;
    bool m_frame_complete = false;

    // Catch-up rendering state
    // ========================================================================
    // The PPU tracks two positions:
    // - Current position (m_scanline, m_dot): Where the PPU "clock" is now
    // - Rendered position (m_rendered_scanline, m_rendered_dot): Last pixel rendered
    //
    // When advance() is called, we render from rendered position to current position.
    // When a register write occurs, sync_to_current() renders up to the current
    // dot before applying the new register value.
    // ========================================================================
    int m_rendered_scanline = 0;   // Last scanline that was fully/partially rendered
    int m_rendered_dot = 0;        // Last dot position rendered on that scanline

    // Sprite evaluation tracking
    // ========================================================================
    // Reference: Mesen-S, nesdev forum research on HblankEmuTest
    //
    // SNES sprite rendering has two distinct phases with separate timing:
    //
    // 1. SPRITE EVALUATION (OAM range scan): H=0-270
    //    - PPU scans all 128 OAM entries to find up to 32 sprites on this line
    //    - If force_blank is enabled DURING evaluation, the scan is paused/blocked
    //    - We latch force_blank state at dot 270 for this phase
    //
    // 2. SPRITE TILE FETCH: H=272-339
    //    - PPU fetches tile data from VRAM for the sprites found in phase 1
    //    - If force_blank is enabled DURING tile fetch, tiles are NOT loaded
    //    - We latch force_blank state at dot 272 for this phase
    //
    // HblankEmuTest specifically tests the case where:
    // - force_blank is OFF during evaluation (sprites get found)
    // - force_blank is ON during tile fetch (tiles not loaded)
    // - Result: sprites should NOT appear (no tile data)
    //
    // This requires tracking both latch states separately.
    // ========================================================================
    int m_sprites_for_scanline = -1;  // Scanline that m_sprite_buffer contains sprites for (-1 = none)

    // Latched force_blank state for sprite evaluation (range scan)
    // Checked at dot 270 - determines if sprites are found on this scanline
    bool m_force_blank_latched_eval = true;

    // Latched force_blank state for sprite tile fetching
    // Checked at dot 272 - determines if sprite tiles are loaded from VRAM
    bool m_force_blank_latched_fetch = true;

    // Track recent force_blank activity for sprite timing
    // HblankEmuTest toggles force_blank briefly via H-IRQ. Due to CPU timing
    // drift, the toggle can span scanline boundaries. We track the last N
    // master cycles where force_blank was ON.
    //
    // For sprite rendering on scanline N, if force_blank was ON at any point
    // during scanline N-1's H-blank period (roughly the last 256 dots), we
    // block sprite tile loading.
    uint64_t m_force_blank_on_cycle = 0;    // Cycle count when force_blank was last enabled
    uint64_t m_total_ppu_cycles = 0;        // Running cycle counter for timing

    // Dot accumulator for sub-dot timing (persists across calls to advance())
    int m_dot_accumulator = 0;

    // Screen dimensions
    static constexpr int SCREEN_WIDTH = 256;
    static constexpr int SCREEN_HEIGHT = 224;
    static constexpr int SCANLINES_PER_FRAME = 262;  // NTSC
    static constexpr int DOTS_PER_SCANLINE = 340;

    // Framebuffer (supports hi-res 512x448)
    std::array<uint32_t, 512 * 448> m_framebuffer;

    // VRAM (64KB)
    std::array<uint8_t, 0x10000> m_vram;

    // OAM (544 bytes: 512 + 32 high bytes)
    std::array<uint8_t, 544> m_oam;

    // CGRAM (512 bytes = 256 colors)
    std::array<uint8_t, 512> m_cgram;

    // PPU registers ($2100-$213F)

    // $2100 - INIDISP - Screen display register
    uint8_t m_inidisp = 0x80;  // Force blank on reset
    bool m_force_blank = true;
    uint8_t m_brightness = 0;

    // $2101 - OBSEL - Object size and base
    uint8_t m_obsel = 0;
    uint16_t m_obj_base_addr = 0;
    uint16_t m_obj_name_select = 0;
    int m_obj_size_small = 0;  // 0=8x8, 1=16x16, etc.
    int m_obj_size_large = 0;

    // $2102-$2103 - OAMADD - OAM address
    uint16_t m_oam_addr = 0;
    uint16_t m_oam_addr_reload = 0;
    uint8_t m_oam_latch = 0;
    bool m_oam_high_byte = false;

    // $2105 - BGMODE - BG mode and tile size
    uint8_t m_bgmode = 0;
    int m_bg_mode = 0;
    bool m_bg3_priority = false;
    std::array<bool, 4> m_bg_tile_size;  // 0=8x8, 1=16x16

    // $2106 - MOSAIC
    uint8_t m_mosaic = 0;
    int m_mosaic_size = 1;
    std::array<bool, 4> m_mosaic_enabled;

    // $2107-$210A - BGnSC - BG tilemap addresses
    std::array<uint16_t, 4> m_bg_tilemap_addr;
    std::array<int, 4> m_bg_tilemap_width;   // 0=32, 1=64
    std::array<int, 4> m_bg_tilemap_height;

    // $210B-$210C - BGnNBA - BG character data addresses
    std::array<uint16_t, 4> m_bg_chr_addr;

    // $210D-$2114 - BGnHOFS/BGnVOFS - BG scroll offsets
    std::array<uint16_t, 4> m_bg_hofs;
    std::array<uint16_t, 4> m_bg_vofs;
    // SNES has two latches for scroll registers (quirky PPU1/PPU2 behavior)
    // HOFS formula: (data << 8) | (latch_ppu1 & ~7) | (latch_ppu2 & 7)
    // VOFS formula: (data << 8) | latch_ppu1
    uint8_t m_bgofs_latch_ppu1 = 0;  // Stores previous write value
    uint8_t m_bgofs_latch_ppu2 = 0;  // Stores bits for fine scroll

    // $2115 - VMAIN - VRAM address increment mode
    uint8_t m_vmain = 0;
    int m_vram_increment = 1;
    bool m_vram_increment_high = false;
    int m_vram_remap_mode = 0;

    // $2116-$2117 - VMADD - VRAM address
    uint16_t m_vram_addr = 0;

    // $2118-$2119 - VMDATA - VRAM data (write latch)
    uint8_t m_vram_latch = 0;

    // $2121 - CGADD - CGRAM address
    uint16_t m_cgram_addr = 0;
    uint8_t m_cgram_latch = 0;
    bool m_cgram_high_byte = false;

    // $2123-$2125 - Window settings
    std::array<bool, 4> m_bg_window1_enable;
    std::array<bool, 4> m_bg_window1_invert;
    std::array<bool, 4> m_bg_window2_enable;
    std::array<bool, 4> m_bg_window2_invert;
    bool m_obj_window1_enable = false;
    bool m_obj_window1_invert = false;
    bool m_obj_window2_enable = false;
    bool m_obj_window2_invert = false;
    bool m_color_window1_enable = false;
    bool m_color_window1_invert = false;
    bool m_color_window2_enable = false;
    bool m_color_window2_invert = false;

    // $2126-$2129 - Window positions
    uint8_t m_window1_left = 0;
    uint8_t m_window1_right = 0;
    uint8_t m_window2_left = 0;
    uint8_t m_window2_right = 0;

    // $212A-$212B - Window mask logic
    std::array<int, 4> m_bg_window_logic;
    int m_obj_window_logic = 0;
    int m_color_window_logic = 0;

    // $212C-$212D - Main/Sub screen designation
    uint8_t m_tm = 0;   // Main screen enable
    uint8_t m_ts = 0;   // Sub screen enable

    // $212E-$212F - Window mask designation
    uint8_t m_tmw = 0;  // Main screen window mask
    uint8_t m_tsw = 0;  // Sub screen window mask

    // $2130 - CGWSEL - Color addition select
    uint8_t m_cgwsel = 0;
    int m_color_math_clip = 0;
    int m_color_math_prevent = 0;
    bool m_direct_color = false;
    bool m_sub_screen_bg_obj = false;

    // $2131 - CGADSUB - Color math designation
    uint8_t m_cgadsub = 0;
    bool m_color_math_add = true;
    bool m_color_math_half = false;
    std::array<bool, 4> m_bg_color_math;
    bool m_obj_color_math = false;
    bool m_backdrop_color_math = false;

    // $2132 - COLDATA - Fixed color data
    uint8_t m_fixed_color_r = 0;
    uint8_t m_fixed_color_g = 0;
    uint8_t m_fixed_color_b = 0;

    // $2133 - SETINI - Screen mode/video select
    uint8_t m_setini = 0;
    bool m_interlace = false;
    bool m_obj_interlace = false;
    bool m_overscan = false;
    bool m_pseudo_hires = false;
    bool m_extbg = false;         // Mode 7 EXTBG - BG2 uses bit 7 as priority
    bool m_external_sync = false;

    // Mode 7 registers ($211A-$2120)
    uint8_t m_m7sel = 0;
    bool m_m7_hflip = false;
    bool m_m7_vflip = false;
    int m_m7_wrap = 0;  // 0=wrap, 1=transparent, 2=tile 0, 3=transparent

    int16_t m_m7a = 0;
    int16_t m_m7b = 0;
    int16_t m_m7c = 0;
    int16_t m_m7d = 0;
    int16_t m_m7x = 0;  // Center X
    int16_t m_m7y = 0;  // Center Y
    int16_t m_m7hofs = 0;
    int16_t m_m7vofs = 0;
    uint8_t m_m7_latch = 0;

    // PPU1 read buffer ($2139-$213A)
    uint16_t m_vram_read_buffer = 0;

    // Status registers
    bool m_time_over = false;   // More than 34 sprites on scanline
    bool m_range_over = false;  // More than 32 sprite tiles on scanline
    uint8_t m_ppu1_open_bus = 0;
    uint8_t m_ppu2_open_bus = 0;

    // NMI
    bool m_nmi_flag = false;
    bool m_nmi_enabled = false;
    bool m_nmi_pending = false;

    // H/V counters ($213C-$213D)
    uint16_t m_hcount = 0;
    uint16_t m_vcount = 0;
    bool m_hv_latch = false;
    bool m_hcount_second = false;
    bool m_vcount_second = false;

    // Multiplication result ($2134-$2136)
    int32_t m_mpy_result = 0;

    // Sprite evaluation
    struct SpriteEntry {
        int x;
        int y;
        int tile;
        int palette;
        int priority;
        bool hflip;
        bool vflip;
        bool large;
        int width;
        int height;
    };
    std::array<SpriteEntry, 32> m_sprite_buffer;
    int m_sprite_count = 0;

    // Sprite tile fetches for current scanline
    // Sprites are always 4bpp (16 colors per palette)
    struct SpriteTile {
        int x;                      // X position on screen
        uint8_t planes[4];          // 4 bitplanes for one 8-pixel row
        int palette;                // Palette 0-7
        int priority;               // Priority 0-3
        bool hflip;                 // Horizontal flip
    };
    std::array<SpriteTile, 34> m_sprite_tiles;
    int m_sprite_tile_count = 0;

    // Sprite sizes lookup (small, large)
    static constexpr int SPRITE_SIZES[8][2][2] = {
        {{8, 8}, {16, 16}},    // 0: 8x8, 16x16
        {{8, 8}, {32, 32}},    // 1: 8x8, 32x32
        {{8, 8}, {64, 64}},    // 2: 8x8, 64x64
        {{16, 16}, {32, 32}},  // 3: 16x16, 32x32
        {{16, 16}, {64, 64}},  // 4: 16x16, 64x64
        {{32, 32}, {64, 64}},  // 5: 32x32, 64x64
        {{16, 32}, {32, 64}},  // 6: 16x32, 32x64
        {{16, 32}, {32, 32}}   // 7: 16x32, 32x32
    };
};

} // namespace snes
