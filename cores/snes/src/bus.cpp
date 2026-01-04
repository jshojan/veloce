#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "dma.hpp"
#include "cartridge.hpp"
#include "debug.hpp"
#include <cstring>

namespace snes {

Bus::Bus() {
    m_wram.fill(0);
    m_controller_state.fill(0);
    m_controller_latch.fill(0);
    m_wram_addr = 0;
    m_irq_lock = false;
    m_irq_lock_cycles = 0;
}

Bus::~Bus() = default;

// ============================================================================
// MEMORY ACCESS TIMING
// ============================================================================
// Reference: bsnes/sfc/cpu/timing.cpp, anomie's SNES docs, fullsnes
//
// SNES memory access speed varies by region:
// - SlowROM:    8 master cycles (2.68 MHz effective)
// - FastROM:    6 master cycles (3.58 MHz effective) - requires MEMSEL.0=1
// - WRAM:       8 master cycles
// - I/O:        6-12 master cycles depending on register
//
// FastROM only affects ROM in banks $80-$FF when:
// 1. The cartridge supports FastROM (header bit)
// 2. MEMSEL ($420D) bit 0 is set
// ============================================================================

bool Bus::is_fast_rom_enabled() const {
    // FastROM requires both MEMSEL bit 0 set AND cartridge FastROM support
    if (!m_cartridge) return false;
    return (m_memsel & 0x01) && m_cartridge->is_fast_rom();
}

int Bus::get_access_cycles(uint32_t address) const {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    // Banks $7E-$7F: WRAM - always 8 cycles
    if (bank == 0x7E || bank == 0x7F) {
        return 8;
    }

    // Banks $40-$7D: Cartridge space - 8 cycles (6 if FastROM and HiROM)
    if (bank >= 0x40 && bank <= 0x7D) {
        return is_fast_rom_enabled() ? 6 : 8;
    }

    // Banks $C0-$FF: ROM (HiROM upper banks or LoROM mirrors)
    if (bank >= 0xC0) {
        return is_fast_rom_enabled() ? 6 : 8;
    }

    // Banks $00-$3F and $80-$BF
    // Map $80-$BF to $00-$3F for timing lookup
    uint8_t effective_bank = bank;
    if (bank >= 0x80 && bank <= 0xBF) {
        effective_bank = bank - 0x80;
    }

    if (effective_bank <= 0x3F) {
        // $0000-$1FFF: WRAM mirror - 8 cycles
        if (offset < 0x2000) {
            return 8;
        }
        // $2000-$20FF: Unused - 6 cycles
        if (offset < 0x2100) {
            return 6;
        }
        // $2100-$21FF: PPU registers - 6 cycles
        if (offset < 0x2200) {
            return 6;
        }
        // $2200-$3FFF: Unused/expansion - 6 cycles
        if (offset < 0x4000) {
            return 6;
        }
        // $4000-$41FF: Joypad registers - 12 cycles (XSlow)
        if (offset < 0x4200) {
            return 12;
        }
        // $4200-$43FF: CPU/DMA registers - 6 cycles
        if (offset < 0x4400) {
            return 6;
        }
        // $4400-$5FFF: Unused - 6 cycles
        if (offset < 0x6000) {
            return 6;
        }
        // $6000-$7FFF: Expansion RAM/SRAM - 8 cycles
        if (offset < 0x8000) {
            return 8;
        }
        // $8000-$FFFF: ROM
        // In banks $80-$BF with FastROM enabled: 6 cycles
        // Otherwise: 8 cycles
        if (bank >= 0x80 && is_fast_rom_enabled()) {
            return 6;
        }
        return 8;
    }

    // Default: 8 cycles
    return 8;
}

uint8_t Bus::read(uint32_t address) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    // Banks $00-$3F and $80-$BF (mirrors)
    if (bank <= 0x3F || (bank >= 0x80 && bank <= 0xBF)) {
        if (offset < 0x2000) {
            // WRAM mirror (first 8KB)
            m_open_bus = m_wram[offset];
            return m_open_bus;
        }
        if (offset >= 0x2140 && offset < 0x2144) {
            // APU ports (must be checked BEFORE PPU range)
            if (m_apu) {
                m_open_bus = m_apu->read_port(offset - 0x2140);
                static int apu_read_count = 0;
                apu_read_count++;
                if (is_debug_mode() && apu_read_count <= 10) {
                    SNES_DEBUG_PRINT("APU port %d read: $%02X (count=%d)\n",
                        (int)(offset - 0x2140), m_open_bus, apu_read_count);
                }
                return m_open_bus;
            }
        }
        if (offset >= 0x2100 && offset < 0x2200) {
            // WRAM data port ($2180) has priority
            if (offset == 0x2180) {
                m_open_bus = m_wram[m_wram_addr];
                m_wram_addr = (m_wram_addr + 1) & 0x1FFFF;  // 17-bit wrap
                return m_open_bus;
            }
            // PPU registers ($2100-$213F, but not $2140-$2143 which are APU)
            if (offset < 0x2140 || offset >= 0x2144) {
                if (m_ppu) {
                    m_open_bus = m_ppu->read(offset);
                    return m_open_bus;
                }
            }
        }
        if (offset >= 0x4000 && offset < 0x4200) {
            // CPU I/O (old style joypad)
            return read_cpu_io(offset);
        }
        if (offset >= 0x4200 && offset < 0x4400) {
            // CPU I/O registers and DMA
            if (offset < 0x4220) {
                return read_cpu_io(offset);
            }
            if (offset >= 0x4300 && offset < 0x4380 && m_dma) {
                m_open_bus = m_dma->read(offset);
                return m_open_bus;
            }
        }
        if (offset >= 0x8000) {
            // Cartridge ROM
            if (m_cartridge) {
                m_open_bus = m_cartridge->read(address);
                return m_open_bus;
            }
        }
        // SRAM region ($6000-$7FFF in some mappers)
        if (offset >= 0x6000 && offset < 0x8000 && m_cartridge) {
            m_open_bus = m_cartridge->read(address);
            return m_open_bus;
        }
    }

