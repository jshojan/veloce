#include "bus.hpp"
#include "lr35902.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"
#include <cstring>

namespace gb {

Bus::Bus() {
    m_wram.fill(0);
    m_wram_cgb.fill(0);
    m_hram.fill(0);
}

Bus::~Bus() = default;

uint8_t Bus::read(uint16_t address) {
    // ROM Bank 0 (0x0000-0x3FFF)
    if (address < 0x4000) {
        if (m_cartridge) {
            return m_cartridge->read_rom(address);
        }
        return 0xFF;
    }

    // ROM Bank 1-N (0x4000-0x7FFF)
    if (address < 0x8000) {
        if (m_cartridge) {
            return m_cartridge->read_rom(address);
        }
        return 0xFF;
    }

    // VRAM (0x8000-0x9FFF)
    if (address < 0xA000) {
        if (m_ppu) {
            return m_ppu->read_vram(address - 0x8000);
        }
        return 0xFF;
    }

    // External RAM (0xA000-0xBFFF)
    if (address < 0xC000) {
        if (m_cartridge) {
            return m_cartridge->read_ram(address - 0xA000);
        }
        return 0xFF;
    }

    // Work RAM Bank 0 (0xC000-0xCFFF)
    if (address < 0xD000) {
        return m_wram[address - 0xC000];
    }

    // Work RAM Bank 1-7 (0xD000-0xDFFF)
    if (address < 0xE000) {
        if (m_cgb_mode && m_svbk > 0) {
            int bank = (m_svbk == 0) ? 1 : (m_svbk & 7);
            if (bank == 0) bank = 1;
            if (bank == 1) {
                return m_wram[address - 0xC000];
            } else {
                return m_wram_cgb[(bank - 2) * 0x1000 + (address - 0xD000)];
            }
        }
        return m_wram[address - 0xC000];
    }

    // Echo RAM (0xE000-0xFDFF)
    if (address < 0xFE00) {
        return read(address - 0x2000);
    }

    // OAM (0xFE00-0xFE9F)
    if (address < 0xFEA0) {
        if (m_ppu && !m_oam_dma_active) {
            return m_ppu->read_oam(address - 0xFE00);
        }
        return 0xFF;
    }

    // Not usable (0xFEA0-0xFEFF)
    if (address < 0xFF00) {
        return 0xFF;
    }

    // I/O Registers (0xFF00-0xFF7F)
    if (address < 0xFF80) {
        return read_io(address);
    }

    // High RAM (0xFF80-0xFFFE)
    if (address < 0xFFFF) {
        return m_hram[address - 0xFF80];
    }

    // Interrupt Enable (0xFFFF)
    return m_ie;
}

void Bus::write(uint16_t address, uint8_t value) {
    // ROM (0x0000-0x7FFF) - writes go to MBC
    if (address < 0x8000) {
        if (m_cartridge) {
            m_cartridge->write_mbc(address, value);
        }
        return;
    }

    // VRAM (0x8000-0x9FFF)
    if (address < 0xA000) {
        if (m_ppu) {
            m_ppu->write_vram(address - 0x8000, value);
        }
        return;
    }

    // External RAM (0xA000-0xBFFF)
    if (address < 0xC000) {
        if (m_cartridge) {
            m_cartridge->write_ram(address - 0xA000, value);
        }
        return;
    }

    // Work RAM Bank 0 (0xC000-0xCFFF)
    if (address < 0xD000) {
        m_wram[address - 0xC000] = value;
        return;
    }

    // Work RAM Bank 1-7 (0xD000-0xDFFF)
    if (address < 0xE000) {
        if (m_cgb_mode && m_svbk > 0) {
            int bank = (m_svbk == 0) ? 1 : (m_svbk & 7);
            if (bank == 0) bank = 1;
            if (bank == 1) {
                m_wram[address - 0xC000] = value;
            } else {
                m_wram_cgb[(bank - 2) * 0x1000 + (address - 0xD000)] = value;
            }
        } else {
            m_wram[address - 0xC000] = value;
        }
        return;
    }

    // Echo RAM (0xE000-0xFDFF)
    if (address < 0xFE00) {
        write(address - 0x2000, value);
        return;
    }

    // OAM (0xFE00-0xFE9F)
    if (address < 0xFEA0) {
        if (m_ppu && !m_oam_dma_active) {
            m_ppu->write_oam(address - 0xFE00, value);
        }
        return;
    }

    // Not usable (0xFEA0-0xFEFF)
    if (address < 0xFF00) {
        return;
    }

    // I/O Registers (0xFF00-0xFF7F)
    if (address < 0xFF80) {
        write_io(address, value);
        return;
    }

    // High RAM (0xFF80-0xFFFE)
    if (address < 0xFFFF) {
        m_hram[address - 0xFF80] = value;
        return;
    }

    // Interrupt Enable (0xFFFF)
    m_ie = value;
}

uint8_t Bus::read_io(uint16_t address) {
    switch (address & 0xFF) {
        case 0x00:  // JOYP
            if (!(m_joyp & 0x20)) {
                return (m_joyp & 0xF0) | (m_joypad_buttons & 0x0F);
            }
            if (!(m_joyp & 0x10)) {
                return (m_joyp & 0xF0) | (m_joypad_directions & 0x0F);
            }
            return m_joyp | 0x0F;

        case 0x01: return m_sb;
        case 0x02: return m_sc | 0x7E;  // Bits 1-6 always 1
        case 0x04: return static_cast<uint8_t>(m_div_counter >> 8);
        case 0x05: return m_tima;
        case 0x06: return m_tma;
        case 0x07: return m_tac | 0xF8;
        case 0x0F: return m_if | 0xE0;

        // Sound registers (0xFF10-0xFF26)
        case 0x10 ... 0x26:
            if (m_apu) {
                return m_apu->read_register(address);
            }
            return 0xFF;

        // Wave RAM (0xFF30-0xFF3F)
        case 0x30 ... 0x3F:
            if (m_apu) {
                return m_apu->read_register(address);
            }
            return 0xFF;

        // LCD registers
        case 0x40 ... 0x4B:
            if (m_ppu) {
                return m_ppu->read_register(address);
            }
            return 0xFF;

        // CGB registers
        case 0x4D:
            if (m_cgb_mode) {
                return (m_double_speed ? 0x80 : 0) | (m_key1 & 1) | 0x7E;
            }
            return 0xFF;

        case 0x4F:
            if (m_cgb_mode) {
                return m_vbk | 0xFE;
            }
            return 0xFF;

        case 0x51: return m_hdma1;
        case 0x52: return m_hdma2;
        case 0x53: return m_hdma3;
        case 0x54: return m_hdma4;
        case 0x55: return m_hdma5;
        case 0x56: return m_rp;

        // CGB palette registers
        case 0x68 ... 0x6B:
            if (m_cgb_mode && m_ppu) {
                return m_ppu->read_register(address);
            }
            return 0xFF;

        case 0x70:
            if (m_cgb_mode) {
                return m_svbk | 0xF8;
            }
            return 0xFF;

        default:
            return 0xFF;
    }
}

void Bus::write_io(uint16_t address, uint8_t value) {
    switch (address & 0xFF) {
        case 0x00:  // JOYP
            m_joyp = (m_joyp & 0x0F) | (value & 0x30);
            break;

        case 0x01: m_sb = value; break;
        case 0x02:
            m_sc = value;
            if (value == 0x81) {
                // Blargg test ROMs use internal clock (bit 0 = 1) with transfer start (bit 7 = 1)
                // to output characters via serial. Capture the byte for test detection.
                if (m_sb >= 0x20 && m_sb < 0x7F) {
                    m_serial_output += static_cast<char>(m_sb);
                } else if (m_sb == '\n') {
                    m_serial_output += '\n';
                }
            }
            if (value & 0x80) {
                // Start transfer
                m_serial_counter = 0;
                m_serial_bits = 0;
            }
            break;

        case 0x04: {
            // Writing any value resets DIV
            // This can trigger a timer increment if the selected bit was 1
            bool old_bit = get_timer_bit();
            m_div_counter = 0;
            // Resetting DIV means the selected bit is now 0, check for falling edge
            if (old_bit) {
                check_timer_falling_edge(false);
            }
            m_prev_timer_bit = false;
            break;
        }

        case 0x05:
            // Writing to TIMA during overflow cycle cancels the reload
            if (m_tima_overflow_cycle > 0) {
                m_tima_overflow_cycle = 0;  // Cancel pending overflow
            }
            m_tima = value;
            break;
        case 0x06: m_tma = value; break;
        case 0x07: {
            // TAC write can trigger timer increment via glitch
            // If changing from enabled to disabled, or changing clock select
            // while the selected bit is 1, a falling edge may occur
            bool old_bit = get_timer_bit();
            m_tac = value;
            bool new_bit = get_timer_bit();
            // Check for falling edge caused by the change
            if (old_bit && !new_bit) {
                check_timer_falling_edge(false);
            }
            m_prev_timer_bit = new_bit;
            break;
        }

        case 0x0F: m_if = value & 0x1F; break;

        // Sound registers
        case 0x10 ... 0x26:
            if (m_apu) {
                m_apu->write_register(address, value);
            }
            break;

        // Wave RAM
        case 0x30 ... 0x3F:
            if (m_apu) {
                m_apu->write_register(address, value);
            }
            break;

        // LCD registers
        case 0x40 ... 0x45:
            if (m_ppu) {
                m_ppu->write_register(address, value);
            }
            break;

        case 0x46:  // OAM DMA
            start_oam_dma(value);
            break;

        case 0x47 ... 0x4B:
            if (m_ppu) {
                m_ppu->write_register(address, value);
            }
            break;

        // CGB registers
        case 0x4D:
            if (m_cgb_mode) {
                m_key1 = (m_key1 & 0x80) | (value & 1);
            }
            break;

        case 0x4F:
            if (m_cgb_mode) {
                m_vbk = value & 1;
                if (m_ppu) {
                    m_ppu->set_vram_bank(m_vbk);
                }
            }
            break;

        case 0x51: m_hdma1 = value; break;
        case 0x52: m_hdma2 = value & 0xF0; break;
        case 0x53: m_hdma3 = value & 0x1F; break;
        case 0x54: m_hdma4 = value & 0xF0; break;
        case 0x55:
            // HDMA control
            if (m_cgb_mode) {
                m_hdma5 = value;
                // TODO: Implement HDMA
            }
            break;

        case 0x56:
            m_rp = value;
            break;

        // CGB palette registers
        case 0x68 ... 0x6B:
            if (m_cgb_mode && m_ppu) {
                m_ppu->write_register(address, value);
            }
            break;

        case 0x70:
            if (m_cgb_mode) {
                m_svbk = value & 7;
                if (m_svbk == 0) m_svbk = 1;
            }
            break;

        default:
            break;
    }
}

void Bus::set_input_state(uint32_t buttons) {
    // Convert from Veloce button layout to GB joypad
    // GB buttons: bit 0=A, 1=B, 2=Select, 3=Start (active low)
    // GB directions: bit 0=Right, 1=Left, 2=Up, 3=Down (active low)

    m_joypad_buttons = 0x0F;
    m_joypad_directions = 0x0F;

    // Veloce: A=0, B=1, Start=6, Select=7, Up=8, Down=9, Left=10, Right=11
    if (buttons & (1 << 0)) m_joypad_buttons &= ~0x01;  // A
    if (buttons & (1 << 1)) m_joypad_buttons &= ~0x02;  // B
    if (buttons & (1 << 7)) m_joypad_buttons &= ~0x04;  // Select
    if (buttons & (1 << 6)) m_joypad_buttons &= ~0x08;  // Start

    if (buttons & (1 << 11)) m_joypad_directions &= ~0x01;  // Right
    if (buttons & (1 << 10)) m_joypad_directions &= ~0x02;  // Left
    if (buttons & (1 << 8)) m_joypad_directions &= ~0x04;   // Up
    if (buttons & (1 << 9)) m_joypad_directions &= ~0x08;   // Down

    // Check for joypad interrupt (any button pressed)
    if ((m_joypad_buttons & 0x0F) != 0x0F || (m_joypad_directions & 0x0F) != 0x0F) {
        // Only trigger if the appropriate selection is made
        if (!(m_joyp & 0x20) || !(m_joyp & 0x10)) {
            request_interrupt(0x10);  // Joypad interrupt
        }
    }
}

uint8_t Bus::get_pending_interrupts() {
    return m_ie & m_if;
}

void Bus::request_interrupt(uint8_t irq) {
    m_if |= irq;
}

void Bus::clear_interrupt(uint8_t irq) {
    m_if &= ~irq;
}

void Bus::start_oam_dma(uint8_t page) {
    m_oam_dma_active = true;
    m_oam_dma_src = static_cast<uint16_t>(page) << 8;
    m_oam_dma_offset = 0;
}

void Bus::step_oam_dma() {
    if (!m_oam_dma_active) return;

    // Transfer one byte per cycle
    if (m_ppu) {
        uint8_t value = read(m_oam_dma_src + m_oam_dma_offset);
        m_ppu->write_oam(m_oam_dma_offset, value);
    }

    m_oam_dma_offset++;
    if (m_oam_dma_offset >= 160) {
        m_oam_dma_active = false;
    }
}

void Bus::trigger_oam_bug(uint16_t address, bool is_read) {
    // DMG OAM bug - only triggers in OAM range during mode 2
    if (address >= 0xFE00 && address < 0xFF00 && m_ppu) {
        m_ppu->trigger_oam_bug(address, is_read);
    }
}

bool Bus::get_timer_bit() const {
    // Timer is clocked by falling edge of specific DIV bit based on TAC
    // Only check if timer is enabled (TAC bit 2)
    if (!(m_tac & 0x04)) {
        return false;
    }
    int bit_pos = TIMER_DIV_BITS[m_tac & 3];
    return (m_div_counter >> bit_pos) & 1;
}

void Bus::check_timer_falling_edge(bool new_bit) {
    // Falling edge detection: TIMA increments when timer bit goes from 1 to 0
    if (m_prev_timer_bit && !new_bit) {
        m_tima++;
        if (m_tima == 0) {
            // TIMA overflow - schedule delayed reload (happens 1 M-cycle later)
            m_tima_overflow_cycle = 1;
        }
    }
    m_prev_timer_bit = new_bit;
}

void Bus::step_timer(int m_cycles) {
    // Timer uses falling edge detection on specific DIV bits
    // We need to tick one M-cycle at a time for accuracy
    for (int i = 0; i < m_cycles; i++) {
        // Handle delayed TIMA overflow reload
        if (m_tima_overflow_cycle > 0) {
            m_tima_overflow_cycle--;
            if (m_tima_overflow_cycle == 0) {
                // TMA reload and interrupt happen now
                m_tima = m_tma;
                request_interrupt(0x04);  // Timer interrupt
            }
        }

        // Increment DIV counter by 4 T-cycles (1 M-cycle)
        m_div_counter += 4;

        // Check for timer falling edge
        bool new_bit = get_timer_bit();
        check_timer_falling_edge(new_bit);
    }
}

void Bus::tick_m_cycle() {
    // Single M-cycle tick for cycle-accurate timing
    // This is called during memory accesses to tick components
    step_timer(1);
    step_oam_dma();
    step_serial(1);
    // PPU and APU are stepped by the plugin with T-cycles for their own timing
}

void Bus::step_serial(int m_cycles) {
    if (!(m_sc & 0x80)) return;  // Transfer not active

    // Only handle internal clock
    if (!(m_sc & 0x01)) return;

    // Serial operates on T-cycles
    int t_cycles = m_cycles * 4;
    m_serial_counter += t_cycles;

    // Period in T-cycles: 512 T-cycles per bit (8192 Hz) for DMG
    // CGB with fast mode (bit 1 set): 16 T-cycles per bit
    int period = m_cgb_mode && (m_sc & 0x02) ? 16 : 512;

    while (m_serial_counter >= period && m_serial_bits < 8) {
        m_serial_counter -= period;
        m_serial_bits++;

        // Shift data out (receive 0xFF since no link partner)
        m_sb = (m_sb << 1) | 1;
    }

    if (m_serial_bits >= 8) {
        m_sc &= ~0x80;  // Clear transfer bit
        m_serial_bits = 0;
        request_interrupt(0x08);  // Serial interrupt
    }
}

void Bus::save_state(std::vector<uint8_t>& data) {
    // Save WRAM
    data.insert(data.end(), m_wram.begin(), m_wram.end());
    if (m_cgb_mode) {
        data.insert(data.end(), m_wram_cgb.begin(), m_wram_cgb.end());
    }

    // Save HRAM
    data.insert(data.end(), m_hram.begin(), m_hram.end());

    // Save I/O registers
    data.push_back(m_joyp);
    data.push_back(m_sb);
    data.push_back(m_sc);
    data.push_back(static_cast<uint8_t>(m_div_counter >> 8));
    data.push_back(m_tima);
    data.push_back(m_tma);
    data.push_back(m_tac);
    data.push_back(m_if);
    data.push_back(m_ie);

    // CGB registers
    data.push_back(m_key1);
    data.push_back(m_vbk);
    data.push_back(m_svbk);
}

void Bus::load_state(const uint8_t*& data, size_t& remaining) {
    // Load WRAM
    std::memcpy(m_wram.data(), data, m_wram.size());
    data += m_wram.size();
    remaining -= m_wram.size();

    if (m_cgb_mode) {
        std::memcpy(m_wram_cgb.data(), data, m_wram_cgb.size());
        data += m_wram_cgb.size();
        remaining -= m_wram_cgb.size();
    }

    // Load HRAM
    std::memcpy(m_hram.data(), data, m_hram.size());
    data += m_hram.size();
    remaining -= m_hram.size();

    // Load I/O registers
    m_joyp = *data++; remaining--;
    m_sb = *data++; remaining--;
    m_sc = *data++; remaining--;
    m_div_counter = static_cast<uint16_t>(*data++) << 8; remaining--;
    m_tima = *data++; remaining--;
    m_tma = *data++; remaining--;
    m_tac = *data++; remaining--;
    m_if = *data++; remaining--;
    m_ie = *data++; remaining--;

    // CGB registers
    m_key1 = *data++; remaining--;
    m_vbk = *data++; remaining--;
    m_svbk = *data++; remaining--;

    // Initialize timer falling edge state from current div_counter
    m_prev_timer_bit = get_timer_bit();
    m_tima_overflow_cycle = 0;
}

} // namespace gb
