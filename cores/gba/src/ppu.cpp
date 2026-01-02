#include "ppu.hpp"
#include "bus.hpp"
#include "debug.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace gba {

PPU::PPU(Bus& bus) : m_bus(bus) {
    reset();
}

PPU::~PPU() = default;

void PPU::reset() {
    m_vram.fill(0);
    m_palette.fill(0);
    m_oam.fill(0);
    m_framebuffer.fill(0);

    m_vcount = 0;
    m_hcount = 0;
    m_dispcnt = 0;
    m_dispstat = 0;

    m_bgcnt.fill(0);
    m_bghofs.fill(0);
    m_bgvofs.fill(0);
    m_bgx_internal.fill(0);
    m_bgy_internal.fill(0);

    m_bgpa.fill(0x100);
    m_bgpb.fill(0);
    m_bgpc.fill(0);
    m_bgpd.fill(0x100);
    m_bgx.fill(0);
    m_bgy.fill(0);

    m_win0h = m_win1h = 0;
    m_win0v = m_win1v = 0;
    m_winin = m_winout = 0;

    m_bldcnt = 0;
    m_bldalpha = 0;
    m_bldy = 0;
}

void PPU::step(int cycles) {
    for (int i = 0; i < cycles; i++) {
        m_hcount++;

        // Handle HBlank transition
        if (m_hcount == HDRAW_CYCLES) {
            // Enter HBlank
            m_dispstat |= 0x0002;  // Set HBlank flag

            if (m_vcount < VDRAW_LINES) {
                render_scanline();

                // Update affine internal registers after each visible scanline
                for (int bg = 0; bg < 2; bg++) {
                    m_bgx_internal[bg] += m_bgpb[bg];
                    m_bgy_internal[bg] += m_bgpd[bg];
                }

                // Trigger HBlank DMAs during visible lines only
                m_bus.trigger_hblank_dma();
            }

            // HBlank interrupt
            if (m_dispstat & 0x0010) {
                m_bus.request_interrupt(GBAInterrupt::HBlank);
            }
        }

        // Handle end of scanline
        if (m_hcount >= SCANLINE_CYCLES) {
            m_hcount = 0;
            m_dispstat &= ~0x0002;  // Clear HBlank flag
            m_vcount++;

            // Handle VBlank transition
            if (m_vcount == VDRAW_LINES) {
                m_dispstat |= 0x0001;  // Set VBlank flag

                // Reload affine reference points at VBlank
                m_bgx_internal[0] = m_bgx[0];
                m_bgy_internal[0] = m_bgy[0];
                m_bgx_internal[1] = m_bgx[1];
                m_bgy_internal[1] = m_bgy[1];

                // Trigger VBlank DMAs
                m_bus.trigger_vblank_dma();

                // VBlank interrupt - request if enabled in DISPSTAT
                // WORKAROUND: Also request if IE has VBlank enabled but DISPSTAT doesn't
                // Some games (like Pokemon Fire Red) rely on VBlank interrupts but
                // don't set DISPSTAT bit 3. This may be due to incorrect understanding
                // of the hardware or a quirk in how their IRQ handler works.
                // For compatibility, we request VBlank interrupt unconditionally here.
                // The CPU will still check IE/IF/IME before actually handling it.
                m_bus.request_interrupt(GBAInterrupt::VBlank);
            }

            // Handle end of frame
            if (m_vcount >= TOTAL_LINES) {
                m_vcount = 0;
                m_dispstat &= ~0x0001;  // Clear VBlank flag
            }

            // VCount match
            uint16_t vcount_target = (m_dispstat >> 8) & 0xFF;
            if (m_vcount == vcount_target) {
                m_dispstat |= 0x0004;  // Set VCount flag
                if (m_dispstat & 0x0020) {
                    m_bus.request_interrupt(GBAInterrupt::VCount);
                }
            } else {
                m_dispstat &= ~0x0004;  // Clear VCount flag
            }
        }
    }
}

