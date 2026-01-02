#include "ppu.hpp"
#include "bus.hpp"
#include "debug.hpp"
#include <cstring>
#include <algorithm>

namespace snes {

PPU::PPU(Bus& bus) : m_bus(bus) {
    reset();
}

PPU::~PPU() = default;

void PPU::reset() {
    // Debug: trace reset calls with frame info
    static int reset_count = 0;
    if (is_debug_mode()) {
        fprintf(stderr, "[PPU] reset() called (count=%d) current_frame=%llu CGRAM[0]=$%02X\n",
            reset_count++, (unsigned long long)m_frame, m_cgram[0]);
    }

    m_scanline = 0;
    m_dot = 0;
    m_frame = 0;
    m_frame_complete = false;

    m_framebuffer.fill(0);
    m_vram.fill(0);
    m_oam.fill(0);
    if (is_debug_mode() && m_cgram[0] != 0) {
        fprintf(stderr, "[PPU] reset() clearing CGRAM (was [0]=$%02X [1]=$%02X)\n", m_cgram[0], m_cgram[1]);
    }
    m_cgram.fill(0);

    m_inidisp = 0x80;
    m_force_blank = true;
    m_brightness = 0;
    m_obsel = 0;
    m_obj_base_addr = 0;
    m_obj_name_select = 0;
    m_oam_addr = 0;
    m_oam_addr_reload = 0;
    m_oam_latch = 0;
    m_oam_high_byte = false;

    m_bgmode = 0;
    m_bg_mode = 0;
    m_bg3_priority = false;
    m_bg_tile_size.fill(false);

    m_mosaic = 0;
    m_mosaic_size = 1;
    m_mosaic_enabled.fill(false);

    m_bg_tilemap_addr.fill(0);
    m_bg_tilemap_width.fill(0);
    m_bg_tilemap_height.fill(0);
    m_bg_chr_addr.fill(0);
    m_bg_hofs.fill(0);
    m_bg_vofs.fill(0);
    m_bgofs_latch_ppu1 = 0;
    m_bgofs_latch_ppu2 = 0;

    m_vmain = 0;
    m_vram_increment = 1;
    m_vram_increment_high = false;
    m_vram_remap_mode = 0;
    m_vram_addr = 0;
    m_vram_latch = 0;

    m_cgram_addr = 0;
    m_cgram_latch = 0;
    m_cgram_high_byte = false;

    m_bg_window1_enable.fill(false);
    m_bg_window1_invert.fill(false);
    m_bg_window2_enable.fill(false);
    m_bg_window2_invert.fill(false);
    m_obj_window1_enable = false;
    m_obj_window1_invert = false;
    m_obj_window2_enable = false;
    m_obj_window2_invert = false;
    m_color_window1_enable = false;
    m_color_window1_invert = false;
    m_color_window2_enable = false;
    m_color_window2_invert = false;

    m_window1_left = 0;
    m_window1_right = 0;
    m_window2_left = 0;
    m_window2_right = 0;

    m_bg_window_logic.fill(0);
    m_obj_window_logic = 0;
    m_color_window_logic = 0;

    m_tm = 0;
    m_ts = 0;
    m_tmw = 0;
    m_tsw = 0;

    m_cgwsel = 0;
    m_color_math_clip = 0;
    m_color_math_prevent = 0;
    m_direct_color = false;
    m_sub_screen_bg_obj = false;

    m_cgadsub = 0;
    m_color_math_add = true;
    m_color_math_half = false;
    m_bg_color_math.fill(false);
    m_obj_color_math = false;
    m_backdrop_color_math = false;

    m_fixed_color_r = 0;
    m_fixed_color_g = 0;
    m_fixed_color_b = 0;

    m_setini = 0;
    m_interlace = false;
    m_obj_interlace = false;
    m_overscan = false;
    m_pseudo_hires = false;
    m_external_sync = false;

    m_m7sel = 0;
    m_m7_hflip = false;
    m_m7_vflip = false;
    m_m7_wrap = 0;
    m_m7a = 0;
    m_m7b = 0;
    m_m7c = 0;
    m_m7d = 0;
    m_m7x = 0;
    m_m7y = 0;
    m_m7hofs = 0;
    m_m7vofs = 0;
    m_m7_latch = 0;

    m_vram_read_buffer = 0;

    m_time_over = false;
    m_range_over = false;
    m_ppu1_open_bus = 0;
    m_ppu2_open_bus = 0;

    m_nmi_flag = false;
    m_nmi_enabled = false;
    m_nmi_pending = false;

    m_hcount = 0;
    m_vcount = 0;
    m_hv_latch = false;
    m_hcount_second = false;
    m_vcount_second = false;

    m_mpy_result = 0;
    m_sprite_count = 0;
    m_sprite_tile_count = 0;
}

void PPU::step() {
    // Render visible scanlines (1-224 or 1-239 in overscan)
    int visible_lines = m_overscan ? 239 : 224;

    if (m_scanline >= 1 && m_scanline <= visible_lines && m_dot == 0) {
        // Evaluate sprites at the start of each visible scanline
        evaluate_sprites();
    }

    if (m_scanline >= 1 && m_scanline <= visible_lines && m_dot >= 22 && m_dot < 278) {
        // Render visible pixels (22-277 = 256 pixels)
        int x = m_dot - 22;
        if (!m_force_blank) {
            render_pixel(x);
        } else {
            // Force blank - output black
            int y = m_scanline - 1;
            m_framebuffer[y * SCREEN_WIDTH + x] = 0xFF000000;
        }
    }

    // Update H/V counters
    m_hcount = m_dot;
    m_vcount = m_scanline;

    // Advance dot
    m_dot++;
    if (m_dot >= DOTS_PER_SCANLINE) {
        m_dot = 0;
        m_scanline++;

        // VBlank start (scanline 225 or 240)
        if (m_scanline == visible_lines + 1) {
            m_nmi_flag = true;
            if (m_nmi_enabled) {
                m_nmi_pending = true;
            }
            m_frame_complete = true;

            // Reset OAM address at VBlank start
            m_oam_addr = m_oam_addr_reload;
        }

        // End of frame
        if (m_scanline >= SCANLINES_PER_FRAME) {
            m_scanline = 0;
            m_frame++;
            m_nmi_flag = false;
            m_time_over = false;
            m_range_over = false;

            // Debug: log state at key frames
            if (is_debug_mode() && (m_frame == 1 || m_frame == 60 || m_frame == 120)) {
                fprintf(stderr, "[PPU] FRAME %llu START: force_blank=%d brightness=%d mode=%d TM=$%02X\n",
                    (unsigned long long)m_frame, m_force_blank ? 1 : 0, m_brightness, m_bg_mode, m_tm);
            }
        }
    }
}

void PPU::render_scanline(int scanline) {
    // WATCHPOINT: Detect when CGRAM content changes significantly
    static uint8_t last_cgram[8] = {0};
    static uint64_t last_check_frame = 0;
    static int last_check_scanline = 0;
    static bool had_nonzero_palette = false;
    if (is_debug_mode()) {
        // Check if CGRAM[2] (color 1 low byte) transitions from non-zero to zero
        // This would indicate the palette is being cleared
        if (m_cgram[2] == 0 && last_cgram[2] != 0) {
            fprintf(stderr, "[PPU-WATCHPOINT] CGRAM[2] changed from $%02X to $00! frame=%llu scanline=%d\n",
                last_cgram[2], (unsigned long long)m_frame, scanline);
            fprintf(stderr, "[PPU-WATCHPOINT] CGRAM dump: [0-7]= %02X %02X %02X %02X %02X %02X %02X %02X\n",
                m_cgram[0], m_cgram[1], m_cgram[2], m_cgram[3],
                m_cgram[4], m_cgram[5], m_cgram[6], m_cgram[7]);
            fprintf(stderr, "[PPU-WATCHPOINT] CGRAM ptr=%p, this=%p\n", (void*)m_cgram.data(), (void*)this);
        }
        // Also track CGRAM[0] for consistency
        if (m_cgram[0] == 0 && last_cgram[0] != 0) {
            fprintf(stderr, "[PPU-WATCHPOINT] CGRAM[0] changed from $%02X to $00! frame=%llu scanline=%d (was frame=%llu scanline=%d)\n",
                last_cgram[0], (unsigned long long)m_frame, scanline,
                (unsigned long long)last_check_frame, last_check_scanline);
        }
        // Trace state at frame 274 to see what's in CGRAM before it gets cleared
        if (m_frame == 274 && scanline == 223) {
            fprintf(stderr, "[PPU-TRACE274] End of frame 274: CGRAM[0-7]= %02X %02X %02X %02X %02X %02X %02X %02X\n",
                m_cgram[0], m_cgram[1], m_cgram[2], m_cgram[3],
                m_cgram[4], m_cgram[5], m_cgram[6], m_cgram[7]);
        }
        for (int i = 0; i < 8; i++) last_cgram[i] = m_cgram[i];
        last_check_frame = m_frame;
        last_check_scanline = scanline;
    }

    // Minimal unconditional trace to verify function is called
    static int call_count = 0;
    if (scanline == 0 && call_count < 3) {
        fprintf(stderr, "[PPU-TRACE] render_scanline called, m_frame=%llu\n",
            (unsigned long long)m_frame);
        call_count++;
    }
    // Trace at frame 299 - duplicate of line 239 trace
    static int my_trace = 0;
    if (is_debug_mode() && m_frame >= 298 && m_frame <= 301 && scanline == 112 && my_trace < 5) {
        fprintf(stderr, "[PPU-MY-TRACE] PPU frame=%llu scanline 112\n",
            (unsigned long long)m_frame);
        my_trace++;
    }

    // Set the scanline for rendering
    m_scanline = scanline + 1;  // Internal scanline is 1-based

    // Debug: trace force_blank - PPU m_frame is incremented AFTER render_scanline
    // So when plugin frame_count=N, PPU m_frame=N-1 during rendering
    static int scanline_debug = 0;
    if (is_debug_mode() && scanline == 0 && ((m_frame + 1) % 100 == 0)) {
        SNES_PPU_DEBUG("PPU frame=%llu (plugin ~%llu) scanline=%d force_blank=%d TM=$%02X\n",
            (unsigned long long)m_frame, (unsigned long long)(m_frame + 1),
            scanline, m_force_blank ? 1 : 0, m_tm);
    }
    // Trace around frame 299 PPU (which is plugin frame 300)
    if (is_debug_mode() && m_frame >= 298 && m_frame <= 301 && scanline == 112 && scanline_debug < 5) {
        SNES_PPU_DEBUG("PPU frame=%llu scanline 112: force_blank=%d TM=$%02X mode=%d bright=%d\n",
            (unsigned long long)m_frame, m_force_blank ? 1 : 0, m_tm, m_bg_mode, m_brightness);
        scanline_debug++;
    }

    // Evaluate sprites for this scanline
    evaluate_sprites();

    // Render all 256 visible pixels
    static bool force_blank_traced = false;
    if (is_debug_mode() && m_frame >= 298 && m_frame <= 302 && scanline == 0 && !force_blank_traced) {
        fprintf(stderr, "[PPU-FB] Frame %llu scanline 0: m_force_blank=%d\n",
            (unsigned long long)m_frame, m_force_blank ? 1 : 0);
        force_blank_traced = (m_frame == 302);
    }
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        // Debug: trace first pixel - PPU m_frame is one less than plugin frame
        static bool traced = false;
        if (is_debug_mode() && m_frame == 299 && scanline == 0 && x == 0 && !traced) {
            fprintf(stderr, "[PPU-LOOP] PPU Frame 299 (plugin 300) scanline 0 x=0: m_force_blank=%d\n",
                m_force_blank ? 1 : 0);
            traced = true;
        }
        if (!m_force_blank) {
            render_pixel(x);
        } else {
            m_framebuffer[scanline * SCREEN_WIDTH + x] = 0xFF000000;
        }
    }

}