    // Banks $40-$7D: HiROM or extended ROM
    if (bank >= 0x40 && bank <= 0x7D) {
        if (m_cartridge) {
            m_open_bus = m_cartridge->read(address);
            return m_open_bus;
        }
    }

    // Banks $7E-$7F: WRAM (128KB)
    if (bank == 0x7E) {
        m_open_bus = m_wram[offset];
        return m_open_bus;
    }
    if (bank == 0x7F) {
        m_open_bus = m_wram[0x10000 + offset];
        return m_open_bus;
    }

    // Banks $C0-$FF: HiROM high banks
    if (bank >= 0xC0) {
        if (m_cartridge) {
            m_open_bus = m_cartridge->read(address);
            return m_open_bus;
        }
    }

    return m_open_bus;
}

void Bus::write(uint32_t address, uint8_t value) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    m_open_bus = value;

    // Banks $00-$3F and $80-$BF
    if (bank <= 0x3F || (bank >= 0x80 && bank <= 0xBF)) {
        if (offset < 0x2000) {
            // WRAM mirror
            m_wram[offset] = value;
            return;
        }
        if (offset >= 0x2140 && offset < 0x2144) {
            // APU ports (must be checked BEFORE PPU range)
            if (m_apu) {
                m_apu->write_port(offset - 0x2140, value);
            }
            return;
        }
        if (offset >= 0x2100 && offset < 0x2200) {
            // WRAM data port and address registers ($2180-$2183)
            if (offset == 0x2180) {
                m_wram[m_wram_addr] = value;
                m_wram_addr = (m_wram_addr + 1) & 0x1FFFF;  // 17-bit wrap
                return;
            }
            if (offset == 0x2181) {
                m_wram_addr = (m_wram_addr & 0x1FF00) | value;
                return;
            }
            if (offset == 0x2182) {
                m_wram_addr = (m_wram_addr & 0x100FF) | (value << 8);
                return;
            }
            if (offset == 0x2183) {
                m_wram_addr = (m_wram_addr & 0x0FFFF) | ((value & 0x01) << 16);
                return;
            }
            // PPU registers ($2100-$213F, but not $2140-$2143 which are APU)
            if (offset < 0x2140 || offset >= 0x2144) {
                if (m_ppu) {
                    m_ppu->write(offset, value);
                }
            }
            return;
        }
        if (offset >= 0x4000 && offset < 0x4200) {
            // Old style joypad registers
            write_cpu_io(offset, value);
            return;
        }
        if (offset >= 0x4200 && offset < 0x4400) {
            // CPU I/O and DMA
            if (offset < 0x4220) {
                write_cpu_io(offset, value);
                return;
            }
            if (offset >= 0x4300 && offset < 0x4380 && m_dma) {
                m_dma->write(offset, value);
                return;
            }
        }
        // SRAM region ($6000-$7FFF)
        // Also intercept writes for Blargg test detection
        if (offset >= 0x6000 && offset < 0x8000) {
            // Intercept writes to $6000-$60FF for Blargg test detection
            if (offset < 0x6100) {
                m_blargg_state.on_memory_write(offset - 0x6000, value);
            }
            if (m_cartridge) {
                m_cartridge->write(address, value);
            }
            return;
        }
        // ROM writes (usually ignored, but some mappers use them)
        if (offset >= 0x8000 && m_cartridge) {
            m_cartridge->write(address, value);
            return;
        }
    }

    // Banks $40-$7D: HiROM SRAM
    if (bank >= 0x40 && bank <= 0x7D) {
        if (m_cartridge) {
            m_cartridge->write(address, value);
        }
        return;
    }

    // Banks $7E-$7F: WRAM
    if (bank == 0x7E) {
        m_wram[offset] = value;
        return;
    }
    if (bank == 0x7F) {
        m_wram[0x10000 + offset] = value;
        return;
    }

    // Banks $C0-$FF
    if (bank >= 0xC0 && m_cartridge) {
        m_cartridge->write(address, value);
    }
}