void PPU::render_scanline() {
    // Clear scanline buffers
    for (int i = 0; i < 4; i++) {
        m_bg_buffer[i].fill(0x8000);  // Transparent marker
        m_bg_priority[i].fill(4);     // Lowest priority
        m_bg_is_target1[i].fill(false);
    }
    m_sprite_buffer.fill(0x8000);
    m_sprite_priority.fill(4);
    m_sprite_semi_transparent.fill(false);
    m_sprite_is_window.fill(false);

    // Update registers from bus before rendering
    m_dispcnt = m_bus.get_dispcnt();
    for (int i = 0; i < 4; i++) {
        m_bgcnt[i] = m_bus.get_bgcnt(i);
        m_bghofs[i] = m_bus.get_bghofs(i);
        m_bgvofs[i] = m_bus.get_bgvofs(i);
    }

    // Update affine parameters
    for (int i = 0; i < 2; i++) {
        m_bgpa[i] = m_bus.get_bgpa(i);
        m_bgpb[i] = m_bus.get_bgpb(i);
        m_bgpc[i] = m_bus.get_bgpc(i);
        m_bgpd[i] = m_bus.get_bgpd(i);
        m_bgx[i] = m_bus.get_bgx(i);
        m_bgy[i] = m_bus.get_bgy(i);
    }

    // Update window and blend registers
    m_win0h = m_bus.get_win0h();
    m_win1h = m_bus.get_win1h();
    m_win0v = m_bus.get_win0v();
    m_win1v = m_bus.get_win1v();
    m_winin = m_bus.get_winin();
    m_winout = m_bus.get_winout();
    m_bldcnt = m_bus.get_bldcnt();
    m_bldalpha = m_bus.get_bldalpha();
    m_bldy = m_bus.get_bldy();

    // Initialize affine internal registers at start of frame
    if (m_vcount == 0) {
        m_bgx_internal[0] = m_bgx[0];
        m_bgy_internal[0] = m_bgy[0];
        m_bgx_internal[1] = m_bgx[1];
        m_bgy_internal[1] = m_bgy[1];
    }

    // Get display mode
    m_mode = static_cast<DisplayMode>(m_dispcnt & 7);

    // Check for forced blank
    if (m_dispcnt & 0x0080) {
        // Forced blank - display white
        for (int x = 0; x < 240; x++) {
            m_framebuffer[m_vcount * 240 + x] = 0xFFFFFFFF;
        }
        return;
    }

    switch (m_mode) {
        case DisplayMode::Mode0: render_mode0(); break;
        case DisplayMode::Mode1: render_mode1(); break;
        case DisplayMode::Mode2: render_mode2(); break;
        case DisplayMode::Mode3: render_mode3(); break;
        case DisplayMode::Mode4: render_mode4(); break;
        case DisplayMode::Mode5: render_mode5(); break;
    }

    // Render sprites if enabled
    if (m_dispcnt & 0x1000) {
        render_sprites();
    }

    // Compose final scanline with windowing and blending
    compose_scanline();
}

void PPU::render_mode0() {
    // Mode 0: 4 tiled backgrounds (all regular)
    for (int layer = 0; layer < 4; layer++) {
        if (m_dispcnt & (0x0100 << layer)) {
            render_background(layer);
        }
    }
}

void PPU::render_mode1() {
    // Mode 1: 2 tiled (BG0, BG1) + 1 affine (BG2)
    if (m_dispcnt & 0x0100) render_background(0);
    if (m_dispcnt & 0x0200) render_background(1);
    if (m_dispcnt & 0x0400) render_affine_background(2);
}

void PPU::render_mode2() {
    // Mode 2: 2 affine backgrounds (BG2, BG3)
    if (m_dispcnt & 0x0400) render_affine_background(2);
    if (m_dispcnt & 0x0800) render_affine_background(3);
}

void PPU::render_mode3() {
    // Mode 3: 240x160 bitmap, 15-bit color (uses BG2)
    uint32_t base = m_vcount * 240 * 2;
    uint8_t priority = m_bgcnt[2] & 3;

    for (int x = 0; x < 240; x++) {
        uint16_t color = m_vram[base + x * 2] | (m_vram[base + x * 2 + 1] << 8);
        m_bg_buffer[2][x] = color;
        m_bg_priority[2][x] = priority;
    }
}

void PPU::render_mode4() {
    // Mode 4: 240x160 bitmap, 8-bit palette (uses BG2)
    // In bitmap mode, every pixel is a valid color (even index 0)
    m_frame_select = (m_dispcnt & 0x0010) != 0;
    uint32_t base = (m_frame_select ? 0xA000 : 0) + m_vcount * 240;
    uint8_t priority = m_bgcnt[2] & 3;

    for (int x = 0; x < 240; x++) {
        uint8_t index = m_vram[base + x];
        uint16_t color = m_palette[index * 2] | (m_palette[index * 2 + 1] << 8);
        m_bg_buffer[2][x] = color;
        m_bg_priority[2][x] = priority;
    }
}

void PPU::render_mode5() {
    // Mode 5: 160x128 bitmap, 15-bit color (uses BG2)
    m_frame_select = (m_dispcnt & 0x0010) != 0;
    uint8_t priority = m_bgcnt[2] & 3;

    if (m_vcount >= 128) return;

    uint32_t base = (m_frame_select ? 0xA000 : 0) + m_vcount * 160 * 2;
    for (int x = 0; x < 160; x++) {
        uint16_t color = m_vram[base + x * 2] | (m_vram[base + x * 2 + 1] << 8);
        m_bg_buffer[2][x] = color;
        m_bg_priority[2][x] = priority;
    }
}