void PPU::render_pixel(int x) {
    int y = m_scanline - 1;

    // Debug: unconditional trace of first call
    static bool first_call = true;
    if (first_call && is_debug_mode()) {
        fprintf(stderr, "[PPU-TEST] render_pixel first call: m_frame=%llu m_scanline=%d y=%d x=%d\n",
            (unsigned long long)m_frame, m_scanline, y, x);
        first_call = false;
    }

    // Debug: Track non-black pixel count per frame
    static uint64_t debug_last_frame = 0;
    static int non_black_pixel_count = 0;
    if (m_frame != debug_last_frame) {
        // New frame - report and reset counter
        if (is_debug_mode() && debug_last_frame >= 299 && debug_last_frame <= 305) {
            SNES_PPU_DEBUG("Frame %llu: non-black pixels rendered = %d\n",
                (unsigned long long)debug_last_frame, non_black_pixel_count);
        }
        non_black_pixel_count = 0;
        debug_last_frame = m_frame;
    }

    // Layer pixel structure for priority compositing
    // Reference: bsnes/sfc/ppu-fast/line.cpp, fullsnes PPU documentation
    struct LayerPixel {
        uint16_t color;         // 15-bit BGR color from CGRAM
        uint8_t priority;       // Layer priority (0-3 for sprites, 0-1 for BG)
        uint8_t source;         // Source layer (0=backdrop, 1-4=BG1-4, 5=OBJ)
        bool color_math_enable; // Whether this layer participates in color math
    };

    // Get backdrop color (CGRAM index 0)
    uint16_t backdrop = m_cgram[0] | (m_cgram[1] << 8);

    // Debug: show backdrop value at frame 300 scanline 112
    // Minimal trace to see CGRAM state and verify pointer
    if (is_debug_mode() && m_frame >= 298 && m_frame <= 302 && y == 112 && x == 0) {
        fprintf(stderr, "[PPU-BACK] Frame %llu Backdrop: CGRAM[0]=$%02X CGRAM[1]=$%02X -> $%04X @%p\n",
            (unsigned long long)m_frame, m_cgram[0], m_cgram[1], backdrop, (void*)m_cgram.data());
    }

    // Get fixed color for sub-screen backdrop (register $2132)
    // Debug: trace fixed color
    if (is_debug_mode() && m_frame >= 298 && m_frame <= 302 && y == 112 && x == 0) {
        fprintf(stderr, "[PPU-FIXED] Fixed color R=%d G=%d B=%d, cgwsel=$%02X cgadsub=$%02X sub_screen=%d\n",
            m_fixed_color_r, m_fixed_color_g, m_fixed_color_b,
            m_cgwsel, m_cgadsub, m_sub_screen_bg_obj ? 1 : 0);
    }
    uint16_t fixed_color = m_fixed_color_r | (m_fixed_color_g << 5) | (m_fixed_color_b << 10);

    // Initialize main and sub screen pixels to backdrop/fixed color
    LayerPixel main_pixel = {backdrop, 0, 0, m_backdrop_color_math};
    LayerPixel sub_pixel = {fixed_color, 0, 0, false};

    // ============================================================================
    // LAYER RENDERING FOR BOTH MAIN AND SUB SCREENS
    // ============================================================================
    // Reference: fullsnes PPU documentation, bsnes/sfc/ppu-fast/line.cpp
    //
    // The SNES renders both main screen (TM register $212C) and sub screen
    // (TS register $212D) in parallel. The sub screen is used as a source
    // for color math blending with the main screen.
    //
    // Priority order for Mode 1 (Super Mario All-Stars uses this):
    //   If BG3 priority bit set ($2105.3):
    //     BG3.pri1, OBJ.pri3, BG1.pri1, BG2.pri1, OBJ.pri2, BG1.pri0, BG2.pri0,
    //     OBJ.pri1, BG3.pri0, OBJ.pri0, backdrop
    //   If BG3 priority bit clear:
    //     OBJ.pri3, BG1.pri1, BG2.pri1, OBJ.pri2, BG1.pri0, BG2.pri0, OBJ.pri1,
    //     BG3.pri1, BG3.pri0, OBJ.pri0, backdrop
    // ============================================================================

    // Render all background layers (we need pixel data for priority sorting)
    uint8_t bg_pixel[4] = {0, 0, 0, 0};
    uint8_t bg_priority[4] = {0, 0, 0, 0};

    // Determine which BGs exist in current mode
    int num_bgs = 0;
    switch (m_bg_mode) {
        case 0: num_bgs = 4; break;  // 4 BGs, 2bpp each
        case 1: num_bgs = 3; break;  // 3 BGs (BG1/BG2 4bpp, BG3 2bpp)
        case 2:
        case 3:
        case 4:
        case 5:
        case 6: num_bgs = 2; break;  // 2 BGs
        case 7: num_bgs = 1; break;  // 1 BG (Mode 7)
    }

    // Render all backgrounds (we'll check TM/TS later for main/sub screen enable)
    for (int bg = 0; bg < num_bgs; bg++) {
        if (m_bg_mode == 7 && bg == 0) {
            render_mode7_pixel(x, bg_pixel[0], bg_priority[0]);
        } else {
            render_background_pixel(bg, x, bg_pixel[bg], bg_priority[bg]);
        }
    }

    // Render sprites
    uint8_t sprite_pixel = 0;
    uint8_t sprite_priority = 0;
    bool sprite_palette_4_7 = false;
    render_sprite_pixel(x, sprite_pixel, sprite_priority, sprite_palette_4_7);

    // Helper lambda to composite layers with priority
    // Returns the winning layer's color, source ID, and color math enable flag
    auto composite_screen = [&](uint8_t layer_mask) -> LayerPixel {
        LayerPixel result = {backdrop, 0, 0, m_backdrop_color_math};

        // Priority-based compositing based on BG mode
        // We go from lowest to highest priority, letting higher priority overwrite

        switch (m_bg_mode) {
            case 0: {
                // Mode 0 priority (lowest to highest):
                // BG4.pri0, BG3.pri0, OBJ.pri0, BG4.pri1, BG3.pri1, OBJ.pri1,
                // BG2.pri0, BG1.pri0, OBJ.pri2, BG2.pri1, BG1.pri1, OBJ.pri3

                // BG4 priority 0
                if ((layer_mask & 0x08) && bg_pixel[3] && !bg_priority[3]) {
                    result = {get_color(0, bg_pixel[3]), bg_priority[3], 4, m_bg_color_math[3]};
                }
                // BG3 priority 0
                if ((layer_mask & 0x04) && bg_pixel[2] && !bg_priority[2]) {
                    result = {get_color(0, bg_pixel[2]), bg_priority[2], 3, m_bg_color_math[2]};
                }
                // OBJ priority 0
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 0) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG4 priority 1
                if ((layer_mask & 0x08) && bg_pixel[3] && bg_priority[3]) {
                    result = {get_color(0, bg_pixel[3]), bg_priority[3], 4, m_bg_color_math[3]};
                }
                // BG3 priority 1
                if ((layer_mask & 0x04) && bg_pixel[2] && bg_priority[2]) {
                    result = {get_color(0, bg_pixel[2]), bg_priority[2], 3, m_bg_color_math[2]};
                }
                // OBJ priority 1
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 1) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 0
                if ((layer_mask & 0x02) && bg_pixel[1] && !bg_priority[1]) {
                    result = {get_color(0, bg_pixel[1]), bg_priority[1], 2, m_bg_color_math[1]};
                }
                // BG1 priority 0
                if ((layer_mask & 0x01) && bg_pixel[0] && !bg_priority[0]) {
                    result = {get_color(0, bg_pixel[0]), bg_priority[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 2
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 2) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 1
                if ((layer_mask & 0x02) && bg_pixel[1] && bg_priority[1]) {
                    result = {get_color(0, bg_pixel[1]), bg_priority[1], 2, m_bg_color_math[1]};
                }
                // BG1 priority 1
                if ((layer_mask & 0x01) && bg_pixel[0] && bg_priority[0]) {
                    result = {get_color(0, bg_pixel[0]), bg_priority[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 3
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 3) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                break;
            }

            case 1: {
                // Mode 1 priority depends on BG3 priority bit ($2105.3)
                // If BG3 priority is set, BG3.pri1 goes to the very front

                // Start from lowest priority
                // BG3 priority 0
                if ((layer_mask & 0x04) && bg_pixel[2] && !bg_priority[2]) {
                    result = {get_color(0, bg_pixel[2]), bg_priority[2], 3, m_bg_color_math[2]};
                }
                // OBJ priority 0
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 0) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG3 priority 1 (if BG3 priority bit is NOT set)
                if (!m_bg3_priority && (layer_mask & 0x04) && bg_pixel[2] && bg_priority[2]) {
                    result = {get_color(0, bg_pixel[2]), bg_priority[2], 3, m_bg_color_math[2]};
                }
                // OBJ priority 1
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 1) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 0
                if ((layer_mask & 0x02) && bg_pixel[1] && !bg_priority[1]) {
                    result = {get_color(0, bg_pixel[1]), bg_priority[1], 2, m_bg_color_math[1]};
                }
                // BG1 priority 0
                if ((layer_mask & 0x01) && bg_pixel[0] && !bg_priority[0]) {
                    result = {get_color(0, bg_pixel[0]), bg_priority[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 2
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 2) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 1
                if ((layer_mask & 0x02) && bg_pixel[1] && bg_priority[1]) {
                    result = {get_color(0, bg_pixel[1]), bg_priority[1], 2, m_bg_color_math[1]};
                }
                // BG1 priority 1
                if ((layer_mask & 0x01) && bg_pixel[0] && bg_priority[0]) {
                    result = {get_color(0, bg_pixel[0]), bg_priority[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 3
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 3) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG3 priority 1 (if BG3 priority bit IS set - highest priority)
                if (m_bg3_priority && (layer_mask & 0x04) && bg_pixel[2] && bg_priority[2]) {
                    result = {get_color(0, bg_pixel[2]), bg_priority[2], 3, m_bg_color_math[2]};
                }
                break;
            }

            case 2:
            case 3:
            case 4:
            case 5:
            case 6: {
                // Modes 2-6: 2 BGs with similar priority structure
                // BG2.pri0, OBJ.pri0, BG1.pri0, OBJ.pri1, BG2.pri1, OBJ.pri2,
                // BG1.pri1, OBJ.pri3

                // BG2 priority 0
                if ((layer_mask & 0x02) && bg_pixel[1] && !bg_priority[1]) {
                    result = {get_color(0, bg_pixel[1]), bg_priority[1], 2, m_bg_color_math[1]};
                }
                // OBJ priority 0
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 0) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG1 priority 0
                if ((layer_mask & 0x01) && bg_pixel[0] && !bg_priority[0]) {
                    result = {get_color(0, bg_pixel[0]), bg_priority[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 1
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 1) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 1
                if ((layer_mask & 0x02) && bg_pixel[1] && bg_priority[1]) {
                    result = {get_color(0, bg_pixel[1]), bg_priority[1], 2, m_bg_color_math[1]};
                }
                // OBJ priority 2
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 2) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG1 priority 1
                if ((layer_mask & 0x01) && bg_pixel[0] && bg_priority[0]) {
                    result = {get_color(0, bg_pixel[0]), bg_priority[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 3
                if ((layer_mask & 0x10) && sprite_pixel && sprite_priority == 3) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                break;
            }

            case 7: {
                // Mode 7: Single BG with EXTBG option
                // BG1, OBJ.pri0, OBJ.pri1, OBJ.pri2, OBJ.pri3

                // BG1 (Mode 7 has no priority bit in standard mode)
                if ((layer_mask & 0x01) && bg_pixel[0]) {
                    result = {get_color(0, bg_pixel[0]), 0, 1, m_bg_color_math[0]};
                }
                // OBJ (all priorities above BG in Mode 7)
                if ((layer_mask & 0x10) && sprite_pixel) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                break;
            }
        }

        return result;
    };

    // Composite main screen (using TM register)
    main_pixel = composite_screen(m_tm);

    // Composite sub screen (using TS register)
    // Sub screen uses fixed color as backdrop, not CGRAM[0]
    sub_pixel = composite_screen(m_ts);
    if (sub_pixel.source == 0) {
        // If sub screen shows backdrop, use fixed color instead
        sub_pixel.color = fixed_color;
    }

    // ============================================================================
    // COLOR MATH APPLICATION
    // ============================================================================
    // Reference: fullsnes CGWSEL/CGADSUB, bsnes/sfc/ppu/screen.cpp blend()
    //
    // Color math blends main screen with either sub screen or fixed color.
    // The operation is controlled by:
    //   CGWSEL ($2130): Color math enable conditions, clip to black, sub/fixed select
    //   CGADSUB ($2131): Which layers participate, add/subtract, half-result
    //
    // Sprite palettes 0-3 reject color math (only palettes 4-7 can be blended)
    // ============================================================================

    uint16_t final_color = main_pixel.color;

    // Determine if color math should be applied
    // CGWSEL bits 4-5 control color math enable based on color window
    bool apply_color_math = false;
    switch (m_color_math_prevent) {
        case 0: apply_color_math = true; break;   // Always
        case 1: apply_color_math = !get_color_window(x); break;  // Inside window
        case 2: apply_color_math = get_color_window(x); break;   // Outside window
        case 3: apply_color_math = false; break;  // Never
    }

    // Also check if the main screen layer participates in color math
    if (apply_color_math && main_pixel.color_math_enable) {
        // Get the color to blend with
        // CGWSEL bit 1: 0 = use fixed color, 1 = use sub screen
        uint16_t blend_color;
        if (m_sub_screen_bg_obj) {
            blend_color = sub_pixel.color;
        } else {
            blend_color = fixed_color;
        }

        // Extract RGB components (5 bits each)
        int main_r = main_pixel.color & 0x1F;
        int main_g = (main_pixel.color >> 5) & 0x1F;
        int main_b = (main_pixel.color >> 10) & 0x1F;

        int blend_r = blend_color & 0x1F;
        int blend_g = (blend_color >> 5) & 0x1F;
        int blend_b = (blend_color >> 10) & 0x1F;

        int result_r, result_g, result_b;

        if (m_color_math_add) {
            // Addition
            result_r = main_r + blend_r;
            result_g = main_g + blend_g;
            result_b = main_b + blend_b;
        } else {
            // Subtraction
            result_r = main_r - blend_r;
            result_g = main_g - blend_g;
            result_b = main_b - blend_b;
        }

        // Apply half-brightness if enabled
        // Note: Half only applies when sub screen has a non-backdrop pixel or using fixed color
        if (m_color_math_half) {
            // Only halve if sub screen has content or using fixed color
            bool should_halve = !m_sub_screen_bg_obj || (sub_pixel.source != 0);
            if (should_halve) {
                result_r >>= 1;
                result_g >>= 1;
                result_b >>= 1;
            }
        }

        // Clamp to 0-31 range
        result_r = std::clamp(result_r, 0, 31);
        result_g = std::clamp(result_g, 0, 31);
        result_b = std::clamp(result_b, 0, 31);

        final_color = result_r | (result_g << 5) | (result_b << 10);
    }

    // ============================================================================
    // CLIP TO BLACK (CGWSEL bits 6-7)
    // ============================================================================
    // This can force the main screen to black based on color window
    bool clip_to_black = false;
    switch (m_color_math_clip) {
        case 0: clip_to_black = false; break;  // Never
        case 1: clip_to_black = !get_color_window(x); break;  // Inside window
        case 2: clip_to_black = get_color_window(x); break;   // Outside window
        case 3: clip_to_black = true; break;   // Always
    }

    if (clip_to_black) {
        final_color = 0;
    }

    // ============================================================================
    // PSEUDO-HIRES MODE OUTPUT
    // ============================================================================
    // Reference: sneslab.net Horizontal Pseudo 512 Mode, fullsnes SETINI
    //
    // When pseudo-hires is enabled ($2133.3), the PPU outputs 512 pixels per
    // scanline by alternating between sub screen (even columns) and main screen
    // (odd columns). On a CRT, this creates a transparency effect as adjacent
    // pixels blend together.
    //
    // In our emulator, we render to a 512-wide framebuffer when pseudo-hires
    // is active, or duplicate pixels for standard 256-pixel mode.
    // ============================================================================

    // Helper to convert 15-bit SNES color to 32-bit ARGB with brightness
    auto apply_brightness_and_convert = [this](uint16_t color) -> uint32_t {
        int r = (color & 0x1F) * m_brightness / 15;
        int g = ((color >> 5) & 0x1F) * m_brightness / 15;
        int b = ((color >> 10) & 0x1F) * m_brightness / 15;

        // Convert 5-bit color to 8-bit (expand using upper bits for accuracy)
        r = (r << 3) | (r >> 2);
        g = (g << 3) | (g >> 2);
        b = (b << 3) | (b >> 2);

        return 0xFF000000 | (b << 16) | (g << 8) | r;
    };

    if (m_pseudo_hires) {
        // Pseudo-hires: output 512 pixels per scanline
        // Even pixel (2*x): sub screen color
        // Odd pixel (2*x+1): main screen color
        // This interleaving creates transparency when viewed on CRT

        // Get sub screen color with brightness
        uint16_t sub_color = sub_pixel.color;
        // Apply clip to black to sub screen as well if needed
        if (clip_to_black) {
            sub_color = 0;
        }

        uint32_t main_argb = apply_brightness_and_convert(final_color);
        uint32_t sub_argb = apply_brightness_and_convert(sub_color);

        // Write both pixels (512-wide framebuffer)
        m_framebuffer[y * 512 + x * 2] = sub_argb;      // Even: sub screen
        m_framebuffer[y * 512 + x * 2 + 1] = main_argb; // Odd: main screen
    } else {
        // Standard 256-pixel mode
        uint32_t argb = apply_brightness_and_convert(final_color);
        m_framebuffer[y * SCREEN_WIDTH + x] = argb;

        // Debug: Track non-black pixels and show pixel color details
        if (argb != 0xFF000000) {
            non_black_pixel_count++;
        }

        // Debug: Show detailed pixel info for specific scanlines at frame 300
        if (is_debug_mode() && m_frame == 300 && y == 112 && x < 16) {
            SNES_PPU_DEBUG("PIXEL[%d,%d]: main_src=%d final_color=$%04X -> ARGB=$%08X (bright=%d)\n",
                x, y, main_pixel.source, final_color, argb, m_brightness);
            if (main_pixel.source >= 1 && main_pixel.source <= 4) {
                // BG pixel - show CGRAM lookup details
                int bg_idx = main_pixel.source - 1;
                SNES_PPU_DEBUG("  BG%d pixel=%d color from CGRAM[$%04X]=$%04X\n",
                    main_pixel.source, bg_pixel[bg_idx],
                    bg_pixel[bg_idx] * 2, main_pixel.color);
            }
        }
    }
}

// ============================================================================
// COLOR WINDOW EVALUATION
// ============================================================================
// Reference: fullsnes Window documentation, bsnes/sfc/ppu/window.cpp
//
// The color window is used to mask regions of the screen for color math
// and clip-to-black operations. It uses the same window registers as
// background/sprite masking but with its own enable and logic settings.
// ============================================================================
bool PPU::get_color_window(int x) const {
    // Evaluate window 1
    bool w1 = false;
    if (m_color_window1_enable) {
        w1 = (x >= m_window1_left && x <= m_window1_right);
        if (m_color_window1_invert) w1 = !w1;
    }

    // Evaluate window 2
    bool w2 = false;
    if (m_color_window2_enable) {
        w2 = (x >= m_window2_left && x <= m_window2_right);
        if (m_color_window2_invert) w2 = !w2;
    }

    // Combine windows based on logic mode
    // 0 = OR, 1 = AND, 2 = XOR, 3 = XNOR
    bool result = false;
    if (!m_color_window1_enable && !m_color_window2_enable) {
        // No windows enabled - always outside
        result = false;
    } else if (m_color_window1_enable && !m_color_window2_enable) {
        result = w1;
    } else if (!m_color_window1_enable && m_color_window2_enable) {
        result = w2;
    } else {
        // Both windows enabled - apply logic
        switch (m_color_window_logic) {
            case 0: result = w1 || w2; break;  // OR
            case 1: result = w1 && w2; break;  // AND
            case 2: result = w1 != w2; break;  // XOR
            case 3: result = w1 == w2; break;  // XNOR
        }
    }

    return result;
}

void PPU::render_background_pixel(int bg, int x, uint8_t& pixel, uint8_t& priority) {
    pixel = 0;
    priority = 0;

    // Debug: trace BG rendering on scanline 112 for first 8 pixels
    bool debug_tile = is_debug_mode() && (m_scanline - 1) == 112 && x < 8 &&
                      (m_frame >= 300 && m_frame <= 301 && bg == 1);

    // Comprehensive VRAM dump at frame 300 for Super Mario All-Stars debugging
    static bool dumped_vram = false;
    if (is_debug_mode() && m_frame == 300 && !dumped_vram && x == 0 && (m_scanline-1) == 0 && bg == 0) {
        SNES_PPU_DEBUG("=== FRAME 300 PPU STATE DUMP ===\n");
        SNES_PPU_DEBUG("Mode: %d, BG3 Priority: %d, Brightness: %d\n",
            m_bg_mode, m_bg3_priority ? 1 : 0, m_brightness);
        SNES_PPU_DEBUG("TM=$%02X (BG1:%d BG2:%d BG3:%d BG4:%d OBJ:%d)\n",
            m_tm, (m_tm >> 0) & 1, (m_tm >> 1) & 1, (m_tm >> 2) & 1,
            (m_tm >> 3) & 1, (m_tm >> 4) & 1);
        SNES_PPU_DEBUG("Pseudo-hires: %d, Interlace: %d, Overscan: %d\n",
            m_pseudo_hires ? 1 : 0, m_interlace ? 1 : 0, m_overscan ? 1 : 0);

        // Dump all BG settings
        for (int i = 0; i < 4; i++) {
            SNES_PPU_DEBUG("BG%d: tilemap=$%04X (%dx%d) chr=$%04X tile_size=%d hofs=%d vofs=%d\n",
                i + 1, m_bg_tilemap_addr[i],
                m_bg_tilemap_width[i] ? 64 : 32, m_bg_tilemap_height[i] ? 64 : 32,
                m_bg_chr_addr[i], m_bg_tile_size[i] ? 16 : 8,
                m_bg_hofs[i], m_bg_vofs[i]);
        }

        // Dump first 8 tilemap entries for BG1 and BG2
        for (int bg_idx = 0; bg_idx < 2; bg_idx++) {
            uint16_t tm_base = m_bg_tilemap_addr[bg_idx];
            SNES_PPU_DEBUG("BG%d Tilemap (first 8 entries at $%04X):\n", bg_idx + 1, tm_base);
            for (int i = 0; i < 8; i++) {
                uint16_t addr = tm_base + i * 2;
                uint8_t lo = m_vram[addr & 0xFFFF];
                uint8_t hi = m_vram[(addr+1) & 0xFFFF];
                int tile = lo | ((hi & 0x03) << 8);
                int pal = (hi >> 2) & 0x07;
                int pri = (hi >> 5) & 1;
                int hf = (hi >> 6) & 1;
                int vf = (hi >> 7) & 1;
                SNES_PPU_DEBUG("  [%d] tile=%3d pal=%d pri=%d hf=%d vf=%d\n",
                    i, tile, pal, pri, hf, vf);
            }
        }

        // Dump first 32 bytes of chr data for BG1 and BG2
        for (int bg_idx = 0; bg_idx < 2; bg_idx++) {
            uint16_t chr_base = m_bg_chr_addr[bg_idx];
            SNES_PPU_DEBUG("BG%d Chr data at $%04X (first 32 bytes):\n", bg_idx + 1, chr_base);
            SNES_PPU_DEBUG("  ");
            for (int j = 0; j < 32; j++) {
                fprintf(stderr, "%02X ", m_vram[(chr_base + j) & 0xFFFF]);
                if ((j + 1) % 16 == 0 && j < 31) fprintf(stderr, "\n  ");
            }
            fprintf(stderr, "\n");
        }

        // Dump tile 15 chr data specifically for BG2 (at offset 15*32 = 480 = 0x1E0)
        uint16_t tile15_addr = m_bg_chr_addr[1] + 15 * 32;  // BG2 tile 15
        SNES_PPU_DEBUG("BG2 Tile 15 chr data at $%04X (32 bytes for 4bpp tile):\n", tile15_addr);
        SNES_PPU_DEBUG("  ");
        for (int j = 0; j < 32; j++) {
            fprintf(stderr, "%02X ", m_vram[(tile15_addr + j) & 0xFFFF]);
            if ((j + 1) % 16 == 0 && j < 31) fprintf(stderr, "\n  ");
        }
        fprintf(stderr, "\n");

        // Check total non-zero bytes in BG2 chr area
        int nonzero = 0;
        for (int i = 0; i < 0x2000; i++) {
            if (m_vram[(m_bg_chr_addr[1] + i) & 0xFFFF] != 0) nonzero++;
        }
        SNES_PPU_DEBUG("BG2 chr area ($%04X-$%04X): %d non-zero bytes out of 8192\n",
            m_bg_chr_addr[1], m_bg_chr_addr[1] + 0x2000 - 1, nonzero);

        // Dump CGRAM (palette) - first 32 colors with RGB breakdown
        SNES_PPU_DEBUG("CGRAM (first 32 colors as BGR555, with RGB8 conversion):\n");
        for (int i = 0; i < 32; i++) {
            uint16_t color = m_cgram[i * 2] | (m_cgram[i * 2 + 1] << 8);
            int r5 = color & 0x1F;
            int g5 = (color >> 5) & 0x1F;
            int b5 = (color >> 10) & 0x1F;
            // Convert to 8-bit
            int r8 = (r5 << 3) | (r5 >> 2);
            int g8 = (g5 << 3) | (g5 >> 2);
            int b8 = (b5 << 3) | (b5 >> 2);
            if (i % 4 == 0) SNES_PPU_DEBUG("  [%3d]: ", i);
            fprintf(stderr, "$%04X(R%02X G%02X B%02X) ", color, r8, g8, b8);
            if ((i + 1) % 4 == 0) fprintf(stderr, "\n");
        }

        // Also dump raw CGRAM bytes to check for corruption
        SNES_PPU_DEBUG("CGRAM raw bytes (first 64 bytes):\n  ");
        for (int i = 0; i < 64; i++) {
            fprintf(stderr, "%02X ", m_cgram[i]);
            if ((i + 1) % 16 == 0 && i < 63) fprintf(stderr, "\n  ");
        }
        fprintf(stderr, "\n");

        dumped_vram = true;
    }

    // Dump actual rendered pixels at frame 300 for scanlines 50, 100, 150
    static bool dumped_pixels = false;
    if (is_debug_mode() && m_frame == 300 && !dumped_pixels && bg == 0 && x == 255) {
        int scan = m_scanline - 1;
        if (scan == 50 || scan == 100 || scan == 150) {
            SNES_PPU_DEBUG("=== Rendered pixels at scanline %d, frame 300 ===\n", scan);
            SNES_PPU_DEBUG("First 32 pixels (ARGB32): ");
            int stride = m_pseudo_hires ? 512 : 256;
            for (int px = 0; px < 32; px++) {
                uint32_t pixel = m_framebuffer[scan * stride + px];
                if (px % 8 == 0 && px > 0) fprintf(stderr, "\n  ");
                fprintf(stderr, "%08X ", pixel);
            }
            fprintf(stderr, "\n");
            if (scan == 150) dumped_pixels = true;
        }
    }

    // Unconditional trace to see if BG2 is ever rendered
    static int bg2_call_count = 0;
    if (is_debug_mode() && bg == 1 && m_frame >= 300 && bg2_call_count < 5) {
        SNES_PPU_DEBUG("BG2 render called: frame=%llu scanline=%d x=%d TM=$%02X mode=%d\n",
            (unsigned long long)m_frame, m_scanline - 1, x, m_tm, m_bg_mode);
        bg2_call_count++;
    }

    // Also trace once per frame to see if backgrounds are returning non-zero pixels anywhere
    static int last_nonzero_frame = -1;
    static int nonzero_count = 0;
    if (is_debug_mode() && m_frame >= 10 && m_frame <= 15) {
        // This is after the function returns a pixel, so we can't check here
        // We'll add a different mechanism
    }

    // Get scroll values
    // Note: Scroll registers are 10-bit signed values
    int scroll_x = m_bg_hofs[bg] & 0x3FF;
    int scroll_y = m_bg_vofs[bg] & 0x3FF;

    // Apply mosaic
    int mosaic_x = x;
    int mosaic_y = m_scanline - 1;
    if (m_mosaic_enabled[bg] && m_mosaic_size > 1) {
        mosaic_x = (mosaic_x / m_mosaic_size) * m_mosaic_size;
        mosaic_y = (mosaic_y / m_mosaic_size) * m_mosaic_size;
    }

    // Calculate pixel position in BG (10-bit wrap for 1024 pixel BG space)
    int px = (mosaic_x + scroll_x) & 0x3FF;
    int py = (mosaic_y + scroll_y) & 0x3FF;

    // Get tile size (8x8 or 16x16)
    int tile_size = m_bg_tile_size[bg] ? 16 : 8;

    // Calculate tile coordinates
    int tile_x = px / tile_size;
    int tile_y = py / tile_size;
    int fine_x = px % tile_size;
    int fine_y = py % tile_size;

    // Get tilemap address
    // BGnSC stores BYTE address: (value & 0xFC) << 8
    uint16_t tilemap_base = m_bg_tilemap_addr[bg];
    int tilemap_width = m_bg_tilemap_width[bg] ? 64 : 32;
    int tilemap_height = m_bg_tilemap_height[bg] ? 64 : 32;

    // Handle tilemap wrapping
    int tilemap_x = tile_x % tilemap_width;
    int tilemap_y = tile_y % tilemap_height;

    // Calculate screen offset for 64-wide/tall tilemaps
    // Each 32x32 screen is 2KB (32*32*2 bytes)
    // Layout: SC0 | SC1 (if width=64)
    //         SC2 | SC3 (if both width and height=64)
    int screen_offset = 0;
    if (tilemap_width == 64 && tilemap_x >= 32) {
        screen_offset += 0x800;  // 2KB for second horizontal screen
        tilemap_x -= 32;
    }
    if (tilemap_height == 64 && tilemap_y >= 32) {
        screen_offset += tilemap_width == 64 ? 0x1000 : 0x800;
        tilemap_y -= 32;
    }

    // Get tilemap entry (2 bytes per tile)
    // Each tilemap entry: vhopppcc cccccccc
    uint16_t tilemap_addr = tilemap_base + screen_offset + (tilemap_y * 32 + tilemap_x) * 2;
    uint8_t tile_lo = m_vram[tilemap_addr & 0xFFFF];
    uint8_t tile_hi = m_vram[(tilemap_addr + 1) & 0xFFFF];

    int tile_num = tile_lo | ((tile_hi & 0x03) << 8);  // 10-bit tile number
    int palette = (tile_hi >> 2) & 0x07;               // 3-bit palette
    priority = (tile_hi >> 5) & 0x01;                  // 1-bit priority
    bool hflip = (tile_hi & 0x40) != 0;
    bool vflip = (tile_hi & 0x80) != 0;

    if (debug_tile) {
        SNES_PPU_DEBUG("BG%d x=%d: mode=%d bpp=%d tilemap_base=$%04X tilemap_addr=$%04X tile=%d(%02X %02X) pal=%d pri=%d\n",
            bg + 1, x, m_bg_mode, (m_bg_mode == 3 && bg == 0) ? 8 : 4,
            tilemap_base, tilemap_addr, tile_num, tile_lo, tile_hi, palette, priority);
        // Dump VRAM at tilemap address
        SNES_PPU_DEBUG("  VRAM@tilemap: %02X %02X %02X %02X %02X %02X %02X %02X\n",
            m_vram[(tilemap_addr) & 0xFFFF], m_vram[(tilemap_addr+1) & 0xFFFF],
            m_vram[(tilemap_addr+2) & 0xFFFF], m_vram[(tilemap_addr+3) & 0xFFFF],
            m_vram[(tilemap_addr+4) & 0xFFFF], m_vram[(tilemap_addr+5) & 0xFFFF],
            m_vram[(tilemap_addr+6) & 0xFFFF], m_vram[(tilemap_addr+7) & 0xFFFF]);
    }

    // Handle 16x16 tiles (composed of 4 8x8 tiles in a 2x2 grid)
    if (tile_size == 16) {
        // Tiles are arranged: [N  ][N+1]
        //                     [N+16][N+17]
        int x_offset = (fine_x >= 8) ? 1 : 0;
        int y_offset = (fine_y >= 8) ? 16 : 0;

        if (hflip) x_offset = 1 - x_offset;
        if (vflip) y_offset = (y_offset == 16) ? 0 : 16;

        tile_num += x_offset + y_offset;
        fine_x &= 7;
        fine_y &= 7;
    }

    // Apply flip to fine coordinates
    if (hflip) fine_x = 7 - fine_x;
    if (vflip) fine_y = 7 - fine_y;

    // Get bits per pixel based on mode
    // Mode 0: All BGs 2bpp (4 colors each, 8 palettes)
    // Mode 1: BG1/BG2 4bpp (16 colors), BG3 2bpp
    // Mode 2: BG1/BG2 4bpp, offset-per-tile
    // Mode 3: BG1 8bpp (256 colors), BG2 4bpp
    // Mode 4: BG1 8bpp, BG2 2bpp, offset-per-tile
    // Mode 5: BG1/BG2 4bpp, 16x8 or 16x16, hi-res
    // Mode 6: BG1 4bpp, 16x8 or 16x16, offset-per-tile, hi-res
    // Mode 7: BG1 8bpp, affine transformation
    int bpp;
    switch (m_bg_mode) {
        case 0: bpp = 2; break;
        case 1: bpp = (bg < 2) ? 4 : 2; break;
        case 2: bpp = 4; break;
        case 3: bpp = (bg == 0) ? 8 : 4; break;
        case 4: bpp = (bg == 0) ? 8 : 2; break;
        case 5: bpp = 4; break;
        case 6: bpp = 4; break;
        default: bpp = 8; break;
    }

    // Get character data address
    // BGnNBA stores BYTE address: (value & 0x0F) << 13 for BG1/BG3
    // Tile size in bytes: 8 rows * bpp bytes per row (bitplanes interleaved)
    uint16_t chr_base = m_bg_chr_addr[bg];
    uint16_t chr_addr = chr_base + tile_num * (bpp * 8);

    if (debug_tile) {
        SNES_PPU_DEBUG("  chr_base=$%04X chr_addr=$%04X bpp=%d fine_x=%d fine_y=%d tile_num=%d\n",
            chr_base, chr_addr, bpp, fine_x, fine_y, tile_num);
        // Dump VRAM at character address for first row
        SNES_PPU_DEBUG("  VRAM@chr row0: %02X %02X %02X %02X %02X %02X %02X %02X\n",
            m_vram[(chr_addr) & 0xFFFF], m_vram[(chr_addr+1) & 0xFFFF],
            m_vram[(chr_addr+2) & 0xFFFF], m_vram[(chr_addr+3) & 0xFFFF],
            m_vram[(chr_addr+4) & 0xFFFF], m_vram[(chr_addr+5) & 0xFFFF],
            m_vram[(chr_addr+6) & 0xFFFF], m_vram[(chr_addr+7) & 0xFFFF]);
    }

    // Read tile data using SNES bitplane format
    // 2bpp: planes 0,1 interleaved (bytes 0,1 for row 0, etc.)
    // 4bpp: planes 0,1 first 16 bytes, planes 2,3 next 16 bytes
    // 8bpp: planes 0,1 first 16 bytes, 2,3 next 16, 4,5 next 16, 6,7 next 16
    uint8_t color_index = 0;
    for (int bit = 0; bit < bpp; bit++) {
        // Offset calculation for SNES planar format:
        // Planes are grouped in pairs, with 16 bytes per pair (8 rows * 2 bytes)
        int plane_offset = (bit / 2) * 16 + (bit & 1);
        uint16_t addr = chr_addr + fine_y * 2 + plane_offset;
        uint8_t plane = m_vram[addr & 0xFFFF];
        if (plane & (0x80 >> fine_x)) {
            color_index |= (1 << bit);
        }
        if (debug_tile && bit == 0) {
            SNES_PPU_DEBUG("  plane0 addr=$%04X data=$%02X\n", addr, plane);
        }
    }

    if (debug_tile) {
        SNES_PPU_DEBUG("  color_index=%d (final pixel will be %d)\n", color_index,
            (bpp == 8) ? color_index : ((bpp == 2) ? ((m_bg_mode == 0 ? bg * 32 : 0) + (palette << 2) + color_index) : ((palette << 4) + color_index)));
    }

    // Calculate final pixel value (palette index into CGRAM)
    // Mode 0: Each BG has its own 32-color region (8 palettes * 4 colors)
    //   BG1: colors 0-31, BG2: colors 32-63, BG3: colors 64-95, BG4: colors 96-127
    // Other modes: All BGs share the 128-color BG palette space (8 palettes * 16 colors for 4bpp)
    if (color_index != 0) {
        if (bpp == 8) {
            // 8bpp: direct index into first 256 colors, no palette selection
            pixel = color_index;
        } else if (bpp == 2) {
            // 2bpp: 4 colors per palette
            // In Mode 0, each BG is offset by 32 colors (bg * 32 + palette * 4 + color)
            int bg_offset = (m_bg_mode == 0) ? (bg * 32) : 0;
            pixel = bg_offset + (palette << 2) + color_index;
        } else {
            // 4bpp: 16 colors per palette
            pixel = (palette << 4) + color_index;
        }

        // Debug: count non-zero BG pixels
        if (debug_tile) {
            SNES_PPU_DEBUG("  -> VISIBLE PIXEL: bg=%d pixel=%d (color_index=%d)\n", bg + 1, pixel, color_index);
        }

        // Track BG2 non-transparent pixels per frame for debugging
        static uint64_t bg2_debug_frame = 0;
        static int bg2_nonzero_count = 0;
        if (bg == 1) {
            if (m_frame != bg2_debug_frame) {
                if (is_debug_mode() && bg2_debug_frame >= 299 && bg2_debug_frame <= 305) {
                    SNES_PPU_DEBUG("Frame %llu: BG2 returned %d non-transparent pixels\n",
                        (unsigned long long)bg2_debug_frame, bg2_nonzero_count);
                }
                bg2_nonzero_count = 0;
                bg2_debug_frame = m_frame;
            }
            bg2_nonzero_count++;
        }
    } else if (debug_tile) {
        SNES_PPU_DEBUG("  -> TRANSPARENT (color_index=0)\n");
    }
}

void PPU::render_mode7_pixel(int x, uint8_t& pixel, uint8_t& priority) {
    pixel = 0;
    priority = 0;

    int screen_x = x;
    int screen_y = m_scanline - 1;

    // Apply horizontal flip
    if (m_m7_hflip) {
        screen_x = 255 - screen_x;
    }

    // Apply vertical flip
    if (m_m7_vflip) {
        screen_y = 255 - screen_y;
    }

    // Mode 7 transformation
    // X = A*(x-x0) + B*(y-y0) + x0 + HOFS
    // Y = C*(x-x0) + D*(y-y0) + y0 + VOFS

    int32_t cx = m_m7x;
    int32_t cy = m_m7y;
    int32_t hofs = m_m7hofs;
    int32_t vofs = m_m7vofs;

    // Calculate transformed coordinates (fixed point)
    int32_t vx = screen_x - cx;
    int32_t vy = screen_y - cy;

    int32_t tx = ((m_m7a * vx) + (m_m7b * vy) + (cx << 8) + (hofs << 8)) >> 8;
    int32_t ty = ((m_m7c * vx) + (m_m7d * vy) + (cy << 8) + (vofs << 8)) >> 8;

    // Handle wrapping/clamping
    bool out_of_bounds = (tx < 0 || tx >= 1024 || ty < 0 || ty >= 1024);

    if (out_of_bounds) {
        switch (m_m7_wrap) {
            case 0:  // Wrap
                tx &= 0x3FF;
                ty &= 0x3FF;
                break;
            case 1:  // Transparent
                return;
            case 2:  // Tile 0
            case 3:
                tx = 0;
                ty = 0;
                break;
        }
    }

    // Mode 7 VRAM layout (128x128 tilemap, 8bpp character data):
    // - VRAM is word-addressed in hardware, we use byte addressing
    // - Even bytes contain tile numbers (tilemap)
    // - Odd bytes contain pixel colors (character data)
    // Reference: bsnes/sfc/ppu-fast/mode7.cpp
    int tile_x = (tx >> 3) & 127;
    int tile_y = (ty >> 3) & 127;
    int fine_x = tx & 7;
    int fine_y = ty & 7;

    // Tile address: tileY * 128 + tileX (word address), *2 for byte address
    uint16_t tile_addr = (tile_y * 128 + tile_x) * 2;
    uint8_t tile_num = m_vram[tile_addr & 0xFFFF];

    // Palette address: tile * 64 + fine_y * 8 + fine_x (word address)
    // Each tile is 64 words (8x8 pixels), fine_y * 8 + fine_x gives offset within tile
    uint16_t palette_addr = ((tile_num << 6) | (fine_y << 3) | fine_x) * 2 + 1;
    uint8_t color_index = m_vram[palette_addr & 0xFFFF];

    if (color_index != 0) {
        pixel = color_index;
        priority = 0;  // Mode 7 BG has no priority bit
    }
}

void PPU::render_sprite_pixel(int x, uint8_t& pixel, uint8_t& priority, bool& is_palette_4_7) {
    pixel = 0;
    priority = 0;
    is_palette_4_7 = false;

    // Search through sprite tiles for this X position
    // Sprites are rendered front-to-back (first match wins)
    for (int i = 0; i < m_sprite_tile_count; i++) {
        const auto& tile = m_sprite_tiles[i];

        // Check if this tile covers the current X position
        if (x >= tile.x && x < tile.x + 8) {
            int fine_x = x - tile.x;
            if (tile.hflip) fine_x = 7 - fine_x;

            // Decode 4bpp pixel from the cached pattern data
            // Each plane is a byte with 8 pixels (MSB = leftmost)
            uint8_t mask = 0x80 >> fine_x;
            uint8_t color_index = 0;

            // Combine the 4 bitplanes into a 4-bit color index
            if (tile.planes[0] & mask) color_index |= 0x01;
            if (tile.planes[1] & mask) color_index |= 0x02;
            if (tile.planes[2] & mask) color_index |= 0x04;
            if (tile.planes[3] & mask) color_index |= 0x08;

            // Color index 0 is transparent for sprites
            if (color_index != 0) {
                // Sprite colors use CGRAM 128-255 (second half of palette)
                // 8 palettes of 16 colors each
                pixel = 128 + tile.palette * 16 + color_index;
                priority = tile.priority;
                is_palette_4_7 = tile.palette >= 4;
                return;
            }
        }
    }
}

void PPU::evaluate_sprites() {
    m_sprite_count = 0;
    m_sprite_tile_count = 0;
    m_time_over = false;
    m_range_over = false;

    int screen_y = m_scanline - 1;

    // Get sprite sizes
    int size_index = (m_obsel >> 5) & 0x07;
    int small_width = SPRITE_SIZES[size_index][0][0];
    int small_height = SPRITE_SIZES[size_index][0][1];
    int large_width = SPRITE_SIZES[size_index][1][0];
    int large_height = SPRITE_SIZES[size_index][1][1];

    // Scan all 128 sprites
    for (int i = 0; i < 128 && m_sprite_count < 32; i++) {
        // Read OAM entry
        int oam_addr = i * 4;
        int x = m_oam[oam_addr];
        int y = m_oam[oam_addr + 1];
        int tile = m_oam[oam_addr + 2];
        int attr = m_oam[oam_addr + 3];

        // Read high byte
        int high_byte_index = 512 + (i / 4);
        int high_byte_shift = (i % 4) * 2;
        int high_bits = (m_oam[high_byte_index] >> high_byte_shift) & 0x03;

        // X sign bit
        if (high_bits & 0x01) {
            x = x - 256;
        }

        // Size select
        bool large = (high_bits & 0x02) != 0;
        int width = large ? large_width : small_width;
        int height = large ? large_height : small_height;

        // Check Y range
        int sprite_y = y;
        int offset_y = screen_y - sprite_y;
        if (offset_y < 0) offset_y += 256;

        if (offset_y >= height) continue;

        // Sprite is on this scanline
        SpriteEntry entry;
        entry.x = x;
        entry.y = sprite_y;
        entry.tile = tile | ((attr & 0x01) << 8);
        entry.palette = (attr >> 1) & 0x07;
        entry.priority = (attr >> 4) & 0x03;
        entry.hflip = (attr & 0x40) != 0;
        entry.vflip = (attr & 0x80) != 0;
        entry.large = large;
        entry.width = width;
        entry.height = height;

        m_sprite_buffer[m_sprite_count++] = entry;
    }

    if (m_sprite_count > 32) {
        m_sprite_count = 32;
        m_range_over = true;
    }

    // Generate sprite tiles for this scanline
    // Sprites are 4bpp (16 colors) using second half of CGRAM (palettes 0-7 = colors 128-255)
    int offset_y = screen_y;
    uint16_t base_addr = m_obj_base_addr;      // Base address from OBSEL bits 0-2
    uint16_t name_base = m_obj_name_select;    // Name select offset from OBSEL bits 3-4

    // Process sprites in reverse order (lowest priority first, so higher priority overwrites)
    for (int i = m_sprite_count - 1; i >= 0 && m_sprite_tile_count < 34; i--) {
        const auto& sprite = m_sprite_buffer[i];

        // Calculate Y offset within the sprite
        int sprite_offset_y = offset_y - sprite.y;
        if (sprite_offset_y < 0) sprite_offset_y += 256;
        if (sprite.vflip) sprite_offset_y = sprite.height - 1 - sprite_offset_y;

        int tile_row = sprite_offset_y / 8;
        int fine_y = sprite_offset_y % 8;

        int tiles_wide = sprite.width / 8;
        for (int tx = 0; tx < tiles_wide && m_sprite_tile_count < 34; tx++) {
            // For horizontal flip, reverse the tile order
            int tile_x = sprite.hflip ? (tiles_wide - 1 - tx) : tx;
            int screen_x = sprite.x + tx * 8;

            // Skip off-screen tiles
            if (screen_x >= 256 || screen_x <= -8) continue;

            // Calculate tile number within the sprite
            // Sprite tiles are arranged in rows of 16 tiles
            int tile_num = sprite.tile;
            tile_num += tile_x;
            tile_num += tile_row * 16;

            // Calculate character data address
            // Bit 8 of tile number selects between base_addr and name_base
            // OBSEL name select: base_addr + (name_select << 14) for tiles >= 256
            uint16_t chr_addr;
            if (tile_num & 0x100) {
                // Second page: use name_base offset
                chr_addr = base_addr + name_base + ((tile_num & 0xFF) * 32);
            } else {
                // First page: use base_addr directly
                chr_addr = base_addr + (tile_num * 32);
            }

            // Read 4bpp tile data for this row
            // Sprite tiles use same format as 4bpp BG tiles:
            // Planes 0,1 in first 16 bytes, planes 2,3 in second 16 bytes
            uint16_t row_addr = chr_addr + fine_y * 2;

            SpriteTile tile_entry;
            tile_entry.x = screen_x;
            tile_entry.planes[0] = m_vram[row_addr & 0xFFFF];           // Plane 0
            tile_entry.planes[1] = m_vram[(row_addr + 1) & 0xFFFF];     // Plane 1
            tile_entry.planes[2] = m_vram[(row_addr + 16) & 0xFFFF];    // Plane 2
            tile_entry.planes[3] = m_vram[(row_addr + 17) & 0xFFFF];    // Plane 3
            tile_entry.palette = sprite.palette;
            tile_entry.priority = sprite.priority;
            tile_entry.hflip = sprite.hflip;

            m_sprite_tiles[m_sprite_tile_count++] = tile_entry;
        }
    }

    // Set time over flag if we exceeded 34 tiles
    if (m_sprite_tile_count > 34) {
        m_sprite_tile_count = 34;
        m_time_over = true;
    }
}

uint16_t PPU::get_color(uint8_t palette, uint8_t index, bool sprite) {
    (void)palette;  // Palette offset is already baked into index by callers

    if (index == 0 && !sprite) {
        // Transparent - use backdrop
        return m_cgram[0] | (m_cgram[1] << 8);
    }

    // For both BG and sprites, the index already contains the full color offset:
    // - BG: palette * colors_per_palette + color_index (e.g., 0-127)
    // - Sprites: 128 + palette * 16 + color_index (e.g., 128-255)
    // So we just convert color index to byte address (each color is 2 bytes)
    uint16_t addr = index * 2;

    uint16_t color = m_cgram[addr & 0x1FF] | (m_cgram[(addr + 1) & 0x1FF] << 8);

    // Debug: Log color lookups for frame 300, scanline 112, first few calls
    static int color_debug_count = 0;
    if (is_debug_mode() && m_frame == 300 && (m_scanline - 1) == 112 && color_debug_count < 20) {
        SNES_PPU_DEBUG("get_color: index=%d sprite=%d -> CGRAM[$%03X]=$%04X (bytes: %02X %02X)\n",
            index, sprite ? 1 : 0, addr, color,
            m_cgram[addr & 0x1FF], m_cgram[(addr + 1) & 0x1FF]);
        color_debug_count++;
    }

    return color;
}

uint16_t PPU::remap_vram_address(uint16_t addr) const {
    // VRAM address remapping based on VMAIN ($2115) bits 2-3
    // This is used for efficient DMA transfers of tile data
    //
    // The remapping reorders bits within the address to allow linear
    // DMA to write data in the correct interleaved format for tiles.
    //
    // Mode 0: No remapping
    // Mode 1: 8-bit  rotation: aaaaaaaabbbccccc -> aaaaaaaacccccbbb (for 8x8 tiles)
    // Mode 2: 9-bit  rotation: aaaaaaabbbcccccc -> aaaaaaaccccccbbb (for 16x8 tiles)
    // Mode 3: 10-bit rotation: aaaaaabbbccccccc -> aaaaaacccccccbbb (for 32x8 tiles)
    //
    // Reference: bsnes/sfc/ppu/io.cpp, fullsnes VMAIN documentation
    switch (m_vram_remap_mode) {
        case 0:
            // No remapping
            return addr;

        case 1:
            // 8-bit rotation: remap bits 0-7
            // Original: aaaaaaaabbbccccc (a=bits 8-15, b=bits 5-7, c=bits 0-4)
            // Remapped: aaaaaaaacccccbbb
            return (addr & 0xFF00) | ((addr & 0x001F) << 3) | ((addr & 0x00E0) >> 5);

        case 2:
            // 9-bit rotation: remap bits 0-8
            // Original: aaaaaaabbbcccccc (a=bits 9-15, b=bits 6-8, c=bits 0-5)
            // Remapped: aaaaaaaccccccbbb
            return (addr & 0xFE00) | ((addr & 0x003F) << 3) | ((addr & 0x01C0) >> 6);

        case 3:
            // 10-bit rotation: remap bits 0-9
            // Original: aaaaaabbbccccccc (a=bits 10-15, b=bits 7-9, c=bits 0-6)
            // Remapped: aaaaaacccccccbbb
            return (addr & 0xFC00) | ((addr & 0x007F) << 3) | ((addr & 0x0380) >> 7);

        default:
            return addr;
    }
}

bool PPU::check_frame_complete() {
    bool complete = m_frame_complete;
    m_frame_complete = false;
    return complete;
}

bool PPU::check_nmi() {
    bool pending = m_nmi_pending;
    m_nmi_pending = false;
    return pending;
}

uint8_t PPU::read(uint16_t address) {
    uint8_t value = 0;

    switch (address) {
        case 0x2134:  // MPYL - Multiplication result (low)
            m_mpy_result = static_cast<int16_t>(m_m7a) * static_cast<int8_t>(m_m7b >> 8);
            value = m_mpy_result & 0xFF;
            break;
        case 0x2135:  // MPYM - Multiplication result (middle)
            value = (m_mpy_result >> 8) & 0xFF;
            break;
        case 0x2136:  // MPYH - Multiplication result (high)
            value = (m_mpy_result >> 16) & 0xFF;
            break;

        case 0x2137:  // SLHV - Software latch for H/V counters
            m_hv_latch = true;
            m_hcount_second = false;
            m_vcount_second = false;
            break;

        case 0x2138:  // OAMDATAREAD
            value = m_oam[m_oam_addr & 0x3FF];
            m_oam_addr = (m_oam_addr + 1) & 0x3FF;
            m_ppu1_open_bus = value;
            break;

        case 0x2139:  // VMDATALREAD
            value = m_vram_read_buffer & 0xFF;
            if (!m_vram_increment_high) {
                uint16_t addr = remap_vram_address(m_vram_addr);
                m_vram_read_buffer = m_vram[(addr * 2) & 0xFFFF] |
                                     (m_vram[(addr * 2 + 1) & 0xFFFF] << 8);
                m_vram_addr += m_vram_increment;
            }
            m_ppu1_open_bus = value;
            break;

        case 0x213A:  // VMDATAHREAD
            value = (m_vram_read_buffer >> 8) & 0xFF;
            if (m_vram_increment_high) {
                uint16_t addr = remap_vram_address(m_vram_addr);
                m_vram_read_buffer = m_vram[(addr * 2) & 0xFFFF] |
                                     (m_vram[(addr * 2 + 1) & 0xFFFF] << 8);
                m_vram_addr += m_vram_increment;
            }
            m_ppu1_open_bus = value;
            break;

        case 0x213B:  // CGDATAREAD
            if (!m_cgram_high_byte) {
                value = m_cgram[m_cgram_addr * 2];
            } else {
                value = m_cgram[m_cgram_addr * 2 + 1] & 0x7F;
                m_cgram_addr = (m_cgram_addr + 1) & 0xFF;
            }
            m_cgram_high_byte = !m_cgram_high_byte;
            m_ppu2_open_bus = value;
            break;

        case 0x213C:  // OPHCT - Horizontal counter
            if (!m_hcount_second) {
                value = m_hcount & 0xFF;
            } else {
                value = (m_hcount >> 8) & 0x01;
            }
            m_hcount_second = !m_hcount_second;
            m_ppu2_open_bus = value;
            break;

        case 0x213D:  // OPVCT - Vertical counter
            if (!m_vcount_second) {
                value = m_vcount & 0xFF;
            } else {
                value = (m_vcount >> 8) & 0x01;
            }
            m_vcount_second = !m_vcount_second;
            m_ppu2_open_bus = value;
            break;

        case 0x213E:  // STAT77 - PPU1 status
            value = (m_ppu1_open_bus & 0x10) |
                    (m_time_over ? 0x80 : 0) |
                    (m_range_over ? 0x40 : 0) |
                    0x01;  // PPU1 version
            m_ppu1_open_bus = value;
            break;

        case 0x213F:  // STAT78 - PPU2 status
            value = (m_ppu2_open_bus & 0x20) |
                    (m_hv_latch ? 0x40 : 0) |
                    (m_interlace && (m_frame & 1) ? 0x80 : 0) |
                    0x03;  // PPU2 version
            m_hv_latch = false;
            m_ppu2_open_bus = value;
            break;

        default:
            value = m_ppu1_open_bus;
            break;
    }

    return value;
}

void PPU::write(uint16_t address, uint8_t value) {
    // Debug key PPU registers
    if (address == 0x2100 || address == 0x2105 || address == 0x212C || address == 0x212D) {
        SNES_PPU_DEBUG("Write $%04X = $%02X (INIDISP=%02X force_blank=%d bright=%d mode=%d TM=%02X)\n",
            address, value, m_inidisp, m_force_blank ? 1 : 0, m_brightness, m_bg_mode, m_tm);
    }

    switch (address) {
        case 0x2100: {  // INIDISP
            bool was_blank = m_force_blank;
            m_inidisp = value;
            m_force_blank = (value & 0x80) != 0;
            m_brightness = value & 0x0F;
            if (is_debug_mode() && was_blank && !m_force_blank) {
                fprintf(stderr, "[PPU] FORCE BLANK DISABLED at frame %llu, scanline %d (brightness=%d)\n",
                    (unsigned long long)m_frame, m_scanline, m_brightness);
            }
            break;
        }

        case 0x2101:  // OBSEL
            m_obsel = value;
            // Object base addresses are word addresses, convert to byte addresses
            // Bits 0-2: Base address in 8KB word units (16KB byte units)
            m_obj_base_addr = (value & 0x07) << 14;  // Byte address
            // Bits 3-4: Name select in 8KB word units, added to base
            m_obj_name_select = ((value >> 3) & 0x03) << 14;  // Byte address offset
            break;

        case 0x2102:  // OAMADDL
            m_oam_addr_reload = (m_oam_addr_reload & 0x100) | value;
            m_oam_addr = m_oam_addr_reload << 1;
            m_oam_high_byte = false;
            break;

        case 0x2103:  // OAMADDH
            m_oam_addr_reload = (m_oam_addr_reload & 0xFF) | ((value & 0x01) << 8);
            m_oam_addr = m_oam_addr_reload << 1;
            m_oam_high_byte = false;
            break;

        case 0x2104:  // OAMDATA
            if (m_oam_addr < 512) {
                if (!m_oam_high_byte) {
                    m_oam_latch = value;
                } else {
                    m_oam[m_oam_addr - 1] = m_oam_latch;
                    m_oam[m_oam_addr] = value;
                }
            } else {
                m_oam[m_oam_addr & 0x21F] = value;
            }
            m_oam_high_byte = !m_oam_high_byte;
            if (m_oam_high_byte == false) {
                m_oam_addr = (m_oam_addr + 2) & 0x3FF;
            }
            break;

        case 0x2105:  // BGMODE
            m_bgmode = value;
            m_bg_mode = value & 0x07;
            m_bg3_priority = (value & 0x08) != 0;
            for (int i = 0; i < 4; i++) {
                m_bg_tile_size[i] = (value & (0x10 << i)) != 0;
            }
            break;

        case 0x2106:  // MOSAIC
            m_mosaic = value;
            m_mosaic_size = ((value >> 4) & 0x0F) + 1;
            for (int i = 0; i < 4; i++) {
                m_mosaic_enabled[i] = (value & (1 << i)) != 0;
            }
            break;

        case 0x2107:  // BG1SC
        case 0x2108:  // BG2SC
        case 0x2109:  // BG3SC
        case 0x210A: {  // BG4SC
            int bg = address - 0x2107;
            // BGnSC bits 2-7 specify word address bits 10-15
            // Convert to byte address by shifting left 9 instead of 8 (word addr << 1)
            // Register format: aaaaaass, where a = address bits 10-15 of word address
            // Word address = (value & 0xFC) << 8, byte address = word_addr * 2
            m_bg_tilemap_addr[bg] = (value & 0xFC) << 9;  // Byte address in VRAM
            m_bg_tilemap_width[bg] = (value & 0x01) ? 1 : 0;
            m_bg_tilemap_height[bg] = (value & 0x02) ? 1 : 0;
            SNES_PPU_DEBUG("BG%dSC=$%02X -> tilemap=$%04X size=%dx%d\n",
                bg + 1, value, m_bg_tilemap_addr[bg],
                m_bg_tilemap_width[bg] ? 64 : 32, m_bg_tilemap_height[bg] ? 64 : 32);
            break;
        }

        case 0x210B:  // BG12NBA
            // Character base is stored as word address in register, convert to byte address
            // Value bits 0-3 give word address bits 12-15, so shift left 12 then multiply by 2 (shift 13)
            m_bg_chr_addr[0] = (value & 0x0F) << 13;  // Byte address
            m_bg_chr_addr[1] = (value & 0xF0) << 9;   // Byte address
            SNES_PPU_DEBUG("BG12NBA=$%02X -> BG1 chr=$%04X, BG2 chr=$%04X\n",
                value, m_bg_chr_addr[0], m_bg_chr_addr[1]);
            break;

        case 0x210C:  // BG34NBA
            m_bg_chr_addr[2] = (value & 0x0F) << 13;  // Byte address
            m_bg_chr_addr[3] = (value & 0xF0) << 9;   // Byte address
            break;

        case 0x210D:  // BG1HOFS / M7HOFS
            // BG scroll registers use a quirky dual-latch mechanism (PPU1/PPU2 behavior):
            // HOFS = (data << 8) | (latch_ppu1 & ~7) | (latch_ppu2 & 7)
            // This preserves fine scroll bits from latch_ppu2 and coarse bits from latch_ppu1
            // Reference: bsnes/sfc/ppu/io.cpp
            m_bg_hofs[0] = (value << 8) | (m_bgofs_latch_ppu1 & ~7) | (m_bgofs_latch_ppu2 & 7);
            m_bgofs_latch_ppu1 = value;
            m_bgofs_latch_ppu2 = value;
            // Mode 7 uses 13-bit signed values with separate latch
            m_m7hofs = ((value << 8) | m_m7_latch) & 0x1FFF;
            if (m_m7hofs & 0x1000) m_m7hofs |= ~0x1FFF;  // Sign extend
            m_m7_latch = value;
            break;

        case 0x210E:  // BG1VOFS / M7VOFS
            // VOFS = (data << 8) | latch_ppu1
            m_bg_vofs[0] = (value << 8) | m_bgofs_latch_ppu1;
            m_bgofs_latch_ppu1 = value;
            // Mode 7 uses 13-bit signed values
            m_m7vofs = ((value << 8) | m_m7_latch) & 0x1FFF;
            if (m_m7vofs & 0x1000) m_m7vofs |= ~0x1FFF;  // Sign extend
            m_m7_latch = value;
            break;

        case 0x210F:  // BG2HOFS
            m_bg_hofs[1] = (value << 8) | (m_bgofs_latch_ppu1 & ~7) | (m_bgofs_latch_ppu2 & 7);
            m_bgofs_latch_ppu1 = value;
            m_bgofs_latch_ppu2 = value;
            break;

        case 0x2110:  // BG2VOFS
            m_bg_vofs[1] = (value << 8) | m_bgofs_latch_ppu1;
            m_bgofs_latch_ppu1 = value;
            break;

        case 0x2111:  // BG3HOFS
            m_bg_hofs[2] = (value << 8) | (m_bgofs_latch_ppu1 & ~7) | (m_bgofs_latch_ppu2 & 7);
            m_bgofs_latch_ppu1 = value;
            m_bgofs_latch_ppu2 = value;
            break;

        case 0x2112:  // BG3VOFS
            m_bg_vofs[2] = (value << 8) | m_bgofs_latch_ppu1;
            m_bgofs_latch_ppu1 = value;
            break;

        case 0x2113:  // BG4HOFS
            m_bg_hofs[3] = (value << 8) | (m_bgofs_latch_ppu1 & ~7) | (m_bgofs_latch_ppu2 & 7);
            m_bgofs_latch_ppu1 = value;
            m_bgofs_latch_ppu2 = value;
            break;

        case 0x2114:  // BG4VOFS
            m_bg_vofs[3] = (value << 8) | m_bgofs_latch_ppu1;
            m_bgofs_latch_ppu1 = value;
            break;

        case 0x2115:  // VMAIN
            m_vmain = value;
            m_vram_increment_high = (value & 0x80) != 0;
            switch (value & 0x03) {
                case 0: m_vram_increment = 1; break;
                case 1: m_vram_increment = 32; break;
                case 2:
                case 3: m_vram_increment = 128; break;
            }
            m_vram_remap_mode = (value >> 2) & 0x03;
            break;

        case 0x2116:  // VMADDL
            m_vram_addr = (m_vram_addr & 0xFF00) | value;
            {
                uint16_t addr = remap_vram_address(m_vram_addr);
                m_vram_read_buffer = m_vram[(addr * 2) & 0xFFFF] |
                                     (m_vram[(addr * 2 + 1) & 0xFFFF] << 8);
            }
            break;

        case 0x2117:  // VMADDH
            m_vram_addr = (m_vram_addr & 0x00FF) | (value << 8);
            {
                // Debug: trace VMADD writes to high addresses (BG2 chr area)
                static int vmadd_trace_count = 0;
                if (is_debug_mode() && vmadd_trace_count < 30) {
                    fprintf(stderr, "[PPU] VMADD set to $%04X (byte_addr=$%04X)\n",
                        m_vram_addr, m_vram_addr * 2);
                    vmadd_trace_count++;
                }
                uint16_t addr = remap_vram_address(m_vram_addr);
                m_vram_read_buffer = m_vram[(addr * 2) & 0xFFFF] |
                                     (m_vram[(addr * 2 + 1) & 0xFFFF] << 8);
            }
            break;

        case 0x2118:  // VMDATAL
            {
                // Apply VRAM address remapping based on VMAIN bits 2-3
                // This remapping is used for efficient tile data DMA
                // Reference: fullsnes, bsnes/snes9x VMAIN documentation
                uint16_t addr = remap_vram_address(m_vram_addr);
                uint32_t byte_addr = (addr * 2) & 0xFFFF;
                m_vram[byte_addr] = value;
                // Debug: trace VRAM writes to BG2 chr area or first 50 writes
                static int vram_write_count = 0;
                bool is_bg2_chr = (byte_addr >= 0xA000 && byte_addr < 0xC000);
                if (is_debug_mode() && (is_bg2_chr || vram_write_count < 50) && value != 0) {
                    fprintf(stderr, "[PPU] VRAM write: word_addr=$%04X -> byte_addr=$%04X val=$%02X%s\n",
                        m_vram_addr, byte_addr, value, is_bg2_chr ? " (BG2 CHR)" : "");
                    if (!is_bg2_chr) vram_write_count++;
                }
                if (!m_vram_increment_high) {
                    m_vram_addr += m_vram_increment;
                }
            }
            break;

        case 0x2119:  // VMDATAH
            {
                uint16_t addr = remap_vram_address(m_vram_addr);
                uint32_t byte_addr = (addr * 2 + 1) & 0xFFFF;
                m_vram[byte_addr] = value;
                // Debug: trace VRAM high writes to BG2 chr area with remap info
                static int vram_h_write_count = 0;
                bool is_bg2_chr = (byte_addr >= 0xA000 && byte_addr < 0xC000);
                if (is_debug_mode() && is_bg2_chr && value != 0) {
                    fprintf(stderr, "[PPU] VRAM writeH: raw=$%04X remap=$%04X byte=$%04X val=$%02X remap_mode=%d\n",
                        m_vram_addr, addr, byte_addr, value, m_vram_remap_mode);
                } else if (is_debug_mode() && value != 0 && vram_h_write_count < 10) {
                    vram_h_write_count++;
                }
                if (m_vram_increment_high) {
                    m_vram_addr += m_vram_increment;
                }
            }
            break;

        case 0x211A:  // M7SEL
            m_m7sel = value;
            m_m7_hflip = (value & 0x01) != 0;
            m_m7_vflip = (value & 0x02) != 0;
            m_m7_wrap = (value >> 6) & 0x03;
            break;

        case 0x211B:  // M7A
            m_m7a = (value << 8) | m_m7_latch;
            m_m7_latch = value;
            break;

        case 0x211C:  // M7B
            m_m7b = (value << 8) | m_m7_latch;
            m_m7_latch = value;
            break;

        case 0x211D:  // M7C
            m_m7c = (value << 8) | m_m7_latch;
            m_m7_latch = value;
            break;

        case 0x211E:  // M7D
            m_m7d = (value << 8) | m_m7_latch;
            m_m7_latch = value;
            break;

        case 0x211F:  // M7X
            m_m7x = (value << 8) | m_m7_latch;
            m_m7_latch = value;
            break;

        case 0x2120:  // M7Y
            m_m7y = (value << 8) | m_m7_latch;
            m_m7_latch = value;
            break;

        case 0x2121:  // CGADD
            m_cgram_addr = value;
            m_cgram_high_byte = false;
            break;

        case 0x2122:  // CGDATA - Palette data write
            // CGRAM writes use a double-byte buffer
            // First write: store low byte in latch
            // Second write: combine with latch and write 15-bit color to CGRAM
            {
                static int cgram_write_trace = 0;
                static int cgram_write_after_275 = 0;
                // Count total writes after frame 275
                if (m_frame >= 275) {
                    cgram_write_after_275++;
                    // Report every 512 writes (one full palette transfer)
                    if (is_debug_mode() && (cgram_write_after_275 <= 20 || cgram_write_after_275 % 512 == 0)) {
                        fprintf(stderr, "[PPU-CGRAM-275+] Write $2122=$%02X high=%d addr=%d frame=%llu (count after f275: %d)\n",
                            value, m_cgram_high_byte ? 1 : 0, m_cgram_addr, (unsigned long long)m_frame, cgram_write_after_275);
                    }
                }
                // Original trace for early frames
                bool should_trace = (cgram_write_trace < 20);
                if (is_debug_mode() && should_trace) {
                    fprintf(stderr, "[PPU-CGRAM] Write $2122=$%02X high=%d addr=%d frame=%llu (total trace: %d)\n",
                        value, m_cgram_high_byte ? 1 : 0, m_cgram_addr, (unsigned long long)m_frame, cgram_write_trace);
                    cgram_write_trace++;
                }
            }
            if (!m_cgram_high_byte) {
                m_cgram_latch = value;
            } else {
                // CGRAM stores 15-bit BGR colors (5:5:5 format)
                // WATCHPOINT: Track writes to CGRAM[0]
                if (is_debug_mode() && m_cgram_addr == 0 && m_frame >= 106) {
                    uint16_t old_color = m_cgram[0] | (m_cgram[1] << 8);
                    uint16_t new_color = m_cgram_latch | ((value & 0x7F) << 8);
                    fprintf(stderr, "[PPU-CGRAM-WRITE0] Writing CGRAM[0]: $%04X -> $%04X at frame=%llu\n",
                        old_color, new_color, (unsigned long long)m_frame);
                }
                m_cgram[m_cgram_addr * 2] = m_cgram_latch;
                m_cgram[m_cgram_addr * 2 + 1] = value & 0x7F;  // Bit 7 ignored
                // Debug: count total CGRAM writes
                static int cgram_write_total = 0;
                cgram_write_total++;
                // Trace first 10, 256, and verify array address
                if (is_debug_mode() && (cgram_write_total <= 10 || cgram_write_total == 256)) {
                    uint16_t color = m_cgram_latch | ((value & 0x7F) << 8);
                    fprintf(stderr, "[PPU-CGRAM] Wrote color %d: $%04X (total: %d) frame=%llu @%p\n",
                        m_cgram_addr, color, cgram_write_total, (unsigned long long)m_frame, (void*)m_cgram.data());
                }
                // Also verify CGRAM[0] after write 256
                if (is_debug_mode() && cgram_write_total == 256) {
                    fprintf(stderr, "[PPU-CGRAM] After 256 writes: CGRAM[0]=$%02X CGRAM[1]=$%02X frame=%llu\n",
                        m_cgram[0], m_cgram[1], (unsigned long long)m_frame);
                }
                m_cgram_addr = (m_cgram_addr + 1) & 0xFF;
            }
            m_cgram_high_byte = !m_cgram_high_byte;
            break;

        case 0x2123:  // W12SEL
            m_bg_window1_invert[0] = (value & 0x01) != 0;
            m_bg_window1_enable[0] = (value & 0x02) != 0;
            m_bg_window2_invert[0] = (value & 0x04) != 0;
            m_bg_window2_enable[0] = (value & 0x08) != 0;
            m_bg_window1_invert[1] = (value & 0x10) != 0;
            m_bg_window1_enable[1] = (value & 0x20) != 0;
            m_bg_window2_invert[1] = (value & 0x40) != 0;
            m_bg_window2_enable[1] = (value & 0x80) != 0;
            break;

        case 0x2124:  // W34SEL
            m_bg_window1_invert[2] = (value & 0x01) != 0;
            m_bg_window1_enable[2] = (value & 0x02) != 0;
            m_bg_window2_invert[2] = (value & 0x04) != 0;
            m_bg_window2_enable[2] = (value & 0x08) != 0;
            m_bg_window1_invert[3] = (value & 0x10) != 0;
            m_bg_window1_enable[3] = (value & 0x20) != 0;
            m_bg_window2_invert[3] = (value & 0x40) != 0;
            m_bg_window2_enable[3] = (value & 0x80) != 0;
            break;

        case 0x2125:  // WOBJSEL
            m_obj_window1_invert = (value & 0x01) != 0;
            m_obj_window1_enable = (value & 0x02) != 0;
            m_obj_window2_invert = (value & 0x04) != 0;
            m_obj_window2_enable = (value & 0x08) != 0;
            m_color_window1_invert = (value & 0x10) != 0;
            m_color_window1_enable = (value & 0x20) != 0;
            m_color_window2_invert = (value & 0x40) != 0;
            m_color_window2_enable = (value & 0x80) != 0;
            break;

        case 0x2126:  // WH0
            m_window1_left = value;
            break;

        case 0x2127:  // WH1
            m_window1_right = value;
            break;

        case 0x2128:  // WH2
            m_window2_left = value;
            break;

        case 0x2129:  // WH3
            m_window2_right = value;
            break;

        case 0x212A:  // WBGLOG
            m_bg_window_logic[0] = (value >> 0) & 0x03;
            m_bg_window_logic[1] = (value >> 2) & 0x03;
            m_bg_window_logic[2] = (value >> 4) & 0x03;
            m_bg_window_logic[3] = (value >> 6) & 0x03;
            break;

        case 0x212B:  // WOBJLOG
            m_obj_window_logic = (value >> 0) & 0x03;
            m_color_window_logic = (value >> 2) & 0x03;
            break;

        case 0x212C:  // TM
            m_tm = value;
            SNES_PPU_DEBUG("TM=$%02X (BG1:%d BG2:%d BG3:%d BG4:%d OBJ:%d)\n",
                value, (value >> 0) & 1, (value >> 1) & 1, (value >> 2) & 1,
                (value >> 3) & 1, (value >> 4) & 1);
            break;

        case 0x212D:  // TS
            m_ts = value;
            break;

        case 0x212E:  // TMW
            m_tmw = value;
            break;

        case 0x212F:  // TSW
            m_tsw = value;
            break;

        case 0x2130:  // CGWSEL
            m_cgwsel = value;
            m_direct_color = (value & 0x01) != 0;
            m_sub_screen_bg_obj = (value & 0x02) != 0;
            m_color_math_prevent = (value >> 4) & 0x03;
            m_color_math_clip = (value >> 6) & 0x03;
            break;

        case 0x2131:  // CGADSUB
            m_cgadsub = value;
            for (int i = 0; i < 4; i++) {
                m_bg_color_math[i] = (value & (1 << i)) != 0;
            }
            m_obj_color_math = (value & 0x10) != 0;
            m_backdrop_color_math = (value & 0x20) != 0;
            m_color_math_half = (value & 0x40) != 0;
            m_color_math_add = (value & 0x80) == 0;
            break;

        case 0x2132:  // COLDATA
            if (value & 0x20) m_fixed_color_r = value & 0x1F;
            if (value & 0x40) m_fixed_color_g = value & 0x1F;
            if (value & 0x80) m_fixed_color_b = value & 0x1F;
            break;

        case 0x2133:  // SETINI
            m_setini = value;
            m_interlace = (value & 0x01) != 0;
            m_obj_interlace = (value & 0x02) != 0;
            m_overscan = (value & 0x04) != 0;
            m_pseudo_hires = (value & 0x08) != 0;
            m_external_sync = (value & 0x80) != 0;
            break;
    }
}

void PPU::oam_write(uint16_t address, uint8_t value) {
    m_oam[address & 0x21F] = value;
}

uint8_t PPU::oam_read(uint16_t address) {
    return m_oam[address & 0x21F];
}

void PPU::cgram_write(uint8_t value) {
    // Debug: trace DMA CGRAM writes
    static int dma_cgram_trace = 0;
    if (is_debug_mode() && dma_cgram_trace < 20) {
        fprintf(stderr, "[PPU-CGRAM-DMA] cgram_write($%02X) high=%d addr=%d\n",
            value, m_cgram_high_byte ? 1 : 0, m_cgram_addr);
        dma_cgram_trace++;
    }
    if (!m_cgram_high_byte) {
        m_cgram_latch = value;
    } else {
        m_cgram[m_cgram_addr * 2] = m_cgram_latch;
        m_cgram[m_cgram_addr * 2 + 1] = value & 0x7F;
        m_cgram_addr = (m_cgram_addr + 1) & 0xFF;
    }
    m_cgram_high_byte = !m_cgram_high_byte;
}

uint8_t PPU::cgram_read() {
    uint8_t value;
    if (!m_cgram_high_byte) {
        value = m_cgram[m_cgram_addr * 2];
    } else {
        value = m_cgram[m_cgram_addr * 2 + 1];
        m_cgram_addr = (m_cgram_addr + 1) & 0xFF;
    }
    m_cgram_high_byte = !m_cgram_high_byte;
    return value;
}

void PPU::vram_write(uint16_t address, uint8_t value, bool high_byte) {
    uint16_t addr = (address * 2 + (high_byte ? 1 : 0)) & 0xFFFF;
    m_vram[addr] = value;
}

uint8_t PPU::vram_read(uint16_t address, bool high_byte) {
    uint16_t addr = (address * 2 + (high_byte ? 1 : 0)) & 0xFFFF;
    return m_vram[addr];
}

void PPU::save_state(std::vector<uint8_t>& data) {
    // Save timing
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&m_scanline),
                reinterpret_cast<uint8_t*>(&m_scanline) + sizeof(m_scanline));
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&m_dot),
                reinterpret_cast<uint8_t*>(&m_dot) + sizeof(m_dot));
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&m_frame),
                reinterpret_cast<uint8_t*>(&m_frame) + sizeof(m_frame));

    // Save VRAM, OAM, CGRAM
    data.insert(data.end(), m_vram.begin(), m_vram.end());
    data.insert(data.end(), m_oam.begin(), m_oam.end());
    data.insert(data.end(), m_cgram.begin(), m_cgram.end());

    // Save key registers (simplified)
    data.push_back(m_inidisp);
    data.push_back(m_obsel);
    data.push_back(m_bgmode);
    data.push_back(m_tm);
    data.push_back(m_ts);
    data.push_back(m_nmi_enabled ? 1 : 0);
    data.push_back(m_nmi_flag ? 1 : 0);
}

void PPU::load_state(const uint8_t*& data, size_t& remaining) {
    // Load timing
    std::memcpy(&m_scanline, data, sizeof(m_scanline));
    data += sizeof(m_scanline); remaining -= sizeof(m_scanline);
    std::memcpy(&m_dot, data, sizeof(m_dot));
    data += sizeof(m_dot); remaining -= sizeof(m_dot);
    std::memcpy(&m_frame, data, sizeof(m_frame));
    data += sizeof(m_frame); remaining -= sizeof(m_frame);

    // Load VRAM, OAM, CGRAM
    std::memcpy(m_vram.data(), data, m_vram.size());
    data += m_vram.size(); remaining -= m_vram.size();
    std::memcpy(m_oam.data(), data, m_oam.size());
    data += m_oam.size(); remaining -= m_oam.size();
    if (is_debug_mode()) {
        fprintf(stderr, "[PPU] load_state: loading CGRAM, data[0]=$%02X data[1]=$%02X\n",
            data[0], data[1]);
    }
    std::memcpy(m_cgram.data(), data, m_cgram.size());
    data += m_cgram.size(); remaining -= m_cgram.size();
    if (is_debug_mode()) {
        fprintf(stderr, "[PPU] load_state: after CGRAM load, [0]=$%02X [1]=$%02X\n",
            m_cgram[0], m_cgram[1]);
    }

    // Load key registers
    m_inidisp = *data++; remaining--;
    m_obsel = *data++; remaining--;
    m_bgmode = *data++; remaining--;
    m_tm = *data++; remaining--;
    m_ts = *data++; remaining--;
    m_nmi_enabled = (*data++ != 0); remaining--;
    m_nmi_flag = (*data++ != 0); remaining--;

    // Recalculate derived values
    m_force_blank = (m_inidisp & 0x80) != 0;
    m_brightness = m_inidisp & 0x0F;
    m_bg_mode = m_bgmode & 0x07;
}

} // namespace snes