uint8_t Bus::read_cpu_io(uint16_t address) {
    switch (address) {
        case 0x4016:  // JOYSER0 - Joypad 1 data
            {
                uint8_t result = m_controller_latch[0] & 1;
                m_controller_latch[0] >>= 1;
                m_controller_latch[0] |= 0x8000;  // Return 1s after all bits read
                return result;
            }

        case 0x4017:  // JOYSER1 - Joypad 2 data
            {
                uint8_t result = m_controller_latch[1] & 1;
                m_controller_latch[1] >>= 1;
                m_controller_latch[1] |= 0x8000;
                return result;
            }

        case 0x4210:  // RDNMI - NMI flag
            {
                // Reference: bsnes irq.cpp rdnmi() function
                // The NMI flag (bit 7) reflects the NMI line state.
                // Reading RDNMI clears the NMI line ONLY if it's not currently
                // in the "hold" period. The hold period is ~4 cycles after NMI
                // becomes active, which protects the NMI from being accidentally
                // cleared by code that reads RDNMI immediately after VBlank starts.
                //
                // This is critical for games like Super Mario All-Stars that
                // may poll RDNMI in a tight loop during screen transitions.
                uint8_t result = m_rdnmi;

                // Only clear NMI line if not in hold period
                // During hold, return the flag but don't clear it
                if (!m_nmi_hold) {
                    m_nmi_line = false;
                    m_rdnmi &= 0x7F;  // Clear NMI flag
                }

                return result;
            }

        case 0x4211:  // TIMEUP - IRQ flag
            {
                uint8_t result = m_timeup;
                m_timeup = 0;  // Clear IRQ flag on read
                m_irq_flag = false;
                return result;
            }

        case 0x4212:  // HVBJOY - H/V blank flags and auto-joypad
            {
                uint8_t result = 0;
                if (m_ppu) {
                    int scanline = m_ppu->get_scanline();
                    int dot = m_ppu->get_dot();
                    // V-blank flag (scanlines 225-261 for NTSC)
                    if (scanline >= 225) result |= 0x80;
                    // H-blank flag (dots 274-339)
                    if (dot >= 274) result |= 0x40;
                }
                // Auto-joypad busy (first ~4224 master cycles of V-blank)
                if (m_joypad_counter > 0) result |= 0x01;
                return result;
            }

        case 0x4213:  // RDIO - Programmable I/O port (input)
            return m_wrio;

        case 0x4214:  // RDDIVL - Division result (low)
            return m_rddiv & 0xFF;

        case 0x4215:  // RDDIVH - Division result (high)
            return (m_rddiv >> 8) & 0xFF;

        case 0x4216:  // RDMPYL - Multiplication result (low)
            return m_rdmpy & 0xFF;

        case 0x4217:  // RDMPYH - Multiplication result (high)
            return (m_rdmpy >> 8) & 0xFF;

        case 0x4218:  // JOY1L - Joypad 1 (low)
            return m_controller_state[0] & 0xFF;

        case 0x4219:  // JOY1H - Joypad 1 (high)
            return (m_controller_state[0] >> 8) & 0xFF;

        case 0x421A:  // JOY2L - Joypad 2 (low)
            return m_controller_state[1] & 0xFF;

        case 0x421B:  // JOY2H - Joypad 2 (high)
            return (m_controller_state[1] >> 8) & 0xFF;

        default:
            return m_open_bus;
    }
}