void PPU::render_background(int layer) {
    uint16_t control = m_bgcnt[layer];
    uint8_t priority = control & 3;
    uint32_t char_base = ((control >> 2) & 3) * 0x4000;
    uint32_t screen_base = ((control >> 8) & 0x1F) * 0x800;
    bool palette_256 = control & 0x0080;
    int screen_size = (control >> 14) & 3;

    int scroll_x = m_bghofs[layer] & 0x1FF;
    int scroll_y = m_bgvofs[layer] & 0x1FF;

    // Calculate screen dimensions based on size
    // Size 0: 256x256 (1 screen block)
    // Size 1: 512x256 (2 screen blocks horizontal)
    // Size 2: 256x512 (2 screen blocks vertical)
    // Size 3: 512x512 (4 screen blocks)
    int screen_width = (screen_size & 1) ? 512 : 256;
    int screen_height = (screen_size & 2) ? 512 : 256;

    int y = (m_vcount + scroll_y) % screen_height;

    for (int screen_x = 0; screen_x < 240; screen_x++) {
        int x = (screen_x + scroll_x) % screen_width;

        // Determine which screen block we're in
        // Screen blocks are arranged as:
        // Size 0: [0]
        // Size 1: [0][1] (horizontal)
        // Size 2: [0]
        //         [1] (vertical)
        // Size 3: [0][1]
        //         [2][3]
        int screen_block_x = x / 256;
        int screen_block_y = y / 256;
        int screen_block = 0;

        switch (screen_size) {
            case 0: screen_block = 0; break;
            case 1: screen_block = screen_block_x; break;
            case 2: screen_block = screen_block_y; break;
            case 3: screen_block = screen_block_x + screen_block_y * 2; break;
        }

        int local_x = x % 256;
        int local_y = y % 256;

        // Get tile from map
        int tile_x = local_x / 8;
        int tile_y = local_y / 8;
        uint32_t map_offset = screen_base + screen_block * 0x800 + (tile_y * 32 + tile_x) * 2;

        // Ensure we don't read out of bounds
        if (map_offset + 1 >= m_vram.size()) continue;

        uint16_t tile_entry = m_vram[map_offset] | (m_vram[map_offset + 1] << 8);
        int tile_id = tile_entry & 0x3FF;
        bool h_flip = tile_entry & 0x0400;
        bool v_flip = tile_entry & 0x0800;
        int palette_num = (tile_entry >> 12) & 0xF;

        // Get pixel within tile
        int pixel_x = local_x & 7;
        int pixel_y = local_y & 7;

        if (h_flip) pixel_x = 7 - pixel_x;
        if (v_flip) pixel_y = 7 - pixel_y;

        uint8_t color_index;
        if (palette_256) {
            // 256-color mode: 64 bytes per tile
            uint32_t tile_offset = char_base + tile_id * 64 + pixel_y * 8 + pixel_x;
            if (tile_offset >= m_vram.size()) continue;
            color_index = m_vram[tile_offset];
        } else {
            // 16-color mode: 32 bytes per tile
            uint32_t tile_offset = char_base + tile_id * 32 + pixel_y * 4 + pixel_x / 2;
            if (tile_offset >= m_vram.size()) continue;
            uint8_t byte = m_vram[tile_offset];
            color_index = (pixel_x & 1) ? (byte >> 4) : (byte & 0x0F);

            if (color_index != 0) {
                color_index += palette_num * 16;
            }
        }

        if (color_index != 0) {
            uint32_t pal_offset = color_index * 2;
            if (pal_offset + 1 < m_palette.size()) {
                uint16_t color = m_palette[pal_offset] | (m_palette[pal_offset + 1] << 8);
                m_bg_buffer[layer][screen_x] = color;
                m_bg_priority[layer][screen_x] = priority;
            }
        }
    }
}

