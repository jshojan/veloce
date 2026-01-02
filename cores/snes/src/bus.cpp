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
}

Bus::~Bus() = default;

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
                    // Debug: trace non-zero VRAM writes
                    static int vram_bus_debug = 0;
                    if (is_debug_mode() && (offset == 0x2118 || offset == 0x2119) && value != 0 && vram_bus_debug < 10) {
                        fprintf(stderr, "[BUS] PPU VRAM write: addr=$%04X val=$%02X\n", offset, value);
                        vram_bus_debug++;
                    }
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
                uint8_t result = m_rdnmi;
                m_rdnmi &= 0x7F;  // Clear NMI flag on read
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
                // Latch controller state
                m_controller_latch[0] = m_controller_state[0] & 0xFFFF;
                m_controller_latch[1] = m_controller_state[1] & 0xFFFF;
            }
            break;

        case 0x4200:  // NMITIMEN - NMI/IRQ enable
            m_nmitimen = value;
            m_auto_joypad_read = (value & 0x01) != 0;
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
    // Reset frame state
    if (m_dma) {
        m_dma->hdma_init();
    }
}

void Bus::start_hblank() {
    // Process HDMA
    if (m_dma && m_hdmaen) {
        m_dma->hdma_transfer();
    }

    // Check H-IRQ
    if (m_ppu && (m_nmitimen & 0x10)) {
        if (m_ppu->get_dot() >= m_htime) {
            if (!(m_nmitimen & 0x20) || m_ppu->get_scanline() == m_vtime) {
                m_irq_flag = true;
                m_timeup = 0x80;
            }
        }
    }
}

void Bus::start_vblank() {
    // Set NMI flag
    m_nmi_flag = true;
    m_rdnmi = 0x80 | 0x02;  // NMI occurred + CPU version

    // Trigger NMI if enabled
    if (m_nmitimen & 0x80) {
        m_nmi_pending = true;
    }

    // Auto-joypad read
    if (m_auto_joypad_read) {
        m_joypad_counter = 4224;  // Takes ~4224 master cycles
        // Latch controllers
        m_controller_latch[0] = m_controller_state[0];
        m_controller_latch[1] = m_controller_state[1];
    }

    // Check V-IRQ
    if ((m_nmitimen & 0x20) && !(m_nmitimen & 0x10)) {
        if (m_ppu && m_ppu->get_scanline() == m_vtime) {
            m_irq_flag = true;
            m_timeup = 0x80;
        }
    }
}

bool Bus::irq_pending() const {
    return m_irq_flag || m_irq_line;
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
    data.push_back(m_irq_flag ? 1 : 0);
    data.push_back(m_rdnmi);
    data.push_back(m_timeup);

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
    m_irq_flag = (*data++ != 0); remaining--;
    m_rdnmi = *data++; remaining--;
    m_timeup = *data++; remaining--;

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
