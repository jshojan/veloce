#include "ppu.hpp"
#include "bus.hpp"

#include <cstring>

namespace nes {

// NES color palette (2C02) - ABGR format for OpenGL RGBA on little-endian
const uint32_t PPU::s_palette[64] = {
    0xFF545454, 0xFF741E00, 0xFF901008, 0xFF880030, 0xFF640044, 0xFF30005C, 0xFF000454, 0xFF00183C,
    0xFF002A20, 0xFF003A08, 0xFF004000, 0xFF003C00, 0xFF3C3200, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF989698, 0xFFC44C08, 0xFFEC3230, 0xFFE41E5C, 0xFFB01488, 0xFF6414A0, 0xFF202298, 0xFF003C78,
    0xFF005A54, 0xFF007228, 0xFF007C08, 0xFF287600, 0xFF786600, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFECEEEC, 0xFFEC9A4C, 0xFFEC7C78, 0xFFEC62B0, 0xFFEC54E4, 0xFFB458EC, 0xFF646AEC, 0xFF2088D4,
    0xFF00AAA0, 0xFF00C474, 0xFF20D04C, 0xFF6CCC38, 0xFFCCB438, 0xFF3C3C3C, 0xFF000000, 0xFF000000,
    0xFFECEEEC, 0xFFECCCA8, 0xFFECBCBC, 0xFFECB2D4, 0xFFECAEEC, 0xFFD4AEEC, 0xFFB0B4EC, 0xFF90C4E4,
    0xFF78D2CC, 0xFF78DEB4, 0xFF90E2A8, 0xFFB4E298, 0xFFE4D6A0, 0xFFA0A2A0, 0xFF000000, 0xFF000000,
};

PPU::PPU(Bus& bus) : m_bus(bus) {
    reset();
}

PPU::~PPU() = default;

void PPU::reset() {
    m_ctrl = 0;
    m_mask = 0;
    m_status = 0;
    m_oam_addr = 0;
    m_v = 0;
    m_t = 0;
    m_x = 0;
    m_w = false;
    m_data_buffer = 0;
    m_scanline = 0;
    m_cycle = 0;
    m_frame = 0;
    m_odd_frame = false;
    m_nmi_occurred = false;
    m_nmi_output = false;
    m_nmi_triggered = false;
    m_frame_complete = false;

    m_oam.fill(0);
    m_nametable.fill(0);
    m_palette.fill(0);
    m_framebuffer.fill(0);
}

void PPU::step() {
    // Visible scanlines (0-239)
    if (m_scanline >= 0 && m_scanline < 240) {
        if (m_cycle >= 1 && m_cycle <= 256) {
            render_pixel();

            // Background fetches
            update_shifters();

            switch ((m_cycle - 1) % 8) {
                case 0:
                    load_background_shifters();
                    m_bg_next_tile_id = m_bus.ppu_read(0x2000 | (m_v & 0x0FFF));
                    break;
                case 2:
                    m_bg_next_tile_attrib = m_bus.ppu_read(0x23C0 | (m_v & 0x0C00) |
                        ((m_v >> 4) & 0x38) | ((m_v >> 2) & 0x07));
                    if (m_v & 0x40) m_bg_next_tile_attrib >>= 4;
                    if (m_v & 0x02) m_bg_next_tile_attrib >>= 2;
                    break;
                case 4: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7);
                    m_bg_next_tile_lo = m_bus.ppu_read(addr);
                    break;
                }
                case 6: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7) + 8;
                    m_bg_next_tile_hi = m_bus.ppu_read(addr);
                    break;
                }
                case 7:
                    // Increment horizontal
                    if ((m_mask & 0x18) != 0) {
                        if ((m_v & 0x001F) == 31) {
                            m_v &= ~0x001F;
                            m_v ^= 0x0400;
                        } else {
                            m_v++;
                        }
                    }
                    break;
            }
        }

        // Increment vertical at cycle 256
        if (m_cycle == 256 && (m_mask & 0x18) != 0) {
            if ((m_v & 0x7000) != 0x7000) {
                m_v += 0x1000;
            } else {
                m_v &= ~0x7000;
                int y = (m_v & 0x03E0) >> 5;
                if (y == 29) {
                    y = 0;
                    m_v ^= 0x0800;
                } else if (y == 31) {
                    y = 0;
                } else {
                    y++;
                }
                m_v = (m_v & ~0x03E0) | (y << 5);
            }
        }

        // Copy horizontal bits at cycle 257
        if (m_cycle == 257 && (m_mask & 0x18) != 0) {
            m_v = (m_v & ~0x041F) | (m_t & 0x041F);
        }

        // Sprite evaluation at cycle 257
        if (m_cycle == 257) {
            evaluate_sprites();
        }

        // MMC3 scanline counter - clock at cycle 260 during sprite fetch phase
        // This timing is critical for games like Kirby's Adventure that use mid-screen IRQs
        if (m_cycle == 260 && (m_mask & 0x18) != 0) {
            m_bus.mapper_scanline();
        }

        // Prefetch first two tiles for next scanline during cycles 321-340
        // This primes the shifters so pixels 0-15 of the next scanline render correctly
        // Shifters must be updated during prefetch to position data correctly
        if (m_cycle >= 321 && m_cycle <= 340 && (m_mask & 0x18) != 0) {
            update_shifters();

            switch ((m_cycle - 1) % 8) {
                case 0:
                    load_background_shifters();
                    m_bg_next_tile_id = m_bus.ppu_read(0x2000 | (m_v & 0x0FFF));
                    break;
                case 2:
                    m_bg_next_tile_attrib = m_bus.ppu_read(0x23C0 | (m_v & 0x0C00) |
                        ((m_v >> 4) & 0x38) | ((m_v >> 2) & 0x07));
                    if (m_v & 0x40) m_bg_next_tile_attrib >>= 4;
                    if (m_v & 0x02) m_bg_next_tile_attrib >>= 2;
                    break;
                case 4: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7);
                    m_bg_next_tile_lo = m_bus.ppu_read(addr);
                    break;
                }
                case 6: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7) + 8;
                    m_bg_next_tile_hi = m_bus.ppu_read(addr);
                    break;
                }
                case 7:
                    // Increment horizontal for prefetch
                    if ((m_v & 0x001F) == 31) {
                        m_v &= ~0x001F;
                        m_v ^= 0x0400;
                    } else {
                        m_v++;
                    }
                    break;
            }
        }
    }

    // Pre-render scanline (261)
    if (m_scanline == 261) {
        if (m_cycle == 1) {
            m_status &= ~0xE0;  // Clear VBlank, Sprite 0, Overflow
            m_nmi_occurred = false;
        }

        // Copy vertical bits during cycles 280-304
        if (m_cycle >= 280 && m_cycle <= 304 && (m_mask & 0x18) != 0) {
            m_v = (m_v & ~0x7BE0) | (m_t & 0x7BE0);
        }

        // Evaluate sprites for scanline 0 at cycle 257
        // This ensures sprites are ready before scanline 0 starts rendering
        if (m_cycle == 257) {
            evaluate_sprites_for_scanline(0);
        }

        // MMC3 scanline counter also clocks on pre-render scanline
        if (m_cycle == 260 && (m_mask & 0x18) != 0) {
            m_bus.mapper_scanline();
        }

        // Prefetch first two tiles for scanline 0 during cycles 321-340
        if (m_cycle >= 321 && m_cycle <= 340 && (m_mask & 0x18) != 0) {
            update_shifters();

            switch ((m_cycle - 1) % 8) {
                case 0:
                    load_background_shifters();
                    m_bg_next_tile_id = m_bus.ppu_read(0x2000 | (m_v & 0x0FFF));
                    break;
                case 2:
                    m_bg_next_tile_attrib = m_bus.ppu_read(0x23C0 | (m_v & 0x0C00) |
                        ((m_v >> 4) & 0x38) | ((m_v >> 2) & 0x07));
                    if (m_v & 0x40) m_bg_next_tile_attrib >>= 4;
                    if (m_v & 0x02) m_bg_next_tile_attrib >>= 2;
                    break;
                case 4: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7);
                    m_bg_next_tile_lo = m_bus.ppu_read(addr);
                    break;
                }
                case 6: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7) + 8;
                    m_bg_next_tile_hi = m_bus.ppu_read(addr);
                    break;
                }
                case 7:
                    // Increment horizontal for prefetch
                    if ((m_v & 0x001F) == 31) {
                        m_v &= ~0x001F;
                        m_v ^= 0x0400;
                    } else {
                        m_v++;
                    }
                    break;
            }
        }
    }

    // VBlank start - frame is complete and ready for display
    if (m_scanline == 241 && m_cycle == 1) {
        m_status |= 0x80;  // Set VBlank flag
        m_nmi_occurred = true;
        m_frame_complete = true;  // Signal frame is ready
        if (m_nmi_output) {
            m_nmi_triggered = true;
        }
    }

    // Advance timing
    m_cycle++;
    if (m_cycle > 340) {
        m_cycle = 0;
        m_scanline++;
        if (m_scanline > 261) {
            m_scanline = 0;
            m_frame++;
            m_odd_frame = !m_odd_frame;

            // Skip cycle on odd frames when rendering enabled
            if (m_odd_frame && (m_mask & 0x18) != 0) {
                m_cycle = 1;
            }
        }
    }
}