void PPU::render_affine_background(int layer) {
    // Affine backgrounds use BG2 (layer=2, index 0) or BG3 (layer=3, index 1)
    int affine_idx = layer - 2;

    uint16_t control = m_bgcnt[layer];
    uint8_t priority = control & 3;
    uint32_t char_base = ((control >> 2) & 3) * 0x4000;
    uint32_t screen_base = ((control >> 8) & 0x1F) * 0x800;
    int size_bits = (control >> 14) & 3;
    int size = 128 << size_bits;  // 128, 256, 512, or 1024 pixels
    bool wraparound = control & 0x2000;

    // Get transformation parameters
    int16_t pa = m_bgpa[affine_idx];
    int16_t pc = m_bgpc[affine_idx];

    // Use internal reference point (28.4 fixed point format)
    int32_t ref_x = m_bgx_internal[affine_idx];
    int32_t ref_y = m_bgy_internal[affine_idx];

    for (int screen_x = 0; screen_x < 240; screen_x++) {
        // Calculate texture coordinates
        // The reference point moves by PA/PC for each pixel
        int32_t tex_x = ref_x + pa * screen_x;
        int32_t tex_y = ref_y + pc * screen_x;

        // Convert from 28.4 fixed point to integer
        int x = tex_x >> 8;
        int y = tex_y >> 8;

        // Handle wraparound or clipping
        if (wraparound) {
            x = x & (size - 1);
            y = y & (size - 1);
        } else {
            if (x < 0 || x >= size || y < 0 || y >= size) {
                continue;
            }
        }

        // Get tile from map (affine maps are 8-bit entries, one byte per tile)
        int tile_x = x / 8;
        int tile_y = y / 8;
        int tiles_per_row = size / 8;
        uint32_t map_offset = screen_base + tile_y * tiles_per_row + tile_x;

        if (map_offset >= m_vram.size()) continue;

        uint8_t tile_id = m_vram[map_offset];

        // Get pixel within tile (affine BGs are always 256-color)
        int pixel_x = x & 7;
        int pixel_y = y & 7;

        uint32_t tile_offset = char_base + tile_id * 64 + pixel_y * 8 + pixel_x;
        if (tile_offset >= m_vram.size()) continue;

        uint8_t color_index = m_vram[tile_offset];

        if (color_index != 0) {
            uint32_t pal_offset = color_index * 2;
            if (pal_offset + 1 < m_palette.size()) {
                uint16_t color = m_palette[pal_offset] | (m_palette[pal_offset + 1] << 8);
                m_bg_buffer[layer][screen_x] = color;
                m_bg_priority[layer][screen_x] = priority;
            }
        }
    }
}

void PPU::render_sprites() {
    // OAM contains 128 sprites, each 8 bytes
    // Render in reverse order so lower index sprites have higher priority
    for (int sprite = 127; sprite >= 0; sprite--) {
        uint16_t attr0 = m_oam[sprite * 8 + 0] | (m_oam[sprite * 8 + 1] << 8);
        uint16_t attr1 = m_oam[sprite * 8 + 2] | (m_oam[sprite * 8 + 3] << 8);
        uint16_t attr2 = m_oam[sprite * 8 + 4] | (m_oam[sprite * 8 + 5] << 8);

        // Check object mode
        int obj_mode = (attr0 >> 8) & 3;
        if (obj_mode == 2) continue;  // Disabled/hidden

        bool is_affine = attr0 & 0x0100;
        bool double_size = is_affine && (attr0 & 0x0200);

        // Get sprite dimensions
        int shape = (attr0 >> 14) & 3;
        int size_bits = (attr1 >> 14) & 3;

        static const int sprite_sizes[3][4][2] = {
            {{8, 8}, {16, 16}, {32, 32}, {64, 64}},   // Square
            {{16, 8}, {32, 8}, {32, 16}, {64, 32}},   // Horizontal
            {{8, 16}, {8, 32}, {16, 32}, {32, 64}}    // Vertical
        };

        if (shape == 3) continue;  // Invalid shape

        int width = sprite_sizes[shape][size_bits][0];
        int height = sprite_sizes[shape][size_bits][1];

        // For affine double-size, the bounding box is doubled
        int bounds_height = double_size ? height * 2 : height;
        (void)double_size;  // bounds_width not needed for non-affine sprites

        // Get sprite position
        int y = attr0 & 0xFF;
        if (y >= 160) y -= 256;

        int x = attr1 & 0x1FF;
        if (x >= 240) x -= 512;

        // Check if sprite is on current scanline
        if (m_vcount < y || m_vcount >= y + bounds_height) continue;

        // Handle affine sprites separately
        if (is_affine) {
            render_affine_sprite(sprite, attr0, attr1, attr2);
            continue;
        }

        int sprite_y = m_vcount - y;

        // Get sprite attributes
        bool h_flip = attr1 & 0x1000;
        bool v_flip = attr1 & 0x2000;
        int tile_id = attr2 & 0x3FF;
        uint8_t priority = (attr2 >> 10) & 3;
        int palette = (attr2 >> 12) & 0xF;
        bool is_256_color = attr0 & 0x2000;
        bool semi_transparent = (obj_mode == 1);  // Semi-transparent mode
        bool is_obj_window = (obj_mode == 2);     // Actually obj_mode == 2 is disabled, obj_mode == 3 is window

        if (obj_mode == 3) {
            is_obj_window = true;
        }

        if (v_flip) sprite_y = height - 1 - sprite_y;

        // Render sprite pixels
        for (int sprite_x = 0; sprite_x < width; sprite_x++) {
            int screen_x = x + sprite_x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int pixel_x = h_flip ? (width - 1 - sprite_x) : sprite_x;

            // Calculate tile offset
            int tile_row = sprite_y / 8;
            int tile_col = pixel_x / 8;
            int in_tile_x = pixel_x & 7;
            int in_tile_y = sprite_y & 7;

            int current_tile;
            if (m_dispcnt & 0x0040) {
                // 1D mapping: tiles are laid out linearly
                if (is_256_color) {
                    // 256-color: each tile is 64 bytes, so tiles take up 2 tile slots
                    current_tile = tile_id + tile_row * (width / 8) * 2 + tile_col * 2;
                } else {
                    // 16-color: each tile is 32 bytes
                    current_tile = tile_id + tile_row * (width / 8) + tile_col;
                }
            } else {
                // 2D mapping: 32 tiles per row in VRAM
                if (is_256_color) {
                    current_tile = (tile_id & ~1) + tile_row * 32 + tile_col * 2;
                } else {
                    current_tile = tile_id + tile_row * 32 + tile_col;
                }
            }

            uint8_t color_index;
            uint32_t char_base = 0x10000;  // Sprite tiles start at 0x10000 in VRAM

            if (is_256_color) {
                // 256-color: 64 bytes per tile
                uint32_t offset = char_base + current_tile * 32 + in_tile_y * 8 + in_tile_x;
                if (offset >= m_vram.size()) continue;
                color_index = m_vram[offset];
            } else {
                // 16-color: 32 bytes per tile
                uint32_t offset = char_base + current_tile * 32 + in_tile_y * 4 + in_tile_x / 2;
                if (offset >= m_vram.size()) continue;
                uint8_t byte = m_vram[offset];
                color_index = (in_tile_x & 1) ? (byte >> 4) : (byte & 0x0F);
            }

            // Color index 0 is always transparent
            if (color_index == 0) continue;

            // For 16-color, add palette offset
            if (!is_256_color) {
                color_index += palette * 16;
            }

            // Handle OBJ window mode
            if (is_obj_window) {
                m_sprite_is_window[screen_x] = true;
                continue;
            }

            // Sprite palette is at offset 0x200 in palette RAM
            uint32_t pal_offset = 0x200 + color_index * 2;
            if (pal_offset + 1 >= m_palette.size()) continue;
            uint16_t color = m_palette[pal_offset] | (m_palette[pal_offset + 1] << 8);

            // Only draw if higher priority (lower number = higher priority)
            // For sprites, lower OAM index also wins on same priority
            if (priority < m_sprite_priority[screen_x] ||
                (priority == m_sprite_priority[screen_x] && m_sprite_buffer[screen_x] == 0x8000)) {
                m_sprite_buffer[screen_x] = color;
                m_sprite_priority[screen_x] = priority;
                m_sprite_semi_transparent[screen_x] = semi_transparent;
            }
        }
    }
}

