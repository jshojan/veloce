#include "ppu.hpp"
#include "bus.hpp"

#include <cstring>

namespace nes {

// Standard NES palette (RP2C02 NTSC) - ABGR format for OpenGL RGBA on little-endian
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

// Vs. System RP2C03 RGB PPU palette (same colors as standard but through RGB output)
const uint32_t PPU::s_palette_rp2c03[64] = {
    0xFF585858, 0xFF00238C, 0xFF00139B, 0xFF2D0585, 0xFF5D0052, 0xFF7A0017, 0xFF7A0800, 0xFF5F1800,
    0xFF352A00, 0xFF093900, 0xFF003F00, 0xFF003C22, 0xFF00325D, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFA1A1A1, 0xFF0053EE, 0xFF153CFE, 0xFF6028E4, 0xFFA91D98, 0xFFD41E41, 0xFFD22C00, 0xFFAA4400,
    0xFF6C5E00, 0xFF2D7300, 0xFF007D06, 0xFF007852, 0xFF0069A9, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFF1FA5FE, 0xFF5E89FE, 0xFFB572FE, 0xFFFE65F6, 0xFFFE6790, 0xFFFE773C, 0xFFFE9308,
    0xFFC4B200, 0xFF79CA10, 0xFF3AD54A, 0xFF11D1A4, 0xFF06BFFE, 0xFF424242, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFFA0D9FE, 0xFFBDCCFE, 0xFFE1C2FE, 0xFFFEBCFB, 0xFFFEBDD0, 0xFFFEC5A9, 0xFFFED18E,
    0xFFE9DE86, 0xFFC7E992, 0xFFA8EEB0, 0xFF95ECD9, 0xFF91E4FE, 0xFFACACAC, 0xFF000000, 0xFF000000,
};

// Vs. System RP2C04-0001 palette (scrambled palette)
const uint32_t PPU::s_palette_rp2c04_0001[64] = {
    0xFFFFB6B6, 0xFF00FFFF, 0xFF6A6AFF, 0xFF9292FF, 0xFFB6B6FF, 0xFFDAB6FF, 0xFFFFB6FF, 0xFFFFB6DA,
    0xFFFFB6B6, 0xFFFFDAB6, 0xFFFFFFB6, 0xFFDAFFB6, 0xFFB6FFB6, 0xFF000000, 0xFFB6FFDA, 0xFFB6FFFF,
    0xFF006D6D, 0xFF246DFF, 0xFFFF6DFF, 0xFFB66DFF, 0xFF6D6DFF, 0xFF6D6DB6, 0xFF6D6D6D, 0xFF6D6D24,
    0xFF6D6D00, 0xFF6DB624, 0xFF6DFF6D, 0xFF24B66D, 0xFF006D6D, 0xFF000000, 0xFF6DB6B6, 0xFF6DB6FF,
    0xFF009292, 0xFF4892FF, 0xFFFF92FF, 0xFFDA92FF, 0xFF9292FF, 0xFF9292DA, 0xFF929292, 0xFF929248,
    0xFF929200, 0xFF92DA48, 0xFF92FF92, 0xFF48DA92, 0xFF009292, 0xFF484848, 0xFF92DADA, 0xFF92DAFF,
    0xFF00B6B6, 0xFF6CB6FF, 0xFFFFB6FF, 0xFFFEB6FF, 0xFFB6B6FF, 0xFFB6B6FE, 0xFFB6B6B6, 0xFFB6B66C,
    0xFFB6B600, 0xFFB6FE6C, 0xFFB6FFB6, 0xFF6CFEB6, 0xFF00B6B6, 0xFF6C6C6C, 0xFFB6FEFE, 0xFFB6FEFF,
};

// Vs. System RP2C04-0002 palette
const uint32_t PPU::s_palette_rp2c04_0002[64] = {
    0xFF000000, 0xFFFFB6FF, 0xFFFF9200, 0xFFDA6D00, 0xFFB64800, 0xFF920000, 0xFF6D0000, 0xFF480000,
    0xFF240000, 0xFF000000, 0xFF004800, 0xFF006D00, 0xFF009200, 0xFF000000, 0xFF00B600, 0xFF00DA00,
    0xFF000000, 0xFFFFDAFF, 0xFFFFB600, 0xFFFE9200, 0xFFDA6D00, 0xFFB64800, 0xFF924800, 0xFF6D2400,
    0xFF482400, 0xFF242400, 0xFF246D00, 0xFF249200, 0xFF24B600, 0xFF000000, 0xFF24DA00, 0xFF24FE00,
    0xFF484848, 0xFFFFFEFF, 0xFFFFDA00, 0xFFFFB600, 0xFFFE9200, 0xFFDA6D00, 0xFFB66D00, 0xFF924800,
    0xFF6D4800, 0xFF484800, 0xFF489200, 0xFF48B600, 0xFF48DA00, 0xFF242424, 0xFF48FE00, 0xFF48FF00,
    0xFF6C6C6C, 0xFFFFFFFF, 0xFFFFFF00, 0xFFFFDA00, 0xFFFFB600, 0xFFFE9200, 0xFFDA9200, 0xFFB66D00,
    0xFF926D00, 0xFF6D6D00, 0xFF6DB600, 0xFF6DDA00, 0xFF6DFE00, 0xFF484848, 0xFF6DFF00, 0xFF6DFF24,
};

// Vs. System RP2C04-0003 palette
const uint32_t PPU::s_palette_rp2c04_0003[64] = {
    0xFF000000, 0xFF0000FF, 0xFF0024FF, 0xFF0048FF, 0xFF006DFF, 0xFF0092FF, 0xFF00B6FF, 0xFF00DAFF,
    0xFF00FEFF, 0xFF00FFDA, 0xFF00FFB6, 0xFF00FF92, 0xFF00FF6D, 0xFF000000, 0xFF00FF48, 0xFF00FF24,
    0xFF000000, 0xFF2400FF, 0xFF2424FF, 0xFF2448FF, 0xFF246DFF, 0xFF2492FF, 0xFF24B6FF, 0xFF24DAFF,
    0xFF24FEFF, 0xFF24FFDA, 0xFF24FFB6, 0xFF24FF92, 0xFF24FF6D, 0xFF000000, 0xFF24FF48, 0xFF24FF24,
    0xFF484848, 0xFF4800FF, 0xFF4824FF, 0xFF4848FF, 0xFF486DFF, 0xFF4892FF, 0xFF48B6FF, 0xFF48DAFF,
    0xFF48FEFF, 0xFF48FFDA, 0xFF48FFB6, 0xFF48FF92, 0xFF48FF6D, 0xFF242424, 0xFF48FF48, 0xFF48FF24,
    0xFF6C6C6C, 0xFF6C00FF, 0xFF6C24FF, 0xFF6C48FF, 0xFF6C6DFF, 0xFF6C92FF, 0xFF6CB6FF, 0xFF6CDAFF,
    0xFF6CFEFF, 0xFF6CFFDA, 0xFF6CFFB6, 0xFF6CFF92, 0xFF6CFF6D, 0xFF484848, 0xFF6CFF48, 0xFF6CFF24,
};

// Vs. System RP2C04-0004 palette
const uint32_t PPU::s_palette_rp2c04_0004[64] = {
    0xFF000000, 0xFFFF0000, 0xFFFF2400, 0xFFFF4800, 0xFFFF6D00, 0xFFFF9200, 0xFFFFB600, 0xFFFFDA00,
    0xFFFFFE00, 0xFFDAFE00, 0xFFB6FE00, 0xFF92FE00, 0xFF6DFE00, 0xFF000000, 0xFF48FE00, 0xFF24FE00,
    0xFF000000, 0xFFFF0024, 0xFFFF2424, 0xFFFF4824, 0xFFFF6D24, 0xFFFF9224, 0xFFFFB624, 0xFFFFDA24,
    0xFFFFFE24, 0xFFDAFE24, 0xFFB6FE24, 0xFF92FE24, 0xFF6DFE24, 0xFF000000, 0xFF48FE24, 0xFF24FE24,
    0xFF484848, 0xFFFF0048, 0xFFFF2448, 0xFFFF4848, 0xFFFF6D48, 0xFFFF9248, 0xFFFFB648, 0xFFFFDA48,
    0xFFFFFE48, 0xFFDAFE48, 0xFFB6FE48, 0xFF92FE48, 0xFF6DFE48, 0xFF242424, 0xFF48FE48, 0xFF24FE48,
    0xFF6C6C6C, 0xFFFF006C, 0xFFFF246C, 0xFFFF486C, 0xFFFF6D6C, 0xFFFF926C, 0xFFFFB66C, 0xFFFFDA6C,
    0xFFFFFE6C, 0xFFDAFE6C, 0xFFB6FE6C, 0xFF92FE6C, 0xFF6DFE6C, 0xFF484848, 0xFF48FE6C, 0xFF24FE6C,
};

PPU::PPU(Bus& bus) : m_bus(bus) {
    reset();
}

PPU::~PPU() = default;

void PPU::set_region(Region region) {
    m_region = region;
    switch (region) {
        case Region::NTSC:
            m_scanlines_per_frame = 262;
            m_vblank_scanlines = 20;
            m_prerender_scanline = 261;
            break;
        case Region::PAL:
            m_scanlines_per_frame = 312;
            m_vblank_scanlines = 70;
            m_prerender_scanline = 311;
            break;
        case Region::Dendy:
            // Dendy has PAL-like scanline count but different VBlank timing
            // 312 scanlines total, but VBlank is 51 scanlines (not 70)
            // and the pre-render scanline is still 311
            m_scanlines_per_frame = 312;
            m_vblank_scanlines = 51;
            m_prerender_scanline = 311;
            break;
    }
}

void PPU::set_ppu_variant(PPUVariant variant) {
    m_variant = variant;
    switch (variant) {
        case PPUVariant::RP2C02:
        case PPUVariant::RP2C07:
        case PPUVariant::Dendy:
            m_current_palette = s_palette;
            break;
        case PPUVariant::RP2C03:
        case PPUVariant::RC2C05_01:
        case PPUVariant::RC2C05_02:
        case PPUVariant::RC2C05_03:
        case PPUVariant::RC2C05_04:
        case PPUVariant::RC2C05_05:
            m_current_palette = s_palette_rp2c03;
            break;
        case PPUVariant::RP2C04_0001:
            m_current_palette = s_palette_rp2c04_0001;
            break;
        case PPUVariant::RP2C04_0002:
            m_current_palette = s_palette_rp2c04_0002;
            break;
        case PPUVariant::RP2C04_0003:
            m_current_palette = s_palette_rp2c04_0003;
            break;
        case PPUVariant::RP2C04_0004:
            m_current_palette = s_palette_rp2c04_0004;
            break;
    }
}

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
    m_io_latch = 0;
    m_io_latch_decay_frame.fill(0);
    m_scanline = 0;
    m_cycle = 0;
    m_frame = 0;
    m_odd_frame = false;
    m_nmi_occurred = false;
    m_nmi_output = false;
    m_nmi_triggered = false;
    m_nmi_triggered_delayed = false;
    m_nmi_pending = false;
    m_nmi_delay = 0;
    m_nmi_latched = false;
    m_vbl_suppress = false;
    m_suppress_nmi = false;
    m_frame_complete = false;

