#include "ppu.hpp"
#include "bus.hpp"
#include "debug.hpp"
#include <cstring>
#include <algorithm>
#include <string>

namespace snes {

PPU::PPU(Bus& bus) : m_bus(bus) {
    reset();
}

PPU::~PPU() = default;

void PPU::reset() {
    m_scanline = 0;
    m_dot = 0;
    m_frame = 0;
    m_frame_complete = false;

    // Reset catch-up rendering state
    m_rendered_scanline = 0;
    m_rendered_dot = 0;
    m_sprites_for_scanline = -1;  // No sprites evaluated yet
    m_force_blank_latched_eval = true;   // Latched at dot 270 for sprite evaluation
    m_force_blank_latched_fetch = true;  // Latched at dot 272 for sprite tile fetch
    m_force_blank_on_cycle = 0;
    m_total_ppu_cycles = 0;
    m_dot_accumulator = 0;

    m_framebuffer.fill(0);
    m_vram.fill(0);
    // OAM should initialize to $FF, not $00. On SNES hardware, this places all
    // sprites offscreen (Y=$FF). Initializing to $00 causes sprites at Y=0 to
    // appear on every scanline 0-7, blocking actual sprites.
    m_oam.fill(0xFF);
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
    m_extbg = false;
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

// ============================================================================
// CATCH-UP RENDERING IMPLEMENTATION
// ============================================================================
// Reference: Mesen-S ppu.cpp, bsnes/higan ppu timing
//
// The SNES PPU renders 340 dots per scanline:
// - Dots 0-21: HBlank (end of previous scanline's HBlank technically)
// - Dots 22-277: Visible pixels (256 pixels)
// - Dots 278-339: HBlank
//
// Key timing events:
// - Dot 22: First visible pixel
// - Dot 277: Last visible pixel
// - Dot 278: HBlank begins
// - Dot 285: Sprite evaluation for NEXT scanline (force_blank checked here)
// - Dot 339: End of scanline
//
// This implementation uses a "catch-up" approach where rendering is deferred
// until either:
// 1. The CPU advances time (via advance())
// 2. A PPU register is written (via sync_to_current())
//
// This allows mid-scanline effects to work correctly while maintaining
// reasonable performance by batching pixel rendering.
// ============================================================================

void PPU::set_timing(int scanline, int dot) {
    // At frame start (scanline 0, dot 0), initialize state
    if (scanline == 0 && dot == 0) {
        m_force_blank_latched_eval = m_force_blank;
        m_force_blank_latched_fetch = m_force_blank;
        m_sprites_for_scanline = -1;  // No sprites evaluated yet for this frame
        m_rendered_scanline = 0;
        m_rendered_dot = 0;
    }

    m_scanline = scanline;
    m_dot = dot;
}

void PPU::prepare_scanline_sprites(int scanline) {
    // Evaluate sprites for this scanline using current register state
    // This is called at the START of each visible scanline, before CPU runs,
    // matching the timing of the old render_scanline approach.
    //
    // NOTE: With catch-up rendering, this function is NOT called. Instead,
    // sprite evaluation happens at dot 285 via advance(). This function
    // is kept for backward compatibility with old rendering mode.
    //
    // The 'scanline' parameter is 1-based (1 = first visible scanline = screen line 0)

    if (m_force_blank) {
        m_sprite_count = 0;
        m_sprite_tile_count = 0;
    } else {
        int saved = m_scanline;
        m_scanline = scanline;  // Set for evaluate_sprites
        evaluate_sprites();
        m_scanline = saved;
    }

    // Mark sprites as evaluated for this scanline
    m_sprites_for_scanline = scanline;
}

bool PPU::is_rendering() const {
    int visible_lines = m_overscan ? 239 : 224;
    // We're "rendering" if we're on a visible scanline
    // Scanlines are 1-indexed for visible area (scanline 1 = screen line 0)
    return (m_scanline >= 1 && m_scanline <= visible_lines);
}

void PPU::advance(int master_cycles) {
    // Track total cycles for timing-based force_blank detection
    m_total_ppu_cycles += master_cycles;

    // Convert master cycles to dots (4 master cycles per dot)
    // We accumulate partial dots and render when we cross dot boundaries
    m_dot_accumulator += master_cycles;

    int dots_to_advance = m_dot_accumulator / 4;
    m_dot_accumulator %= 4;

    if (dots_to_advance == 0) {
        return;
    }

    // Advance dot-by-dot, checking for timing events
    while (dots_to_advance > 0) {
        // Calculate target dot position
        int target_dot = m_dot + 1;
        int target_scanline = m_scanline;

        if (target_dot >= DOTS_PER_SCANLINE) {
            target_dot = 0;
            target_scanline++;

            // Handle frame wrap
            if (target_scanline >= SCANLINES_PER_FRAME) {
                target_scanline = 0;
            }
        }

        // Render any pixels between m_rendered_dot and m_dot before advancing
        sync_to_current();

        // Now advance the PPU clock
        m_dot = target_dot;
        m_scanline = target_scanline;

        // Check for sprite timing events
        int visible_lines = m_overscan ? 239 : 224;

        // ====================================================================
        // SPRITE TIMING: TWO SEPARATE FORCE_BLANK LATCH POINTS
        // ====================================================================
        // Reference: Mesen-S, nesdev forum HblankEmuTest discussion
        //
        // The SNES PPU has two distinct sprite-related phases with different
        // timing, and force_blank is checked separately for each:
        //
        // 1. Dot 270: Sprite EVALUATION (OAM range scan) completes
        //    - Determines which sprites (up to 32) are on the NEXT scanline
        //    - If force_blank is ON here, no sprites are selected
        //
        // 2. Dot 272: Sprite TILE FETCH begins
        //    - Fetches VRAM tile data for the selected sprites
        //    - If force_blank is ON here, tiles are NOT loaded
        //    - Runs through dot 339
        //
        // HblankEmuTest tests the case where evaluation passes (fb=0 at dot 270)
        // but tile fetch is blocked (fb=1 at dot 272). Result: no sprites shown.
        // ====================================================================

        // ====================================================================
        // FORCE_BLANK LATCHING FOR SPRITE RENDERING
        // ====================================================================
        // Reference: Mesen-S, nesdev HblankEmuTest discussion
        //
        // The tricky part is that games can use H-IRQ to briefly toggle force_blank
        // during H-blank. The test HblankEmuTest fires an IRQ at HTIME=180 (which
        // triggers around H=207), briefly enables force_blank, then disables it.
        //
        // The hardware behavior we need to emulate:
        // - Sprite tile fetch happens H=272-339
        // - If force_blank is ON at ANY point during tile fetch, tiles aren't loaded
        //
        // For a simple implementation, we track if force_blank was EVER enabled
        // in the H-blank region. We do this by latching on EVERY force_blank write
        // and also at the START of H-blank (dot 274).
        //
        // Additionally, we now track force_blank changes via PPU write, and if
        // force_blank becomes enabled during the sprite fetch region (H>=272),
        // we mark sprites as blocked.
        // ====================================================================

        // At the END of each visible scanline, check if force_blank was recently active
        // for sprite tile fetch timing.
        //
        // HblankEmuTest toggles force_blank via H-IRQ. Due to CPU timing drift,
        // the toggle may span scanline boundaries. We check if force_blank was
        // enabled within the last ~3000 master cycles (roughly 2.2 scanlines).
        //
        // One scanline = 340 dots * 4 = 1360 master cycles
        // We use a 3000-cycle window to catch force_blank from the previous
        // scanline's H-blank region, accounting for timing drift that can span
        // across 2 scanlines.
        static constexpr uint64_t FORCE_BLANK_WINDOW = 3000;

        if (m_dot == 339 && m_scanline >= 0 && m_scanline < visible_lines) {
            // Check if force_blank was enabled recently
            bool fb_recent = (m_force_blank_on_cycle > 0) &&
                             (m_total_ppu_cycles - m_force_blank_on_cycle) < FORCE_BLANK_WINDOW;

            // Set the fetch latch if force_blank is currently on OR was recently on
            if (fb_recent || m_force_blank) {
                m_force_blank_latched_fetch = true;
            } else {
                m_force_blank_latched_fetch = false;
            }

            // Copy fetch latch to eval latch (both affect sprite rendering)
            m_force_blank_latched_eval = m_force_blank_latched_fetch;
        }

        dots_to_advance--;
    }

    // Update H/V counters for register reads
    m_hcount = m_dot;
    m_vcount = m_scanline;
}

void PPU::sync_to_current() {
    // Render from the last rendered position up to (but not including) the current dot
    // This ensures that pixels are rendered with the register values that were
    // in effect when those pixels would have been output on real hardware.
    //
    // Scanline numbering convention:
    // - m_scanline from main loop is 0-based (0-261 for NTSC)
    // - Visible scanlines are 0-223 (0-238 with overscan)
    // - render_pixel expects m_scanline = screen_y + 1 (it does y = m_scanline - 1)
    // - So we set m_scanline = current_line + 1 before calling render_pixel

    int visible_lines = m_overscan ? 239 : 224;

    // Debug: track sync calls
    static int sync_debug_count = 0;
    bool debug_sync = is_debug_mode() && sync_debug_count < 10 &&
                      m_frame >= 25 && m_scanline < visible_lines;
    if (debug_sync) {
        sync_debug_count++;
        SNES_PPU_DEBUG("sync_to_current: frame=%lu scanline=%d dot=%d rendered_sl=%d rendered_dot=%d fb=%d TM=$%02X\n",
            m_frame, m_scanline, m_dot, m_rendered_scanline, m_rendered_dot, m_force_blank ? 1 : 0, m_tm);
    }

    // If we're already caught up, nothing to do
    if (m_rendered_scanline == m_scanline && m_rendered_dot >= m_dot) {
        return;
    }

    // Handle the case where we've wrapped around (new frame)
    if (m_rendered_scanline > m_scanline ||
        (m_rendered_scanline == m_scanline && m_rendered_dot > m_dot)) {
        // We've started a new frame - reset rendered position
        m_rendered_scanline = 0;
        m_rendered_dot = 0;
    }

    // Render scanlines from m_rendered_scanline to m_scanline
    while (m_rendered_scanline < m_scanline ||
           (m_rendered_scanline == m_scanline && m_rendered_dot < m_dot)) {

        int current_line = m_rendered_scanline;
        int start_dot = m_rendered_dot;

        // Determine the end dot for this iteration
        int end_dot;
        if (current_line < m_scanline) {
            // Render to end of this scanline
            end_dot = DOTS_PER_SCANLINE;
        } else {
            // Same scanline as target - render up to target dot
            end_dot = m_dot;
        }

        // Render visible pixels on this scanline
        // Main loop uses 0-based scanlines: 0-223 are visible (or 0-238 with overscan)
        // render_pixel expects m_scanline such that y = m_scanline - 1
        // So we set m_scanline = current_line + 1 before calling render_pixel
        if (current_line >= 0 && current_line < visible_lines) {
            int screen_y = current_line;  // 0-based screen coordinate

            // ================================================================
            // SPRITE EVALUATION AND TILE FETCHING
            // ================================================================
            // Reference: Mesen-S, nesdev HblankEmuTest discussion
            //
            // Sprites are processed in two phases with separate force_blank checks:
            //
            // Phase 1: OAM Range Scan (evaluation) - controlled by m_force_blank_latched_eval
            //   - Determines which sprites are on this scanline
            //   - If force_blank was ON at dot 270, no sprites are selected
            //
            // Phase 2: Tile Fetch - controlled by m_force_blank_latched_fetch
            //   - Loads tile data from VRAM for the selected sprites
            //   - If force_blank was ON at dot 272, tiles are NOT loaded
            //   - Sprites selected in phase 1 will not appear if tiles aren't loaded
            //
            // HblankEmuTest tests the case where:
            //   - force_blank OFF at dot 270 -> sprites ARE selected
            //   - force_blank ON at dot 272 -> tiles NOT loaded -> no sprites shown
            // ================================================================

            if (m_sprites_for_scanline != current_line) {
                // Check if sprite evaluation (range scan) should happen
                // based on force_blank state latched at dot 270
                if (!m_force_blank_latched_eval) {
                    // Save and set scanline for sprite evaluation
                    // evaluate_sprites uses m_scanline internally (1-based)
                    int saved = m_scanline;
                    m_scanline = current_line + 1;  // Convert 0-based to 1-based for evaluate_sprites

                    // Check if sprite tile fetch should happen
                    // based on force_blank state latched at dot 272
                    if (!m_force_blank_latched_fetch) {
                        // Both evaluation and tile fetch allowed - full sprite processing
                        evaluate_sprites();
                    } else {
                        // Evaluation allowed, but tile fetch blocked
                        // This is what HblankEmuTest is testing!
                        // Sprites are "found" but their tiles are not loaded
                        // Result: sprites should NOT appear
                        m_sprite_count = 0;
                        m_sprite_tile_count = 0;
                    }

                    m_scanline = saved;
                } else {
                    // Force blank was active at dot 270 - no sprites evaluated at all
                    m_sprite_count = 0;
                    m_sprite_tile_count = 0;
                }
                m_sprites_for_scanline = current_line;
            }

            // Render visible dots (22-277)
            int render_start = std::max(start_dot, 22);
            int render_end = std::min(end_dot, 278);

            for (int dot = render_start; dot < render_end; dot++) {
                int screen_x = dot - 22;

                if (!m_force_blank) {
                    // Save current scanline, set for rendering
                    // render_pixel does: int y = m_scanline - 1
                    // With current_line being 0-based, we need m_scanline = current_line + 1
                    // so that y = (current_line + 1) - 1 = current_line = screen_y
                    int saved = m_scanline;
                    m_scanline = current_line + 1;
                    render_pixel(screen_x);
                    m_scanline = saved;
                } else {
                    // Force blank - output black (512-pixel stride with duplicated pixels)
                    m_framebuffer[screen_y * 512 + screen_x * 2] = 0xFF000000;
                    m_framebuffer[screen_y * 512 + screen_x * 2 + 1] = 0xFF000000;
                }
            }
        }

        // Move to next scanline or update dot position
        if (end_dot >= DOTS_PER_SCANLINE) {
            m_rendered_scanline++;
            m_rendered_dot = 0;
            // Note: We don't reset m_sprites_for_scanline here because
            // sprites for the new scanline should have been evaluated at
            // dot 285 of the previous scanline (or will be when we first
            // try to render visible pixels via the fallback in sync_to_current)

            // Handle frame wrap
            if (m_rendered_scanline >= SCANLINES_PER_FRAME) {
                m_rendered_scanline = 0;
            }
        } else {
            m_rendered_dot = end_dot;
        }
    }
}

void PPU::step() {
    // Render visible scanlines (1-224 or 1-239 in overscan)
    int visible_lines = m_overscan ? 239 : 224;

    if (m_scanline >= 1 && m_scanline <= visible_lines && m_dot >= 22 && m_dot < 278) {
        // Render visible pixels (22-277 = 256 pixels)
        int x = m_dot - 22;
        if (!m_force_blank) {
            render_pixel(x);
        } else {
            // Force blank - output black (512-pixel stride with duplicated pixels)
            int y = m_scanline - 1;
            m_framebuffer[y * 512 + x * 2] = 0xFF000000;
            m_framebuffer[y * 512 + x * 2 + 1] = 0xFF000000;
        }
    }

    // Sprite evaluation happens during HBlank (around dot 278-285)
    // This evaluates sprites for the NEXT scanline.
    // If force_blank is active during HBlank, sprites will not be loaded.
    // Reference: Mesen-S does sprite evaluation at Hdot 285.
    if (m_dot == 285 && m_scanline >= 0 && m_scanline < visible_lines) {
        // Evaluate sprites for scanline (m_scanline + 1)
        // The evaluate_sprites function checks m_force_blank internally
        int next_scanline = m_scanline + 1;
        int saved_scanline = m_scanline;
        m_scanline = next_scanline;
        evaluate_sprites();
        m_scanline = saved_scanline;
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

            // Debug: Dump VRAM at frame 280
            if (is_debug_mode() && m_frame == 280) {
                SNES_PPU_DEBUG("VRAM dump: A1E0=%02X%02X%02X%02X E300=%02X%02X%02X%02X\n",
                    m_vram[0xA1E0], m_vram[0xA1E1], m_vram[0xA1E2], m_vram[0xA1E3],
                    m_vram[0xE300], m_vram[0xE301], m_vram[0xE302], m_vram[0xE303]);
            }
        }
    }
}

void PPU::render_scanline(int scanline) {
    // Set the scanline for rendering
    m_scanline = scanline + 1;  // Internal scanline is 1-based

    // Debug: Dump VRAM at frame 280, scanline 0
    if (is_debug_mode() && m_frame == 280 && scanline == 0) {
        // Scan VRAM to find non-zero regions
        for (int region = 0; region < 16; region++) {
            uint32_t base = region * 0x1000;
            bool has_data = false;
            for (int i = 0; i < 0x1000 && !has_data; i += 16) {
                for (int j = 0; j < 16; j++) {
                    if (m_vram[base + i + j] != 0) { has_data = true; break; }
                }
            }
            if (has_data) {
                SNES_PPU_DEBUG("VRAM region %X000: %02X%02X%02X%02X...\n", region,
                    m_vram[base], m_vram[base+1], m_vram[base+2], m_vram[base+3]);
            }
        }
    }

    // Evaluate sprites for this scanline
    evaluate_sprites();

    // Render all 256 visible pixels
    for (int x = 0; x < 256; x++) {
        if (!m_force_blank) {
            render_pixel(x);
        } else {
            // Force blank - output black (512-pixel stride with duplicated pixels)
            m_framebuffer[scanline * 512 + x * 2] = 0xFF000000;
            m_framebuffer[scanline * 512 + x * 2 + 1] = 0xFF000000;
        }
    }
}

void PPU::end_frame() {
    // Diagnostic: Dump rendering state at key frames (debug mode only)
    if (is_debug_mode() && (m_frame == 150 || m_frame == 200 || m_frame == 250 || m_frame == 270 || m_frame == 280 || m_frame == 300 || m_frame == 350)) {
        // Count non-zero CGRAM colors
        int non_zero_colors = 0;
        for (int i = 0; i < 256; i++) {
            uint16_t color = m_cgram[i * 2] | (m_cgram[i * 2 + 1] << 8);
            if (color != 0) non_zero_colors++;
        }

        // Analyze palette distribution by 16-color groups
        fprintf(stderr, "[SNES/PPU] Frame %lu state: Mode=%d TM=$%02X TS=$%02X Bright=%d ForceBlank=%d\n",
            m_frame, m_bg_mode, m_tm, m_ts, m_brightness, m_force_blank ? 1 : 0);
        fprintf(stderr, "[SNES/PPU]   CGRAM: %d/256 non-zero colors\n", non_zero_colors);

        // Show which BG palettes have data (CGRAM 0-127)
        for (int pal = 0; pal < 8; pal++) {
            int pal_colors = 0;
            for (int c = 0; c < 16; c++) {
                int idx = pal * 16 + c;
                uint16_t color = m_cgram[idx * 2] | (m_cgram[idx * 2 + 1] << 8);
                if (color != 0) pal_colors++;
            }
            if (pal_colors > 0) {
                fprintf(stderr, "[SNES/PPU]   BG Palette %d: %d colors, first=$%04X\n",
                    pal, pal_colors, m_cgram[pal * 32] | (m_cgram[pal * 32 + 1] << 8));
            }
        }

        // Show which SPRITE palettes have data (CGRAM 128-255)
        for (int pal = 0; pal < 8; pal++) {
            int pal_colors = 0;
            for (int c = 0; c < 16; c++) {
                int idx = 128 + pal * 16 + c;  // Sprite palettes start at 128
                uint16_t color = m_cgram[idx * 2] | (m_cgram[idx * 2 + 1] << 8);
                if (color != 0) pal_colors++;
            }
            if (pal_colors > 0) {
                int first_idx = 128 + pal * 16;
                fprintf(stderr, "[SNES/PPU]   Sprite Palette %d: %d colors, first=$%04X\n",
                    pal, pal_colors, m_cgram[first_idx * 2] | (m_cgram[first_idx * 2 + 1] << 8));
            }
        }

        // Show first few pixels of BG2 tilemap for context
        if (m_bg_mode == 3) {
            uint16_t tilemap = m_bg_tilemap_addr[1];
            fprintf(stderr, "[SNES/PPU]   BG2 tilemap at $%04X: ", tilemap);
            for (int t = 0; t < 4; t++) {
                uint8_t lo = m_vram[(tilemap + t * 2) & 0xFFFF];
                uint8_t hi = m_vram[(tilemap + t * 2 + 1) & 0xFFFF];
                int tile = lo | ((hi & 0x03) << 8);
                int pal = (hi >> 2) & 0x07;
                fprintf(stderr, "[T%d:P%d] ", tile, pal);
            }
            fprintf(stderr, "\n");

            // Track VRAM contents at key locations
            int nz_8000 = 0, nz_A000 = 0;
            int first_nz_A000 = -1, last_nz_A000 = -1;
            for (int i = 0; i < 0x2000; i++) {
                if (m_vram[(0x8000 + i) & 0xFFFF] != 0) nz_8000++;
                if (m_vram[(0xA000 + i) & 0xFFFF] != 0) {
                    nz_A000++;
                    if (first_nz_A000 < 0) first_nz_A000 = i;
                    last_nz_A000 = i;
                }
            }
            fprintf(stderr, "[SNES/PPU]   VRAM: $8000-9FFF: %d bytes, $A000-BFFF: %d bytes (BG2 chr=%04X)\n",
                nz_8000, nz_A000, m_bg_chr_addr[1]);
            if (nz_A000 > 0) {
                fprintf(stderr, "[SNES/PPU]   $A000 non-zero range: $%04X-$%04X, first bytes: %02X %02X %02X %02X\n",
                    0xA000 + first_nz_A000, 0xA000 + last_nz_A000,
                    m_vram[(0xA000 + first_nz_A000) & 0xFFFF],
                    m_vram[(0xA000 + first_nz_A000 + 1) & 0xFFFF],
                    m_vram[(0xA000 + first_nz_A000 + 2) & 0xFFFF],
                    m_vram[(0xA000 + first_nz_A000 + 3) & 0xFFFF]);
            }
        }

        // Dump first 10 OAM entries
        fprintf(stderr, "[SNES/PPU]   OAM entries (first 10 non-Y=$FF):\n");
        int shown = 0;
        for (int i = 0; i < 128 && shown < 10; i++) {
            int oam_addr = i * 4;
            int y_pos = m_oam[oam_addr + 1];
            if (y_pos == 0xFF) continue;  // Skip disabled sprites
            int x_pos = m_oam[oam_addr];
            int tile = m_oam[oam_addr + 2];
            int attr = m_oam[oam_addr + 3];
            int high_bits = (m_oam[512 + i/4] >> ((i % 4) * 2)) & 0x03;
            int full_x = x_pos - (high_bits & 1 ? 256 : 0);
            bool large = (high_bits & 2) != 0;
            fprintf(stderr, "    [%d] x=%d y=%d tile=$%02X attr=$%02X %s\n",
                i, full_x, y_pos, tile, attr, large ? "LARGE" : "small");
            shown++;
        }

        // Dump VRAM at sprite tile addresses
        fprintf(stderr, "[SNES/PPU]   OBSEL=$%02X base=$%04X (byte $%04X)\n",
            m_obsel, m_obj_base_addr, m_obj_base_addr * 2);
        // Check if there's any non-zero data in the sprite tile region
        uint32_t base_byte = m_obj_base_addr * 2;
        int non_zero = 0;
        for (uint32_t i = 0; i < 0x2000; i++) {
            if (m_vram[(base_byte + i) & 0xFFFF] != 0) non_zero++;
        }
        fprintf(stderr, "[SNES/PPU]   VRAM at sprite base: %d/8192 non-zero bytes\n", non_zero);
        // Show first few bytes
        fprintf(stderr, "[SNES/PPU]   VRAM[%04X]: %02X %02X %02X %02X %02X %02X %02X %02X\n",
            base_byte,
            m_vram[base_byte], m_vram[base_byte+1], m_vram[base_byte+2], m_vram[base_byte+3],
            m_vram[base_byte+4], m_vram[base_byte+5], m_vram[base_byte+6], m_vram[base_byte+7]);
    }

    // Increment frame counter here for tests that use end_frame() path
    // Note: step()/advance() also has a frame counter increment for cycle-accurate path
    m_frame++;
}

void PPU::render_pixel(int x) {
    int y = m_scanline - 1;

    // Debug: track render_pixel calls (x=50 is near the left text area)
    static int render_pixel_debug_count = 0;
    if (is_debug_mode() && render_pixel_debug_count < 5 && m_frame >= 25 && y >= 70 && y <= 90 && x == 50) {
        render_pixel_debug_count++;
        SNES_PPU_DEBUG("render_pixel: x=%d y=%d (m_scanline=%d) TM=$%02X mode=%d frame=%lu tilemap0=$%04X chr0=$%04X\n",
            x, y, m_scanline, m_tm, m_bg_mode, m_frame, m_bg_tilemap_addr[0], m_bg_chr_addr[0]);
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

    // Get fixed color for sub-screen backdrop (register $2132)
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
    // For Mode 5/6 (hi-res), we need separate pixels for main and sub screens
    uint8_t bg_pixel[4] = {0, 0, 0, 0};
    uint8_t bg_priority[4] = {0, 0, 0, 0};
    uint8_t bg_palette[4] = {0, 0, 0, 0};  // Palette for Direct Color mode
    // Sub-screen pixels for Mode 5/6 hi-res (even tile pixels)
    uint8_t bg_pixel_sub[4] = {0, 0, 0, 0};
    uint8_t bg_priority_sub[4] = {0, 0, 0, 0};

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

    // Check if we're in hi-res mode (Mode 5/6)
    bool is_hires_bg_mode = (m_bg_mode == 5 || m_bg_mode == 6);

    // Render all backgrounds (we'll check TM/TS later for main/sub screen enable)
    // For Modes 3/4, we also capture the palette for Direct Color mode
    // For Mode 5/6, we render with odd pixel selection for main screen
    for (int bg = 0; bg < num_bgs; bg++) {
        if (m_bg_mode == 7 && bg == 0) {
            render_mode7_pixel(x, bg_pixel[0], bg_priority[0]);
        } else if ((m_bg_mode == 3 || m_bg_mode == 4) && bg == 0) {
            // 8bpp BG1 in Mode 3/4 - capture palette for Direct Color
            render_background_pixel(bg, x, bg_pixel[bg], bg_priority[bg], bg_palette[bg]);
        } else if (is_hires_bg_mode) {
            // Mode 5/6: render with hires_odd_pixel=true for main screen
            render_background_pixel(bg, x, bg_pixel[bg], bg_priority[bg], true, true);
        } else {
            // Regular modes: use standard rendering
            render_background_pixel(bg, x, bg_pixel[bg], bg_priority[bg]);
        }
    }

    // For Mode 5/6, also render sub-screen pixels (even tile pixels)
    if (is_hires_bg_mode) {
        for (int bg = 0; bg < num_bgs; bg++) {
            render_background_pixel(bg, x, bg_pixel_sub[bg], bg_priority_sub[bg], true, false);
        }
    }

    // Render sprites
    uint8_t sprite_pixel = 0;
    uint8_t sprite_priority = 0;
    bool sprite_palette_4_7 = false;
    render_sprite_pixel(x, sprite_pixel, sprite_priority, sprite_palette_4_7);

    // Debug pixel rendering
    // Debug at a position where BG tiles should be visible (top-left corner)
    bool debug_pixel = is_debug_mode() && m_frame == 300 && m_scanline == 10 && (x == 16);
    if (debug_pixel) {
        SNES_PPU_DEBUG("render_pixel x=%d: TM=$%02X TS=$%02X mode=%d\n", x, m_tm, m_ts, m_bg_mode);
        SNES_PPU_DEBUG("  BG1: tilemap=$%04X chr=$%04X hofs=%d vofs=%d\n",
            m_bg_tilemap_addr[0], m_bg_chr_addr[0], m_bg_hofs[0], m_bg_vofs[0]);
        SNES_PPU_DEBUG("  BG2: tilemap=$%04X chr=$%04X hofs=%d vofs=%d\n",
            m_bg_tilemap_addr[1], m_bg_chr_addr[1], m_bg_hofs[1], m_bg_vofs[1]);
        SNES_PPU_DEBUG("  BG pixels: [%d,%d,%d,%d] pri=[%d,%d,%d,%d]\n",
            bg_pixel[0], bg_pixel[1], bg_pixel[2], bg_pixel[3],
            bg_priority[0], bg_priority[1], bg_priority[2], bg_priority[3]);
        SNES_PPU_DEBUG("  sprite_pixel=%d sprite_pri=%d\n", sprite_pixel, sprite_priority);
        // Check BG1 tilemap at this position
        int px1 = (x + (m_bg_hofs[0] & 0x3FF)) & 0x3FF;
        int py1 = ((m_scanline - 1) + (m_bg_vofs[0] & 0x3FF)) & 0x3FF;
        int tile_size1 = m_bg_tile_size[0] ? 16 : 8;
        int tile_x1 = px1 / tile_size1;
        int tile_y1 = py1 / tile_size1;
        uint16_t tm_addr1 = m_bg_tilemap_addr[0] + (tile_y1 % 32) * 64 + (tile_x1 % 32) * 2;
        uint8_t lo1 = m_vram[tm_addr1 & 0xFFFF];
        uint8_t hi1 = m_vram[(tm_addr1 + 1) & 0xFFFF];
        int tile1 = lo1 | ((hi1 & 0x03) << 8);
        SNES_PPU_DEBUG("  BG1 at px=%d py=%d: tile_x=%d tile_y=%d tilemap=$%04X tile=%d\n",
            px1, py1, tile_x1, tile_y1, tm_addr1, tile1);

        // Also check BG2 tilemap at this position
        int px2 = (x + (m_bg_hofs[1] & 0x3FF)) & 0x3FF;
        int py2 = ((m_scanline - 1) + (m_bg_vofs[1] & 0x3FF)) & 0x3FF;
        int tile_size2 = m_bg_tile_size[1] ? 16 : 8;
        int tile_x2 = px2 / tile_size2;
        int tile_y2 = py2 / tile_size2;
        uint16_t tm_addr2 = m_bg_tilemap_addr[1] + (tile_y2 % 32) * 64 + (tile_x2 % 32) * 2;
        uint8_t lo2 = m_vram[tm_addr2 & 0xFFFF];
        uint8_t hi2 = m_vram[(tm_addr2 + 1) & 0xFFFF];
        int tile2 = lo2 | ((hi2 & 0x03) << 8);
        SNES_PPU_DEBUG("  BG2 at px=%d py=%d: tile_x=%d tile_y=%d tilemap=$%04X tile=%d\n",
            px2, py2, tile_x2, tile_y2, tm_addr2, tile2);
    }

    // Helper lambda to composite layers with priority
    // Returns the winning layer's color, source ID, and color math enable flag
    // window_mask: TMW for main screen, TSW for sub screen - enables window masking per layer
    // use_hires_sub: if true and in hi-res mode, use bg_pixel_sub/bg_priority_sub instead
    auto composite_screen = [&](uint8_t layer_mask, uint8_t window_mask, bool use_hires_sub = false) -> LayerPixel {
        LayerPixel result = {backdrop, 0, 0, m_backdrop_color_math};

        // In hi-res mode (Mode 5/6), sub screen uses different pixel selections from tiles
        // Reference the appropriate arrays based on use_hires_sub flag
        const uint8_t* pix = (use_hires_sub && is_hires_bg_mode) ? bg_pixel_sub : bg_pixel;
        const uint8_t* pri = (use_hires_sub && is_hires_bg_mode) ? bg_priority_sub : bg_priority;

        // Helper to check if a BG layer is visible (enabled and not masked by window)
        auto bg_visible = [&](int bg) -> bool {
            uint8_t bit = 1 << bg;
            if (!(layer_mask & bit)) return false;  // Not enabled
            if ((window_mask & bit) && get_bg_window(bg, x)) return false;  // Masked by window
            return true;
        };

        // Helper to check if OBJ is visible (enabled and not masked by window)
        auto obj_visible = [&]() -> bool {
            if (!(layer_mask & 0x10)) return false;  // Not enabled
            if ((window_mask & 0x10) && get_obj_window(x)) return false;  // Masked by window
            return true;
        };

        // Priority-based compositing based on BG mode
        // We go from lowest to highest priority, letting higher priority overwrite

        switch (m_bg_mode) {
            case 0: {
                // Mode 0 priority (lowest to highest):
                // BG4.pri0, BG3.pri0, OBJ.pri0, BG4.pri1, BG3.pri1, OBJ.pri1,
                // BG2.pri0, BG1.pri0, OBJ.pri2, BG2.pri1, BG1.pri1, OBJ.pri3

                // BG4 priority 0
                if (bg_visible(3) && pix[3] && !pri[3]) {
                    result = {get_color(0, pix[3]), pri[3], 4, m_bg_color_math[3]};
                }
                // BG3 priority 0
                if (bg_visible(2) && pix[2] && !pri[2]) {
                    result = {get_color(0, pix[2]), pri[2], 3, m_bg_color_math[2]};
                }
                // OBJ priority 0
                if (obj_visible() && sprite_pixel && sprite_priority == 0) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG4 priority 1
                if (bg_visible(3) && pix[3] && pri[3]) {
                    result = {get_color(0, pix[3]), pri[3], 4, m_bg_color_math[3]};
                }
                // BG3 priority 1
                if (bg_visible(2) && pix[2] && pri[2]) {
                    result = {get_color(0, pix[2]), pri[2], 3, m_bg_color_math[2]};
                }
                // OBJ priority 1
                if (obj_visible() && sprite_pixel && sprite_priority == 1) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 0
                if (bg_visible(1) && pix[1] && !pri[1]) {
                    result = {get_color(0, pix[1]), pri[1], 2, m_bg_color_math[1]};
                }
                // BG1 priority 0
                if (bg_visible(0) && pix[0] && !pri[0]) {
                    result = {get_color(0, pix[0]), pri[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 2
                if (obj_visible() && sprite_pixel && sprite_priority == 2) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 1
                if (bg_visible(1) && pix[1] && pri[1]) {
                    result = {get_color(0, pix[1]), pri[1], 2, m_bg_color_math[1]};
                }
                // BG1 priority 1
                if (bg_visible(0) && pix[0] && pri[0]) {
                    result = {get_color(0, pix[0]), pri[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 3
                if (obj_visible() && sprite_pixel && sprite_priority == 3) {
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
                if (bg_visible(2) && pix[2] && !pri[2]) {
                    result = {get_color(0, pix[2]), pri[2], 3, m_bg_color_math[2]};
                }
                // OBJ priority 0
                if (obj_visible() && sprite_pixel && sprite_priority == 0) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG3 priority 1 (if BG3 priority bit is NOT set)
                if (!m_bg3_priority && bg_visible(2) && pix[2] && pri[2]) {
                    result = {get_color(0, pix[2]), pri[2], 3, m_bg_color_math[2]};
                }
                // OBJ priority 1
                if (obj_visible() && sprite_pixel && sprite_priority == 1) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 0
                if (bg_visible(1) && pix[1] && !pri[1]) {
                    result = {get_color(0, pix[1]), pri[1], 2, m_bg_color_math[1]};
                }
                // BG1 priority 0
                if (bg_visible(0) && pix[0] && !pri[0]) {
                    result = {get_color(0, pix[0]), pri[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 2
                if (obj_visible() && sprite_pixel && sprite_priority == 2) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 1
                if (bg_visible(1) && pix[1] && pri[1]) {
                    result = {get_color(0, pix[1]), pri[1], 2, m_bg_color_math[1]};
                }
                // BG1 priority 1
                if (bg_visible(0) && pix[0] && pri[0]) {
                    result = {get_color(0, pix[0]), pri[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 3
                if (obj_visible() && sprite_pixel && sprite_priority == 3) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG3 priority 1 (if BG3 priority bit IS set - highest priority)
                if (m_bg3_priority && bg_visible(2) && pix[2] && pri[2]) {
                    result = {get_color(0, pix[2]), pri[2], 3, m_bg_color_math[2]};
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

                // Debug: one-time check for Mode 3 BG2 compositing
                static bool mode3_composite_debugged = false;
                if (is_debug_mode() && m_bg_mode == 3 && m_frame == 285 && y == 112 && x == 128 && !mode3_composite_debugged) {
                    mode3_composite_debugged = true;
                    fprintf(stderr, "[SNES/PPU] Mode 3 composite debug:\n");
                    fprintf(stderr, "  layer_mask=$%02X (TM) window_mask=$%02X (TMW)\n", layer_mask, window_mask);
                    fprintf(stderr, "  bg_visible(1)=%d (should be true if TM bit 1 set)\n", bg_visible(1) ? 1 : 0);
                    fprintf(stderr, "  bg_pixel[0]=%d bg_pixel[1]=%d\n", bg_pixel[0], bg_pixel[1]);
                    fprintf(stderr, "  bg_priority[0]=%d bg_priority[1]=%d\n", bg_priority[0], bg_priority[1]);
                    fprintf(stderr, "  backdrop color=$%04X\n", backdrop);
                }

                // Helper to get BG1 color (handles Direct Color mode for Modes 3/4)
                // Note: Direct Color uses the pix pointer (which points to the correct
                // array based on whether this is main or sub screen in hi-res mode)
                auto get_bg1_color = [&]() -> uint16_t {
                    // Direct Color is available for 8bpp BG1 in Modes 3 and 4
                    if (m_direct_color && (m_bg_mode == 3 || m_bg_mode == 4)) {
                        return get_direct_color(bg_palette[0], pix[0]);
                    }
                    return get_color(0, pix[0]);
                };

                // BG2 priority 0
                if (bg_visible(1) && pix[1] && !pri[1]) {
                    result = {get_color(0, pix[1]), pri[1], 2, m_bg_color_math[1]};
                }
                // OBJ priority 0
                if (obj_visible() && sprite_pixel && sprite_priority == 0) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG1 priority 0
                if (bg_visible(0) && pix[0] && !pri[0]) {
                    result = {get_bg1_color(), pri[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 1
                if (obj_visible() && sprite_pixel && sprite_priority == 1) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG2 priority 1
                if (bg_visible(1) && pix[1] && pri[1]) {
                    result = {get_color(0, pix[1]), pri[1], 2, m_bg_color_math[1]};
                }
                // OBJ priority 2
                if (obj_visible() && sprite_pixel && sprite_priority == 2) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                // BG1 priority 1
                if (bg_visible(0) && pix[0] && pri[0]) {
                    result = {get_bg1_color(), pri[0], 1, m_bg_color_math[0]};
                }
                // OBJ priority 3
                if (obj_visible() && sprite_pixel && sprite_priority == 3) {
                    result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                              m_obj_color_math && sprite_palette_4_7};
                }
                break;
            }

            case 7: {
                // ================================================================
                // MODE 7 WITH EXTBG SUPPORT
                // ================================================================
                // Reference: SNESdev wiki Backgrounds, sneslab.net Mode_7
                //
                // Standard Mode 7: BG1 only, sprites above
                //
                // With EXTBG ($2133 bit 6): BG2 uses same Mode 7 data but treats
                // bit 7 of color as priority. Priority order (low to high):
                //   BG2.pri0 (bit7=0), OBJ.pri0, BG1, OBJ.pri1, BG2.pri1 (bit7=1), OBJ.pri2/3
                //
                // BG2 in EXTBG:
                //   - Uses bits 0-6 for color (7bpp, 128 colors)
                //   - Bit 7 determines priority (0=low, 1=high)
                //   - Does NOT support direct color (always indexed)
                // ================================================================

                if (m_extbg) {
                    // EXTBG Mode 7 priority compositing
                    // pix[0] is the raw 8-bit Mode 7 color value

                    // BG2 low priority (bit 7 = 0) - lowest layer
                    if (bg_visible(1) && pix[0] && !(pix[0] & 0x80)) {
                        uint8_t color_index = pix[0] & 0x7F;  // 7-bit color
                        if (color_index != 0) {
                            result = {get_color(0, color_index), 0, 2, m_bg_color_math[1]};
                        }
                    }

                    // OBJ priority 0
                    if (obj_visible() && sprite_pixel && sprite_priority == 0) {
                        result = {get_color(0, sprite_pixel, true), 0, 5,
                                  m_obj_color_math && sprite_palette_4_7};
                    }

                    // BG1 (full 8-bit color, direct color available)
                    if (bg_visible(0) && pix[0]) {
                        uint16_t color = m_direct_color ? get_direct_color(0, pix[0])
                                                        : get_color(0, pix[0]);
                        result = {color, 0, 1, m_bg_color_math[0]};
                    }

                    // OBJ priority 1
                    if (obj_visible() && sprite_pixel && sprite_priority == 1) {
                        result = {get_color(0, sprite_pixel, true), 1, 5,
                                  m_obj_color_math && sprite_palette_4_7};
                    }

                    // BG2 high priority (bit 7 = 1)
                    if (bg_visible(1) && pix[0] && (pix[0] & 0x80)) {
                        uint8_t color_index = pix[0] & 0x7F;  // 7-bit color
                        if (color_index != 0) {
                            result = {get_color(0, color_index), 1, 2, m_bg_color_math[1]};
                        }
                    }

                    // OBJ priority 2 and 3 (highest)
                    if (obj_visible() && sprite_pixel && sprite_priority >= 2) {
                        result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                                  m_obj_color_math && sprite_palette_4_7};
                    }
                } else {
                    // Standard Mode 7: BG1 and sprites only
                    // BG1 (Mode 7 has no priority bit in standard mode)
                    // Direct Color available for Mode 7 (no palette bits, so palette = 0)
                    if (bg_visible(0) && pix[0]) {
                        uint16_t color = m_direct_color ? get_direct_color(0, pix[0])
                                                        : get_color(0, pix[0]);
                        result = {color, 0, 1, m_bg_color_math[0]};
                    }
                    // OBJ (all priorities above BG in standard Mode 7)
                    if (obj_visible() && sprite_pixel) {
                        result = {get_color(0, sprite_pixel, true), sprite_priority, 5,
                                  m_obj_color_math && sprite_palette_4_7};
                    }
                }
                break;
            }
        }

        return result;
    };

    // Composite main screen (using TM register and TMW window mask)
    main_pixel = composite_screen(m_tm, m_tmw);

    if (debug_pixel) {
        SNES_PPU_DEBUG("  composite result: color=$%04X source=%d\n",
            main_pixel.color, main_pixel.source);
    }

    // Composite sub screen (using TS register and TSW window mask)
    // Sub screen uses fixed color as backdrop, not CGRAM[0]
    // In hi-res modes (Mode 5/6), pass true to use the even-pixel BG data
    sub_pixel = composite_screen(m_ts, m_tsw, true);
    if (debug_pixel) {
        SNES_PPU_DEBUG("  sub_screen before fix: color=$%04X source=%d\n",
            sub_pixel.color, sub_pixel.source);
        SNES_PPU_DEBUG("  fixed_color=$%04X (r=%d g=%d b=%d)\n",
            fixed_color, m_fixed_color_r, m_fixed_color_g, m_fixed_color_b);
    }
    if (sub_pixel.source == 0) {
        // If sub screen shows backdrop, use fixed color instead
        sub_pixel.color = fixed_color;
    }
    if (debug_pixel) {
        SNES_PPU_DEBUG("  sub_screen after fix: color=$%04X\n", sub_pixel.color);
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

        if (debug_pixel) {
            SNES_PPU_DEBUG("  color math: apply=1 main=$%04X blend=$%04X sub_bg_obj=%d add=%d\n",
                main_pixel.color, blend_color, m_sub_screen_bg_obj, m_color_math_add);
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
    // HI-RES AND PSEUDO-HIRES MODE OUTPUT
    // ============================================================================
    // Reference: sneslab.net Horizontal Pseudo 512 Mode, fullsnes SETINI
    // Reference: SNESdev wiki Backgrounds, Mode_5 documentation
    //
    // Hi-res output (512 pixels per scanline) is used in two cases:
    // 1. Pseudo-hires ($2133.3 set) - manually interleaves main/sub screen content
    // 2. Mode 5 or 6 - automatically uses 16-pixel-wide tiles with even/odd split
    //
    // In Mode 5/6:
    // - Tiles are always 16 pixels wide
    // - Even pixels (0,2,4,6...) go to sub screen
    // - Odd pixels (1,3,5,7...) go to main screen
    // - The hardware automatically de-interleaves tile data
    //
    // For our emulator, we treat Mode 5/6 similar to pseudo-hires by outputting
    // to a 512-wide framebuffer. The tile rendering already handles 16-pixel
    // tiles, so main and sub screens naturally get different portions of tiles.
    // ============================================================================
    bool use_hires_output = m_pseudo_hires || (m_bg_mode == 5) || (m_bg_mode == 6);

    // Helper to convert 15-bit SNES color to 32-bit ARGB with brightness
    // Reference: bsnes/sfc/ppu/ppu.cpp lightTable generation
    // Formula: luma = brightness / 15.0; output = round(input * luma)
    // This matches hardware behavior where brightness 15 = full, 0 = black
    auto apply_brightness_and_convert = [this](uint16_t color) -> uint32_t {
        // Extract 5-bit RGB components from 15-bit SNES BGR555 color
        int r_in = color & 0x1F;
        int g_in = (color >> 5) & 0x1F;
        int b_in = (color >> 10) & 0x1F;

        // Apply brightness with rounding (matching bsnes: (input * brightness + 7) / 15)
        // Adding 7 (half of 15) provides proper rounding for values 0-15
        int r = (r_in * m_brightness + 7) / 15;
        int g = (g_in * m_brightness + 7) / 15;
        int b = (b_in * m_brightness + 7) / 15;

        // Clamp to 0-31 (shouldn't be necessary but safe)
        r = std::min(r, 31);
        g = std::min(g, 31);
        b = std::min(b, 31);

        // Convert 5-bit color to 8-bit (expand using upper bits for accuracy)
        // This replicates the high bits into the low bits for proper 8-bit range
        r = (r << 3) | (r >> 2);
        g = (g << 3) | (g >> 2);
        b = (b << 3) | (b >> 2);

        // Return as 32-bit ABGR (0xAABBGGRR format matching other cores)
        // On little-endian systems, this byte order is RGBA when accessed as bytes
        // This matches the GB and NES core output format
        return 0xFF000000 | (b << 16) | (g << 8) | r;
    };

    if (use_hires_output) {
        // Hi-res output: 512 pixels per scanline
        // Even pixel (2*x): sub screen color
        // Odd pixel (2*x+1): main screen color
        // This interleaving creates transparency when viewed on CRT
        //
        // For Mode 5/6: Each 16-pixel tile is split so even pixels go to sub
        // screen and odd pixels go to main screen. Since our BG rendering
        // already handles 16-pixel tiles, both main and sub screens have
        // full content from the same tiles (just different pixel selections).

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
        // Standard 256-pixel mode - output duplicated pixels to 512-wide framebuffer
        // This ensures consistent framebuffer stride when modes are mixed mid-frame
        // (e.g., SplitScreen test switches between Mode 5 and Mode 3)
        uint32_t argb = apply_brightness_and_convert(final_color);
        m_framebuffer[y * 512 + x * 2] = argb;
        m_framebuffer[y * 512 + x * 2 + 1] = argb;  // Duplicate for 256-pixel mode

        // Debug: track pixel output
        static int pixel_output_debug = 0;
        if (is_debug_mode() && pixel_output_debug < 5 && m_frame >= 25 && y >= 70 && y <= 90 && x == 50) {
            pixel_output_debug++;
            SNES_PPU_DEBUG("  PIXEL OUTPUT: x=%d y=%d final=$%04X bright=%d argb=$%08X fb_idx=%d\n",
                x, y, final_color, m_brightness, argb, y * 512 + x * 2);
        }

        if (debug_pixel) {
            SNES_PPU_DEBUG("  final_color=$%04X brightness=%d -> argb=$%08X\n",
                final_color, m_brightness, argb);
        }

        // One-time diagnostic: count non-black pixels in Mode 3
        static bool mode3_pixel_diagnosed = false;
        if (is_debug_mode() && m_bg_mode == 3 && m_frame == 285 && y == 112 && !mode3_pixel_diagnosed) {
            if (x == 128) {  // Check at center pixel
                mode3_pixel_diagnosed = true;
                fprintf(stderr, "[SNES/PPU] Mode 3 center pixel: final_color=$%04X argb=$%08X\n",
                    final_color, argb);
                fprintf(stderr, "  main_pixel: color=$%04X source=%d (0=backdrop,2=BG2)\n",
                    main_pixel.color, main_pixel.source);
                fprintf(stderr, "  bg_pixel[1]=%d (BG2 raw pixel value)\n", bg_pixel[1]);
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

bool PPU::get_bg_window(int bg, int x) const {
    // Evaluate window 1 for this BG
    bool w1 = false;
    if (m_bg_window1_enable[bg]) {
        w1 = (x >= m_window1_left && x <= m_window1_right);
        if (m_bg_window1_invert[bg]) w1 = !w1;
    }

    // Evaluate window 2 for this BG
    bool w2 = false;
    if (m_bg_window2_enable[bg]) {
        w2 = (x >= m_window2_left && x <= m_window2_right);
        if (m_bg_window2_invert[bg]) w2 = !w2;
    }

    // Combine windows based on logic mode
    bool result = false;
    if (!m_bg_window1_enable[bg] && !m_bg_window2_enable[bg]) {
        result = false;
    } else if (m_bg_window1_enable[bg] && !m_bg_window2_enable[bg]) {
        result = w1;
    } else if (!m_bg_window1_enable[bg] && m_bg_window2_enable[bg]) {
        result = w2;
    } else {
        switch (m_bg_window_logic[bg]) {
            case 0: result = w1 || w2; break;  // OR
            case 1: result = w1 && w2; break;  // AND
            case 2: result = w1 != w2; break;  // XOR
            case 3: result = w1 == w2; break;  // XNOR
        }
    }

    return result;
}

bool PPU::get_obj_window(int x) const {
    // Evaluate window 1 for OBJ
    bool w1 = false;
    if (m_obj_window1_enable) {
        w1 = (x >= m_window1_left && x <= m_window1_right);
        if (m_obj_window1_invert) w1 = !w1;
    }

    // Evaluate window 2 for OBJ
    bool w2 = false;
    if (m_obj_window2_enable) {
        w2 = (x >= m_window2_left && x <= m_window2_right);
        if (m_obj_window2_invert) w2 = !w2;
    }

    // Combine windows based on logic mode
    bool result = false;
    if (!m_obj_window1_enable && !m_obj_window2_enable) {
        result = false;
    } else if (m_obj_window1_enable && !m_obj_window2_enable) {
        result = w1;
    } else if (!m_obj_window1_enable && m_obj_window2_enable) {
        result = w2;
    } else {
        switch (m_obj_window_logic) {
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

    // Debug BG rendering - once per frame at a specific pixel for Mode 3 BG2
    bool debug_bg = is_debug_mode() && m_frame == 300 && m_scanline == 10 && x == 16 && bg == 1;
    // Also do a one-time summary when Mode 3 BG2 is first rendered
    static bool mode3_bg2_diagnosed = false;
    bool do_mode3_diagnosis = is_debug_mode() && m_bg_mode == 3 && bg == 1 && m_frame >= 280 && !mode3_bg2_diagnosed;
    if (do_mode3_diagnosis) {
        mode3_bg2_diagnosed = true;
        uint16_t tilemap_base = m_bg_tilemap_addr[1];
        uint16_t chr_base = m_bg_chr_addr[1];
        fprintf(stderr, "[SNES/PPU] Mode 3 BG2 diagnosis at frame %lu:\n", m_frame);
        fprintf(stderr, "  BG2 tilemap=$%04X chr=$%04X TM=$%02X\n", tilemap_base, chr_base, m_tm);

        // Check if BG2 is enabled on main screen
        if (!(m_tm & 0x02)) {
            fprintf(stderr, "  WARNING: BG2 NOT enabled on main screen (TM bit 1 = 0)!\n");
        }

        // Scan tilemap for non-zero entries
        int nonzero_entries = 0;
        int first_nonzero_tile = -1;
        for (int i = 0; i < 2048; i += 2) {
            uint8_t lo = m_vram[(tilemap_base + i) & 0xFFFF];
            uint8_t hi = m_vram[(tilemap_base + i + 1) & 0xFFFF];
            if (lo != 0 || hi != 0) {
                nonzero_entries++;
                if (first_nonzero_tile < 0) {
                    first_nonzero_tile = i / 2;
                    int tile = lo | ((hi & 0x03) << 8);
                    int pal = (hi >> 2) & 0x07;
                    fprintf(stderr, "  First non-zero tilemap entry at %d: tile=%d pal=%d\n",
                        first_nonzero_tile, tile, pal);
                }
            }
        }
        fprintf(stderr, "  Tilemap: %d/1024 non-zero entries\n", nonzero_entries);

        // Check chr data at chr_base
        int chr_nonzero = 0;
        for (int i = 0; i < 0x2000; i++) {
            if (m_vram[(chr_base + i) & 0xFFFF] != 0) chr_nonzero++;
        }
        fprintf(stderr, "  Chr data at $%04X: %d/8192 non-zero bytes\n", chr_base, chr_nonzero);

        // Check first tile's data
        fprintf(stderr, "  Tile 0 chr data: ");
        for (int i = 0; i < 8; i++) {
            fprintf(stderr, "%02X ", m_vram[(chr_base + i) & 0xFFFF]);
        }
        fprintf(stderr, "\n");

        // Check CGRAM palette 0 (first BG palette)
        fprintf(stderr, "  CGRAM palette 0: ");
        for (int c = 0; c < 16; c++) {
            uint16_t color = m_cgram[c * 2] | (m_cgram[c * 2 + 1] << 8);
            if (color != 0) fprintf(stderr, "[%d]=$%04X ", c, color);
        }
        fprintf(stderr, "\n");
    }

    if (debug_bg) {
        SNES_PPU_DEBUG(">>> render_background_pixel ENTRY: bg=%d x=%d scanline=%d frame=%llu\n",
            bg, x, m_scanline, (unsigned long long)m_frame);
        // Scan entire BG2 tilemap for unique tiles and check their chr data
        uint16_t tilemap_base = m_bg_tilemap_addr[1];
        SNES_PPU_DEBUG("  BG2 Tilemap at $%04X scan - unique tiles:\n", tilemap_base);
        int tile_counts[256] = {0};
        for (int i = 0; i < 2048; i += 2) {  // 2KB tilemap = 1024 entries
            uint8_t lo = m_vram[(tilemap_base + i) & 0xFFFF];
            uint8_t hi = m_vram[(tilemap_base + i + 1) & 0xFFFF];
            int tile = lo | ((hi & 0x03) << 8);
            if (tile < 256) tile_counts[tile]++;
        }
        for (int t = 0; t < 256; t++) {
            if (tile_counts[t] > 0) {
                uint16_t tile_addr = m_bg_chr_addr[1] + t * 32;
                int nonzero = 0;
                for (int j = 0; j < 32; j++) {
                    if (m_vram[(tile_addr + j) & 0xFFFF] != 0) nonzero++;
                }
                SNES_PPU_DEBUG("    Tile %d: %d uses, chr $%04X: %d/32 bytes\n",
                    t, tile_counts[t], tile_addr, nonzero);
            }
        }
    }

    // Get scroll values
    // Note: Scroll registers are 10-bit signed values
    int scroll_x = m_bg_hofs[bg] & 0x3FF;
    int scroll_y = m_bg_vofs[bg] & 0x3FF;

    // ========================================================================
    // OFFSET-PER-TILE (OPT) FOR MODES 2, 4, 6
    // ========================================================================
    // Reference: SNESdev wiki Offset-per-tile, sneslab.net Offset_Change_Mode
    //
    // In Modes 2/4/6, BG3's tilemap is repurposed as an offset table.
    // Each 8-pixel column of BG1/BG2 can have a different scroll offset.
    // The leftmost visible column uses normal scroll values.
    // Columns 1-32 use offsets from BG3 tilemap entries.
    //
    // BG3 offset table format (16-bit entries):
    //   Bits 0-9:  Offset value (same format as scroll register)
    //   Bits 10-12: Unused
    //   Bit 13:    Apply to BG1
    //   Bit 14:    Apply to BG2
    //   Bit 15:    Mode 4 only: 0=horizontal, 1=vertical
    //
    // For Mode 2/6: Two rows - row 0 = H offset, row 1 = V offset
    // For Mode 4:   One row, bit 15 selects H or V
    //
    // Horizontal offset: Replaces upper bits of HOFS, keeps low 3 bits (fine scroll)
    // Vertical offset: Replaces entire VOFS value
    // ========================================================================
    bool opt_mode = (m_bg_mode == 2 || m_bg_mode == 4 || m_bg_mode == 6);
    if (opt_mode && (bg == 0 || bg == 1)) {
        // Calculate screen column (0-32 visible tiles)
        // Column 0 uses normal scroll, columns 1-32 use OPT
        int screen_column = (x + (scroll_x & 7)) >> 3;

        if (screen_column > 0 && screen_column <= 32) {
            // Read from BG3 offset table at position (screen_column - 1)
            // BG3 tilemap base address
            uint16_t bg3_base = m_bg_tilemap_addr[2];
            int bg3_hofs = m_bg_hofs[2] & 0x3FF;
            int bg3_vofs = m_bg_vofs[2] & 0x3FF;

            // BG3 tile size affects column granularity
            int bg3_tile_size = m_bg_tile_size[2] ? 16 : 8;

            // Calculate offset table entry address
            // The offset table is indexed by (screen_column - 1)
            // Each entry is 2 bytes (16-bit)
            int opt_column = screen_column - 1;

            // Calculate BG3 tilemap position based on BG3 scroll and tile size
            // The upper bits of BG3 scroll determine which row of the tilemap we read
            int opt_row = (bg3_vofs / bg3_tile_size) & 0x1F;
            int opt_col = ((bg3_hofs / bg3_tile_size) + opt_column) & 0x1F;

            // Handle 64-wide tilemap
            int screen_offset = 0;
            if (m_bg_tilemap_width[2] && opt_col >= 32) {
                screen_offset = 0x800;
                opt_col -= 32;
            }

            // Read horizontal offset entry (row 0 in Mode 2/6)
            uint16_t h_entry_addr = bg3_base + screen_offset + (opt_row * 32 + opt_col) * 2;
            uint8_t h_lo = m_vram[h_entry_addr & 0xFFFF];
            uint8_t h_hi = m_vram[(h_entry_addr + 1) & 0xFFFF];
            uint16_t h_entry = h_lo | (h_hi << 8);

            // Check if this entry applies to this BG
            bool apply_h = (bg == 0 && (h_entry & 0x2000)) || (bg == 1 && (h_entry & 0x4000));

            if (m_bg_mode == 4) {
                // Mode 4: Single entry, bit 15 determines H or V
                if (!(h_entry & 0x8000) && apply_h) {
                    // Horizontal offset - keep low 3 bits of original HOFS
                    scroll_x = (h_entry & 0x3F8) | (scroll_x & 7);
                } else if ((h_entry & 0x8000) && apply_h) {
                    // Vertical offset - replace entire VOFS
                    scroll_y = h_entry & 0x3FF;
                }
            } else {
                // Mode 2/6: Read H from row 0, V from row 1
                if (apply_h) {
                    // Horizontal offset - keep low 3 bits of original HOFS
                    scroll_x = (h_entry & 0x3F8) | (scroll_x & 7);
                }

                // Read vertical offset entry (row 1 = 32 entries = 64 bytes later)
                uint16_t v_entry_addr = h_entry_addr + 64;
                uint8_t v_lo = m_vram[v_entry_addr & 0xFFFF];
                uint8_t v_hi = m_vram[(v_entry_addr + 1) & 0xFFFF];
                uint16_t v_entry = v_lo | (v_hi << 8);

                bool apply_v = (bg == 0 && (v_entry & 0x2000)) || (bg == 1 && (v_entry & 0x4000));
                if (apply_v) {
                    // Vertical offset - replace entire VOFS
                    scroll_y = v_entry & 0x3FF;
                }
            }
        }
    }

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

    // Get tile size
    // Reference: SNESdev wiki Backgrounds, sneslab.net Mode_5
    //
    // In Modes 5 and 6 (hi-res modes), tiles are ALWAYS 16 pixels wide.
    // The tile size bit ($2105 bits 4-7) only affects the height:
    //   - Bit clear: 16x8 tiles (16 wide, 8 tall)
    //   - Bit set:   16x16 tiles (16 wide, 16 tall)
    //
    // In non-hires modes (0-4, 7), the tile size bit affects both dimensions:
    //   - Bit clear: 8x8 tiles
    //   - Bit set:   16x16 tiles
    bool is_hires_mode = (m_bg_mode == 5 || m_bg_mode == 6);
    int tile_width = is_hires_mode ? 16 : (m_bg_tile_size[bg] ? 16 : 8);
    int tile_height = m_bg_tile_size[bg] ? 16 : 8;

    // Calculate tile coordinates
    int tile_x = px / tile_width;
    int tile_y = py / tile_height;
    int fine_x = px % tile_width;
    int fine_y = py % tile_height;

    // Get tilemap address (pre-calculated from BGnSC register as byte address)
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

    if (debug_bg) {
        SNES_PPU_DEBUG("BG2 render: px=%d py=%d tile_x=%d tile_y=%d fine_x=%d fine_y=%d\n",
            px, py, tile_x, tile_y, fine_x, fine_y);
        SNES_PPU_DEBUG("  tilemap_addr=$%04X tile_lo=%02X tile_hi=%02X -> tile=%d pal=%d pri=%d hflip=%d vflip=%d\n",
            tilemap_addr, tile_lo, tile_hi, tile_num, palette, priority, hflip, vflip);
    }

    // Handle large tiles (composed of multiple 8x8 tiles)
    // Reference: SNESdev wiki Backgrounds
    //
    // For 16-pixel wide tiles (Mode 5/6 always, or 16x16 mode in other modes):
    //   Tiles are arranged horizontally: [N][N+1]
    // For 16-pixel tall tiles (16x16 mode in any mode):
    //   Tiles are arranged vertically in rows of 16
    //
    // Combined for 16x16: [N  ][N+1]
    //                     [N+16][N+17]
    int x_offset = 0;
    int y_offset = 0;

    if (tile_width == 16) {
        x_offset = (fine_x >= 8) ? 1 : 0;
        if (hflip) x_offset = 1 - x_offset;
        fine_x &= 7;
    }

    if (tile_height == 16) {
        y_offset = (fine_y >= 8) ? 16 : 0;
        if (vflip) y_offset = (y_offset == 16) ? 0 : 16;
        fine_y &= 7;
    }

    tile_num += x_offset + y_offset;

    // Apply flip to fine coordinates (within 8x8 sub-tile)
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

    // Get character data address (pre-calculated from BGnNBA register as byte address)
    // Tile size in bytes: 8 rows * bpp bytes per row (bitplanes interleaved)
    uint16_t chr_base = m_bg_chr_addr[bg];
    uint16_t chr_addr = chr_base + tile_num * (bpp * 8);

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
    }

    if (debug_bg) {
        SNES_PPU_DEBUG("  chr_base=$%04X chr_addr=$%04X tile_num=%d bpp=%d\n",
            chr_base, chr_addr, tile_num, bpp);
        SNES_PPU_DEBUG("  VRAM data at chr_addr: %02X %02X %02X %02X %02X %02X %02X %02X\n",
            m_vram[(chr_addr) & 0xFFFF], m_vram[(chr_addr+1) & 0xFFFF],
            m_vram[(chr_addr+2) & 0xFFFF], m_vram[(chr_addr+3) & 0xFFFF],
            m_vram[(chr_addr+4) & 0xFFFF], m_vram[(chr_addr+5) & 0xFFFF],
            m_vram[(chr_addr+6) & 0xFFFF], m_vram[(chr_addr+7) & 0xFFFF]);
        SNES_PPU_DEBUG("  color_index=%d (0x%02X)\n", color_index, color_index);
        // Check for non-zero data in BG2 chr area
        int nonzero = 0;
        for (int i = 0; i < 0x2000; i++) {
            if (m_vram[(chr_base + i) & 0xFFFF] != 0) nonzero++;
        }
        SNES_PPU_DEBUG("  Non-zero bytes in BG2 chr area ($%04X-$%04X): %d\n",
            chr_base, chr_base + 0x1FFF, nonzero);

        // Check key VRAM locations
        SNES_PPU_DEBUG("  VRAM at $08000 (where graphics DMA went): %02X %02X %02X %02X %02X %02X %02X %02X\n",
            m_vram[0x8000], m_vram[0x8001], m_vram[0x8002], m_vram[0x8003],
            m_vram[0x8004], m_vram[0x8005], m_vram[0x8006], m_vram[0x8007]);
        SNES_PPU_DEBUG("  VRAM at $0A000 (BG2 chr area): %02X %02X %02X %02X %02X %02X %02X %02X\n",
            m_vram[0xA000], m_vram[0xA001], m_vram[0xA002], m_vram[0xA003],
            m_vram[0xA004], m_vram[0xA005], m_vram[0xA006], m_vram[0xA007]);
        // Count non-zero at each location
        int nz_8000 = 0, nz_A000 = 0;
        for (int i = 0; i < 0x2000; i++) {
            if (m_vram[(0x8000 + i) & 0xFFFF] != 0) nz_8000++;
            if (m_vram[(0xA000 + i) & 0xFFFF] != 0) nz_A000++;
        }
        SNES_PPU_DEBUG("  Non-zero: $08000-09FFF: %d bytes, $0A000-0BFFF: %d bytes\n", nz_8000, nz_A000);
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
    }
}

void PPU::render_background_pixel(int bg, int x, uint8_t& pixel, uint8_t& priority, uint8_t& out_palette) {
    // Wrapper that also returns the palette from tilemap
    // This is needed for Direct Color mode which uses the palette bits differently
    pixel = 0;
    priority = 0;
    out_palette = 0;

    // Get scroll values
    int scroll_x = m_bg_hofs[bg] & 0x3FF;
    int scroll_y = m_bg_vofs[bg] & 0x3FF;

    // Apply offset-per-tile for Modes 2, 4, 6 (same logic as primary function)
    bool opt_mode = (m_bg_mode == 2 || m_bg_mode == 4 || m_bg_mode == 6);
    if (opt_mode && (bg == 0 || bg == 1)) {
        int screen_column = (x + (scroll_x & 7)) >> 3;
        if (screen_column > 0 && screen_column <= 32) {
            uint16_t bg3_base = m_bg_tilemap_addr[2];
            int bg3_hofs = m_bg_hofs[2] & 0x3FF;
            int bg3_vofs = m_bg_vofs[2] & 0x3FF;
            int bg3_tile_size = m_bg_tile_size[2] ? 16 : 8;
            int opt_column = screen_column - 1;
            int opt_row = (bg3_vofs / bg3_tile_size) & 0x1F;
            int opt_col = ((bg3_hofs / bg3_tile_size) + opt_column) & 0x1F;
            int screen_offset = 0;
            if (m_bg_tilemap_width[2] && opt_col >= 32) {
                screen_offset = 0x800;
                opt_col -= 32;
            }
            uint16_t h_entry_addr = bg3_base + screen_offset + (opt_row * 32 + opt_col) * 2;
            uint8_t h_lo = m_vram[h_entry_addr & 0xFFFF];
            uint8_t h_hi = m_vram[(h_entry_addr + 1) & 0xFFFF];
            uint16_t h_entry = h_lo | (h_hi << 8);
            bool apply_h = (bg == 0 && (h_entry & 0x2000)) || (bg == 1 && (h_entry & 0x4000));
            if (m_bg_mode == 4) {
                if (!(h_entry & 0x8000) && apply_h) {
                    scroll_x = (h_entry & 0x3F8) | (scroll_x & 7);
                } else if ((h_entry & 0x8000) && apply_h) {
                    scroll_y = h_entry & 0x3FF;
                }
            } else {
                if (apply_h) {
                    scroll_x = (h_entry & 0x3F8) | (scroll_x & 7);
                }
                uint16_t v_entry_addr = h_entry_addr + 64;
                uint8_t v_lo = m_vram[v_entry_addr & 0xFFFF];
                uint8_t v_hi = m_vram[(v_entry_addr + 1) & 0xFFFF];
                uint16_t v_entry = v_lo | (v_hi << 8);
                bool apply_v = (bg == 0 && (v_entry & 0x2000)) || (bg == 1 && (v_entry & 0x4000));
                if (apply_v) {
                    scroll_y = v_entry & 0x3FF;
                }
            }
        }
    }

    // Apply mosaic
    int mosaic_x = x;
    int mosaic_y = m_scanline - 1;
    if (m_mosaic_enabled[bg] && m_mosaic_size > 1) {
        mosaic_x = (mosaic_x / m_mosaic_size) * m_mosaic_size;
        mosaic_y = (mosaic_y / m_mosaic_size) * m_mosaic_size;
    }

    // Calculate pixel position in BG
    int px = (mosaic_x + scroll_x) & 0x3FF;
    int py = (mosaic_y + scroll_y) & 0x3FF;

    // Get tile size (see main render_background_pixel for detailed comments)
    bool is_hires_mode = (m_bg_mode == 5 || m_bg_mode == 6);
    int tile_width = is_hires_mode ? 16 : (m_bg_tile_size[bg] ? 16 : 8);
    int tile_height = m_bg_tile_size[bg] ? 16 : 8;

    // Calculate tile coordinates
    int tile_x = px / tile_width;
    int tile_y = py / tile_height;
    int fine_x = px % tile_width;
    int fine_y = py % tile_height;

    // Get tilemap address
    uint16_t tilemap_base = m_bg_tilemap_addr[bg];
    int tilemap_width = m_bg_tilemap_width[bg] ? 64 : 32;
    int tilemap_height = m_bg_tilemap_height[bg] ? 64 : 32;

    // Handle tilemap wrapping
    int tilemap_x = tile_x % tilemap_width;
    int tilemap_y = tile_y % tilemap_height;

    // Calculate screen offset for 64-wide/tall tilemaps
    int screen_offset = 0;
    if (tilemap_width == 64 && tilemap_x >= 32) {
        screen_offset += 0x800;
        tilemap_x -= 32;
    }
    if (tilemap_height == 64 && tilemap_y >= 32) {
        screen_offset += tilemap_width == 64 ? 0x1000 : 0x800;
        tilemap_y -= 32;
    }

    // Get tilemap entry
    uint16_t tilemap_addr = tilemap_base + screen_offset + (tilemap_y * 32 + tilemap_x) * 2;
    uint8_t tile_lo = m_vram[tilemap_addr & 0xFFFF];
    uint8_t tile_hi = m_vram[(tilemap_addr + 1) & 0xFFFF];

    int tile_num = tile_lo | ((tile_hi & 0x03) << 8);
    int palette = (tile_hi >> 2) & 0x07;
    priority = (tile_hi >> 5) & 0x01;
    bool hflip = (tile_hi & 0x40) != 0;
    bool vflip = (tile_hi & 0x80) != 0;

    // Output the palette for Direct Color mode
    out_palette = palette;

    // Handle large tiles (see main render_background_pixel for detailed comments)
    int x_offset = 0;
    int y_offset = 0;

    if (tile_width == 16) {
        x_offset = (fine_x >= 8) ? 1 : 0;
        if (hflip) x_offset = 1 - x_offset;
        fine_x &= 7;
    }

    if (tile_height == 16) {
        y_offset = (fine_y >= 8) ? 16 : 0;
        if (vflip) y_offset = (y_offset == 16) ? 0 : 16;
        fine_y &= 7;
    }

    tile_num += x_offset + y_offset;

    // Apply flip to fine coordinates (within 8x8 sub-tile)
    if (hflip) fine_x = 7 - fine_x;
    if (vflip) fine_y = 7 - fine_y;

    // Get bits per pixel
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
    uint16_t chr_base = m_bg_chr_addr[bg];
    uint16_t chr_addr = chr_base + tile_num * (bpp * 8);

    // Read tile data
    uint8_t color_index = 0;
    for (int bit = 0; bit < bpp; bit++) {
        int plane_offset = (bit / 2) * 16 + (bit & 1);
        uint16_t addr = chr_addr + fine_y * 2 + plane_offset;
        uint8_t plane = m_vram[addr & 0xFFFF];
        if (plane & (0x80 >> fine_x)) {
            color_index |= (1 << bit);
        }
    }

    // For 8bpp, return raw color index (for Direct Color mode)
    // For other BPP, compute full CGRAM index
    if (color_index != 0) {
        if (bpp == 8) {
            pixel = color_index;
        } else if (bpp == 2) {
            int bg_offset = (m_bg_mode == 0) ? (bg * 32) : 0;
            pixel = bg_offset + (palette << 2) + color_index;
        } else {
            pixel = (palette << 4) + color_index;
        }
    }
}

// Hi-res mode background pixel rendering (Mode 5/6)
// ============================================================================
// Reference: bsnes/ares background.cpp fetchNameTable()
//
// In Mode 5/6, tiles are always 16 pixels wide in hi-res coordinate space.
// The 16 pixels are split between main and sub screens:
//   - Even pixels (0,2,4,6,8,10,12,14) go to sub screen
//   - Odd pixels (1,3,5,7,9,11,13,15) go to main screen
//
// The output to the TV interleaves: column 0=sub, column 1=main, column 2=sub, etc.
//
// Scroll handling in hi-res modes (from bsnes):
//   hpixel = x << hires()      // screen x is doubled
//   hscroll = io.hoffset
//   if(hires()) hscroll <<= 1  // scroll is also doubled
//   hoffset = hpixel + hscroll
//
// This means BOTH the pixel position AND the scroll value are doubled.
// ============================================================================
void PPU::render_background_pixel(int bg, int x, uint8_t& pixel, uint8_t& priority,
                                   bool hires_mode, bool hires_odd_pixel) {
    pixel = 0;
    priority = 0;

    // Get scroll values
    // Reference: bsnes - in hi-res mode, scroll is doubled (hscroll <<= 1)
    int scroll_x = m_bg_hofs[bg] & 0x3FF;
    int scroll_y = m_bg_vofs[bg] & 0x3FF;

    // In hi-res mode, scroll is doubled to match the doubled coordinate space
    if (hires_mode) {
        scroll_x <<= 1;
    }

    // Apply mosaic
    int mosaic_x = x;
    int mosaic_y = m_scanline - 1;
    if (m_mosaic_enabled[bg] && m_mosaic_size > 1) {
        mosaic_x = (mosaic_x / m_mosaic_size) * m_mosaic_size;
        mosaic_y = (mosaic_y / m_mosaic_size) * m_mosaic_size;
    }

    // In hi-res mode (Mode 5/6), each screen X (0-255) maps to 2 hi-res pixels (0-511)
    // Main screen gets odd pixels (1,3,5...), sub screen gets even pixels (0,2,4...)
    // So screen X=0 -> hires 0/1, screen X=1 -> hires 2/3, etc.
    // Reference: bsnes - hpixel = x << hires() (i.e., x * 2 when in hi-res)
    int hires_x = mosaic_x;
    if (hires_mode) {
        // Convert to 512-pixel hi-res space
        // Add 1 for odd pixel (main screen), 0 for even pixel (sub screen)
        hires_x = mosaic_x * 2 + (hires_odd_pixel ? 1 : 0);
    }

    // Calculate pixel position in BG (now in correct coordinate space)
    // Both hires_x and scroll_x are in the doubled coordinate space for Mode 5/6
    int px = (hires_x + scroll_x) & 0x3FF;
    int py = (mosaic_y + scroll_y) & 0x3FF;

    // In hi-res mode (Mode 5/6), tiles are always 16 pixels wide
    int tile_width = 16;  // Always 16 in hi-res modes
    int tile_height = m_bg_tile_size[bg] ? 16 : 8;

    // Calculate tile coordinates
    int tile_x = px / tile_width;
    int tile_y = py / tile_height;
    int fine_x = px % tile_width;
    int fine_y = py % tile_height;

    // Get tilemap address
    uint16_t tilemap_base = m_bg_tilemap_addr[bg];
    int tilemap_width = m_bg_tilemap_width[bg] ? 64 : 32;
    int tilemap_height = m_bg_tilemap_height[bg] ? 64 : 32;

    // Handle tilemap wrapping
    int tilemap_x = tile_x % tilemap_width;
    int tilemap_y = tile_y % tilemap_height;

    // Calculate screen offset for 64-wide/tall tilemaps
    int screen_offset = 0;
    if (tilemap_width == 64 && tilemap_x >= 32) {
        screen_offset += 0x800;
        tilemap_x -= 32;
    }
    if (tilemap_height == 64 && tilemap_y >= 32) {
        screen_offset += tilemap_width == 64 ? 0x1000 : 0x800;
        tilemap_y -= 32;
    }

    // Get tilemap entry
    uint16_t tilemap_addr = tilemap_base + screen_offset + (tilemap_y * 32 + tilemap_x) * 2;
    uint8_t tile_lo = m_vram[tilemap_addr & 0xFFFF];
    uint8_t tile_hi = m_vram[(tilemap_addr + 1) & 0xFFFF];

    int tile_num = tile_lo | ((tile_hi & 0x03) << 8);
    int palette = (tile_hi >> 2) & 0x07;
    priority = (tile_hi >> 5) & 0x01;
    bool hflip = (tile_hi & 0x40) != 0;
    bool vflip = (tile_hi & 0x80) != 0;

    // Handle large tiles (16-pixel wide tiles are composed of two 8x8 tiles)
    int x_offset = 0;
    int y_offset = 0;

    if (tile_width == 16) {
        x_offset = (fine_x >= 8) ? 1 : 0;
        if (hflip) x_offset = 1 - x_offset;
        fine_x &= 7;
    }

    if (tile_height == 16) {
        y_offset = (fine_y >= 8) ? 16 : 0;
        if (vflip) y_offset = (y_offset == 16) ? 0 : 16;
        fine_y &= 7;
    }

    tile_num += x_offset + y_offset;

    // Apply flip to fine coordinates (within 8x8 sub-tile)
    if (hflip) fine_x = 7 - fine_x;
    if (vflip) fine_y = 7 - fine_y;

    // Mode 5/6: BG1 is 4bpp, BG2 is 2bpp
    int bpp = (bg == 0) ? 4 : 2;

    // Get character data address
    uint16_t chr_base = m_bg_chr_addr[bg];
    uint16_t chr_addr = chr_base + tile_num * (bpp * 8);

    // Read tile data using SNES bitplane format
    uint8_t color_index = 0;
    for (int bit = 0; bit < bpp; bit++) {
        int plane_offset = (bit / 2) * 16 + (bit & 1);
        uint16_t addr = chr_addr + fine_y * 2 + plane_offset;
        uint8_t plane = m_vram[addr & 0xFFFF];
        if (plane & (0x80 >> fine_x)) {
            color_index |= (1 << bit);
        }
    }

    // Compute CGRAM index
    // BG1 (4bpp): 16 colors per palette
    // BG2 (2bpp): 4 colors per palette
    if (color_index != 0) {
        if (bg == 0) {
            // 4bpp: palette * 16 + color_index
            pixel = (palette << 4) + color_index;
        } else {
            // 2bpp: palette * 4 + color_index
            pixel = (palette << 2) + color_index;
        }
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

    // ========================================================================
    // MODE 7 TRANSFORMATION
    // ========================================================================
    // Reference: fullsnes Mode 7, bsnes/sfc/ppu/mode7.cpp
    //
    // The hardware formula is:
    //   X = A*(ScreenX + HOFS - CenterX) + B*(ScreenY + VOFS - CenterY) + CenterX
    //   Y = C*(ScreenX + HOFS - CenterX) + D*(ScreenY + VOFS - CenterY) + CenterY
    //
    // Where A/B/C/D are 16-bit signed (1.7.8 fixed point)
    // HOFS/VOFS are 13-bit signed values
    // CenterX/CenterY (M7X/M7Y) are 13-bit signed values
    //
    // The result is 10-bit coordinates (0-1023 range for 128x8 = 1024 pixel space)
    // ========================================================================

    // Sign-extend 13-bit values to 32-bit
    // M7HOFS/M7VOFS/M7X/M7Y are stored as 16-bit but only 13 bits are significant
    int32_t hofs = (static_cast<int16_t>(m_m7hofs << 3)) >> 3;  // Sign extend from bit 12
    int32_t vofs = (static_cast<int16_t>(m_m7vofs << 3)) >> 3;
    int32_t cx = (static_cast<int16_t>(m_m7x << 3)) >> 3;
    int32_t cy = (static_cast<int16_t>(m_m7y << 3)) >> 3;

    // Calculate input coordinates (screen position + scroll - center)
    int32_t px = screen_x + hofs - cx;
    int32_t py = screen_y + vofs - cy;

    // Apply matrix transformation
    // A/B/C/D are 16-bit signed with 8 fractional bits (1.7.8 format)
    // Multiply, then add center (in 8.8 format), then shift down
    int32_t tx = ((static_cast<int16_t>(m_m7a) * px) +
                  (static_cast<int16_t>(m_m7b) * py) + (cx << 8)) >> 8;
    int32_t ty = ((static_cast<int16_t>(m_m7c) * px) +
                  (static_cast<int16_t>(m_m7d) * py) + (cy << 8)) >> 8;

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

    bool debug_render = is_debug_mode() && m_frame == 200 && m_scanline == 86 && (x == 96 || x == 128);

    if (debug_render) {
        SNES_PPU_DEBUG("render_sprite_pixel x=%d: tile_count=%d\n", x, m_sprite_tile_count);
        for (int j = 0; j < m_sprite_tile_count; j++) {
            SNES_PPU_DEBUG("  m_sprite_tiles[%d].x = %d\n", j, m_sprite_tiles[j].x);
        }
    }

    // Search through sprite tiles for this X position
    // Tiles were added in reverse OAM order (high index first), so we search
    // from the END to find the lowest OAM index (highest sprite priority) first.
    // On SNES, lower OAM index = higher priority (sprite 0 appears on top).
    for (int i = m_sprite_tile_count - 1; i >= 0; i--) {
        const auto& tile = m_sprite_tiles[i];

        if (debug_render && i == 6) {
            SNES_PPU_DEBUG("  checking i=6: tile.x=%d, condition=%d\n",
                tile.x, (x >= tile.x && x < tile.x + 8) ? 1 : 0);
        }

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

            if (debug_render) {
                SNES_PPU_DEBUG("  tile[%d] x=%d fine=%d mask=%02X planes=[%02X,%02X,%02X,%02X] -> color=%d\n",
                    i, tile.x, fine_x, mask, tile.planes[0], tile.planes[1], tile.planes[2], tile.planes[3], color_index);
            }

            // Color index 0 is transparent for sprites
            if (color_index != 0) {
                // Sprite colors use CGRAM 128-255 (second half of palette)
                // 8 palettes of 16 colors each
                pixel = 128 + tile.palette * 16 + color_index;
                priority = tile.priority;
                is_palette_4_7 = tile.palette >= 4;

                if (debug_render) {
                    uint16_t cgram_color = m_cgram[pixel * 2] | (m_cgram[pixel * 2 + 1] << 8);
                    SNES_PPU_DEBUG("  -> pixel=%d (pal=%d idx=%d) cgram=$%04X\n",
                        pixel, tile.palette, color_index, cgram_color);
                }

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

    // Force blank prevents sprite caching - sprites should not be loaded
    // when force blank is active. This is important for games that rely on
    // enabling force blank during HBlank to clear sprites.
    if (m_force_blank) {
        return;
    }

    int screen_y = m_scanline - 1;

    // Debug: log sprite evaluation on specific frames/scanlines
    bool debug_sprites = false;  // Disabled for normal operation

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
        // SNES hardware: sprites appear one scanline LATER than their OAM Y value.
        // bsnes handles this by storing (y + 1) internally during OAM writes.
        // However, since we use a 0-based scanline system (screen_y = 0 is the first
        // visible line), we should NOT apply the +1 offset here.
        //
        // The hardware's "one line late" behavior is accounted for by how scanlines
        // map to the visible screen:
        // - Hardware vcounter 1 is the first visible line
        // - Our screen_y 0 is the first visible line
        // - A sprite at OAM Y=0 should appear on screen_y=0 (our first visible line)
        //
        // Reference: SNESdev Wiki - "sprites appear 1 line lower than their Y value,
        // however because the first line of rendering is always hidden on SNES, a
        // sprite with Y=0 will appear to begin on the first visible line."
        int sprite_y = y;
        int offset_y = (screen_y - sprite_y) & 0xFF;  // 8-bit wrap

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

        if (debug_sprites && m_sprite_count < 5) {
            SNES_PPU_DEBUG("Sprite[%d]: x=%d y=%d tile=$%03X pal=%d pri=%d size=%dx%d\n",
                i, x, sprite_y, entry.tile, entry.palette, entry.priority, width, height);
        }

        m_sprite_buffer[m_sprite_count++] = entry;
    }

    if (m_sprite_count > 32) {
        m_sprite_count = 32;
        m_range_over = true;
    }

    // Generate sprite tiles for this scanline
    // Sprites are 4bpp (16 colors) using second half of CGRAM (palettes 0-7 = colors 128-255)
    // Reference: bsnes/sfc/ppu-fast/object.cpp renderObject()

    if (debug_sprites) {
        SNES_PPU_DEBUG("Sprite eval: OBSEL=$%02X base=$%04X namesel=%d sprites=%d\n",
            m_obsel, m_obj_base_addr, m_obj_name_select, m_sprite_count);
    }


    // Process sprites in reverse order (lowest priority first, so higher priority overwrites)
    for (int i = m_sprite_count - 1; i >= 0 && m_sprite_tile_count < 34; i--) {
        const auto& sprite = m_sprite_buffer[i];

        // Calculate Y offset within the sprite (same formula as evaluation)
        int y = (screen_y - sprite.y) & 0xFF;
        if (sprite.vflip) y = sprite.height - 1 - y;

        int tiles_wide = sprite.width / 8;
        for (int tx = 0; tx < tiles_wide && m_sprite_tile_count < 34; tx++) {
            int screen_x = sprite.x + tx * 8;

            // Skip off-screen tiles
            if (screen_x >= 256 || screen_x <= -8) continue;

            // Reference: bsnes object.cpp
            // uint mirrorX = !object.hflip ? tileX : tileWidth - 1 - tileX;
            int mirrorX = sprite.hflip ? (tiles_wide - 1 - tx) : tx;

            // Calculate tile address using bsnes formula (word addresses throughout)
            // characterX = (object.character & 15) - lower 4 bits of tile number
            // characterY = ((object.character >> 4) + (y >> 3) & 15) << 4 - upper 4 bits + row offset
            uint16_t characterX = sprite.tile & 0x0F;
            uint16_t characterY = (((sprite.tile >> 4) + (y >> 3)) & 0x0F) << 4;

            // tiledataAddress is base word address from OBSEL
            // If nameselect bit (bit 8 of tile number) is set, add ((nameselect + 1) << 12)
            uint16_t tiledataAddress = m_obj_base_addr;  // Word address
            bool nameSelectBit = (sprite.tile & 0x100) != 0;
            if (nameSelectBit) {
                tiledataAddress += (1 + m_obj_name_select) << 12;  // Word address offset
            }

            // address = tiledataAddress + ((characterY + (characterX + mirrorX & 15)) << 4)
            // Then: address = (address & 0xfff0) + (y & 7)
            // Reference: bsnes uses 0xfff0 mask, not 0x7ff0
            uint16_t address = tiledataAddress + ((characterY + ((characterX + mirrorX) & 0x0F)) << 4);
            address = (address & 0xFFF0) + (y & 7);

            // Now 'address' is a word address. For byte access: address * 2
            // bsnes reads: tile.data = ppu.vram[address + 0] | (ppu.vram[address + 8] << 16)
            // ppu.vram is word-indexed in bsnes, so vram[address] and vram[address+8]
            // In our byte-indexed VRAM: address*2 and (address+8)*2

            uint32_t byte_addr = (address * 2) & 0xFFFF;

            SpriteTile tile_entry;
            tile_entry.x = screen_x;
            // Read 4bpp tile data (2 words = 4 bytes for planes 0-3 at this row)
            // bsnes: tile.data = vram[address + 0] << 0 | vram[address + 8] << 16
            // vram entries are 16-bit words containing planes 0+1 and planes 2+3
            // Low word at address+0: plane0 (low byte) + plane1 (high byte)
            // High word at address+8: plane2 (low byte) + plane3 (high byte)
            tile_entry.planes[0] = m_vram[byte_addr];               // Plane 0
            tile_entry.planes[1] = m_vram[(byte_addr + 1) & 0xFFFF]; // Plane 1
            tile_entry.planes[2] = m_vram[(byte_addr + 16) & 0xFFFF]; // Plane 2 (word offset 8 = byte offset 16)
            tile_entry.planes[3] = m_vram[(byte_addr + 17) & 0xFFFF]; // Plane 3
            tile_entry.palette = sprite.palette;
            tile_entry.priority = sprite.priority;
            tile_entry.hflip = sprite.hflip;

            if (debug_sprites && m_sprite_tile_count < 8) {
                SNES_PPU_DEBUG("  Tile[%d]: spr=%d x=%d tile=$%03X addr=$%04X byte=$%04X planes=[%02X,%02X,%02X,%02X]\n",
                    m_sprite_tile_count, i, screen_x, sprite.tile, address, byte_addr,
                    tile_entry.planes[0], tile_entry.planes[1], tile_entry.planes[2], tile_entry.planes[3]);
            }

            m_sprite_tiles[m_sprite_tile_count++] = tile_entry;
        }
    }

    // Set time over flag if we exceeded 34 tiles
    if (m_sprite_tile_count > 34) {
        m_sprite_tile_count = 34;
        m_time_over = true;
    }

    // Debug: log sprite counts after evaluation
    if (debug_sprites) {
        SNES_PPU_DEBUG("Sprite totals: sprites=%d tiles=%d range_over=%d time_over=%d\n",
            m_sprite_count, m_sprite_tile_count, m_range_over ? 1 : 0, m_time_over ? 1 : 0);
    }
}

uint16_t PPU::get_color(uint8_t palette, uint8_t index, bool sprite) {
    // ============================================================================
    // CGRAM COLOR LOOKUP
    // ============================================================================
    // Reference: fullsnes CGRAM, bsnes/sfc/ppu/screen.cpp
    //
    // For BG layers, the caller has already computed the full CGRAM index:
    // - 2bpp: palette * 4 + color_index (Mode 0 offsets by BG number)
    // - 4bpp: palette * 16 + color_index
    // - 8bpp: direct color_index (0-255)
    //
    // For sprites, index = 128 + palette * 16 + color_index
    // ============================================================================

    if (index == 0 && !sprite) {
        // Transparent - use backdrop (CGRAM index 0)
        return m_cgram[0] | (m_cgram[1] << 8);
    }

    // Convert color index to byte address (each color is 2 bytes)
    // CGRAM is 512 bytes = 256 colors (indices 0-255)
    uint16_t addr = (index & 0xFF) * 2;
    uint16_t color = m_cgram[addr] | (m_cgram[addr + 1] << 8);

    // Mask to 15 bits (bit 15 is always 0 in CGRAM)
    return color & 0x7FFF;
}

uint16_t PPU::get_direct_color(uint8_t palette, uint8_t color_index) {
    // ============================================================================
    // DIRECT COLOR MODE
    // ============================================================================
    // Reference: fullsnes, SNESdev wiki Direct Color, bsnes/sfc/ppu/screen.cpp
    //
    // Direct Color is used in Modes 3, 4, and 7 when CGWSEL bit 0 is set.
    // Instead of using the 8-bit color index as a CGRAM lookup, the bits are
    // used directly to form a 15-bit BGR555 color.
    //
    // For 8bpp BG pixels in Modes 3/4:
    //   Color index format: BBGGGRRR (8 bits)
    //   Palette from tilemap: ppp (3 bits, normally unused for 8bpp)
    //
    // Output 15-bit color:
    //   Red   = RRR r 0  (R from index bits 0-2, r from palette bit 0)
    //   Green = GGG g 0  (G from index bits 3-5, g from palette bit 1)
    //   Blue  = BB p 0 0 (B from index bits 6-7, p from palette bit 2)
    //
    // For Mode 7 (no palette bits):
    //   Red   = RRR 0 0
    //   Green = GGG 0 0
    //   Blue  = BB 0 0 0
    //
    // Note: Color index 0 is still transparent (not black)
    // ============================================================================

    if (color_index == 0) {
        // Transparent - return backdrop color
        return m_cgram[0] | (m_cgram[1] << 8);
    }

    // Extract RGB components from the 8-bit color index
    // Index format: BBGGGRRR
    int r_base = color_index & 0x07;        // Bits 0-2 -> R2 R1 R0
    int g_base = (color_index >> 3) & 0x07; // Bits 3-5 -> G2 G1 G0
    int b_base = (color_index >> 6) & 0x03; // Bits 6-7 -> B1 B0

    // Expand to 5-bit values using palette bits
    // Palette format: ppp where p2 -> blue, p1 -> green, p0 -> red
    int r = (r_base << 2) | ((palette & 0x01) << 1);  // RRR r 0
    int g = (g_base << 2) | ((palette & 0x02));       // GGG g 0
    int b = (b_base << 3) | ((palette & 0x04) << 1);  // BB p 0 0

    // Combine into 15-bit BGR555 color
    return (b << 10) | (g << 5) | r;
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
            // Bit 7 toggles every frame (even/odd field) regardless of interlace mode
            // This is used by timing tests to synchronize to frame boundaries
            value = (m_ppu2_open_bus & 0x20) |
                    (m_hv_latch ? 0x40 : 0) |
                    ((m_frame & 1) ? 0x80 : 0) |
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
    // ========================================================================
    // CATCH-UP RENDERING: SYNC BEFORE REGISTER WRITE
    // ========================================================================
    // Before modifying any PPU register, we must render all pixels up to the
    // current dot position. This ensures that pixels rendered before this
    // write use the OLD register values, while pixels rendered after use NEW.
    //
    // This is critical for mid-scanline effects like:
    // - Changing INIDISP (force_blank/brightness) mid-scanline
    // - HDMA-driven color/scroll changes
    // - Raster effects that modify registers during active display
    //
    // Reference: Mesen-S, bsnes - both sync PPU state before register writes
    // ========================================================================
    sync_to_current();

    // Debug key PPU registers
    if (address == 0x2100 || address == 0x2105 || address == 0x212C || address == 0x212D) {
        SNES_PPU_DEBUG("Write $%04X = $%02X (INIDISP=%02X force_blank=%d bright=%d mode=%d TM=%02X)\n",
            address, value, m_inidisp, m_force_blank ? 1 : 0, m_brightness, m_bg_mode, m_tm);
    }

    switch (address) {
        case 0x2100: {  // INIDISP
            m_inidisp = value;
            bool old_force_blank = m_force_blank;
            m_force_blank = (value & 0x80) != 0;
            m_brightness = value & 0x0F;

            // ================================================================
            // HBlank Force Blank Detection for Sprite Tile Fetch
            // ================================================================
            // Reference: Mesen-S, HblankEmuTest
            //
            // If force_blank is enabled during the sprite tile fetch window
            // (approximately H=274-339), sprite tiles are NOT fetched.
            // Games like HblankEmuTest use H-IRQ to briefly enable force_blank
            // during H-blank to suppress sprite rendering.
            //
            // We track this by setting m_force_blank_latched_fetch = true
            // whenever force_blank transitions to ON during the fetch window.
            // ================================================================
            // ================================================================
            // Track force_blank for sprite tile fetch timing
            // ================================================================
            // HblankEmuTest briefly enables force_blank during H-IRQ on every
            // scanline. Due to CPU timing drift, the exact dot position varies.
            // We track when force_blank was last enabled and check if it was
            // "recent" when deciding whether to block sprite tile fetches.
            // ================================================================
            int visible_lines = m_overscan ? 239 : 224;

            // Track when force_blank transitions to ON during visible scanlines
            if (m_force_blank && !old_force_blank &&
                m_scanline >= 0 && m_scanline < visible_lines) {

                // Record the cycle when force_blank was enabled
                m_force_blank_on_cycle = m_total_ppu_cycles;
            }
            break;
        }

        case 0x2101:  // OBSEL
            m_obsel = value;
            // Reference: bsnes/ares - io.obj.tiledataAddress = data.bit(0,2) << 13
            // Bits 0-2 specify word address base: 0x0000, 0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xE000
            // Note: Addresses above 0x7FFF will wrap since VRAM is 32K words
            m_obj_base_addr = (value & 0x07) << 13;  // Word address
            // Bits 3-4: Name select value (0-3), used in address calculation
            m_obj_name_select = (value >> 3) & 0x03;
            SNES_PPU_DEBUG("OBSEL=$%02X base=$%04X namesel=%d size=%d\n",
                value, m_obj_base_addr, m_obj_name_select, (value >> 5) & 0x07);
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

        case 0x2104: {  // OAMDATA
            // OAM is 544 bytes: 512 bytes for 128 sprites (4 bytes each) + 32 high bytes
            // Reference: bsnes/ares io.cpp OAMDATA handler
            // Address increments after EVERY write, latch bit is address bit 0
            bool latch_bit = (m_oam_addr & 1) != 0;
            uint16_t address = m_oam_addr;
            m_oam_addr = (m_oam_addr + 1) & 0x3FF;  // Increment BEFORE the write logic

            if ((address & 0x200) != 0) {
                // High OAM (addresses 512-543): direct byte writes, bypass latch
                m_oam[0x200 + (address & 0x1F)] = value;
            } else {
                // Low OAM (addresses 0-511): word-based writes
                if (!latch_bit) {
                    // Even address: just latch the byte
                    m_oam_latch = value;
                } else {
                    // Odd address: write both bytes to the word-aligned address
                    uint16_t word_addr = address & 0x1FE;
                    m_oam[word_addr] = m_oam_latch;      // Low byte (latched)
                    m_oam[word_addr + 1] = value;        // High byte (current)
                }
            }
            break;
        }

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
            // Tilemap screen base address in VRAM
            // Reference: bsnes - io.bg1.screenAddress = data << 8 & 0x7c00 (word address)
            // Register format: aaaaaass (bits 2-6 = address bits 11-15 of word address)
            // Word address = (value & 0xFC) << 8 & 0x7C00 = (value & 0x7C) << 8
            // Byte address = word_addr * 2 = (value & 0x7C) << 9
            // This gives 2KB-aligned byte addresses from 0x0000 to 0xF800
            m_bg_tilemap_addr[bg] = (value & 0x7C) << 9;  // Byte address (0x0000-0xF800)
            m_bg_tilemap_width[bg] = (value & 0x01) ? 1 : 0;
            m_bg_tilemap_height[bg] = (value & 0x02) ? 1 : 0;
            SNES_PPU_DEBUG("BG%dSC=$%02X -> tilemap=$%04X size=%dx%d\n",
                bg + 1, value, m_bg_tilemap_addr[bg],
                m_bg_tilemap_width[bg] ? 64 : 32, m_bg_tilemap_height[bg] ? 64 : 32);
            break;
        }

        case 0x210B:  // BG12NBA
            // Character base address in VRAM
            // Reference: bsnes - io.bg1.tiledataAddress = data << 12 & 0x7000 (word address)
            // Only bits 0-2 (BG1) and 4-6 (BG2) are used, giving 8KB-aligned addresses
            // Convert word address to byte address by shifting left 1 additional bit
            // BG1: bits 0-2 -> word address bits 12-14 -> byte address = (value & 0x07) << 13
            // BG2: bits 4-6 -> word address bits 12-14 -> byte address = ((value >> 4) & 0x07) << 13
            m_bg_chr_addr[0] = (value & 0x07) << 13;  // Byte address (0x0000-0xE000)
            m_bg_chr_addr[1] = ((value >> 4) & 0x07) << 13;  // Byte address (0x0000-0xE000)
            {
                static int bg12nba_write_count = 0;
                static uint8_t last_bg12nba = 0xFF;
                bg12nba_write_count++;
                // Log first few writes and any changes
                if (is_debug_mode() && (bg12nba_write_count <= 10 || value != last_bg12nba)) {
                    SNES_DEBUG_PRINT("BG12NBA write #%d: $%02X -> BG1=$%04X BG2=$%04X (frame %lu)\n",
                        bg12nba_write_count, value, m_bg_chr_addr[0], m_bg_chr_addr[1], m_frame);
                    last_bg12nba = value;
                }
            }
            SNES_PPU_DEBUG("BG12NBA=$%02X -> BG1 chr=$%04X, BG2 chr=$%04X\n",
                value, m_bg_chr_addr[0], m_bg_chr_addr[1]);
            break;

        case 0x210C:  // BG34NBA
            // Same format as BG12NBA
            m_bg_chr_addr[2] = (value & 0x07) << 13;  // Byte address (0x0000-0xE000)
            m_bg_chr_addr[3] = ((value >> 4) & 0x07) << 13;  // Byte address (0x0000-0xE000)
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
                // Debug: track CPU writes to $A000-$BFFF region
                if (is_debug_mode() && byte_addr >= 0xA000 && byte_addr < 0xC000 && value != 0) {
                    static int cpu_vram_a000_writes = 0;
                    cpu_vram_a000_writes++;
                    if (cpu_vram_a000_writes <= 20) {
                        SNES_DEBUG_PRINT("CPU VRAM write (low): byte $%04X = $%02X (word_addr=$%04X, frame %lu)\n",
                            byte_addr, value, m_vram_addr, m_frame);
                    }
                }
                m_vram[byte_addr] = value;
                if (!m_vram_increment_high) {
                    m_vram_addr += m_vram_increment;
                }
            }
            break;

        case 0x2119:  // VMDATAH
            {
                uint16_t addr = remap_vram_address(m_vram_addr);
                uint32_t byte_addr = (addr * 2 + 1) & 0xFFFF;
                // Debug: track CPU writes to $A000-$BFFF region
                if (is_debug_mode() && byte_addr >= 0xA000 && byte_addr < 0xC000 && value != 0) {
                    static int cpu_vram_a000_writes_h = 0;
                    cpu_vram_a000_writes_h++;
                    if (cpu_vram_a000_writes_h <= 20) {
                        SNES_DEBUG_PRINT("CPU VRAM write (high): byte $%04X = $%02X (word_addr=$%04X, frame %lu)\n",
                            byte_addr, value, m_vram_addr, m_frame);
                    }
                }
                m_vram[byte_addr] = value;
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

        case 0x2122: {  // CGDATA - Palette data write
            // CGRAM writes use a double-byte buffer
            // First write: store low byte in latch
            // Second write: combine with latch and write 15-bit color to CGRAM
            if (!m_cgram_high_byte) {
                m_cgram_latch = value;
            } else {
                // CGRAM stores 15-bit BGR colors (5:5:5 format)
                m_cgram[m_cgram_addr * 2] = m_cgram_latch;
                m_cgram[m_cgram_addr * 2 + 1] = value & 0x7F;  // Bit 7 ignored

                // Debug: Log CGRAM writes during transition frames
                if (is_debug_mode() && m_frame >= 255 && m_frame <= 275 && m_cgram_addr < 16) {
                    uint16_t color = m_cgram_latch | ((value & 0x7F) << 8);
                    fprintf(stderr, "[SNES/PPU] F%lu CGRAM[%d]=$%04X\n",
                        m_frame, m_cgram_addr, color);
                }

                m_cgram_addr = (m_cgram_addr + 1) & 0xFF;
            }
            m_cgram_high_byte = !m_cgram_high_byte;
            break;
        }

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
            SNES_PPU_DEBUG("CGWSEL=$%02X direct=%d sub_bg_obj=%d prevent=%d clip=%d\n",
                value, m_direct_color, m_sub_screen_bg_obj, m_color_math_prevent, m_color_math_clip);
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
            SNES_PPU_DEBUG("CGADSUB=$%02X BG=%d%d%d%d OBJ=%d BACK=%d half=%d add=%d\n",
                value, m_bg_color_math[0], m_bg_color_math[1], m_bg_color_math[2], m_bg_color_math[3],
                m_obj_color_math, m_backdrop_color_math, m_color_math_half, m_color_math_add);
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
            m_extbg = (value & 0x40) != 0;  // Bit 6: Mode 7 EXTBG
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
    // CGRAM write handler - uses same double-byte latch as register $2122
    // This function exists for potential direct DMA access but currently
    // DMA goes through the bus which routes to ppu->write($2122, value).
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
    std::memcpy(m_cgram.data(), data, m_cgram.size());
    data += m_cgram.size(); remaining -= m_cgram.size();

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