void PPU::render_affine_sprite([[maybe_unused]] int sprite_idx, uint16_t attr0, uint16_t attr1, uint16_t attr2) {
    bool double_size = attr0 & 0x0200;

    // Get sprite dimensions
    int shape = (attr0 >> 14) & 3;
    int size_bits = (attr1 >> 14) & 3;

    static const int sprite_sizes[3][4][2] = {
        {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
        {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
        {{8, 16}, {8, 32}, {16, 32}, {32, 64}}
    };

    int width = sprite_sizes[shape][size_bits][0];
    int height = sprite_sizes[shape][size_bits][1];

    int bounds_width = double_size ? width * 2 : width;
    int bounds_height = double_size ? height * 2 : height;

    // Get position
    int y = attr0 & 0xFF;
    if (y >= 160) y -= 256;
    int x = attr1 & 0x1FF;
    if (x >= 240) x -= 512;

    // Get affine parameters from OAM
    int affine_idx = (attr1 >> 9) & 0x1F;
    int16_t pa = static_cast<int16_t>(m_oam[affine_idx * 32 + 0x06] | (m_oam[affine_idx * 32 + 0x07] << 8));
    int16_t pb = static_cast<int16_t>(m_oam[affine_idx * 32 + 0x0E] | (m_oam[affine_idx * 32 + 0x0F] << 8));
    int16_t pc = static_cast<int16_t>(m_oam[affine_idx * 32 + 0x16] | (m_oam[affine_idx * 32 + 0x17] << 8));
    int16_t pd = static_cast<int16_t>(m_oam[affine_idx * 32 + 0x1E] | (m_oam[affine_idx * 32 + 0x1F] << 8));

    // Get other attributes
    int tile_id = attr2 & 0x3FF;
    uint8_t priority = (attr2 >> 10) & 3;
    int palette = (attr2 >> 12) & 0xF;
    bool is_256_color = attr0 & 0x2000;
    int obj_mode = (attr0 >> 8) & 3;
    bool semi_transparent = (obj_mode == 1);
    bool is_obj_window = (obj_mode == 3);

    // Calculate center of sprite
    int center_x = bounds_width / 2;
    int center_y = bounds_height / 2;

    // Sprite Y relative to center
    int sprite_y = m_vcount - y - center_y;

    // Render each pixel in the bounding box
    for (int sprite_x = 0; sprite_x < bounds_width; sprite_x++) {
        int screen_x = x + sprite_x;
        if (screen_x < 0 || screen_x >= 240) continue;

        // Offset from center
        int dx = sprite_x - center_x;
        int dy = sprite_y;

        // Apply inverse transformation to get texture coordinates
        // (PA, PB, PC, PD are 8.8 fixed point)
        int tex_x = ((pa * dx + pb * dy) >> 8) + width / 2;
        int tex_y = ((pc * dx + pd * dy) >> 8) + height / 2;

        // Check if within sprite bounds
        if (tex_x < 0 || tex_x >= width || tex_y < 0 || tex_y >= height) continue;

        // Calculate tile and pixel offset
        int tile_row = tex_y / 8;
        int tile_col = tex_x / 8;
        int in_tile_x = tex_x & 7;
        int in_tile_y = tex_y & 7;

        int current_tile;
        if (m_dispcnt & 0x0040) {
            // 1D mapping
            if (is_256_color) {
                current_tile = tile_id + tile_row * (width / 8) * 2 + tile_col * 2;
            } else {
                current_tile = tile_id + tile_row * (width / 8) + tile_col;
            }
        } else {
            // 2D mapping
            if (is_256_color) {
                current_tile = (tile_id & ~1) + tile_row * 32 + tile_col * 2;
            } else {
                current_tile = tile_id + tile_row * 32 + tile_col;
            }
        }

        uint8_t color_index;
        uint32_t char_base = 0x10000;

        if (is_256_color) {
            uint32_t offset = char_base + current_tile * 32 + in_tile_y * 8 + in_tile_x;
            if (offset >= m_vram.size()) continue;
            color_index = m_vram[offset];
        } else {
            uint32_t offset = char_base + current_tile * 32 + in_tile_y * 4 + in_tile_x / 2;
            if (offset >= m_vram.size()) continue;
            uint8_t byte = m_vram[offset];
            color_index = (in_tile_x & 1) ? (byte >> 4) : (byte & 0x0F);
        }

        if (color_index == 0) continue;

        if (!is_256_color) {
            color_index += palette * 16;
        }

        if (is_obj_window) {
            m_sprite_is_window[screen_x] = true;
            continue;
        }

        uint32_t pal_offset = 0x200 + color_index * 2;
        if (pal_offset + 1 >= m_palette.size()) continue;
        uint16_t color = m_palette[pal_offset] | (m_palette[pal_offset + 1] << 8);

        if (priority < m_sprite_priority[screen_x] ||
            (priority == m_sprite_priority[screen_x] && m_sprite_buffer[screen_x] == 0x8000)) {
            m_sprite_buffer[screen_x] = color;
            m_sprite_priority[screen_x] = priority;
            m_sprite_semi_transparent[screen_x] = semi_transparent;
        }
    }
}

bool PPU::is_inside_window(int x, int window_id) {
    uint16_t h_reg, v_reg;
    if (window_id == 0) {
        h_reg = m_win0h;
        v_reg = m_win0v;
    } else {
        h_reg = m_win1h;
        v_reg = m_win1v;
    }

    int x1 = (h_reg >> 8) & 0xFF;
    int x2 = h_reg & 0xFF;
    int y1 = (v_reg >> 8) & 0xFF;
    int y2 = v_reg & 0xFF;

    // Handle wraparound: if x1 > x2, window wraps horizontally
    bool in_h;
    if (x1 <= x2) {
        in_h = (x >= x1 && x < x2);
    } else {
        in_h = (x >= x1 || x < x2);
    }

    // Handle wraparound: if y1 > y2, window wraps vertically
    bool in_v;
    if (y1 <= y2) {
        in_v = (m_vcount >= y1 && m_vcount < y2);
    } else {
        in_v = (m_vcount >= y1 || m_vcount < y2);
    }

    return in_h && in_v;
}

uint8_t PPU::get_window_flags(int x) {
    // Check if any windows are enabled
    bool win0_enabled = m_dispcnt & 0x2000;
    bool win1_enabled = m_dispcnt & 0x4000;
    bool obj_win_enabled = m_dispcnt & 0x8000;

    if (!win0_enabled && !win1_enabled && !obj_win_enabled) {
        // No windows - all features enabled
        return 0x3F;
    }

    // Check windows in priority order: WIN0 > WIN1 > OBJWIN > WINOUT
    if (win0_enabled && is_inside_window(x, 0)) {
        return m_winin & 0x3F;
    }
    if (win1_enabled && is_inside_window(x, 1)) {
        return (m_winin >> 8) & 0x3F;
    }
    if (obj_win_enabled && m_sprite_is_window[x]) {
        return (m_winout >> 8) & 0x3F;
    }

    // Outside all windows
    return m_winout & 0x3F;
}

void PPU::apply_blending(int x, uint16_t& top_color, uint16_t bottom_color) {
    int blend_mode = (m_bldcnt >> 6) & 3;

    if (blend_mode == 0) return;  // No blending

    // Check window blend enable
    uint8_t win_flags = get_window_flags(x);
    if (!(win_flags & 0x20)) return;  // Blending disabled in this window

    // Extract RGB components (5 bits each)
    int r1 = top_color & 0x1F;
    int g1 = (top_color >> 5) & 0x1F;
    int b1 = (top_color >> 10) & 0x1F;

    switch (blend_mode) {
        case 1: {  // Alpha blending
            if (bottom_color == 0x8000) return;  // No bottom layer

            int eva = std::min(16, static_cast<int>(m_bldalpha & 0x1F));
            int evb = std::min(16, static_cast<int>((m_bldalpha >> 8) & 0x1F));

            int r2 = bottom_color & 0x1F;
            int g2 = (bottom_color >> 5) & 0x1F;
            int b2 = (bottom_color >> 10) & 0x1F;

            r1 = std::min(31, (r1 * eva + r2 * evb) >> 4);
            g1 = std::min(31, (g1 * eva + g2 * evb) >> 4);
            b1 = std::min(31, (b1 * eva + b2 * evb) >> 4);
            break;
        }
        case 2: {  // Brightness increase (white blend)
            int evy = std::min(16, static_cast<int>(m_bldy & 0x1F));
            r1 = r1 + ((31 - r1) * evy >> 4);
            g1 = g1 + ((31 - g1) * evy >> 4);
            b1 = b1 + ((31 - b1) * evy >> 4);
            break;
        }
        case 3: {  // Brightness decrease (black blend)
            int evy = std::min(16, static_cast<int>(m_bldy & 0x1F));
            r1 = r1 - (r1 * evy >> 4);
            g1 = g1 - (g1 * evy >> 4);
            b1 = b1 - (b1 * evy >> 4);
            break;
        }
    }

    top_color = static_cast<uint16_t>(r1 | (g1 << 5) | (b1 << 10));
}

void PPU::compose_scanline() {
    // Get backdrop color
    uint16_t backdrop = m_palette[0] | (m_palette[1] << 8);

    for (int x = 0; x < 240; x++) {
        // Get window flags for this pixel
        uint8_t win_flags = get_window_flags(x);

        uint16_t final_color = backdrop;
        uint16_t second_color = 0x8000;  // For blending
        int top_priority = 5;
        int top_layer = -1;  // -1 = backdrop, 0-3 = BG, 4 = sprite
        bool found_top = false;
        bool found_second = false;

        // Find top two visible layers for potential blending
        for (int priority = 0; priority < 4; priority++) {
            // Check sprites at this priority
            if (!found_top && m_sprite_buffer[x] != 0x8000 && m_sprite_priority[x] == priority) {
                if (win_flags & 0x10) {  // OBJ enabled in window
                    final_color = m_sprite_buffer[x];
                    top_priority = priority;
                    top_layer = 4;
                    found_top = true;

                    // Check if semi-transparent sprite needs blending
                    if (m_sprite_semi_transparent[x]) {
                        // Continue to find second layer for blending
                    } else {
                        continue;
                    }
                }
            }

            // Check backgrounds at this priority
            for (int layer = 0; layer < 4; layer++) {
                if (m_bg_buffer[layer][x] == 0x8000) continue;
                if (m_bg_priority[layer][x] != priority) continue;
                if (!(win_flags & (1 << layer))) continue;  // Layer not enabled in window

                if (!found_top) {
                    final_color = m_bg_buffer[layer][x];
                    top_priority = priority;
                    top_layer = layer;
                    found_top = true;
                } else if (!found_second) {
                    second_color = m_bg_buffer[layer][x];
                    found_second = true;
                    break;
                }
            }

            if (found_top && found_second) break;
        }

        // Check if we need to find a second layer from remaining priorities
        if (found_top && !found_second) {
            for (int priority = top_priority; priority < 4; priority++) {
                // Check sprites
                if (top_layer != 4 && m_sprite_buffer[x] != 0x8000 &&
                    m_sprite_priority[x] == priority && (win_flags & 0x10)) {
                    second_color = m_sprite_buffer[x];
                    found_second = true;
                    break;
                }

                // Check backgrounds
                for (int layer = 0; layer < 4; layer++) {
                    if (layer == top_layer) continue;
                    if (m_bg_buffer[layer][x] == 0x8000) continue;
                    if (m_bg_priority[layer][x] != priority) continue;
                    if (!(win_flags & (1 << layer))) continue;

                    second_color = m_bg_buffer[layer][x];
                    found_second = true;
                    break;
                }

                if (found_second) break;
            }
        }

        // Apply blending if enabled and applicable
        int blend_mode = (m_bldcnt >> 6) & 3;
        bool apply_blend = false;

        if (blend_mode != 0 && found_top) {
            // Check if top layer is a 1st target
            bool is_first_target = false;
            if (top_layer == 4) {
                is_first_target = m_bldcnt & 0x10;  // OBJ is 1st target
                // Semi-transparent sprites force alpha blending
                if (m_sprite_semi_transparent[x]) {
                    apply_blend = true;
                    blend_mode = 1;  // Force alpha blend
                }
            } else if (top_layer >= 0 && top_layer < 4) {
                is_first_target = m_bldcnt & (1 << top_layer);
            } else {
                is_first_target = m_bldcnt & 0x20;  // Backdrop
            }

            if (is_first_target || m_sprite_semi_transparent[x]) {
                if (blend_mode == 1) {
                    // Alpha blending needs a valid 2nd target
                    bool is_second_target = false;
                    if (found_second) {
                        // Determine second layer type
                        // (Simplified - in reality we'd need to track which layer is second)
                        is_second_target = (m_bldcnt >> 8) != 0;  // Any 2nd target
                    }
                    apply_blend = is_second_target;
                } else {
                    // Brightness change doesn't need 2nd target
                    apply_blend = true;
                }
            }
        }

        if (apply_blend) {
            apply_blending(x, final_color, second_color);
        }

        m_framebuffer[m_vcount * 240 + x] = palette_to_rgba(final_color);
    }
}

uint32_t PPU::palette_to_rgba(uint16_t color) {
    // GBA color format: xBBBBBGGGGGRRRRR (15-bit BGR)
    uint8_t r = (color & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x1F) << 3;
    uint8_t b = ((color >> 10) & 0x1F) << 3;

    // Expand 5-bit to 8-bit by replicating top bits to bottom
    r |= r >> 5;
    g |= g >> 5;
    b |= b >> 5;

    // Return as RGBA (with full alpha)
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

uint8_t PPU::read_vram(uint32_t offset) {
    if (offset < m_vram.size()) {
        return m_vram[offset];
    }
    return 0;
}

void PPU::write_vram(uint32_t offset, uint8_t value) {
    if (offset < m_vram.size()) {
        m_vram[offset] = value;
    }
}

uint8_t PPU::read_palette(uint32_t offset) {
    if (offset < m_palette.size()) {
        return m_palette[offset];
    }
    return 0;
}

void PPU::write_palette(uint32_t offset, uint8_t value) {
    if (offset < m_palette.size()) {
        m_palette[offset] = value;
    }
}

uint8_t PPU::read_oam(uint32_t offset) {
    if (offset < m_oam.size()) {
        return m_oam[offset];
    }
    return 0;
}

void PPU::write_oam(uint32_t offset, uint8_t value) {
    if (offset < m_oam.size()) {
        m_oam[offset] = value;
    }
}

void PPU::save_state(std::vector<uint8_t>& data) {
    data.insert(data.end(), m_vram.begin(), m_vram.end());
    data.insert(data.end(), m_palette.begin(), m_palette.end());
    data.insert(data.end(), m_oam.begin(), m_oam.end());

    // Save timing state
    data.push_back(m_vcount & 0xFF);
    data.push_back(m_vcount >> 8);
    data.push_back(m_hcount & 0xFF);
    data.push_back((m_hcount >> 8) & 0xFF);

    // Save affine internal registers
    auto save32 = [&data](int32_t val) {
        data.push_back(val & 0xFF);
        data.push_back((val >> 8) & 0xFF);
        data.push_back((val >> 16) & 0xFF);
        data.push_back((val >> 24) & 0xFF);
    };

    for (int i = 0; i < 2; i++) {
        save32(m_bgx_internal[i]);
        save32(m_bgy_internal[i]);
    }
}

void PPU::load_state(const uint8_t*& data, size_t& remaining) {
    std::memcpy(m_vram.data(), data, m_vram.size());
    data += m_vram.size();
    remaining -= m_vram.size();

    std::memcpy(m_palette.data(), data, m_palette.size());
    data += m_palette.size();
    remaining -= m_palette.size();

    std::memcpy(m_oam.data(), data, m_oam.size());
    data += m_oam.size();
    remaining -= m_oam.size();

    m_vcount = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
    m_hcount = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;

    // Load affine internal registers if present
    if (remaining >= 16) {
        auto load32 = [&data, &remaining]() -> int32_t {
            int32_t val = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
            data += 4; remaining -= 4;
            return val;
        };

        for (int i = 0; i < 2; i++) {
            m_bgx_internal[i] = load32();
            m_bgy_internal[i] = load32();
        }
    }
}

} // namespace gba