    m_oam.fill(0);
    m_nametable.fill(0);
    m_palette.fill(0);
    m_framebuffer.fill(0);
}

void PPU::step() {
    // Calculate frame cycle for MMC3 A12 timing
    uint32_t frame_cycle = m_scanline * 341 + m_cycle;

    // Visible scanlines (0-239)
    if (m_scanline >= 0 && m_scanline < 240) {
        if (m_cycle >= 1 && m_cycle <= 256) {
            render_pixel();

            // Background fetches
            update_shifters();

            switch ((m_cycle - 1) % 8) {
                case 0: {
                    load_background_shifters();
                    uint16_t nt_addr = 0x2000 | (m_v & 0x0FFF);
                    m_bus.notify_ppu_address_bus(nt_addr, frame_cycle);  // A12 tracking for MMC3
                    m_bg_next_tile_id = m_bus.ppu_read(nt_addr, frame_cycle);
                    break;
                }
                case 2: {
                    uint16_t at_addr = 0x23C0 | (m_v & 0x0C00) | ((m_v >> 4) & 0x38) | ((m_v >> 2) & 0x07);
                    m_bus.notify_ppu_address_bus(at_addr, frame_cycle);  // A12 tracking for MMC3
                    m_bg_next_tile_attrib = m_bus.ppu_read(at_addr, frame_cycle);
                    if (m_v & 0x40) m_bg_next_tile_attrib >>= 4;
                    if (m_v & 0x02) m_bg_next_tile_attrib >>= 2;
                    break;
                }
                case 4: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7);
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    m_bg_next_tile_lo = m_bus.ppu_read(addr, frame_cycle);
                    break;
                }
                case 6: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7) + 8;
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    m_bg_next_tile_hi = m_bus.ppu_read(addr, frame_cycle);
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

        // Sprite fetches occur at cycles 257-320, with 8 sprites each taking 8 cycles:
        // Cycle N+0: garbage NT, N+2: garbage AT, N+4: pattern lo, N+6: pattern hi
        // For MMC3, A12 must toggle properly for each sprite's pattern fetches.
        if (m_cycle >= 257 && m_cycle <= 320 && (m_mask & 0x18) != 0) {
            int sprite_phase = (m_cycle - 257) % 8;
            int sprite_slot = (m_cycle - 257) / 8;

            // At cycle 257 (first sprite phase 0), do the sprite evaluation
            if (m_cycle == 257) {
                evaluate_sprites_for_next_scanline(m_scanline + 1);
            }

            switch (sprite_phase) {
                case 0: {
                    // Garbage nametable fetch - address is $2000 | (garbage)
                    uint16_t nt_addr = 0x2000 | 0x0FF;
                    m_bus.notify_ppu_address_bus(nt_addr, frame_cycle);
                    break;
                }
                case 2: {
                    // Garbage attribute fetch - address is $23C0 | (garbage)
                    uint16_t at_addr = 0x23C0;
                    m_bus.notify_ppu_address_bus(at_addr, frame_cycle);
                    break;
                }
                case 4: {
                    // Pattern lo fetch - use sprite data or dummy tile $FF
                    uint16_t addr = get_sprite_pattern_addr(sprite_slot, false);
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    uint8_t lo = m_bus.ppu_read(addr, frame_cycle);
                    if (sprite_slot < m_sprite_count) {
                        m_sprite_shifter_lo[sprite_slot] = maybe_flip_sprite_byte(sprite_slot, lo);
                    }
                    break;
                }
                case 6: {
                    // Pattern hi fetch
                    uint16_t addr = get_sprite_pattern_addr(sprite_slot, true);
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    uint8_t hi = m_bus.ppu_read(addr, frame_cycle);
                    if (sprite_slot < m_sprite_count) {
                        m_sprite_shifter_hi[sprite_slot] = maybe_flip_sprite_byte(sprite_slot, hi);
                    }
                    break;
                }
            }
        }

        // Prefetch first two tiles for next scanline during cycles 321-336
        // This primes the shifters so pixels 0-15 of the next scanline render correctly
        // Note: Cycles 337-340 are "garbage" nametable fetches that only serve to
        // clock the MMC3 scanline counter - they should NOT update shifters
        if (m_cycle >= 321 && m_cycle <= 336 && (m_mask & 0x18) != 0) {
            update_shifters();

            switch ((m_cycle - 1) % 8) {
                case 0: {
                    load_background_shifters();
                    uint16_t nt_addr = 0x2000 | (m_v & 0x0FFF);
                    m_bus.notify_ppu_address_bus(nt_addr, frame_cycle);
                    m_bg_next_tile_id = m_bus.ppu_read(nt_addr, frame_cycle);
                    break;
                }
                case 2: {
                    uint16_t at_addr = 0x23C0 | (m_v & 0x0C00) | ((m_v >> 4) & 0x38) | ((m_v >> 2) & 0x07);
                    m_bus.notify_ppu_address_bus(at_addr, frame_cycle);
                    m_bg_next_tile_attrib = m_bus.ppu_read(at_addr, frame_cycle);
                    if (m_v & 0x40) m_bg_next_tile_attrib >>= 4;
                    if (m_v & 0x02) m_bg_next_tile_attrib >>= 2;
                    break;
                }
                case 4: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7);
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    m_bg_next_tile_lo = m_bus.ppu_read(addr, frame_cycle);
                    break;
                }
                case 6: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7) + 8;
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    m_bg_next_tile_hi = m_bus.ppu_read(addr, frame_cycle);
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

        // Cycle 337: One final shift to complete the prefetch alignment
        // The prefetch loads happen at cycles 321 and 329, each followed by 7 shifts.
        // We need 8 shifts total after each load to move tile data to the correct position.
        // This extra shift at 337 completes the alignment for the second prefetch tile.
        if (m_cycle == 337 && (m_mask & 0x18) != 0) {
            update_shifters();
            // Also load the second prefetched tile into the low byte
            load_background_shifters();
        }

        // Cycles 337-340: Garbage nametable fetches (for MMC3 scanline counter clocking)
        // These reads toggle A12 which clocks the MMC3 counter, but the data is discarded
        if (m_cycle == 337 || m_cycle == 339) {
            if ((m_mask & 0x18) != 0) {
                // Perform dummy nametable read to toggle A12 for MMC3
                uint16_t nt_addr = 0x2000 | (m_v & 0x0FFF);
                m_bus.notify_ppu_address_bus(nt_addr, frame_cycle);
                m_bus.ppu_read(nt_addr, frame_cycle);
            }
        }
    }

    // Pre-render scanline (NTSC: 261, PAL: 311)
    // Note: VBL flag and m_nmi_occurred are cleared AFTER the cycle advance
    // (similar to VBL set timing) so that timing is consistent.
    // See the "VBL clear" section after the cycle advance.
    if (m_scanline == m_prerender_scanline) {
        if (m_cycle == 1) {
            // Reset suppression flags for the next frame
            m_vbl_suppress = false;
            m_suppress_nmi = false;
        }

        // Background fetches during cycles 1-256 (same as visible scanlines)
        // These are "dummy" fetches - we don't render pixels, but we DO make the
        // memory accesses. This is critical for MMC3 A12 timing.
        if (m_cycle >= 1 && m_cycle <= 256 && (m_mask & 0x18) != 0) {
            switch ((m_cycle - 1) % 8) {
                case 0: {
                    uint16_t nt_addr = 0x2000 | (m_v & 0x0FFF);
                    m_bus.notify_ppu_address_bus(nt_addr, frame_cycle);
                    m_bg_next_tile_id = m_bus.ppu_read(nt_addr, frame_cycle);
                    break;
                }
                case 2: {
                    uint16_t at_addr = 0x23C0 | (m_v & 0x0C00) | ((m_v >> 4) & 0x38) | ((m_v >> 2) & 0x07);
                    m_bus.notify_ppu_address_bus(at_addr, frame_cycle);
                    m_bus.ppu_read(at_addr, frame_cycle);
                    break;
                }
                case 4: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7);
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    m_bus.ppu_read(addr, frame_cycle);
                    break;
                }
                case 6: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7) + 8;
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    m_bus.ppu_read(addr, frame_cycle);
                    break;
                }
                case 7:
                    // Increment horizontal
                    if ((m_v & 0x001F) == 31) {
                        m_v &= ~0x001F;
                        m_v ^= 0x0400;
                    } else {
                        m_v++;
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

        // Sprite evaluation and pattern fetches for scanline 0 (cycles 257-320)
        // The pre-render scanline performs the same memory accesses as visible scanlines,
        // including sprite pattern fetches. This is critical for MMC3 A12 clocking -
        // when sprites use pattern table $1000 and background uses $0000, the A12
        // rising edge during sprite fetches provides the scanline counter clock.
        // Without this, MMC3 games with split-screen scrolling (like Kirby's Adventure)
        // will have jittery scroll splits.
        if (m_cycle >= 257 && m_cycle <= 320 && (m_mask & 0x18) != 0) {
            int sprite_phase = (m_cycle - 257) % 8;
            int sprite_slot = (m_cycle - 257) / 8;

            // At cycle 257 (first sprite phase 0), do the sprite evaluation for scanline 0
            if (m_cycle == 257) {
                evaluate_sprites_for_next_scanline(0);
            }

            switch (sprite_phase) {
                case 0: {
                    // Garbage nametable fetch - address is $2000 | (garbage)
                    uint16_t nt_addr = 0x2000 | 0x0FF;
                    m_bus.notify_ppu_address_bus(nt_addr, frame_cycle);
                    break;
                }
                case 2: {
                    // Garbage attribute fetch - address is $23C0 | (garbage)
                    uint16_t at_addr = 0x23C0;
                    m_bus.notify_ppu_address_bus(at_addr, frame_cycle);
                    break;
                }
                case 4: {
                    // Pattern lo fetch - use sprite data or dummy tile $FF
                    uint16_t addr = get_sprite_pattern_addr(sprite_slot, false);
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    uint8_t lo = m_bus.ppu_read(addr, frame_cycle);
                    if (sprite_slot < m_sprite_count) {
                        m_sprite_shifter_lo[sprite_slot] = maybe_flip_sprite_byte(sprite_slot, lo);
                    }
                    break;
                }
                case 6: {
                    // Pattern hi fetch
                    uint16_t addr = get_sprite_pattern_addr(sprite_slot, true);
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    uint8_t hi = m_bus.ppu_read(addr, frame_cycle);
                    if (sprite_slot < m_sprite_count) {
                        m_sprite_shifter_hi[sprite_slot] = maybe_flip_sprite_byte(sprite_slot, hi);
                    }
                    break;
                }
            }
        }

        // Copy vertical bits during cycles 280-304
        if (m_cycle >= 280 && m_cycle <= 304 && (m_mask & 0x18) != 0) {
            m_v = (m_v & ~0x7BE0) | (m_t & 0x7BE0);
        }

        // Prefetch first two tiles for scanline 0 during cycles 321-336
        if (m_cycle >= 321 && m_cycle <= 336 && (m_mask & 0x18) != 0) {
            update_shifters();

            switch ((m_cycle - 1) % 8) {
                case 0: {
                    load_background_shifters();
                    uint16_t nt_addr = 0x2000 | (m_v & 0x0FFF);
                    m_bus.notify_ppu_address_bus(nt_addr, frame_cycle);
                    m_bg_next_tile_id = m_bus.ppu_read(nt_addr, frame_cycle);
                    break;
                }
                case 2: {
                    uint16_t at_addr = 0x23C0 | (m_v & 0x0C00) | ((m_v >> 4) & 0x38) | ((m_v >> 2) & 0x07);
                    m_bus.notify_ppu_address_bus(at_addr, frame_cycle);
                    m_bg_next_tile_attrib = m_bus.ppu_read(at_addr, frame_cycle);
                    if (m_v & 0x40) m_bg_next_tile_attrib >>= 4;
                    if (m_v & 0x02) m_bg_next_tile_attrib >>= 2;
                    break;
                }
                case 4: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7);
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    m_bg_next_tile_lo = m_bus.ppu_read(addr, frame_cycle);
                    break;
                }
                case 6: {
                    uint16_t addr = ((m_ctrl & 0x10) << 8) + (m_bg_next_tile_id << 4) + ((m_v >> 12) & 7) + 8;
                    m_bus.notify_ppu_address_bus(addr, frame_cycle);
                    m_bg_next_tile_hi = m_bus.ppu_read(addr, frame_cycle);
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

        // Cycle 337: Final shift and load to complete prefetch alignment
        if (m_cycle == 337 && (m_mask & 0x18) != 0) {
            update_shifters();
            load_background_shifters();
        }

        // Cycles 337-340: Garbage nametable fetches (for MMC3 scanline counter clocking)
        if (m_cycle == 337 || m_cycle == 339) {
            if ((m_mask & 0x18) != 0) {
                uint16_t nt_addr = 0x2000 | (m_v & 0x0FFF);
                m_bus.notify_ppu_address_bus(nt_addr, frame_cycle);
                m_bus.ppu_read(nt_addr, frame_cycle);
            }
        }

    }

    // Post-render scanline 240 is idle, nothing happens

    // Advance timing first
    m_cycle++;

    // Odd frame cycle skip: On odd frames with rendering enabled, the PPU
    // skips cycle 340 of the pre-render scanline. The decision is made at cycle 340.
    // Note: PAL NES does NOT have the odd frame skip - only NTSC.
    //
    // IMPORTANT: Because our emulator runs CPU instructions atomically before
    // stepping the PPU, PPUMASK writes appear to happen earlier than they should.
    // A 4-cycle STY instruction that writes PPUMASK on its last cycle appears
    // to have the write visible for all 12 PPU cycles of that instruction.
    //
    // To compensate, if PPUMASK was written "too recently" (within the last
    // few PPU cycles), we use the PREVIOUS mask value for the skip decision.
    // This simulates the write happening on the last CPU cycle.
    if (m_scanline == m_prerender_scanline && m_cycle == 340 && m_odd_frame && m_region == Region::NTSC) {
        uint32_t decision_cycle = static_cast<uint32_t>(m_prerender_scanline * 341 + 340);
        uint32_t cycles_since_write = (decision_cycle >= m_mask_write_cycle)
            ? (decision_cycle - m_mask_write_cycle) : 0xFFFFFFFF;

        // Use previous mask value if write happened within last 2 PPU cycles
        uint8_t effective_mask = (cycles_since_write <= 2) ? m_mask_prev : m_mask;

        if ((effective_mask & 0x18) != 0) {
            // Skip cycle 340: jump directly to (0, 0)
            m_cycle = 0;
            m_scanline = 0;
            m_frame++;
            m_odd_frame = !m_odd_frame;
            // Notify mapper of new frame for timing reset
            m_bus.notify_frame_start();
        }
    }

    if (m_cycle > 340) {
        // Normal scanline wrap
        m_cycle = 0;
        m_scanline++;
        if (m_scanline >= m_scanlines_per_frame) {
            // Frame wrap (normal case, no skip)
            m_scanline = 0;
            m_frame++;
            m_odd_frame = !m_odd_frame;
            // Notify mapper of new frame for timing reset
            m_bus.notify_frame_start();
        }
    }

    // VBlank flag is CLEARED at the start of dot 1 (second dot) of the pre-render scanline.
    // According to blargg's 03-vbl_clear_time test, the expected pattern is:
    //   05 V (flag still set)
    //   06 - (flag cleared)
    // This means the clear takes effect after cycle 1 starts being processed.
    // The clear happens AFTER the cycle advance, and the flag becomes invisible
    // at cycle 1. Reads during cycle 0 see VBL set, reads at cycle 1+ see VBL clear.
    //
    // HOWEVER, for 07-nmi_on_timing, m_nmi_occurred must be cleared 1 cycle EARLIER
    // than the VBL status flag. This allows the VBL flag to be visible for reading
    // while preventing new NMI triggers. Offsets in 07-nmi_on_timing:
    //   04 N (NMI fires - m_nmi_occurred still true)
    //   05 - (NMI doesn't fire - m_nmi_occurred cleared)
    // This corresponds to clearing m_nmi_occurred at cycle 0, VBL at cycle 1.
    if (m_scanline == m_prerender_scanline && m_cycle == 0) {
        m_nmi_occurred = false;  // Clear 1 cycle before VBL status flag
    }
    if (m_scanline == m_prerender_scanline && m_cycle == 1) {
        m_status &= ~0xE0;  // Clear VBlank, Sprite 0, Overflow at cycle 1
    }

    // VBlank flag is set at the START of dot 1 (cycle 1) of scanline 241.
    // We set it here, AFTER the cycle advance, so that when PPU is at cycle 1,
    // the VBL flag is already set and visible to any CPU read.
    //
    // For suppression to work:
    // - A read at cycle 0 (before VBL set) sets m_vbl_suppress, preventing VBL from being set
    // - A read at cycle 1-2 (at/after VBL set) suppresses NMI but flag is visible
    if (m_scanline == m_vblank_start_scanline && m_cycle == 1) {
        m_frame_complete = true;  // Signal frame is ready

        if (!m_vbl_suppress) {
            m_status |= 0x80;  // Set VBlank flag
            m_nmi_occurred = true;
            if (m_nmi_output && !m_suppress_nmi) {
                // NMI has a propagation delay of ~15 PPU cycles (5 CPU cycles)
                m_nmi_delay = 15;
                // Latch the NMI - once generated, it will fire even if NMI is later disabled
                m_nmi_latched = true;
            }
        }
        m_vbl_suppress = false;  // Reset for next frame
    }

    // NMI trigger logic:
    // NMI is delayed by ~2 PPU cycles from when it's requested
    // This applies to both VBL NMI and PPUCTRL-enabled NMI

    // Handle delayed NMI countdown
    if (m_nmi_delay > 0) {
        m_nmi_delay--;
        if (m_nmi_delay == 0 && m_nmi_latched) {
            // Delay expired and NMI was latched - trigger NMI
            // The latched flag means NMI edge was generated and should fire
            // regardless of current m_nmi_output state (test 08-nmi_off_timing)
            if (!m_suppress_nmi) {
                m_nmi_triggered = true;
            }
            m_nmi_latched = false;
        }
    }
}