void Bus::write_cpu_io(uint16_t address, uint8_t value) {
    switch (address) {
        case 0x4016:  // JOYSER0 - Joypad strobe
            if (value & 1) {
                m_controller_latch[0] = m_controller_state[0] & 0xFFFF;
                m_controller_latch[1] = m_controller_state[1] & 0xFFFF;
            }
            break;

        case 0x4200:  // NMITIMEN - NMI/IRQ enable
            {
                static int nmitimen_count = 0;
                nmitimen_count++;
                // Log all NMITIMEN writes that enable NMI
                if (is_debug_mode() && ((value & 0x80) != 0 || nmitimen_count <= 5)) {
                    SNES_DEBUG_PRINT("NMITIMEN write #%d: $%02X (NMI=%s, nmi_line=%d)\n",
                        nmitimen_count, value, (value & 0x80) ? "enabled" : "disabled",
                        m_nmi_line ? 1 : 0);
                }

                // Reference: bsnes irq.cpp nmitimenUpdate()
                //
                // Critical hardware behavior: NMI uses edge detection.
                // The NMI fires when the combined condition (NMI enabled AND NMI line active)
                // transitions from false to true.
                //
                // This means:
                // 1. If NMI line is already active (we're in VBlank) and we enable NMI,
                //    that's an edge transition -> immediate NMI
                // 2. If NMI is already enabled and line becomes active (VBlank starts),
                //    that's an edge transition -> NMI
                //
                // Super Mario All-Stars depends on this for screen transitions!
                // The game disables NMI, does work, then re-enables NMI during VBlank.
                // If the NMI line is still active, enabling NMI must trigger immediately.

                bool old_nmi_enabled = (m_nmitimen & 0x80) != 0;
                bool new_nmi_enabled = (value & 0x80) != 0;

                // Calculate old and new combined states
                bool old_nmi_active = old_nmi_enabled && m_nmi_line;
                bool new_nmi_active = new_nmi_enabled && m_nmi_line;

                // Detect rising edge (0->1 transition)
                if (new_nmi_active && !old_nmi_active) {
                    m_nmi_transition = true;
                    m_nmi_pending = true;
                    if (is_debug_mode()) {
                        SNES_DEBUG_PRINT("NMI edge-triggered by NMITIMEN write! (nmi_line=%d)\n",
                            m_nmi_line ? 1 : 0);
                    }
                }

                // Also update the previous state for poll_nmi
                m_prev_nmi_active = new_nmi_active;

                // Reference: bsnes irq.cpp nmitimenUpdate() - sets IRQ lock on every write
                // This prevents the NMI from being serviced for ~12 cycles, which is
                // important for timing-sensitive games.
                set_irq_lock();
            }
            m_nmitimen = value;
            m_auto_joypad_read = (value & 0x01) != 0;
            // Bit 7: VBlank NMI enable - must pass to PPU
            if (m_ppu) {
                m_ppu->set_nmi_enabled((value & 0x80) != 0);
            }
            break;

        case 0x4201:  // WRIO - Programmable I/O port (output)
            m_wrio = value;
            break;

        case 0x4202:  // WRMPYA - Multiplication operand A
            m_wrmpya = value;
            break;

        case 0x4203:  // WRMPYB - Multiplication operand B (triggers multiply)
            m_wrmpyb = value;
            m_rdmpy = m_wrmpya * m_wrmpyb;
            break;

        case 0x4204:  // WRDIVL - Division dividend (low)
            m_wrdiv = (m_wrdiv & 0xFF00) | value;
            break;

        case 0x4205:  // WRDIVH - Division dividend (high)
            m_wrdiv = (m_wrdiv & 0x00FF) | (value << 8);
            break;

        case 0x4206:  // WRDIVB - Division divisor (triggers divide)
            m_wrdivb = value;
            if (m_wrdivb != 0) {
                m_rddiv = m_wrdiv / m_wrdivb;
                m_rdmpy = m_wrdiv % m_wrdivb;
            } else {
                m_rddiv = 0xFFFF;
                m_rdmpy = m_wrdiv;
            }
            break;

        case 0x4207:  // HTIMEL - H-counter IRQ time (low)
            m_htime = (m_htime & 0x100) | value;
            break;

        case 0x4208:  // HTIMEH - H-counter IRQ time (high)
            m_htime = (m_htime & 0xFF) | ((value & 0x01) << 8);
            break;

        case 0x4209:  // VTIMEL - V-counter IRQ time (low)
            m_vtime = (m_vtime & 0x100) | value;
            break;

        case 0x420A:  // VTIMEH - V-counter IRQ time (high)
            m_vtime = (m_vtime & 0xFF) | ((value & 0x01) << 8);
            break;

        case 0x420B:  // MDMAEN - General purpose DMA
            m_mdmaen = value;
            if (m_dma && value) {
                m_dma->write_mdmaen(value);
            }
            break;

        case 0x420C:  // HDMAEN - HDMA enable
            if (is_debug_mode() && value != 0) {
                static int hdmaen_count = 0;
                hdmaen_count++;
                if (hdmaen_count <= 5) {
                    SNES_DEBUG_PRINT("HDMAEN write #%d: $%02X\n", hdmaen_count, value);
                }
            }
            m_hdmaen = value;
            if (m_dma) {
                m_dma->write_hdmaen(value);
            }
            break;

        case 0x420D:  // MEMSEL - FastROM select
            m_memsel = value;
            break;
    }
}

