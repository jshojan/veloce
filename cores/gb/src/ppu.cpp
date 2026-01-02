#include "ppu.hpp"
#include "bus.hpp"
#include <cstring>

namespace gb {

// Classic Game Boy LCD colors (greenish)
// Format: 0xAABBGGRR for OpenGL GL_RGBA on little-endian systems
// Original DMG green tint: lightest to darkest
const uint32_t PPU::s_dmg_palette[4] = {
    0xFF0FBC9B,  // Lightest: RGB(155, 188, 15) -> 0xAABBGGRR = 0xFF0FBC9B
    0xFF0FAC8B,  // Light: RGB(139, 172, 15)
    0xFF306230,  // Dark: RGB(48, 98, 48)
    0xFF0F380F   // Darkest: RGB(15, 56, 15)
};

PPU::PPU(Bus& bus) : m_bus(bus) {
    reset();
}

PPU::~PPU() = default;

void PPU::reset() {
    m_vram.fill(0);
    m_oam.fill(0);
    m_framebuffer.fill(s_dmg_palette[0]);
    m_bg_priority.fill(0);

    m_lcdc = 0x91;  // LCD on, BG on
    m_stat = 0;
    m_scy = 0;
    m_scx = 0;
    m_ly = 0;
    m_lyc = 0;
    m_bgp = 0xFC;
    m_obp0 = 0xFF;
    m_obp1 = 0xFF;
    m_wy = 0;
    m_wx = 0;

    m_bcps = 0;
    m_ocps = 0;
    m_bg_palette.fill(0xFF);
    m_obj_palette.fill(0xFF);

    m_cycle = 0;
    m_mode = Mode::OAMScan;
    m_window_line = 0;
    m_vram_bank = 0;
}

void PPU::set_mode(Mode mode) {
    m_mode = mode;
    m_stat = (m_stat & 0xFC) | static_cast<uint8_t>(mode);

    // Check for STAT interrupts
    bool interrupt = false;
    switch (mode) {
        case Mode::HBlank:
            if (m_stat & 0x08) interrupt = true;
            break;
        case Mode::VBlank:
            if (m_stat & 0x10) interrupt = true;
            m_bus.request_interrupt(0x01);  // VBlank interrupt
            break;
        case Mode::OAMScan:
            if (m_stat & 0x20) interrupt = true;
            break;
        default:
            break;
    }

    if (interrupt) {
        m_bus.request_interrupt(0x02);  // STAT interrupt
    }
}

void PPU::step() {
    // LCD off
    if (!(m_lcdc & 0x80)) {
        m_cycle = 0;
        m_ly = 0;
        m_mode = Mode::HBlank;
        m_stat = (m_stat & 0xFC);
        return;
    }

    m_cycle++;

    switch (m_mode) {
        case Mode::OAMScan:
            if (m_cycle >= OAM_SCAN_CYCLES) {
                set_mode(Mode::Drawing);
            }
            break;

        case Mode::Drawing:
            if (m_cycle >= OAM_SCAN_CYCLES + DRAWING_MIN_CYCLES) {
                set_mode(Mode::HBlank);
                render_scanline();
            }
            break;

        case Mode::HBlank:
            if (m_cycle >= SCANLINE_CYCLES) {
                m_cycle = 0;
                m_ly++;

                if (m_ly >= 144) {
                    set_mode(Mode::VBlank);
                    m_window_line = 0;
                } else {
                    set_mode(Mode::OAMScan);
                }

                // LYC compare
                if (m_ly == m_lyc) {
                    m_stat |= 0x04;
                    if (m_stat & 0x40) {
                        m_bus.request_interrupt(0x02);
                    }
                } else {
                    m_stat &= ~0x04;
                }
            }
            break;

        case Mode::VBlank:
            if (m_cycle >= SCANLINE_CYCLES) {
                m_cycle = 0;
                m_ly++;

                if (m_ly >= TOTAL_LINES) {
                    m_ly = 0;
                    set_mode(Mode::OAMScan);
                }

                // LYC compare in VBlank too
                if (m_ly == m_lyc) {
                    m_stat |= 0x04;
                    if (m_stat & 0x40) {
                        m_bus.request_interrupt(0x02);
                    }
                } else {
                    m_stat &= ~0x04;
                }
            }
            break;
    }
}

void PPU::render_scanline() {
    if (m_ly >= 144) return;

    m_bg_priority.fill(0);

    // Render layers
    if (m_lcdc & 0x01 || m_cgb_mode) {  // BG enable (always on for CGB)
        render_background();
    } else {
        // Fill with color 0
        for (int x = 0; x < 160; x++) {
            m_framebuffer[m_ly * 160 + x] = get_dmg_color(0);
        }
    }

    if ((m_lcdc & 0x20) && m_wy <= m_ly) {  // Window enable
        render_window();
    }

    if (m_lcdc & 0x02) {  // Sprites enable
        render_sprites();
    }
}

void PPU::render_background() {
    bool use_tile_map_1 = m_lcdc & 0x08;
    bool use_tile_data_1 = m_lcdc & 0x10;

    uint16_t tile_map_base = use_tile_map_1 ? 0x1C00 : 0x1800;

    for (int screen_x = 0; screen_x < 160; screen_x++) {
        int map_x = (screen_x + m_scx) & 0xFF;
        int map_y = (m_ly + m_scy) & 0xFF;

        int tile_x = map_x / 8;
        int tile_y = map_y / 8;
        int pixel_x = map_x & 7;
        int pixel_y = map_y & 7;

        uint16_t tile_addr = tile_map_base + tile_y * 32 + tile_x;
        uint8_t tile_num = m_vram[tile_addr];

        // CGB attributes (from VRAM bank 1)
        uint8_t attr = 0;
        int palette_num = 0;
        bool h_flip = false;
        bool v_flip = false;
        bool priority = false;
        int tile_bank = 0;

        if (m_cgb_mode) {
            attr = m_vram[0x2000 + tile_addr];
            palette_num = attr & 0x07;
            tile_bank = (attr >> 3) & 1;
            h_flip = attr & 0x20;
            v_flip = attr & 0x40;
            priority = attr & 0x80;
        }

        if (h_flip) pixel_x = 7 - pixel_x;
        if (v_flip) pixel_y = 7 - pixel_y;

        // Get tile data address
        uint16_t tile_data_addr;
        if (use_tile_data_1) {
            tile_data_addr = tile_num * 16;
        } else {
            tile_data_addr = 0x1000 + static_cast<int8_t>(tile_num) * 16;
        }

        if (m_cgb_mode && tile_bank) {
            tile_data_addr += 0x2000;
        }

        uint8_t lo = m_vram[tile_data_addr + pixel_y * 2];
        uint8_t hi = m_vram[tile_data_addr + pixel_y * 2 + 1];

        int color_bit = 7 - pixel_x;
        uint8_t color_num = ((hi >> color_bit) & 1) << 1 | ((lo >> color_bit) & 1);

        uint32_t color;
        if (m_cgb_mode) {
            uint16_t cgb_color = m_bg_palette[palette_num * 8 + color_num * 2] |
                                 (m_bg_palette[palette_num * 8 + color_num * 2 + 1] << 8);
            color = get_cgb_color(cgb_color);
        } else {
            uint8_t shade = (m_bgp >> (color_num * 2)) & 3;
            color = get_dmg_color(shade);
        }

        m_framebuffer[m_ly * 160 + screen_x] = color;
        m_bg_priority[screen_x] = color_num != 0 ? (priority ? 2 : 1) : 0;
    }
}

void PPU::render_window() {
    if (m_wx > 166) return;

    int window_x = m_wx - 7;
    if (window_x < 0) window_x = 0;

    bool use_tile_map_1 = m_lcdc & 0x40;
    bool use_tile_data_1 = m_lcdc & 0x10;

    uint16_t tile_map_base = use_tile_map_1 ? 0x1C00 : 0x1800;

    for (int screen_x = window_x; screen_x < 160; screen_x++) {
        int win_x = screen_x - window_x;
        int win_y = m_window_line;

        int tile_x = win_x / 8;
        int tile_y = win_y / 8;
        int pixel_x = win_x & 7;
        int pixel_y = win_y & 7;

        uint16_t tile_addr = tile_map_base + tile_y * 32 + tile_x;
        uint8_t tile_num = m_vram[tile_addr];

        // CGB attributes
        uint8_t attr = 0;
        int palette_num = 0;
        bool h_flip = false;
        bool v_flip = false;
        int tile_bank = 0;

        if (m_cgb_mode) {
            attr = m_vram[0x2000 + tile_addr];
            palette_num = attr & 0x07;
            tile_bank = (attr >> 3) & 1;
            h_flip = attr & 0x20;
            v_flip = attr & 0x40;
        }

        if (h_flip) pixel_x = 7 - pixel_x;
        if (v_flip) pixel_y = 7 - pixel_y;

        uint16_t tile_data_addr;
        if (use_tile_data_1) {
            tile_data_addr = tile_num * 16;
        } else {
            tile_data_addr = 0x1000 + static_cast<int8_t>(tile_num) * 16;
        }

        if (m_cgb_mode && tile_bank) {
            tile_data_addr += 0x2000;
        }

        uint8_t lo = m_vram[tile_data_addr + pixel_y * 2];
        uint8_t hi = m_vram[tile_data_addr + pixel_y * 2 + 1];

        int color_bit = 7 - pixel_x;
        uint8_t color_num = ((hi >> color_bit) & 1) << 1 | ((lo >> color_bit) & 1);

        uint32_t color;
        if (m_cgb_mode) {
            uint16_t cgb_color = m_bg_palette[palette_num * 8 + color_num * 2] |
                                 (m_bg_palette[palette_num * 8 + color_num * 2 + 1] << 8);
            color = get_cgb_color(cgb_color);
        } else {
            uint8_t shade = (m_bgp >> (color_num * 2)) & 3;
            color = get_dmg_color(shade);
        }

        m_framebuffer[m_ly * 160 + screen_x] = color;
        m_bg_priority[screen_x] = color_num != 0 ? 1 : 0;
    }

    m_window_line++;
}

void PPU::render_sprites() {
    bool tall_sprites = m_lcdc & 0x04;
    int sprite_height = tall_sprites ? 16 : 8;

    // Collect sprites on this scanline (max 10)
    struct SpriteEntry {
        int x;      // Signed to handle sprites partially off-screen left
        int y;      // Signed to handle sprites partially off-screen top
        uint8_t tile;
        uint8_t attr;
        int oam_index;
    };
    SpriteEntry sprites[10];
    int sprite_count = 0;

    for (int i = 0; i < 40 && sprite_count < 10; i++) {
        // OAM Y is stored as Y + 16, X as X + 8
        // Use signed arithmetic to properly handle sprites at screen edges
        int y = static_cast<int>(m_oam[i * 4 + 0]) - 16;
        int x = static_cast<int>(m_oam[i * 4 + 1]) - 8;
        uint8_t tile = m_oam[i * 4 + 2];
        uint8_t attr = m_oam[i * 4 + 3];

        // Check if sprite is on this scanline
        // m_ly must be >= y and < y + sprite_height
        if (m_ly >= y && m_ly < y + sprite_height) {
            sprites[sprite_count].x = x;
            sprites[sprite_count].y = y;
            sprites[sprite_count].tile = tile;
            sprites[sprite_count].attr = attr;
            sprites[sprite_count].oam_index = i;
            sprite_count++;
        }
    }

    // Sort by X coordinate (lower X = higher priority)
    // On CGB, OAM order takes priority
    if (!m_cgb_mode) {
        for (int i = 0; i < sprite_count - 1; i++) {
            for (int j = i + 1; j < sprite_count; j++) {
                if (sprites[j].x < sprites[i].x) {
                    SpriteEntry temp = sprites[i];
                    sprites[i] = sprites[j];
                    sprites[j] = temp;
                }
            }
        }
    }

    // Render sprites (back to front for correct priority)
    for (int i = sprite_count - 1; i >= 0; i--) {
        SpriteEntry& sprite = sprites[i];

        bool h_flip = sprite.attr & 0x20;
        bool v_flip = sprite.attr & 0x40;
        bool bg_priority = sprite.attr & 0x80;
        int palette_num = m_cgb_mode ? (sprite.attr & 0x07) : ((sprite.attr >> 4) & 1);
        int tile_bank = m_cgb_mode ? ((sprite.attr >> 3) & 1) : 0;

        int sprite_y = m_ly - sprite.y;
        if (v_flip) sprite_y = sprite_height - 1 - sprite_y;

        uint8_t tile = sprite.tile;
        if (tall_sprites) {
            tile &= 0xFE;
            if (sprite_y >= 8) {
                tile++;
                sprite_y -= 8;
            }
        }

        uint16_t tile_addr = tile * 16 + sprite_y * 2;
        if (m_cgb_mode && tile_bank) {
            tile_addr += 0x2000;
        }

        uint8_t lo = m_vram[tile_addr];
        uint8_t hi = m_vram[tile_addr + 1];

        for (int pixel = 0; pixel < 8; pixel++) {
            int screen_x = sprite.x + pixel;
            if (screen_x < 0 || screen_x >= 160) continue;

            int actual_pixel = h_flip ? (7 - pixel) : pixel;
            int color_bit = 7 - actual_pixel;
            uint8_t color_num = ((hi >> color_bit) & 1) << 1 | ((lo >> color_bit) & 1);

            if (color_num == 0) continue;  // Transparent

            // Check priority
            if (bg_priority && m_bg_priority[screen_x] != 0) continue;

            uint32_t color;
            if (m_cgb_mode) {
                uint16_t cgb_color = m_obj_palette[palette_num * 8 + color_num * 2] |
                                     (m_obj_palette[palette_num * 8 + color_num * 2 + 1] << 8);
                color = get_cgb_color(cgb_color);
            } else {
                uint8_t palette = palette_num ? m_obp1 : m_obp0;
                uint8_t shade = (palette >> (color_num * 2)) & 3;
                color = get_dmg_color(shade);
            }

            m_framebuffer[m_ly * 160 + screen_x] = color;
        }
    }
}

uint32_t PPU::get_dmg_color(uint8_t shade) {
    return s_dmg_palette[shade & 3];
}

uint32_t PPU::get_cgb_color(uint16_t color) {
    // CGB: xBBBBBGGGGGRRRRR
    uint8_t r = (color & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x1F) << 3;
    uint8_t b = ((color >> 10) & 0x1F) << 3;

    r |= r >> 5;
    g |= g >> 5;
    b |= b >> 5;

    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

uint8_t PPU::read_vram(uint16_t offset) {
    if (m_cgb_mode && m_vram_bank) {
        return m_vram[0x2000 + (offset & 0x1FFF)];
    }
    return m_vram[offset & 0x1FFF];
}

void PPU::write_vram(uint16_t offset, uint8_t value) {
    if (m_cgb_mode && m_vram_bank) {
        m_vram[0x2000 + (offset & 0x1FFF)] = value;
    } else {
        m_vram[offset & 0x1FFF] = value;
    }
}

uint8_t PPU::read_oam(uint16_t offset) {
    if (offset < 160) {
        return m_oam[offset];
    }
    return 0xFF;
}

void PPU::write_oam(uint16_t offset, uint8_t value) {
    if (offset < 160) {
        m_oam[offset] = value;
    }
}

uint8_t PPU::read_register(uint16_t address) {
    switch (address & 0xFF) {
        case 0x40: return m_lcdc;
        case 0x41: return m_stat | 0x80;
        case 0x42: return m_scy;
        case 0x43: return m_scx;
        case 0x44: return m_ly;
        case 0x45: return m_lyc;
        case 0x47: return m_bgp;
        case 0x48: return m_obp0;
        case 0x49: return m_obp1;
        case 0x4A: return m_wy;
        case 0x4B: return m_wx;

        // CGB palette registers
        case 0x68: return m_bcps | 0x40;
        case 0x69: return m_bg_palette[m_bcps & 0x3F];
        case 0x6A: return m_ocps | 0x40;
        case 0x6B: return m_obj_palette[m_ocps & 0x3F];

        default: return 0xFF;
    }
}

void PPU::write_register(uint16_t address, uint8_t value) {
    switch (address & 0xFF) {
        case 0x40:
            m_lcdc = value;
            if (!(value & 0x80)) {
                m_ly = 0;
                m_cycle = 0;
                m_mode = Mode::HBlank;
                m_stat &= 0xFC;
            }
            break;
        case 0x41: m_stat = (m_stat & 0x07) | (value & 0x78); break;
        case 0x42: m_scy = value; break;
        case 0x43: m_scx = value; break;
        case 0x45: m_lyc = value; break;
        case 0x47: m_bgp = value; break;
        case 0x48: m_obp0 = value; break;
        case 0x49: m_obp1 = value; break;
        case 0x4A: m_wy = value; break;
        case 0x4B: m_wx = value; break;

        // CGB palette registers
        case 0x68: m_bcps = value; break;
        case 0x69:
            m_bg_palette[m_bcps & 0x3F] = value;
            if (m_bcps & 0x80) m_bcps = (m_bcps & 0x80) | ((m_bcps + 1) & 0x3F);
            break;
        case 0x6A: m_ocps = value; break;
        case 0x6B:
            m_obj_palette[m_ocps & 0x3F] = value;
            if (m_ocps & 0x80) m_ocps = (m_ocps & 0x80) | ((m_ocps + 1) & 0x3F);
            break;
    }
}

void PPU::save_state(std::vector<uint8_t>& data) {
    data.insert(data.end(), m_vram.begin(), m_vram.end());
    data.insert(data.end(), m_oam.begin(), m_oam.end());

    data.push_back(m_lcdc);
    data.push_back(m_stat);
    data.push_back(m_scy);
    data.push_back(m_scx);
    data.push_back(m_ly);
    data.push_back(m_lyc);
    data.push_back(m_bgp);
    data.push_back(m_obp0);
    data.push_back(m_obp1);
    data.push_back(m_wy);
    data.push_back(m_wx);

    data.push_back(m_cycle & 0xFF);
    data.push_back((m_cycle >> 8) & 0xFF);
    data.push_back(static_cast<uint8_t>(m_mode));
    data.push_back(m_window_line);

    if (m_cgb_mode) {
        data.insert(data.end(), m_bg_palette.begin(), m_bg_palette.end());
        data.insert(data.end(), m_obj_palette.begin(), m_obj_palette.end());
        data.push_back(m_bcps);
        data.push_back(m_ocps);
    }
}

void PPU::load_state(const uint8_t*& data, size_t& remaining) {
    std::memcpy(m_vram.data(), data, m_vram.size());
    data += m_vram.size();
    remaining -= m_vram.size();

    std::memcpy(m_oam.data(), data, m_oam.size());
    data += m_oam.size();
    remaining -= m_oam.size();

    m_lcdc = *data++; remaining--;
    m_stat = *data++; remaining--;
    m_scy = *data++; remaining--;
    m_scx = *data++; remaining--;
    m_ly = *data++; remaining--;
    m_lyc = *data++; remaining--;
    m_bgp = *data++; remaining--;
    m_obp0 = *data++; remaining--;
    m_obp1 = *data++; remaining--;
    m_wy = *data++; remaining--;
    m_wx = *data++; remaining--;

    m_cycle = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
    m_mode = static_cast<Mode>(*data++); remaining--;
    m_window_line = *data++; remaining--;

    if (m_cgb_mode) {
        std::memcpy(m_bg_palette.data(), data, m_bg_palette.size());
        data += m_bg_palette.size();
        remaining -= m_bg_palette.size();

        std::memcpy(m_obj_palette.data(), data, m_obj_palette.size());
        data += m_obj_palette.size();
        remaining -= m_obj_palette.size();

        m_bcps = *data++; remaining--;
        m_ocps = *data++; remaining--;
    }
}

} // namespace gb