uint8_t PPU::cpu_read(uint16_t address) {
    uint8_t data = 0;

    switch (address) {
        case 2: // PPUSTATUS
            data = (m_status & 0xE0) | (m_data_buffer & 0x1F);
            m_status &= ~0x80;  // Clear VBlank
            m_nmi_occurred = false;
            m_w = false;
            break;

        case 4: // OAMDATA
            data = m_oam[m_oam_addr];
            break;

        case 7: // PPUDATA
            data = m_data_buffer;
            m_data_buffer = ppu_read(m_v);

            // Palette reads are not buffered
            if (m_v >= 0x3F00) {
                data = m_data_buffer;
            }

            // Increment VRAM address
            m_v += (m_ctrl & 0x04) ? 32 : 1;
            break;
    }

    return data;
}

void PPU::cpu_write(uint16_t address, uint8_t value) {
    switch (address) {
        case 0: // PPUCTRL
            m_ctrl = value;
            m_t = (m_t & ~0x0C00) | ((value & 0x03) << 10);
            m_nmi_output = (value & 0x80) != 0;
            if (m_nmi_output && m_nmi_occurred) {
                m_nmi_triggered = true;
            }
            break;

        case 1: // PPUMASK
            m_mask = value;
            break;

        case 3: // OAMADDR
            m_oam_addr = value;
            break;

        case 4: // OAMDATA
            m_oam[m_oam_addr++] = value;
            break;

        case 5: // PPUSCROLL
            if (!m_w) {
                m_t = (m_t & ~0x001F) | (value >> 3);
                m_x = value & 0x07;
            } else {
                m_t = (m_t & ~0x73E0) | ((value & 0x07) << 12) | ((value & 0xF8) << 2);
            }
            m_w = !m_w;
            break;

        case 6: // PPUADDR
            if (!m_w) {
                m_t = (m_t & 0x00FF) | ((value & 0x3F) << 8);
            } else {
                m_t = (m_t & 0xFF00) | value;
                m_v = m_t;
            }
            m_w = !m_w;
            break;

        case 7: // PPUDATA
            ppu_write(m_v, value);
            m_v += (m_ctrl & 0x04) ? 32 : 1;
            break;
    }
}