void Bus::set_controller_state(int controller, uint32_t buttons) {
    if (controller < 2) {
        // Convert from VirtualButton bitmask to SNES format
        // SNES format: B Y Select Start Up Down Left Right A X L R (12 buttons)
        // VirtualButton: A=0, B=1, X=2, Y=3, L=4, R=5, Start=6, Select=7, Up=8, Down=9, Left=10, Right=11

        uint16_t snes_buttons = 0;

        // B button (bit 15 in SNES, bit 1 in VirtualButton)
        if (buttons & (1 << 1)) snes_buttons |= 0x8000;
        // Y button (bit 14, bit 3)
        if (buttons & (1 << 3)) snes_buttons |= 0x4000;
        // Select (bit 13, bit 7)
        if (buttons & (1 << 7)) snes_buttons |= 0x2000;
        // Start (bit 12, bit 6)
        if (buttons & (1 << 6)) snes_buttons |= 0x1000;
        // Up (bit 11, bit 8)
        if (buttons & (1 << 8)) snes_buttons |= 0x0800;
        // Down (bit 10, bit 9)
        if (buttons & (1 << 9)) snes_buttons |= 0x0400;
        // Left (bit 9, bit 10)
        if (buttons & (1 << 10)) snes_buttons |= 0x0200;
        // Right (bit 8, bit 11)
        if (buttons & (1 << 11)) snes_buttons |= 0x0100;
        // A button (bit 7, bit 0)
        if (buttons & (1 << 0)) snes_buttons |= 0x0080;
        // X button (bit 6, bit 2)
        if (buttons & (1 << 2)) snes_buttons |= 0x0040;
        // L button (bit 5, bit 4)
        if (buttons & (1 << 4)) snes_buttons |= 0x0020;
        // R button (bit 4, bit 5)
        if (buttons & (1 << 5)) snes_buttons |= 0x0010;

        m_controller_state[controller] = snes_buttons;
    }
}

void Bus::start_frame() {
    // Reset frame state at V=0, H=0
    // Reference: fullsnes.txt, anomie's SNES docs, snes wiki timing
    //
    // At the start of a new frame (V=0, H=0):
    // 1. The NMI output line goes HIGH (inactive)
    // 2. The RDNMI flag is cleared
    // 3. This happens BEFORE any CPU instructions execute on scanline 0
    //
    // This is critical for edge detection: the NMI line must go inactive
    // at V=0 so that it can trigger again at the next VBlank.

    // Clear internal NMI flag
    m_nmi_flag = false;

    // Clear NMI output line (goes HIGH/inactive)
    m_nmi_line = false;
    m_rdnmi &= 0x7F;  // Clear NMI flag but preserve CPU version (bits 0-3)

    // Clear hold state (should already be clear, but be safe)
    m_nmi_hold = false;
    m_nmi_hold_cycles = 0;

    // Update edge detection state - NMI is now inactive
    // This is important: when NMI line goes low, m_prev_nmi_active should update
    bool nmi_enabled = (m_nmitimen & 0x80) != 0;
    m_prev_nmi_active = nmi_enabled && m_nmi_line;  // Will be false since m_nmi_line is false

    if (m_dma) {
        m_dma->hdma_init();
    }
}

void Bus::start_scanline() {
    // Reset H-counter and IRQ state for new scanline
    m_hcounter = 0;
    m_prev_hcounter = 0;
    m_irq_triggered_this_line = false;

    // Check V-IRQ at the start of each scanline (when H-IRQ is disabled)
    // V-IRQ fires at dot ~10.5 of scanline VTIME (but we approximate as dot 0)
    // Reference: anomie's SNES timing doc, bsnes source
    if ((m_nmitimen & 0x20) && !(m_nmitimen & 0x10)) {
        // V-IRQ enabled, H-IRQ disabled (mode 10)
        if (m_ppu && m_ppu->get_scanline() == (m_vtime & 0x1FF)) {
            m_irq_flag = true;
            m_timeup = 0x80;
            m_irq_triggered_this_line = true;

            if (is_debug_mode()) {
                static int virq_count = 0;
                virq_count++;
                if (virq_count <= 10 || virq_count % 100 == 0) {
                    SNES_DEBUG_PRINT("V-IRQ triggered #%d at V=%d (VTIME=%d)\n",
                        virq_count, m_ppu->get_scanline(), m_vtime & 0x1FF);
                }
            }
        }
    }
}