uint8_t PPU::cpu_read(uint16_t address) {
    // Apply per-bit open bus decay
    // Each bit decays to 0 after ~600ms (~36 frames at 60fps) if not refreshed with 1
    // Bits that were refreshed with 0 decay immediately (they're already 0)
    constexpr uint64_t DECAY_FRAMES = 36;
    for (int i = 0; i < 8; i++) {
        if (m_frame > m_io_latch_decay_frame[i] + DECAY_FRAMES) {
            m_io_latch &= ~(1 << i);  // Decay this bit to 0
        }
    }

    uint8_t data = m_io_latch;  // Default: return open bus for write-only registers

    switch (address) {
        case 0: // PPUCTRL (write-only) - return open bus
        case 1: // PPUMASK (write-only) - return open bus
        case 3: // OAMADDR (write-only) - return open bus
        case 5: // PPUSCROLL (write-only) - return open bus
        case 6: // PPUADDR (write-only) - return open bus
            // data already set to m_io_latch
            break;

        case 2: // PPUSTATUS
            // Upper 3 bits from status, lower 5 bits from open bus latch
            // Vs. System RC2C05 PPUs return specific values in the lower 5 bits
            // for copy protection purposes
            switch (m_variant) {
                case PPUVariant::RC2C05_01:
                    data = (m_status & 0xE0) | 0x1B;
                    break;
                case PPUVariant::RC2C05_02:
                    data = (m_status & 0xE0) | 0x3D;
                    break;
                case PPUVariant::RC2C05_03:
                    data = (m_status & 0xE0) | 0x1C;
                    break;
                case PPUVariant::RC2C05_04:
                    data = (m_status & 0xE0) | 0x1B;
                    break;
                case PPUVariant::RC2C05_05:
                    data = (m_status & 0xE0) | 0x00;
                    break;
                default:
                    data = (m_status & 0xE0) | (m_io_latch & 0x1F);
                    break;
            }

            // VBL suppression timing:
            // According to 06-suppression test:
            // - offset 04 (cycle 0): Read before VBL - no flag, no NMI (both suppressed)
            // - offset 05 (cycle 1): Read at VBL - flag set, no NMI (NMI suppressed)
            // - offset 06 (cycle 2): Read after VBL - flag set, no NMI (NMI still suppressed)
            // - offset 07+ (cycle 3+): Read after VBL - flag set, NMI fires normally
            if (m_scanline == m_vblank_start_scanline) {
                if (m_cycle == 0) {
                    // Reading 1 PPU clock before VBL set
                    // Suppress VBL flag from being set AND suppress NMI
                    m_vbl_suppress = true;
                    m_suppress_nmi = true;
                } else if (m_cycle == 1 || m_cycle == 2) {
                    // Reading at or just after VBL set
                    // VBL is already set (visible in returned data), but suppress NMI
                    m_suppress_nmi = true;
                    // Also cancel any pending NMI delay
                    m_nmi_delay = 0;
                }
                // At cycle 3+, NMI is NOT suppressed
            }

            m_status &= ~0x80;  // Clear VBlank
            // Reading $2002 clears VBL flag.
            // NMI behavior depends on timing:
            // - Suppression window (sl 241, cycle 0-2): Cancel any in-flight NMI
            // - Outside suppression: If NMI is in-flight (m_nmi_delay > 0), let it fire
            // - If no in-flight NMI, clear m_nmi_occurred to prevent PPUCTRL enable trigger
            if (m_scanline == m_vblank_start_scanline && m_cycle <= 2) {
                // Suppression window: cancel everything including latched NMI
                m_nmi_delay = 0;
                m_nmi_latched = false;
                m_nmi_occurred = false;
            } else if (m_nmi_delay == 0) {
                // No in-flight NMI, clear m_nmi_occurred
                m_nmi_occurred = false;
            }
            // If m_nmi_delay > 0 and outside suppression, keep m_nmi_occurred for in-flight NMI
            m_w = false;

            // Reading PPUSTATUS refreshes ONLY the upper 3 bits of the latch.
            // The lower 5 bits retain their previous decay value and continue
            // to decay based on when they were last written.
            // Use 'data' which contains the status before VBlank was cleared.
            m_io_latch = (data & 0xE0) | (m_io_latch & 0x1F);
            // Refresh decay timers for upper 3 bits that were read as 1
            for (int i = 5; i < 8; i++) {
                if (data & (1 << i)) {
                    m_io_latch_decay_frame[i] = m_frame;
                }
            }
            break;

        case 4: // OAMDATA
            data = m_oam[m_oam_addr];
            // Bits 2-4 of the attribute byte (byte 2 of each sprite) are unimplemented
            // and always read as 0. Byte 2 is at address % 4 == 2.
            if ((m_oam_addr & 0x03) == 2) {
                data &= 0xE3;  // Clear bits 2-4
            }
            // Reading OAMDATA refreshes all 8 bits of the latch
            m_io_latch = data;
            for (int i = 0; i < 8; i++) {
                if (data & (1 << i)) {
                    m_io_latch_decay_frame[i] = m_frame;
                }
            }
            break;

        case 7: { // PPUDATA
            data = m_data_buffer;
            m_data_buffer = ppu_read(m_v);

            // Palette reads are not buffered - return directly
            // But the latch behavior is special: palette reads put the palette value
            // in data, but the buffer gets the underlying nametable data at that address
            if (m_v >= 0x3F00) {
                data = m_data_buffer;
                // For palette reads, the lower 6 bits come from the palette,
                // upper 2 bits come from open bus
                data = (data & 0x3F) | (m_io_latch & 0xC0);
                // Refresh only lower 6 bits for palette reads
                m_io_latch = data;
                for (int i = 0; i < 6; i++) {
                    if (data & (1 << i)) {
                        m_io_latch_decay_frame[i] = m_frame;
                    }
                }
            } else {
                // Non-palette reads refresh all 8 bits
                m_io_latch = data;
                for (int i = 0; i < 8; i++) {
                    if (data & (1 << i)) {
                        m_io_latch_decay_frame[i] = m_frame;
                    }
                }
            }

            // Increment VRAM address and notify mapper (for MMC3 A12 clocking)
            uint16_t old_v = m_v;
            m_v += (m_ctrl & 0x04) ? 32 : 1;
            uint32_t fc = static_cast<uint32_t>(m_scanline * 341 + m_cycle);
            m_bus.notify_ppu_addr_change(old_v, m_v, fc);
            break;
        }
    }

    return data;
}