uint8_t PPU::ppu_read(uint16_t address) {
    address &= 0x3FFF;

    if (address < 0x2000) {
        return m_bus.ppu_read(address);
    }
    else if (address < 0x3F00) {
        // Nametables with mirroring - get current mode from mapper
        address &= 0x0FFF;
        int mirror = m_bus.get_mirror_mode();
        switch (mirror) {
            case 0:  // Horizontal mirroring
                // NT0($2000) and NT1($2400) share first 1KB
                // NT2($2800) and NT3($2C00) share second 1KB
                if (address >= 0x0800) {
                    address = 0x0400 + (address & 0x03FF);
                } else {
                    address = address & 0x03FF;
                }
                break;
            case 1:  // Vertical mirroring
                // NT0($2000) and NT2($2800) share first 1KB
                // NT1($2400) and NT3($2C00) share second 1KB
                address &= 0x07FF;
                break;
            case 2:  // Single-screen, lower bank (first 1KB)
                address &= 0x03FF;
                break;
            case 3:  // Single-screen, upper bank (second 1KB)
                address = 0x0400 + (address & 0x03FF);
                break;
            case 4:  // Four-screen (no mirroring, needs 4KB VRAM on cart)
            default:
                address &= 0x0FFF;
                break;
        }
        return m_nametable[address];
    }
    else {
        // Palette
        address &= 0x1F;
        if (address == 0x10 || address == 0x14 || address == 0x18 || address == 0x1C) {
            address &= 0x0F;
        }
        return m_palette[address];
    }
}