void Bus::start_hblank() {
    // Process HDMA
    if (m_dma && m_hdmaen) {
        m_dma->hdma_transfer();
    }

    // NOTE: H-IRQ is now properly handled by check_irq_trigger() which is called
    // during the main CPU step loop. The previous code here was redundantly
    // setting m_irq_flag without checking m_irq_triggered_this_line, causing
    // IRQs to fire continuously and lock up games that use H-IRQ.
    //
    // Reference: bsnes/ares - IRQ is triggered at the exact dot position, not
    // just "sometime during H-blank". The check_irq_trigger() function properly
    // handles edge detection and prevents re-triggering on the same line.
}

void Bus::start_vblank() {
    // Reference: bsnes irq.cpp, fullsnes.txt, snes wiki timing
    //
    // At VBlank start (V=225 or 240, H=0.5):
    // 1. The internal NMI flag is set
    // 2. The NMI output line goes LOW (active)
    // 3. A "hold" period of ~4 cycles begins, during which reading RDNMI
    //    will NOT clear the NMI flag. This protects the NMI from being
    //    lost if code happens to read RDNMI right at VBlank start.
    // 4. NMI triggers on the 0->1 edge of (NMI enabled AND NMI line active)

    // Set internal NMI flag
    m_nmi_flag = true;

    // Set NMI line active (this is what RDNMI reads)
    m_nmi_line = true;
    m_rdnmi = 0x80 | 0x02;  // NMI occurred + CPU version

    // Start NMI hold period (~4 master cycles, but we use a counter
    // that decrements each time poll_nmi is called)
    m_nmi_hold = true;
    m_nmi_hold_cycles = 4;  // Will be decremented by poll_nmi

    // Edge detection: if NMI is enabled, trigger transition
    // The CPU will fire NMI when test_nmi() is called (at end of instruction)
    if (m_nmitimen & 0x80) {
        m_nmi_transition = true;
        m_nmi_pending = true;

        static int nmi_count = 0;
        nmi_count++;
        if (is_debug_mode() && (nmi_count <= 5 || nmi_count % 50 == 0)) {
            SNES_DEBUG_PRINT("NMI triggered #%d (NMITIMEN=$%02X, hold=%d)\n",
                nmi_count, m_nmitimen, m_nmi_hold_cycles);
        }
    }

    // Auto-joypad read
    if (m_auto_joypad_read) {
        m_joypad_counter = 4224;  // Takes ~4224 master cycles
        // Latch controllers
        m_controller_latch[0] = m_controller_state[0];
        m_controller_latch[1] = m_controller_state[1];
    }

    // Note: V-IRQ is checked in start_scanline(), not here
    // This ensures V-IRQ fires at any scanline VTIME, not just at vblank
}

void Bus::add_cycles(int master_cycles) {
    // Decrement auto-joypad counter
    if (m_joypad_counter > 0) {
        m_joypad_counter -= master_cycles;
        if (m_joypad_counter < 0) {
            m_joypad_counter = 0;
        }
    }

    // Decrement NMI hold counter
    // Reference: bsnes irq.cpp - NMI has a ~4 cycle hold period where
    // reading RDNMI won't clear the flag. This protects against accidental
    // NMI loss when code reads RDNMI right at VBlank start.
    if (m_nmi_hold_cycles > 0) {
        m_nmi_hold_cycles -= master_cycles;
        if (m_nmi_hold_cycles <= 0) {
            m_nmi_hold_cycles = 0;
            m_nmi_hold = false;
        }
    }

    // Decrement IRQ lock counter
    // Reference: bsnes irq.cpp - prevents interrupt servicing for ~12 cycles
    // after DMA completion and NMITIMEN writes
    if (m_irq_lock_cycles > 0) {
        m_irq_lock_cycles -= master_cycles;
        if (m_irq_lock_cycles <= 0) {
            m_irq_lock_cycles = 0;
            m_irq_lock = false;
        }
    }
}

