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

    // Pre-render scanline (261)
    // Note: VBL flag and m_nmi_occurred are cleared AFTER the cycle advance
    // (similar to VBL set timing) so that timing is consistent.
    // See the "VBL clear" section after the cycle advance.
    if (m_scanline == 261) {
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
    // skips cycle 340 of scanline 261. The decision is made at cycle 340.
    //
    // IMPORTANT: Because our emulator runs CPU instructions atomically before
    // stepping the PPU, PPUMASK writes appear to happen earlier than they should.
    // A 4-cycle STY instruction that writes PPUMASK on its last cycle appears
    // to have the write visible for all 12 PPU cycles of that instruction.
    //
    // To compensate, if PPUMASK was written "too recently" (within the last
    // few PPU cycles), we use the PREVIOUS mask value for the skip decision.
    // This simulates the write happening on the last CPU cycle.
    if (m_scanline == 261 && m_cycle == 340 && m_odd_frame) {
        uint32_t decision_cycle = static_cast<uint32_t>(261 * 341 + 340);
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
        if (m_scanline > 261) {
            // Frame wrap (normal case, no skip)
            m_scanline = 0;
            m_frame++;
            m_odd_frame = !m_odd_frame;
            // Notify mapper of new frame for timing reset
            m_bus.notify_frame_start();
        }
    }

    // VBlank flag is CLEARED at cycle 1 of scanline 261 (pre-render scanline).
    // According to 07-nmi_on_timing test, the clear should happen such that
    // PPUCTRL writes at offset 05+ don't trigger NMI.
    // Testing shows we need to clear at m_cycle == 0 (after advancing from cycle 340->0->new scanline)
    // No wait - cycle 0 is at the START of the scanline. Let me try cycle 0.
    if (m_scanline == 261 && m_cycle == 0) {
        m_status &= ~0xE0;  // Clear VBlank, Sprite 0, Overflow
        m_nmi_occurred = false;
    }

    // VBlank flag is set at the START of dot 1 (cycle 1) of scanline 241.
    // We set it here, AFTER the cycle advance, so that when PPU is at cycle 1,
    // the VBL flag is already set and visible to any CPU read.
    //
    // For suppression to work:
    // - A read at cycle 0 (before VBL set) sets m_vbl_suppress, preventing VBL from being set
    // - A read at cycle 1-2 (at/after VBL set) suppresses NMI but flag is visible
    if (m_scanline == 241 && m_cycle == 1) {
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
    uint8_t data = 0;

    switch (address) {
        case 2: // PPUSTATUS
            data = (m_status & 0xE0) | (m_data_buffer & 0x1F);

            // VBL suppression timing:
            // According to 06-suppression test:
            // - offset 04 (cycle 0): Read before VBL - no flag, no NMI (both suppressed)
            // - offset 05 (cycle 1): Read at VBL - flag set, no NMI (NMI suppressed)
            // - offset 06 (cycle 2): Read after VBL - flag set, no NMI (NMI still suppressed)
            // - offset 07+ (cycle 3+): Read after VBL - flag set, NMI fires normally
            if (m_scanline == 241) {
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
            if (m_scanline == 241 && m_cycle <= 2) {
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
            break;

        case 4: // OAMDATA
            data = m_oam[m_oam_addr];
            break;

        case 7: { // PPUDATA
            data = m_data_buffer;
            m_data_buffer = ppu_read(m_v);

            // Palette reads are not buffered
            if (m_v >= 0x3F00) {
                data = m_data_buffer;
            }

            // Increment VRAM address and notify mapper (for MMC3 A12 clocking)
            uint16_t old_v = m_v;
            m_v += (m_ctrl & 0x04) ? 32 : 1;
            m_bus.notify_ppu_addr_change(old_v, m_v);
            break;
        }
    }

    return data;
}

void PPU::cpu_write(uint16_t address, uint8_t value) {
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
                // VBL is set at cycle 1 of sl 241. Cancellation window is cycles 1-2.
                if (m_scanline == 241 && m_cycle >= 1 && m_cycle <= 2) {
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
                m_bus.notify_ppu_addr_change(old_v, m_v);
            }
            m_w = !m_w;
            break;

        case 7: { // PPUDATA
            ppu_write(m_v, value);
            // Increment VRAM address and notify mapper (for MMC3 A12 clocking)
            uint16_t old_v = m_v;
            m_v += (m_ctrl & 0x04) ? 32 : 1;
            m_bus.notify_ppu_addr_change(old_v, m_v);
            break;
        }
    }
}

uint8_t PPU::ppu_read(uint16_t address) {
    address &= 0x3FFF;

    if (address < 0x2000) {
        // Ensure we have valid PPU cycle values for MMC3 timing
        int sl = (m_scanline >= 0 && m_scanline <= 261) ? m_scanline : 0;
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
    uint32_t fc = m_scanline * 341 + m_cycle;
    evaluate_sprites_for_scanline(m_scanline, fc);
}

// New function: Only evaluate which sprites are on the next scanline, without fetching patterns
// Pattern fetches are now done incrementally during cycles 257-320
void PPU::evaluate_sprites_for_next_scanline(int scanline) {
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
            if (i == 0) {
                m_sprite_zero_hit_possible = true;
                m_sprite_zero_index = m_sprite_count;
            }

            m_scanline_sprites[m_sprite_count].y = m_oam[i * 4];
            m_scanline_sprites[m_sprite_count].tile = m_oam[i * 4 + 1];
            m_scanline_sprites[m_sprite_count].attr = m_oam[i * 4 + 2];
            m_scanline_sprites[m_sprite_count].x = m_oam[i * 4 + 3];

            m_sprite_count++;
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