void PPU::ppu_write(uint16_t address, uint8_t value) {
    address &= 0x3FFF;

    if (address < 0x2000) {
        m_bus.ppu_write(address, value);
    }
    else if (address < 0x3F00) {
        // Nametables with mirroring - get current mode from mapper
        address &= 0x0FFF;
        int mirror = m_bus.get_mirror_mode();
        switch (mirror) {
            case 0:  // Horizontal mirroring
                if (address >= 0x0800) {
                    address = 0x0400 + (address & 0x03FF);
                } else {
                    address = address & 0x03FF;
                }
                break;
            case 1:  // Vertical mirroring
                address &= 0x07FF;
                break;
            case 2:  // Single-screen, lower bank (first 1KB)
                address &= 0x03FF;
                break;
            case 3:  // Single-screen, upper bank (second 1KB)
                address = 0x0400 + (address & 0x03FF);
                break;
            case 4:  // Four-screen
            default:
                address &= 0x0FFF;
                break;
        }
        m_nametable[address] = value;
    }
    else {
        // Palette
        address &= 0x1F;
        if (address == 0x10 || address == 0x14 || address == 0x18 || address == 0x1C) {
            address &= 0x0F;
        }
        m_palette[address] = value;
    }
}

void PPU::oam_write(int address, uint8_t value) {
    m_oam[address & 0xFF] = value;
}

bool PPU::check_nmi() {
    if (m_nmi_triggered) {
        m_nmi_triggered = false;
        return true;
    }
    return false;
}

bool PPU::check_frame_complete() {
    if (m_frame_complete) {
        m_frame_complete = false;
        return true;
    }
    return false;
}

void PPU::render_pixel() {
    int x = m_cycle - 1;
    int y = m_scanline;

    if (x < 0 || x >= 256 || y < 0 || y >= 240) return;

    uint8_t bg_pixel = 0;
    uint8_t bg_palette = 0;

    // Background rendering
    if (m_mask & 0x08) {
        if ((m_mask & 0x02) || x >= 8) {
            uint16_t bit = 0x8000 >> m_x;
            uint8_t p0 = (m_bg_shifter_pattern_lo & bit) ? 1 : 0;
            uint8_t p1 = (m_bg_shifter_pattern_hi & bit) ? 2 : 0;
            bg_pixel = p0 | p1;

            uint8_t a0 = (m_bg_shifter_attrib_lo & bit) ? 1 : 0;
            uint8_t a1 = (m_bg_shifter_attrib_hi & bit) ? 2 : 0;
            bg_palette = a0 | a1;
        }
    }

    // Sprite rendering
    uint8_t sprite_pixel = 0;
    uint8_t sprite_palette = 0;
    uint8_t sprite_priority = 0;

    if (m_mask & 0x10) {
        if ((m_mask & 0x04) || x >= 8) {
            m_sprite_zero_rendering = false;

            for (int i = 0; i < m_sprite_count; i++) {
                if (m_scanline_sprites[i].x == 0) {
                    uint8_t p0 = (m_sprite_shifter_lo[i] & 0x80) ? 1 : 0;
                    uint8_t p1 = (m_sprite_shifter_hi[i] & 0x80) ? 2 : 0;
                    uint8_t pixel = p0 | p1;

                    if (pixel != 0) {
                        // Check if this is OAM sprite 0 (not just index 0 in scanline list)
                        if (i == m_sprite_zero_index) m_sprite_zero_rendering = true;
                        sprite_pixel = pixel;
                        sprite_palette = (m_scanline_sprites[i].attr & 0x03) + 4;
                        sprite_priority = (m_scanline_sprites[i].attr & 0x20) ? 1 : 0;
                        break;
                    }
                }
            }
        }
    }

    // Combine background and sprite
    uint8_t pixel = 0;
    uint8_t palette = 0;

    if (bg_pixel == 0 && sprite_pixel == 0) {
        pixel = 0;
        palette = 0;
    } else if (bg_pixel == 0 && sprite_pixel != 0) {
        pixel = sprite_pixel;
        palette = sprite_palette;
    } else if (bg_pixel != 0 && sprite_pixel == 0) {
        pixel = bg_pixel;
        palette = bg_palette;
    } else {
        // Sprite 0 hit detection
        if (m_sprite_zero_hit_possible && m_sprite_zero_rendering) {
            if ((m_mask & 0x18) == 0x18) {
                if (!((m_mask & 0x06) != 0x06 && x < 8)) {
                    m_status |= 0x40;
                }
            }
        }

        if (sprite_priority == 0) {
            pixel = sprite_pixel;
            palette = sprite_palette;
        } else {
            pixel = bg_pixel;
            palette = bg_palette;
        }
    }

    // Get color from palette
    uint8_t color_index = ppu_read(0x3F00 + (palette << 2) + pixel) & 0x3F;
    m_framebuffer[y * 256 + x] = s_palette[color_index];

    // Update sprite shifters
    for (int i = 0; i < m_sprite_count; i++) {
        if (m_scanline_sprites[i].x > 0) {
            m_scanline_sprites[i].x--;
        } else {
            m_sprite_shifter_lo[i] <<= 1;
            m_sprite_shifter_hi[i] <<= 1;
        }
    }
}