void Bus::poll_nmi() {
    // Reference: bsnes irq.cpp nmiPoll()
    //
    // This is called periodically (approximately every 4 cycles) to update
    // NMI edge detection state. The NMI fires on the transition from
    // "NMI not active" to "NMI active", where "active" means:
    //   (NMI enabled in NMITIMEN) AND (NMI line is low/active)
    //
    // The internal NMI flag (m_nmi_flag) is set at VBlank start and cleared
    // at V=0. But the actual NMI to the CPU fires on the EDGE of the
    // combined condition, not the level.

    // Check if NMI should be active
    bool nmi_enabled = (m_nmitimen & 0x80) != 0;
    bool nmi_active = nmi_enabled && m_nmi_line;

    // Detect rising edge (0->1 transition)
    if (nmi_active && !m_prev_nmi_active) {
        m_nmi_transition = true;
        m_nmi_pending = true;

        if (is_debug_mode()) {
            static int edge_count = 0;
            edge_count++;
            if (edge_count <= 10) {
                SNES_DEBUG_PRINT("NMI edge detected #%d (poll_nmi)\n", edge_count);
            }
        }
    }
    m_prev_nmi_active = nmi_active;
}

bool Bus::test_nmi() {
    // Reference: bsnes irq.cpp nmiTest()
    //
    // Called at the end of each CPU instruction to check if NMI should fire.
    // Returns true if an NMI transition was detected and should be serviced.
    // Clears the transition flag after returning.

    if (m_nmi_transition) {
        m_nmi_transition = false;
        return true;
    }
    return false;
}

void Bus::update_hcounter(int master_cycles) {
    // SNES has 1364 master cycles per scanline (including H-blank)
    // H-counter increments every 4 master cycles, giving 341 dots per scanline
    // Dots 0-255 = active display, 256-339 = H-blank
    constexpr int MASTER_CYCLES_PER_DOT = 4;
    constexpr int DOTS_PER_SCANLINE = 341;

    m_prev_hcounter = m_hcounter;
    m_hcounter += master_cycles / MASTER_CYCLES_PER_DOT;

    // Keep H-counter in range (wrap handled by start_scanline)
    if (m_hcounter >= DOTS_PER_SCANLINE) {
        m_hcounter = DOTS_PER_SCANLINE - 1;  // Clamp for now, will reset at next scanline
    }
}

bool Bus::check_irq_trigger() {
    // Check if H-IRQ or V-IRQ should fire based on NMITIMEN settings
    // Reference: fullsnes.txt, anomie's SNES timing doc, bsnes source
    //
    // NMITIMEN ($4200):
    //   Bit 5: V-count IRQ enable
    //   Bit 4: H-count IRQ enable
    //
    // IRQ modes:
    //   00: No IRQ
    //   01: H-IRQ at H=HTIME on every scanline
    //   10: V-IRQ at V=VTIME, H=0 (but actually ~10.5 dots after dot 0)
    //   11: HV-IRQ at V=VTIME, H=HTIME

    if (m_irq_triggered_this_line) {
        return false;  // Already triggered this line
    }

    bool h_irq_enabled = (m_nmitimen & 0x10) != 0;
    bool v_irq_enabled = (m_nmitimen & 0x20) != 0;

    if (!h_irq_enabled && !v_irq_enabled) {
        return false;  // No IRQ enabled
    }

    // Get current V position from PPU
    int vcounter = m_ppu ? m_ppu->get_scanline() : 0;
    int hcounter = m_hcounter;
    int prev_h = m_prev_hcounter;

    // HTIME is 9-bit value (0-339), but only lower 9 bits used
    int htime = m_htime & 0x1FF;
    int vtime = m_vtime & 0x1FF;

    bool should_trigger = false;

    if (h_irq_enabled && !v_irq_enabled) {
        // Mode 01: H-IRQ only - fires at H=HTIME on every scanline
        // Check if we crossed HTIME this update
        if (prev_h < htime && hcounter >= htime) {
            should_trigger = true;
        }
    } else if (v_irq_enabled && !h_irq_enabled) {
        // Mode 10: V-IRQ only - fires at V=VTIME, H≈0
        // This is handled in start_scanline() at dot 0
        // Don't trigger here to avoid double-triggering
    } else if (h_irq_enabled && v_irq_enabled) {
        // Mode 11: HV-IRQ - fires at V=VTIME, H=HTIME
        if (vcounter == vtime && prev_h < htime && hcounter >= htime) {
            should_trigger = true;
        }
    }

    if (should_trigger) {
        m_irq_flag = true;
        m_timeup = 0x80;
        m_irq_triggered_this_line = true;

        if (is_debug_mode()) {
            static int irq_count = 0;
            irq_count++;
            if (irq_count <= 10 || irq_count % 100 == 0) {
                SNES_DEBUG_PRINT("IRQ triggered #%d: V=%d H=%d (VTIME=%d HTIME=%d NMITIMEN=$%02X)\n",
                    irq_count, vcounter, hcounter, vtime, htime, m_nmitimen);
            }
        }
        return true;
    }

    return false;
}