void PPU::cpu_write(uint16_t address, uint8_t value) {
    // Any write to any PPU register fills the IO latch (open bus)
    // and refreshes decay timers for bits that are 1
    m_io_latch = value;
    for (int i = 0; i < 8; i++) {
        if (value & (1 << i)) {
            m_io_latch_decay_frame[i] = m_frame;
        }
    }

    switch (address) {
        case 0: { // PPUCTRL
            bool was_nmi_enabled = m_nmi_output;
            m_ctrl = value;
            m_t = (m_t & ~0x0C00) | ((value & 0x03) << 10);
            m_nmi_output = (value & 0x80) != 0;

            // If NMI is being disabled (1->0 transition) and we're in the VBL window,
            // cancel any latched NMI. According to test 08-nmi_off_timing:
            // - offset 05-06 (disabling at/near VBL): No NMI
            // - offset 07+ (disabling 2+ cycles after VBL): NMI fires
            if (was_nmi_enabled && !m_nmi_output) {
                // VBL is set at cycle 1 of the VBlank start scanline. Cancellation window is cycles 1-2.
                if (m_scanline == m_vblank_start_scanline && m_cycle >= 1 && m_cycle <= 2) {
                    m_nmi_latched = false;
                    m_nmi_delay = 0;
                }
            }

            // NMI triggers on 0->1 transition of NMI enable while VBL flag is set.
            // The NMI should occur AFTER the NEXT instruction (delayed NMI).
            if (!was_nmi_enabled && m_nmi_output && m_nmi_occurred && !m_suppress_nmi) {
                m_nmi_triggered_delayed = true;
            }
            break;
        }

        case 1: // PPUMASK
            // Track when mask changes for accurate odd-frame skip timing.
            // The CPU executes instructions atomically, but the write "should" happen
            // on the last cycle of the instruction. Track the previous value so we
            // can use it if the skip decision point is too close to the write.
            m_mask_prev = m_mask;
            m_mask_write_cycle = static_cast<uint32_t>(m_scanline * 341 + m_cycle);
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
                uint16_t old_v = m_v;
                m_t = (m_t & 0xFF00) | value;
                m_v = m_t;
                // Notify mapper of address change (for MMC3 A12 clocking)
                uint32_t fc = static_cast<uint32_t>(m_scanline * 341 + m_cycle);
                m_bus.notify_ppu_addr_change(old_v, m_v, fc);
            }
            m_w = !m_w;
            break;

        case 7: { // PPUDATA
            ppu_write(m_v, value);
            // Increment VRAM address and notify mapper (for MMC3 A12 clocking)
            uint16_t old_v = m_v;
            m_v += (m_ctrl & 0x04) ? 32 : 1;
            uint32_t fc = static_cast<uint32_t>(m_scanline * 341 + m_cycle);
            m_bus.notify_ppu_addr_change(old_v, m_v, fc);
            break;
        }
    }
}