void PPU::evaluate_sprites() {
    evaluate_sprites_for_scanline(m_scanline);
}

void PPU::evaluate_sprites_for_scanline(int scanline) {
    m_sprite_count = 0;
    m_sprite_zero_hit_possible = false;
    m_sprite_zero_index = -1;

    for (int i = 0; i < 8; i++) {
        m_sprite_shifter_lo[i] = 0;
        m_sprite_shifter_hi[i] = 0;
    }

    uint8_t sprite_height = (m_ctrl & 0x20) ? 16 : 8;

    for (int i = 0; i < 64 && m_sprite_count < 8; i++) {
        int diff = scanline - m_oam[i * 4];

        if (diff >= 0 && diff < sprite_height) {
            if (m_sprite_count < 8) {
                if (i == 0) {
                    m_sprite_zero_hit_possible = true;
                    m_sprite_zero_index = m_sprite_count;  // Track where sprite 0 is in the list
                }

                m_scanline_sprites[m_sprite_count].y = m_oam[i * 4];
                m_scanline_sprites[m_sprite_count].tile = m_oam[i * 4 + 1];
                m_scanline_sprites[m_sprite_count].attr = m_oam[i * 4 + 2];
                m_scanline_sprites[m_sprite_count].x = m_oam[i * 4 + 3];

                // Fetch sprite pattern
                uint16_t addr;
                uint8_t row = diff;

                if (m_scanline_sprites[m_sprite_count].attr & 0x80) {
                    // Vertical flip
                    row = sprite_height - 1 - row;
                }

                if (sprite_height == 16) {
                    addr = ((m_scanline_sprites[m_sprite_count].tile & 0x01) << 12) |
                           ((m_scanline_sprites[m_sprite_count].tile & 0xFE) << 4);
                    if (row >= 8) {
                        addr += 16;
                        row -= 8;
                    }
                } else {
                    addr = ((m_ctrl & 0x08) << 9) |
                           (m_scanline_sprites[m_sprite_count].tile << 4);
                }
                addr += row;

                uint8_t lo = m_bus.ppu_read(addr);
                uint8_t hi = m_bus.ppu_read(addr + 8);

                // Horizontal flip
                if (m_scanline_sprites[m_sprite_count].attr & 0x40) {
                    // Bit reverse
                    auto flip = [](uint8_t b) {
                        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
                        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
                        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
                        return b;
                    };
                    lo = flip(lo);
                    hi = flip(hi);
                }

                m_sprite_shifter_lo[m_sprite_count] = lo;
                m_sprite_shifter_hi[m_sprite_count] = hi;

                m_sprite_count++;
            }
        }
    }
}

void PPU::load_background_shifters() {
    m_bg_shifter_pattern_lo = (m_bg_shifter_pattern_lo & 0xFF00) | m_bg_next_tile_lo;
    m_bg_shifter_pattern_hi = (m_bg_shifter_pattern_hi & 0xFF00) | m_bg_next_tile_hi;

    m_bg_shifter_attrib_lo = (m_bg_shifter_attrib_lo & 0xFF00) |
        ((m_bg_next_tile_attrib & 0x01) ? 0xFF : 0x00);
    m_bg_shifter_attrib_hi = (m_bg_shifter_attrib_hi & 0xFF00) |
        ((m_bg_next_tile_attrib & 0x02) ? 0xFF : 0x00);
}

void PPU::update_shifters() {
    if (m_mask & 0x08) {
        m_bg_shifter_pattern_lo <<= 1;
        m_bg_shifter_pattern_hi <<= 1;
        m_bg_shifter_attrib_lo <<= 1;
        m_bg_shifter_attrib_hi <<= 1;
    }
}