bool Bus::irq_pending() const {
    // IRQ can only be serviced if the IRQ lock is not active
    return (m_irq_flag || m_irq_line) && !m_irq_lock;
}

void Bus::set_irq_lock() {
    // Reference: bsnes irq.cpp - sets IRQ lock for ~12 master cycles
    // This prevents NMI/IRQ from being serviced immediately after
    // certain operations like DMA completion or NMITIMEN writes.
    m_irq_lock = true;
    m_irq_lock_cycles = 12;  // ~12 master cycles
}

void Bus::set_irq_line(bool active) {
    m_irq_line = active;
}

void Bus::save_state(std::vector<uint8_t>& data) {
    // Save WRAM
    data.insert(data.end(), m_wram.begin(), m_wram.end());

    // Save I/O state
    data.push_back(m_nmitimen);
    data.push_back(m_wrio);
    data.push_back(m_htime & 0xFF);
    data.push_back((m_htime >> 8) & 0xFF);
    data.push_back(m_vtime & 0xFF);
    data.push_back((m_vtime >> 8) & 0xFF);
    data.push_back(m_mdmaen);
    data.push_back(m_hdmaen);
    data.push_back(m_memsel);

    // Save math state
    data.push_back(m_rddiv & 0xFF);
    data.push_back((m_rddiv >> 8) & 0xFF);
    data.push_back(m_rdmpy & 0xFF);
    data.push_back((m_rdmpy >> 8) & 0xFF);

    // Save NMI/IRQ state
    data.push_back(m_nmi_pending ? 1 : 0);
    data.push_back(m_nmi_flag ? 1 : 0);
    data.push_back(m_nmi_line ? 1 : 0);
    data.push_back(m_nmi_hold ? 1 : 0);
    data.push_back(m_nmi_hold_cycles & 0xFF);
    data.push_back(m_nmi_transition ? 1 : 0);
    data.push_back(m_prev_nmi_active ? 1 : 0);
    data.push_back(m_irq_flag ? 1 : 0);
    data.push_back(m_rdnmi);
    data.push_back(m_timeup);
    data.push_back(m_irq_lock ? 1 : 0);
    data.push_back(m_irq_lock_cycles & 0xFF);

    // Save controller state
    for (int i = 0; i < 2; i++) {
        data.push_back(m_controller_state[i] & 0xFF);
        data.push_back((m_controller_state[i] >> 8) & 0xFF);
        data.push_back((m_controller_state[i] >> 16) & 0xFF);
        data.push_back((m_controller_state[i] >> 24) & 0xFF);
    }

    // Save WRAM port address
    data.push_back(m_wram_addr & 0xFF);
    data.push_back((m_wram_addr >> 8) & 0xFF);
    data.push_back((m_wram_addr >> 16) & 0xFF);
}

void Bus::load_state(const uint8_t*& data, size_t& remaining) {
    // Load WRAM
    std::memcpy(m_wram.data(), data, m_wram.size());
    data += m_wram.size(); remaining -= m_wram.size();

    // Load I/O state
    m_nmitimen = *data++; remaining--;
    m_wrio = *data++; remaining--;
    m_htime = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
    m_vtime = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
    m_mdmaen = *data++; remaining--;
    m_hdmaen = *data++; remaining--;
    m_memsel = *data++; remaining--;

    // Load math state
    m_rddiv = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
    m_rdmpy = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;

    // Load NMI/IRQ state
    m_nmi_pending = (*data++ != 0); remaining--;
    m_nmi_flag = (*data++ != 0); remaining--;
    m_nmi_line = (*data++ != 0); remaining--;
    m_nmi_hold = (*data++ != 0); remaining--;
    m_nmi_hold_cycles = *data++; remaining--;
    m_nmi_transition = (*data++ != 0); remaining--;
    m_prev_nmi_active = (*data++ != 0); remaining--;
    m_irq_flag = (*data++ != 0); remaining--;
    m_rdnmi = *data++; remaining--;
    m_timeup = *data++; remaining--;
    m_irq_lock = (*data++ != 0); remaining--;
    m_irq_lock_cycles = *data++; remaining--;

    // Load controller state
    for (int i = 0; i < 2; i++) {
        m_controller_state[i] = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        data += 4; remaining -= 4;
    }

    // Load WRAM port address
    m_wram_addr = data[0] | (data[1] << 8) | ((data[2] & 0x01) << 16);
    data += 3; remaining -= 3;
}

} // namespace snes