uint8_t PPU::ppu_read(uint16_t address) {
    address &= 0x3FFF;

    if (address < 0x2000) {
        // Ensure we have valid PPU cycle values for MMC3 timing
        int sl = (m_scanline >= 0 && m_scanline <= m_prerender_scanline) ? m_scanline : 0;
        int cy = (m_cycle >= 0 && m_cycle <= 340) ? m_cycle : 0;
        uint32_t fc = static_cast<uint32_t>(sl * 341 + cy);
        return m_bus.ppu_read(address, fc);
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

int PPU::check_nmi() {
    if (m_nmi_triggered) {
        m_nmi_triggered = false;
        return 1;  // Immediate NMI
    }
    if (m_nmi_triggered_delayed) {
        m_nmi_triggered_delayed = false;
        return 2;  // Delayed NMI (fire after next instruction)
    }
    return 0;  // No NMI
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

    // Overscan cropping: render black for top/bottom 8 rows
    if (m_crop_overscan && (y < 8 || y >= 232)) {
        m_framebuffer[y * 256 + x] = 0xFF000000;  // Black with full alpha
        return;
    }

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

    // Get color from palette (use current palette for region/Vs. System support)
    uint8_t color_index = ppu_read(0x3F00 + (palette << 2) + pixel) & 0x3F;
    m_framebuffer[y * 256 + x] = m_current_palette[color_index];

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
    uint32_t fc = m_scanline * 341 + m_cycle;
    evaluate_sprites_for_scanline(m_scanline, fc);
}

// New function: Only evaluate which sprites are on the next scanline, without fetching patterns
// Pattern fetches are now done incrementally during cycles 257-320
//
// This function implements the PPU sprite overflow bug. The NES PPU has a hardware bug
// where sprite overflow detection uses a glitched evaluation pattern once 8 sprites
// have been found. Instead of continuing to check byte 0 (Y position) of each sprite,
// it incorrectly increments both the sprite number (m) AND the byte offset (n).
//
// Normal evaluation (sprites 0-7, m_sprite_count < 8):
//   - Read OAM[m*4 + 0] for Y position
//   - If in range, copy all 4 bytes and increment m
//   - If not in range, just increment m
//
// Buggy overflow evaluation (after 8 sprites found):
//   - Start with n=0, m=next_sprite
//   - Read OAM[m*4 + n] as Y position  <-- BUG: n should always be 0!
//   - If "in range" (comparing wrong byte to scanline):
//     - Set overflow flag
//     - n = (n + 1) & 3, don't increment m
//   - If "not in range":
//     - n = (n + 1) & 3
//     - m = m + 1  <-- only increment m when "not in range"
//
// This causes false positives (overflow set when <= 8 sprites) and false negatives
// (overflow not set when > 8 sprites), depending on OAM contents.
//
// Reference: https://www.nesdev.org/wiki/PPU_sprite_evaluation
void PPU::evaluate_sprites_for_next_scanline(int scanline) {
    m_sprite_count = 0;
    m_sprite_zero_hit_possible = false;
    m_sprite_zero_index = -1;

    // Sprite limit: 8 when enabled (accurate), 64 when disabled (no flicker)
    int sprite_limit = m_sprite_limit_enabled ? 8 : MAX_SPRITES_PER_SCANLINE;

    for (int i = 0; i < sprite_limit; i++) {
        m_sprite_shifter_lo[i] = 0;
        m_sprite_shifter_hi[i] = 0;
    }

    uint8_t sprite_height = (m_ctrl & 0x20) ? 16 : 8;

    // Phase 1: Normal sprite evaluation (find sprites on scanline up to limit)
    int m = 0;  // OAM sprite index (0-63)
    while (m < 64 && m_sprite_count < sprite_limit) {
        int y = m_oam[m * 4];
        int diff = scanline - y;

        if (diff >= 0 && diff < sprite_height) {
            if (m == 0) {
                m_sprite_zero_hit_possible = true;
                m_sprite_zero_index = m_sprite_count;
            }

            m_scanline_sprites[m_sprite_count].y = m_oam[m * 4];
            m_scanline_sprites[m_sprite_count].tile = m_oam[m * 4 + 1];
            m_scanline_sprites[m_sprite_count].attr = m_oam[m * 4 + 2];
            m_scanline_sprites[m_sprite_count].x = m_oam[m * 4 + 3];

            m_sprite_count++;
        }
        m++;
    }

    // Phase 2: Buggy overflow evaluation (only if we found 8 sprites and there are more to check)
    // This implements the hardware bug in sprite overflow detection.
    // Skip this phase if sprite limit is disabled (no overflow possible)
    if (m_sprite_limit_enabled && m_sprite_count == 8 && m < 64) {
        int n = 0;  // Byte offset within sprite (should always be 0, but PPU bug increments it)

        while (m < 64) {
            // Bug: Read OAM[m*4 + n] instead of OAM[m*4 + 0]
            // This compares the wrong byte (could be tile, attr, or x) to the scanline
            int y = m_oam[m * 4 + n];
            int diff = scanline - y;

            if (diff >= 0 && diff < sprite_height) {
                // "In range" (may be a false positive due to reading wrong byte)
                // Set the overflow flag
                m_status |= 0x20;

                // Bug: increment n, but NOT m
                // This means we keep reading bytes from the same sprite
                // until we find one that's "not in range"
                n = (n + 1) & 3;

                // After setting overflow, the PPU stops evaluation
                // (it doesn't actually copy sprite data, just sets the flag)
                break;
            } else {
                // "Not in range"
                // Bug: increment BOTH n and m
                n = (n + 1) & 3;
                m++;
            }
        }
    }
}

// Get the pattern table address for a sprite slot's pattern fetch
uint16_t PPU::get_sprite_pattern_addr(int sprite_slot, bool hi_byte) {
    uint8_t sprite_height = (m_ctrl & 0x20) ? 16 : 8;
    uint16_t addr;

    if (sprite_slot < m_sprite_count) {
        // Real sprite - calculate address from sprite data
        const Sprite& sprite = m_scanline_sprites[sprite_slot];
        int diff = (m_scanline + 1) - sprite.y;  // +1 because we evaluate for NEXT scanline
        uint8_t row = diff;

        if (sprite.attr & 0x80) {
            // Vertical flip
            row = sprite_height - 1 - row;
        }

        if (sprite_height == 16) {
            addr = ((sprite.tile & 0x01) << 12) |
                   ((sprite.tile & 0xFE) << 4);
            if (row >= 8) {
                addr += 16;
                row -= 8;
            }
        } else {
            addr = ((m_ctrl & 0x08) << 9) | (sprite.tile << 4);
        }
        addr += row;
    } else {
        // Empty slot - use dummy tile $FF at row 0
        if (sprite_height == 16) {
            addr = 0x1FF0;  // 8x16 mode: tile $FF uses $1xxx
        } else {
            addr = ((m_ctrl & 0x08) << 9) | 0x0FF0;  // 8x8 mode: use PPUCTRL bit 3
        }
    }

    if (hi_byte) {
        addr += 8;
    }
    return addr;
}

// Apply horizontal flip to a sprite byte if needed
uint8_t PPU::maybe_flip_sprite_byte(int sprite_slot, uint8_t byte) {
    if (sprite_slot < m_sprite_count) {
        const Sprite& sprite = m_scanline_sprites[sprite_slot];
        if (sprite.attr & 0x40) {
            // Horizontal flip - bit reverse
            byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
            byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
            byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
        }
    }
    return byte;
}

// Legacy function - still needed for some call sites
void PPU::evaluate_sprites_for_scanline(int scanline, uint32_t frame_cycle) {
    evaluate_sprites_for_next_scanline(scanline);

    // Fetch all patterns at once (for backward compatibility with pre-render scanline etc.)
    uint8_t sprite_height = (m_ctrl & 0x20) ? 16 : 8;
    for (int i = 0; i < 8; i++) {
        uint16_t addr = get_sprite_pattern_addr(i, false);
        m_bus.notify_ppu_address_bus(addr, frame_cycle);
        uint8_t lo = m_bus.ppu_read(addr, frame_cycle);

        addr = get_sprite_pattern_addr(i, true);
        m_bus.notify_ppu_address_bus(addr, frame_cycle);
        uint8_t hi = m_bus.ppu_read(addr, frame_cycle);

        if (i < m_sprite_count) {
            m_sprite_shifter_lo[i] = maybe_flip_sprite_byte(i, lo);
            m_sprite_shifter_hi[i] = maybe_flip_sprite_byte(i, hi);
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
    write_value(data, m_mask_prev);
    write_value(data, m_mask_write_cycle);
    write_value(data, m_status);
    write_value(data, m_oam_addr);

    // Internal registers
    write_value(data, m_v);
    write_value(data, m_t);
    write_value(data, m_x);
    write_value(data, static_cast<uint8_t>(m_w ? 1 : 0));
    write_value(data, m_data_buffer);
    write_value(data, m_io_latch);
    write_value(data, m_io_latch_decay_frame);

    // Timing
    write_value(data, m_scanline);
    write_value(data, m_cycle);
    write_value(data, m_frame);
    write_value(data, static_cast<uint8_t>(m_odd_frame ? 1 : 0));

    // NMI state - all flags needed for cycle-accurate restoration
    write_value(data, static_cast<uint8_t>(m_nmi_occurred ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_nmi_output ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_nmi_triggered ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_nmi_triggered_delayed ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_nmi_pending ? 1 : 0));
    write_value(data, m_nmi_delay);
    write_value(data, static_cast<uint8_t>(m_nmi_latched ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_vbl_suppress ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_suppress_nmi ? 1 : 0));

    // Background shifters
    write_value(data, m_bg_shifter_pattern_lo);
    write_value(data, m_bg_shifter_pattern_hi);
    write_value(data, m_bg_shifter_attrib_lo);
    write_value(data, m_bg_shifter_attrib_hi);
    write_value(data, m_bg_next_tile_id);
    write_value(data, m_bg_next_tile_attrib);
    write_value(data, m_bg_next_tile_lo);
    write_value(data, m_bg_next_tile_hi);

    // Sprite state for accurate mid-frame restoration
    write_value(data, m_sprite_count);
    write_value(data, m_sprite_zero_index);
    write_value(data, static_cast<uint8_t>(m_sprite_zero_hit_possible ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_sprite_zero_rendering ? 1 : 0));

    // Scanline sprite data
    for (int i = 0; i < 8; i++) {
        write_value(data, m_scanline_sprites[i].y);
        write_value(data, m_scanline_sprites[i].tile);
        write_value(data, m_scanline_sprites[i].attr);
        write_value(data, m_scanline_sprites[i].x);
        write_value(data, m_sprite_shifter_lo[i]);
        write_value(data, m_sprite_shifter_hi[i]);
    }

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
    read_value(data, remaining, m_mask_prev);
    read_value(data, remaining, m_mask_write_cycle);
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
    read_value(data, remaining, m_io_latch);
    read_value(data, remaining, m_io_latch_decay_frame);

    // Timing
    read_value(data, remaining, m_scanline);
    read_value(data, remaining, m_cycle);
    read_value(data, remaining, m_frame);
    uint8_t odd_flag;
    read_value(data, remaining, odd_flag);
    m_odd_frame = odd_flag != 0;

    // NMI state - all flags for cycle-accurate restoration
    uint8_t nmi_occurred, nmi_output, nmi_triggered, nmi_triggered_delayed;
    uint8_t nmi_pending, nmi_latched, vbl_suppress, suppress_nmi;
    read_value(data, remaining, nmi_occurred);
    read_value(data, remaining, nmi_output);
    read_value(data, remaining, nmi_triggered);
    read_value(data, remaining, nmi_triggered_delayed);
    read_value(data, remaining, nmi_pending);
    read_value(data, remaining, m_nmi_delay);
    read_value(data, remaining, nmi_latched);
    read_value(data, remaining, vbl_suppress);
    read_value(data, remaining, suppress_nmi);
    m_nmi_occurred = nmi_occurred != 0;
    m_nmi_output = nmi_output != 0;
    m_nmi_triggered = nmi_triggered != 0;
    m_nmi_triggered_delayed = nmi_triggered_delayed != 0;
    m_nmi_pending = nmi_pending != 0;
    m_nmi_latched = nmi_latched != 0;
    m_vbl_suppress = vbl_suppress != 0;
    m_suppress_nmi = suppress_nmi != 0;
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

    // Sprite state
    read_value(data, remaining, m_sprite_count);
    read_value(data, remaining, m_sprite_zero_index);
    uint8_t sprite_zero_hit_possible, sprite_zero_rendering;
    read_value(data, remaining, sprite_zero_hit_possible);
    read_value(data, remaining, sprite_zero_rendering);
    m_sprite_zero_hit_possible = sprite_zero_hit_possible != 0;
    m_sprite_zero_rendering = sprite_zero_rendering != 0;

    // Scanline sprite data
    for (int i = 0; i < 8; i++) {
        read_value(data, remaining, m_scanline_sprites[i].y);
        read_value(data, remaining, m_scanline_sprites[i].tile);
        read_value(data, remaining, m_scanline_sprites[i].attr);
        read_value(data, remaining, m_scanline_sprites[i].x);
        read_value(data, remaining, m_sprite_shifter_lo[i]);
        read_value(data, remaining, m_sprite_shifter_hi[i]);
    }

    // OAM
    read_array(data, remaining, m_oam.data(), m_oam.size());

    // Nametable RAM
    read_array(data, remaining, m_nametable.data(), m_nametable.size());

    // Palette RAM
    read_array(data, remaining, m_palette.data(), m_palette.size());

    // Mirroring
    read_value(data, remaining, m_mirroring);
}

} // namespace nes