// Serialization helpers
namespace {
    template<typename T>
    void write_value(std::vector<uint8_t>& data, T value) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
        data.insert(data.end(), ptr, ptr + sizeof(T));
    }

    template<typename T>
    bool read_value(const uint8_t*& data, size_t& remaining, T& value) {
        if (remaining < sizeof(T)) return false;
        std::memcpy(&value, data, sizeof(T));
        data += sizeof(T);
        remaining -= sizeof(T);
        return true;
    }

    void write_array(std::vector<uint8_t>& data, const uint8_t* arr, size_t size) {
        data.insert(data.end(), arr, arr + size);
    }

    bool read_array(const uint8_t*& data, size_t& remaining, uint8_t* arr, size_t size) {
        if (remaining < size) return false;
        std::memcpy(arr, data, size);
        data += size;
        remaining -= size;
        return true;
    }
}

void PPU::save_state(std::vector<uint8_t>& data) {
    // PPU registers
    write_value(data, m_ctrl);
    write_value(data, m_mask);
    write_value(data, m_status);
    write_value(data, m_oam_addr);

    // Internal registers
    write_value(data, m_v);
    write_value(data, m_t);
    write_value(data, m_x);
    write_value(data, static_cast<uint8_t>(m_w ? 1 : 0));
    write_value(data, m_data_buffer);

    // Timing
    write_value(data, m_scanline);
    write_value(data, m_cycle);
    write_value(data, m_frame);
    write_value(data, static_cast<uint8_t>(m_odd_frame ? 1 : 0));

    // NMI state
    write_value(data, static_cast<uint8_t>(m_nmi_occurred ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_nmi_output ? 1 : 0));

    // Background shifters
    write_value(data, m_bg_shifter_pattern_lo);
    write_value(data, m_bg_shifter_pattern_hi);
    write_value(data, m_bg_shifter_attrib_lo);
    write_value(data, m_bg_shifter_attrib_hi);
    write_value(data, m_bg_next_tile_id);
    write_value(data, m_bg_next_tile_attrib);
    write_value(data, m_bg_next_tile_lo);
    write_value(data, m_bg_next_tile_hi);

    // OAM
    write_array(data, m_oam.data(), m_oam.size());

    // Nametable RAM
    write_array(data, m_nametable.data(), m_nametable.size());

    // Palette RAM
    write_array(data, m_palette.data(), m_palette.size());

    // Mirroring
    write_value(data, m_mirroring);
}

void PPU::load_state(const uint8_t*& data, size_t& remaining) {
    // PPU registers
    read_value(data, remaining, m_ctrl);
    read_value(data, remaining, m_mask);
    read_value(data, remaining, m_status);
    read_value(data, remaining, m_oam_addr);

    // Internal registers
    read_value(data, remaining, m_v);
    read_value(data, remaining, m_t);
    read_value(data, remaining, m_x);
    uint8_t w_flag;
    read_value(data, remaining, w_flag);
    m_w = w_flag != 0;
    read_value(data, remaining, m_data_buffer);

    // Timing
    read_value(data, remaining, m_scanline);
    read_value(data, remaining, m_cycle);
    read_value(data, remaining, m_frame);
    uint8_t odd_flag;
    read_value(data, remaining, odd_flag);
    m_odd_frame = odd_flag != 0;

    // NMI state
    uint8_t nmi_occurred, nmi_output;
    read_value(data, remaining, nmi_occurred);
    read_value(data, remaining, nmi_output);
    m_nmi_occurred = nmi_occurred != 0;
    m_nmi_output = nmi_output != 0;
    m_nmi_triggered = false;
    m_frame_complete = false;

    // Background shifters
    read_value(data, remaining, m_bg_shifter_pattern_lo);
    read_value(data, remaining, m_bg_shifter_pattern_hi);
    read_value(data, remaining, m_bg_shifter_attrib_lo);
    read_value(data, remaining, m_bg_shifter_attrib_hi);
    read_value(data, remaining, m_bg_next_tile_id);
    read_value(data, remaining, m_bg_next_tile_attrib);
    read_value(data, remaining, m_bg_next_tile_lo);
    read_value(data, remaining, m_bg_next_tile_hi);

    // OAM
    read_array(data, remaining, m_oam.data(), m_oam.size());

    // Nametable RAM
    read_array(data, remaining, m_nametable.data(), m_nametable.size());

    // Palette RAM
    read_array(data, remaining, m_palette.data(), m_palette.size());

    // Mirroring
    read_value(data, remaining, m_mirroring);

    // Reset sprite state (will be recalculated)
    m_sprite_count = 0;
    m_sprite_zero_index = -1;
    m_sprite_zero_hit_possible = false;
    m_sprite_zero_rendering = false;
}

} // namespace nes
