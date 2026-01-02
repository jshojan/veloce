#include "spc700.hpp"
#include "dsp.hpp"
#include "debug.hpp"
#include <cstring>

namespace snes {

// IPL ROM - boot code that loads program from main CPU
const uint8_t SPC700::IPL_ROM[64] = {
    0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0,
    0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
    0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4,
    0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
    0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB,
    0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
    0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD,
    0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
};

SPC700::SPC700() {
    reset();
}

SPC700::~SPC700() = default;

void SPC700::reset() {
    m_a = 0;
    m_x = 0;
    m_y = 0;
    m_sp = 0xEF;
    m_pc = 0xFFC0;  // Start at IPL ROM
    m_psw = 0x00;

    m_ram.fill(0);
    m_port_out.fill(0);
    m_port_in.fill(0);
    m_timer_target.fill(0);
    m_timer_counter.fill(0);
    m_timer_output.fill(0);
    m_timer_enabled.fill(false);
    m_timer_divider.fill(0);

    m_control = 0x80;
    m_ipl_rom_enabled = true;
    m_cycles = 0;
}

int SPC700::step() {
    m_cycles = 0;
    execute();

    // Update timers
    // Timer 0/1: 8kHz (128 cycles), Timer 2: 64kHz (16 cycles)
    for (int i = 0; i < 3; i++) {
        if (m_timer_enabled[i]) {
            int divider = (i < 2) ? 128 : 16;
            m_timer_divider[i] += m_cycles;
            while (m_timer_divider[i] >= divider) {
                m_timer_divider[i] -= divider;
                m_timer_counter[i]++;
                if (m_timer_counter[i] >= m_timer_target[i] || m_timer_target[i] == 0) {
                    m_timer_counter[i] = 0;
                    m_timer_output[i] = (m_timer_output[i] + 1) & 0x0F;
                }
            }
        }
    }

    return m_cycles;
}

uint8_t SPC700::read(uint16_t address) {
    m_cycles += 1;

    // I/O registers ($00F0-$00FF when direct page is 0)
    if (address >= 0x00F0 && address <= 0x00FF) {
        switch (address) {
            case 0x00F2:  // DSP address
                return m_dsp ? m_dsp->read_address() : 0;
            case 0x00F3:  // DSP data
                return m_dsp ? m_dsp->read_data() : 0;
            case 0x00F4:
            case 0x00F5:
            case 0x00F6:
            case 0x00F7:
                return m_port_in[address - 0x00F4];
            case 0x00FD:
                { uint8_t v = m_timer_output[0]; m_timer_output[0] = 0; return v; }
            case 0x00FE:
                { uint8_t v = m_timer_output[1]; m_timer_output[1] = 0; return v; }
            case 0x00FF:
                { uint8_t v = m_timer_output[2]; m_timer_output[2] = 0; return v; }
            default:
                return m_ram[address];
        }
    }

    // IPL ROM ($FFC0-$FFFF)
    if (address >= 0xFFC0 && m_ipl_rom_enabled) {
        return IPL_ROM[address - 0xFFC0];
    }

    return m_ram[address];
}

void SPC700::write(uint16_t address, uint8_t value) {
    m_cycles += 1;

    // I/O registers
    if (address >= 0x00F0 && address <= 0x00FF) {
        switch (address) {
            case 0x00F0:  // Test register (undocumented)
                break;
            case 0x00F1:  // Control
                m_control = value;
                m_timer_enabled[0] = (value & 0x01) != 0;
                m_timer_enabled[1] = (value & 0x02) != 0;
                m_timer_enabled[2] = (value & 0x04) != 0;
                if (value & 0x10) {
                    m_port_in[0] = 0;
                    m_port_in[1] = 0;
                }
                if (value & 0x20) {
                    m_port_in[2] = 0;
                    m_port_in[3] = 0;
                }
                m_ipl_rom_enabled = (value & 0x80) != 0;
                break;
            case 0x00F2:  // DSP address
                if (m_dsp) m_dsp->write_address(value);
                break;
            case 0x00F3:  // DSP data
                if (m_dsp) m_dsp->write_data(value);
                break;
            case 0x00F4:
            case 0x00F5:
            case 0x00F6:
            case 0x00F7:
                m_port_out[address - 0x00F4] = value;
                break;
            case 0x00FA:
                m_timer_target[0] = value;
                break;
            case 0x00FB:
                m_timer_target[1] = value;
                break;
            case 0x00FC:
                m_timer_target[2] = value;
                break;
            default:
                m_ram[address] = value;
                break;
        }
        return;
    }

    m_ram[address] = value;
}

uint8_t SPC700::read_dp(uint8_t address) {
    uint16_t full_addr = address;
    if (get_flag(FLAG_P)) {
        full_addr |= 0x0100;
    }
    return read(full_addr);
}

void SPC700::write_dp(uint8_t address, uint8_t value) {
    uint16_t full_addr = address;
    if (get_flag(FLAG_P)) {
        full_addr |= 0x0100;
    }
    write(full_addr, value);
}

void SPC700::push(uint8_t value) {
    write(0x0100 | m_sp, value);
    m_sp--;
}

uint8_t SPC700::pop() {
    m_sp++;
    return read(0x0100 | m_sp);
}

void SPC700::push16(uint16_t value) {
    push(value >> 8);
    push(value & 0xFF);
}

uint16_t SPC700::pop16() {
    uint8_t lo = pop();
    uint8_t hi = pop();
    return lo | (hi << 8);
}

void SPC700::set_flag(uint8_t flag, bool value) {
    if (value) {
        m_psw |= flag;
    } else {
        m_psw &= ~flag;
    }
}

bool SPC700::get_flag(uint8_t flag) const {
    return (m_psw & flag) != 0;
}

void SPC700::update_nz(uint8_t value) {
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, (value & 0x80) != 0);
}

uint8_t SPC700::op_adc(uint8_t a, uint8_t b) {
    int c = get_flag(FLAG_C) ? 1 : 0;
    int result = a + b + c;
    set_flag(FLAG_C, result > 0xFF);
    set_flag(FLAG_H, ((a & 0x0F) + (b & 0x0F) + c) > 0x0F);
    set_flag(FLAG_V, (~(a ^ b) & (a ^ result) & 0x80) != 0);
    update_nz(result & 0xFF);
    return result & 0xFF;
}

uint8_t SPC700::op_sbc(uint8_t a, uint8_t b) {
    int c = get_flag(FLAG_C) ? 0 : 1;
    int result = a - b - c;
    set_flag(FLAG_C, result >= 0);
    set_flag(FLAG_H, ((a & 0x0F) - (b & 0x0F) - c) >= 0);
    set_flag(FLAG_V, ((a ^ b) & (a ^ result) & 0x80) != 0);
    update_nz(result & 0xFF);
    return result & 0xFF;
}

uint8_t SPC700::op_and(uint8_t a, uint8_t b) {
    uint8_t result = a & b;
    update_nz(result);
    return result;
}

uint8_t SPC700::op_or(uint8_t a, uint8_t b) {
    uint8_t result = a | b;
    update_nz(result);
    return result;
}

uint8_t SPC700::op_eor(uint8_t a, uint8_t b) {
    uint8_t result = a ^ b;
    update_nz(result);
    return result;
}

void SPC700::op_cmp(uint8_t a, uint8_t b) {
    int result = a - b;
    set_flag(FLAG_C, result >= 0);
    update_nz(result & 0xFF);
}

uint8_t SPC700::op_asl(uint8_t value) {
    set_flag(FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    update_nz(value);
    return value;
}

uint8_t SPC700::op_lsr(uint8_t value) {
    set_flag(FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    update_nz(value);
    return value;
}

uint8_t SPC700::op_rol(uint8_t value) {
    bool c = get_flag(FLAG_C);
    set_flag(FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (c ? 1 : 0);
    update_nz(value);
    return value;
}

uint8_t SPC700::op_ror(uint8_t value) {
    bool c = get_flag(FLAG_C);
    set_flag(FLAG_C, (value & 0x01) != 0);
    value = (value >> 1) | (c ? 0x80 : 0);
    update_nz(value);
    return value;
}

uint8_t SPC700::op_inc(uint8_t value) {
    value++;
    update_nz(value);
    return value;
}

uint8_t SPC700::op_dec(uint8_t value) {
    value--;
    update_nz(value);
    return value;
}

uint16_t SPC700::addr_dp() {
    uint8_t offset = read(m_pc++);
    return get_flag(FLAG_P) ? (0x0100 | offset) : offset;
}

uint16_t SPC700::addr_dp_x() {
    uint8_t offset = read(m_pc++);
    m_cycles += 1;
    return get_flag(FLAG_P) ? (0x0100 | ((offset + m_x) & 0xFF)) : ((offset + m_x) & 0xFF);
}

uint16_t SPC700::addr_dp_y() {
    uint8_t offset = read(m_pc++);
    m_cycles += 1;
    return get_flag(FLAG_P) ? (0x0100 | ((offset + m_y) & 0xFF)) : ((offset + m_y) & 0xFF);
}

uint16_t SPC700::addr_abs() {
    uint8_t lo = read(m_pc++);
    uint8_t hi = read(m_pc++);
    return lo | (hi << 8);
}

uint16_t SPC700::addr_abs_x() {
    uint16_t base = addr_abs();
    m_cycles += 1;
    return base + m_x;
}

uint16_t SPC700::addr_abs_y() {
    uint16_t base = addr_abs();
    m_cycles += 1;
    return base + m_y;
}

uint16_t SPC700::addr_dp_x_ind() {
    uint8_t offset = read(m_pc++);
    m_cycles += 1;
    uint16_t ptr = get_flag(FLAG_P) ? (0x0100 | ((offset + m_x) & 0xFF)) : ((offset + m_x) & 0xFF);
    uint8_t lo = read(ptr);
    uint8_t hi = read((ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    return lo | (hi << 8);
}

uint16_t SPC700::addr_dp_ind_y() {
    uint8_t offset = read(m_pc++);
    uint16_t ptr = get_flag(FLAG_P) ? (0x0100 | offset) : offset;
    uint8_t lo = read(ptr);
    uint8_t hi = read((ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    m_cycles += 1;
    return (lo | (hi << 8)) + m_y;
}

// Port access
uint8_t SPC700::read_port(int port) {
    return m_port_out[port & 3];
}

void SPC700::write_port(int port, uint8_t value) {
    m_port_in[port & 3] = value;
}

uint8_t SPC700::cpu_read_port(int port) {
    return m_port_out[port & 3];
}

void SPC700::cpu_write_port(int port, uint8_t value) {
    m_port_in[port & 3] = value;
}

void SPC700::execute() {
    uint8_t opcode = read(m_pc++);

    switch (opcode) {
        // MOV A, #imm
        case 0xE8: m_a = read(m_pc++); update_nz(m_a); break;
        // MOV A, (X)
        case 0xE6: m_a = read_dp(m_x); update_nz(m_a); break;
        // MOV A, (X)+
        case 0xBF: m_a = read_dp(m_x++); update_nz(m_a); m_cycles += 1; break;
        // MOV A, dp
        case 0xE4: m_a = read(addr_dp()); update_nz(m_a); break;
        // MOV A, dp+X
        case 0xF4: m_a = read(addr_dp_x()); update_nz(m_a); break;
        // MOV A, !abs
        case 0xE5: m_a = read(addr_abs()); update_nz(m_a); break;
        // MOV A, !abs+X
        case 0xF5: m_a = read(addr_abs_x()); update_nz(m_a); break;
        // MOV A, !abs+Y
        case 0xF6: m_a = read(addr_abs_y()); update_nz(m_a); break;
        // MOV A, (dp+X)
        case 0xE7: m_a = read(addr_dp_x_ind()); update_nz(m_a); break;
        // MOV A, (dp)+Y
        case 0xF7: m_a = read(addr_dp_ind_y()); update_nz(m_a); break;

        // MOV X, #imm
        case 0xCD: m_x = read(m_pc++); update_nz(m_x); break;
        // MOV X, dp
        case 0xF8: m_x = read(addr_dp()); update_nz(m_x); break;
        // MOV X, dp+Y
        case 0xF9: m_x = read(addr_dp_y()); update_nz(m_x); break;
        // MOV X, !abs
        case 0xE9: m_x = read(addr_abs()); update_nz(m_x); break;

        // MOV Y, #imm
        case 0x8D: m_y = read(m_pc++); update_nz(m_y); break;
        // MOV Y, dp
        case 0xEB: m_y = read(addr_dp()); update_nz(m_y); break;
        // MOV Y, dp+X
        case 0xFB: m_y = read(addr_dp_x()); update_nz(m_y); break;
        // MOV Y, !abs
        case 0xEC: m_y = read(addr_abs()); update_nz(m_y); break;

        // MOV (X), A
        case 0xC6: write_dp(m_x, m_a); m_cycles += 1; break;
        // MOV (X)+, A
        case 0xAF: write_dp(m_x++, m_a); m_cycles += 1; break;
        // MOV dp, A
        case 0xC4: write(addr_dp(), m_a); break;
        // MOV dp+X, A
        case 0xD4: write(addr_dp_x(), m_a); break;
        // MOV !abs, A
        case 0xC5: write(addr_abs(), m_a); break;
        // MOV !abs+X, A
        case 0xD5: write(addr_abs_x(), m_a); break;
        // MOV !abs+Y, A
        case 0xD6: write(addr_abs_y(), m_a); break;
        // MOV (dp+X), A
        case 0xC7: write(addr_dp_x_ind(), m_a); break;
        // MOV (dp)+Y, A
        case 0xD7: write(addr_dp_ind_y(), m_a); break;

        // MOV dp, X
        case 0xD8: write(addr_dp(), m_x); break;
        // MOV dp+Y, X
        case 0xD9: write(addr_dp_y(), m_x); break;
        // MOV !abs, X
        case 0xC9: write(addr_abs(), m_x); break;

        // MOV dp, Y
        case 0xCB: write(addr_dp(), m_y); break;
        // MOV dp+X, Y
        case 0xDB: write(addr_dp_x(), m_y); break;
        // MOV !abs, Y
        case 0xCC: write(addr_abs(), m_y); break;

        // MOV A, X
        case 0x7D: m_a = m_x; update_nz(m_a); m_cycles += 1; break;
        // MOV A, Y
        case 0xDD: m_a = m_y; update_nz(m_a); m_cycles += 1; break;
        // MOV X, A
        case 0x5D: m_x = m_a; update_nz(m_x); m_cycles += 1; break;
        // MOV Y, A
        case 0xFD: m_y = m_a; update_nz(m_y); m_cycles += 1; break;
        // MOV X, SP
        case 0x9D: m_x = m_sp; update_nz(m_x); m_cycles += 1; break;
        // MOV SP, X
        case 0xBD: m_sp = m_x; m_cycles += 1; break;

        // MOV dp, dp
        case 0xFA: {
            uint16_t src = addr_dp();
            uint16_t dst = addr_dp();
            write(dst, read(src));
            break;
        }
        // MOV dp, #imm
        case 0x8F: {
            uint8_t imm = read(m_pc++);
            write(addr_dp(), imm);
            break;
        }

        // ADC A, #imm
        case 0x88: m_a = op_adc(m_a, read(m_pc++)); break;
        // ADC A, (X)
        case 0x86: m_a = op_adc(m_a, read_dp(m_x)); break;
        // ADC A, dp
        case 0x84: m_a = op_adc(m_a, read(addr_dp())); break;
        // ADC A, dp+X
        case 0x94: m_a = op_adc(m_a, read(addr_dp_x())); break;
        // ADC A, !abs
        case 0x85: m_a = op_adc(m_a, read(addr_abs())); break;
        // ADC A, !abs+X
        case 0x95: m_a = op_adc(m_a, read(addr_abs_x())); break;
        // ADC A, !abs+Y
        case 0x96: m_a = op_adc(m_a, read(addr_abs_y())); break;
        // ADC A, (dp+X)
        case 0x87: m_a = op_adc(m_a, read(addr_dp_x_ind())); break;
        // ADC A, (dp)+Y
        case 0x97: m_a = op_adc(m_a, read(addr_dp_ind_y())); break;

        // SBC A, #imm
        case 0xA8: m_a = op_sbc(m_a, read(m_pc++)); break;
        // SBC A, (X)
        case 0xA6: m_a = op_sbc(m_a, read_dp(m_x)); break;
        // SBC A, dp
        case 0xA4: m_a = op_sbc(m_a, read(addr_dp())); break;
        // SBC A, dp+X
        case 0xB4: m_a = op_sbc(m_a, read(addr_dp_x())); break;
        // SBC A, !abs
        case 0xA5: m_a = op_sbc(m_a, read(addr_abs())); break;
        // SBC A, !abs+X
        case 0xB5: m_a = op_sbc(m_a, read(addr_abs_x())); break;
        // SBC A, !abs+Y
        case 0xB6: m_a = op_sbc(m_a, read(addr_abs_y())); break;
        // SBC A, (dp+X)
        case 0xA7: m_a = op_sbc(m_a, read(addr_dp_x_ind())); break;
        // SBC A, (dp)+Y
        case 0xB7: m_a = op_sbc(m_a, read(addr_dp_ind_y())); break;

        // CMP A, #imm
        case 0x68: op_cmp(m_a, read(m_pc++)); break;
        // CMP A, (X)
        case 0x66: op_cmp(m_a, read_dp(m_x)); break;
        // CMP A, dp
        case 0x64: op_cmp(m_a, read(addr_dp())); break;
        // CMP A, dp+X
        case 0x74: op_cmp(m_a, read(addr_dp_x())); break;
        // CMP A, !abs
        case 0x65: op_cmp(m_a, read(addr_abs())); break;
        // CMP A, !abs+X
        case 0x75: op_cmp(m_a, read(addr_abs_x())); break;
        // CMP A, !abs+Y
        case 0x76: op_cmp(m_a, read(addr_abs_y())); break;
        // CMP A, (dp+X)
        case 0x67: op_cmp(m_a, read(addr_dp_x_ind())); break;
        // CMP A, (dp)+Y
        case 0x77: op_cmp(m_a, read(addr_dp_ind_y())); break;

        // CMP X, #imm
        case 0xC8: op_cmp(m_x, read(m_pc++)); break;
        // CMP X, dp
        case 0x3E: op_cmp(m_x, read(addr_dp())); break;
        // CMP X, !abs
        case 0x1E: op_cmp(m_x, read(addr_abs())); break;

        // CMP Y, #imm
        case 0xAD: op_cmp(m_y, read(m_pc++)); break;
        // CMP Y, dp
        case 0x7E: op_cmp(m_y, read(addr_dp())); break;
        // CMP Y, !abs
        case 0x5E: op_cmp(m_y, read(addr_abs())); break;

        // AND A, #imm
        case 0x28: m_a = op_and(m_a, read(m_pc++)); break;
        // AND A, (X)
        case 0x26: m_a = op_and(m_a, read_dp(m_x)); break;
        // AND A, dp
        case 0x24: m_a = op_and(m_a, read(addr_dp())); break;
        // AND A, dp+X
        case 0x34: m_a = op_and(m_a, read(addr_dp_x())); break;
        // AND A, !abs
        case 0x25: m_a = op_and(m_a, read(addr_abs())); break;
        // AND A, !abs+X
        case 0x35: m_a = op_and(m_a, read(addr_abs_x())); break;
        // AND A, !abs+Y
        case 0x36: m_a = op_and(m_a, read(addr_abs_y())); break;
        // AND A, (dp+X)
        case 0x27: m_a = op_and(m_a, read(addr_dp_x_ind())); break;
        // AND A, (dp)+Y
        case 0x37: m_a = op_and(m_a, read(addr_dp_ind_y())); break;

        // OR A, #imm
        case 0x08: m_a = op_or(m_a, read(m_pc++)); break;
        // OR A, (X)
        case 0x06: m_a = op_or(m_a, read_dp(m_x)); break;
        // OR A, dp
        case 0x04: m_a = op_or(m_a, read(addr_dp())); break;
        // OR A, dp+X
        case 0x14: m_a = op_or(m_a, read(addr_dp_x())); break;
        // OR A, !abs
        case 0x05: m_a = op_or(m_a, read(addr_abs())); break;
        // OR A, !abs+X
        case 0x15: m_a = op_or(m_a, read(addr_abs_x())); break;
        // OR A, !abs+Y
        case 0x16: m_a = op_or(m_a, read(addr_abs_y())); break;
        // OR A, (dp+X)
        case 0x07: m_a = op_or(m_a, read(addr_dp_x_ind())); break;
        // OR A, (dp)+Y
        case 0x17: m_a = op_or(m_a, read(addr_dp_ind_y())); break;

        // EOR A, #imm
        case 0x48: m_a = op_eor(m_a, read(m_pc++)); break;
        // EOR A, (X)
        case 0x46: m_a = op_eor(m_a, read_dp(m_x)); break;
        // EOR A, dp
        case 0x44: m_a = op_eor(m_a, read(addr_dp())); break;
        // EOR A, dp+X
        case 0x54: m_a = op_eor(m_a, read(addr_dp_x())); break;
        // EOR A, !abs
        case 0x45: m_a = op_eor(m_a, read(addr_abs())); break;
        // EOR A, !abs+X
        case 0x55: m_a = op_eor(m_a, read(addr_abs_x())); break;
        // EOR A, !abs+Y
        case 0x56: m_a = op_eor(m_a, read(addr_abs_y())); break;
        // EOR A, (dp+X)
        case 0x47: m_a = op_eor(m_a, read(addr_dp_x_ind())); break;
        // EOR A, (dp)+Y
        case 0x57: m_a = op_eor(m_a, read(addr_dp_ind_y())); break;

        // INC A
        case 0xBC: m_a = op_inc(m_a); m_cycles += 1; break;
        // INC X
        case 0x3D: m_x = op_inc(m_x); m_cycles += 1; break;
        // INC Y
        case 0xFC: m_y = op_inc(m_y); m_cycles += 1; break;
        // INC dp
        case 0xAB: { uint16_t a = addr_dp(); write(a, op_inc(read(a))); break; }
        // INC dp+X
        case 0xBB: { uint16_t a = addr_dp_x(); write(a, op_inc(read(a))); break; }
        // INC !abs
        case 0xAC: { uint16_t a = addr_abs(); write(a, op_inc(read(a))); break; }

        // DEC A
        case 0x9C: m_a = op_dec(m_a); m_cycles += 1; break;
        // DEC X
        case 0x1D: m_x = op_dec(m_x); m_cycles += 1; break;
        // DEC Y
        case 0xDC: m_y = op_dec(m_y); m_cycles += 1; break;
        // DEC dp
        case 0x8B: { uint16_t a = addr_dp(); write(a, op_dec(read(a))); break; }
        // DEC dp+X
        case 0x9B: { uint16_t a = addr_dp_x(); write(a, op_dec(read(a))); break; }
        // DEC !abs
        case 0x8C: { uint16_t a = addr_abs(); write(a, op_dec(read(a))); break; }

        // ASL A
        case 0x1C: m_a = op_asl(m_a); m_cycles += 1; break;
        // ASL dp
        case 0x0B: { uint16_t a = addr_dp(); write(a, op_asl(read(a))); break; }
        // ASL dp+X
        case 0x1B: { uint16_t a = addr_dp_x(); write(a, op_asl(read(a))); break; }
        // ASL !abs
        case 0x0C: { uint16_t a = addr_abs(); write(a, op_asl(read(a))); break; }

        // LSR A
        case 0x5C: m_a = op_lsr(m_a); m_cycles += 1; break;
        // LSR dp
        case 0x4B: { uint16_t a = addr_dp(); write(a, op_lsr(read(a))); break; }
        // LSR dp+X
        case 0x5B: { uint16_t a = addr_dp_x(); write(a, op_lsr(read(a))); break; }
        // LSR !abs
        case 0x4C: { uint16_t a = addr_abs(); write(a, op_lsr(read(a))); break; }

        // ROL A
        case 0x3C: m_a = op_rol(m_a); m_cycles += 1; break;
        // ROL dp
        case 0x2B: { uint16_t a = addr_dp(); write(a, op_rol(read(a))); break; }
        // ROL dp+X
        case 0x3B: { uint16_t a = addr_dp_x(); write(a, op_rol(read(a))); break; }
        // ROL !abs
        case 0x2C: { uint16_t a = addr_abs(); write(a, op_rol(read(a))); break; }

        // ROR A
        case 0x7C: m_a = op_ror(m_a); m_cycles += 1; break;
        // ROR dp
        case 0x6B: { uint16_t a = addr_dp(); write(a, op_ror(read(a))); break; }
        // ROR dp+X
        case 0x7B: { uint16_t a = addr_dp_x(); write(a, op_ror(read(a))); break; }
        // ROR !abs
        case 0x6C: { uint16_t a = addr_abs(); write(a, op_ror(read(a))); break; }

        // XCN A (exchange nibbles)
        case 0x9F: m_a = ((m_a >> 4) | (m_a << 4)); update_nz(m_a); m_cycles += 4; break;

        // MOVW YA, dp
        case 0xBA: {
            uint16_t addr = addr_dp();
            m_a = read(addr);
            m_y = read((addr & 0xFF00) | ((addr + 1) & 0xFF));
            set_flag(FLAG_Z, (m_a | m_y) == 0);
            set_flag(FLAG_N, (m_y & 0x80) != 0);
            break;
        }
        // MOVW dp, YA
        case 0xDA: {
            uint16_t addr = addr_dp();
            write(addr, m_a);
            write((addr & 0xFF00) | ((addr + 1) & 0xFF), m_y);
            break;
        }

        // INCW dp
        case 0x3A: {
            uint16_t addr = addr_dp();
            uint16_t val = read(addr) | (read((addr & 0xFF00) | ((addr + 1) & 0xFF)) << 8);
            val++;
            write(addr, val & 0xFF);
            write((addr & 0xFF00) | ((addr + 1) & 0xFF), val >> 8);
            set_flag(FLAG_Z, val == 0);
            set_flag(FLAG_N, (val & 0x8000) != 0);
            m_cycles += 1;
            break;
        }
        // DECW dp
        case 0x1A: {
            uint16_t addr = addr_dp();
            uint16_t val = read(addr) | (read((addr & 0xFF00) | ((addr + 1) & 0xFF)) << 8);
            val--;
            write(addr, val & 0xFF);
            write((addr & 0xFF00) | ((addr + 1) & 0xFF), val >> 8);
            set_flag(FLAG_Z, val == 0);
            set_flag(FLAG_N, (val & 0x8000) != 0);
            m_cycles += 1;
            break;
        }

        // ADDW YA, dp
        case 0x7A: {
            uint16_t addr = addr_dp();
            uint16_t val = read(addr) | (read((addr & 0xFF00) | ((addr + 1) & 0xFF)) << 8);
            uint16_t ya = m_a | (m_y << 8);
            uint32_t result = ya + val;
            set_flag(FLAG_C, result > 0xFFFF);
            set_flag(FLAG_H, ((ya & 0x0FFF) + (val & 0x0FFF)) > 0x0FFF);
            set_flag(FLAG_V, (~(ya ^ val) & (ya ^ result) & 0x8000) != 0);
            m_a = result & 0xFF;
            m_y = (result >> 8) & 0xFF;
            set_flag(FLAG_Z, (m_a | m_y) == 0);
            set_flag(FLAG_N, (m_y & 0x80) != 0);
            m_cycles += 1;
            break;
        }
        // SUBW YA, dp
        case 0x9A: {
            uint16_t addr = addr_dp();
            uint16_t val = read(addr) | (read((addr & 0xFF00) | ((addr + 1) & 0xFF)) << 8);
            uint16_t ya = m_a | (m_y << 8);
            int32_t result = ya - val;
            set_flag(FLAG_C, result >= 0);
            set_flag(FLAG_H, ((ya & 0x0FFF) - (val & 0x0FFF)) >= 0);
            set_flag(FLAG_V, ((ya ^ val) & (ya ^ result) & 0x8000) != 0);
            m_a = result & 0xFF;
            m_y = (result >> 8) & 0xFF;
            set_flag(FLAG_Z, (m_a | m_y) == 0);
            set_flag(FLAG_N, (m_y & 0x80) != 0);
            m_cycles += 1;
            break;
        }
        // CMPW YA, dp
        case 0x5A: {
            uint16_t addr = addr_dp();
            uint16_t val = read(addr) | (read((addr & 0xFF00) | ((addr + 1) & 0xFF)) << 8);
            uint16_t ya = m_a | (m_y << 8);
            int32_t result = ya - val;
            set_flag(FLAG_C, result >= 0);
            set_flag(FLAG_Z, (result & 0xFFFF) == 0);
            set_flag(FLAG_N, (result & 0x8000) != 0);
            break;
        }

        // MUL YA
        case 0xCF: {
            uint16_t result = m_y * m_a;
            m_a = result & 0xFF;
            m_y = (result >> 8) & 0xFF;
            set_flag(FLAG_Z, m_y == 0);
            set_flag(FLAG_N, (m_y & 0x80) != 0);
            m_cycles += 8;
            break;
        }
        // DIV YA, X
        case 0x9E: {
            uint16_t ya = m_a | (m_y << 8);
            set_flag(FLAG_H, (m_x & 0x0F) <= (m_y & 0x0F));
            set_flag(FLAG_V, m_y >= m_x);
            if (m_y < (m_x << 1)) {
                m_a = ya / m_x;
                m_y = ya % m_x;
            } else {
                m_a = 255 - (ya - (m_x << 9)) / (256 - m_x);
                m_y = m_x + (ya - (m_x << 9)) % (256 - m_x);
            }
            set_flag(FLAG_Z, m_a == 0);
            set_flag(FLAG_N, (m_a & 0x80) != 0);
            m_cycles += 11;
            break;
        }

        // DAA
        case 0xDF:
            if (get_flag(FLAG_C) || m_a > 0x99) {
                m_a += 0x60;
                set_flag(FLAG_C, true);
            }
            if (get_flag(FLAG_H) || (m_a & 0x0F) > 0x09) {
                m_a += 0x06;
            }
            update_nz(m_a);
            m_cycles += 2;
            break;

        // DAS
        case 0xBE:
            if (!get_flag(FLAG_C) || m_a > 0x99) {
                m_a -= 0x60;
                set_flag(FLAG_C, false);
            }
            if (!get_flag(FLAG_H) || (m_a & 0x0F) > 0x09) {
                m_a -= 0x06;
            }
            update_nz(m_a);
            m_cycles += 2;
            break;

        // Branches
        case 0x2F: {  // BRA rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            m_pc += offset;
            m_cycles += 2;
            break;
        }
        case 0xF0: {  // BEQ rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (get_flag(FLAG_Z)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        case 0xD0: {  // BNE rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (!get_flag(FLAG_Z)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        case 0xB0: {  // BCS rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (get_flag(FLAG_C)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        case 0x90: {  // BCC rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (!get_flag(FLAG_C)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        case 0x70: {  // BVS rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (get_flag(FLAG_V)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        case 0x50: {  // BVC rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (!get_flag(FLAG_V)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        case 0x30: {  // BMI rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (get_flag(FLAG_N)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        case 0x10: {  // BPL rel
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (!get_flag(FLAG_N)) { m_pc += offset; m_cycles += 2; }
            break;
        }

        // CBNE dp, rel
        case 0x2E: {
            uint16_t addr = addr_dp();
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (m_a != read(addr)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        // CBNE dp+X, rel
        case 0xDE: {
            uint16_t addr = addr_dp_x();
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (m_a != read(addr)) { m_pc += offset; m_cycles += 2; }
            break;
        }
        // DBNZ dp, rel
        case 0x6E: {
            uint16_t addr = addr_dp();
            uint8_t val = read(addr) - 1;
            write(addr, val);
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (val != 0) { m_pc += offset; m_cycles += 2; }
            break;
        }
        // DBNZ Y, rel
        case 0xFE: {
            m_y--;
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (m_y != 0) { m_pc += offset; m_cycles += 2; }
            m_cycles += 2;
            break;
        }

        // JMP !abs
        case 0x5F: m_pc = addr_abs(); break;
        // JMP (abs+X)
        case 0x1F: {
            uint16_t addr = addr_abs() + m_x;
            m_pc = read(addr) | (read(addr + 1) << 8);
            break;
        }

        // CALL !abs
        case 0x3F: {
            uint16_t addr = addr_abs();
            push16(m_pc);
            m_pc = addr;
            m_cycles += 3;
            break;
        }
        // PCALL up
        case 0x4F: {
            uint8_t offset = read(m_pc++);
            push16(m_pc);
            m_pc = 0xFF00 | offset;
            m_cycles += 2;
            break;
        }
        // TCALL n
        case 0x01: case 0x11: case 0x21: case 0x31:
        case 0x41: case 0x51: case 0x61: case 0x71:
        case 0x81: case 0x91: case 0xA1: case 0xB1:
        case 0xC1: case 0xD1: case 0xE1: case 0xF1: {
            int n = opcode >> 4;
            push16(m_pc);
            uint16_t addr = 0xFFDE - (n * 2);
            m_pc = read(addr) | (read(addr + 1) << 8);
            m_cycles += 5;
            break;
        }
        // BRK
        case 0x0F: {
            push16(m_pc);
            push(m_psw);
            set_flag(FLAG_B, true);
            set_flag(FLAG_I, false);
            m_pc = read(0xFFDE) | (read(0xFFDF) << 8);
            m_cycles += 5;
            break;
        }
        // RET
        case 0x6F:
            m_pc = pop16();
            m_cycles += 2;
            break;
        // RETI
        case 0x7F:
            m_psw = pop();
            m_pc = pop16();
            m_cycles += 2;
            break;

        // PUSH A/X/Y/PSW
        case 0x2D: push(m_a); m_cycles += 2; break;
        case 0x4D: push(m_x); m_cycles += 2; break;
        case 0x6D: push(m_y); m_cycles += 2; break;
        case 0x0D: push(m_psw); m_cycles += 2; break;
        // POP A/X/Y/PSW
        case 0xAE: m_a = pop(); m_cycles += 2; break;
        case 0xCE: m_x = pop(); m_cycles += 2; break;
        case 0xEE: m_y = pop(); m_cycles += 2; break;
        case 0x8E: m_psw = pop(); m_cycles += 2; break;

        // SET1 dp.n / CLR1 dp.n
        case 0x02: case 0x22: case 0x42: case 0x62:
        case 0x82: case 0xA2: case 0xC2: case 0xE2: {
            uint16_t addr = addr_dp();
            int bit = (opcode >> 5);
            write(addr, read(addr) | (1 << bit));
            break;
        }
        case 0x12: case 0x32: case 0x52: case 0x72:
        case 0x92: case 0xB2: case 0xD2: case 0xF2: {
            uint16_t addr = addr_dp();
            int bit = (opcode >> 5);
            write(addr, read(addr) & ~(1 << bit));
            break;
        }

        // BBC dp.n, rel / BBS dp.n, rel
        case 0x13: case 0x33: case 0x53: case 0x73:
        case 0x93: case 0xB3: case 0xD3: case 0xF3: {
            uint16_t addr = addr_dp();
            int bit = (opcode >> 5);
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (!(read(addr) & (1 << bit))) { m_pc += offset; m_cycles += 2; }
            break;
        }
        case 0x03: case 0x23: case 0x43: case 0x63:
        case 0x83: case 0xA3: case 0xC3: case 0xE3: {
            uint16_t addr = addr_dp();
            int bit = (opcode >> 5);
            int8_t offset = static_cast<int8_t>(read(m_pc++));
            if (read(addr) & (1 << bit)) { m_pc += offset; m_cycles += 2; }
            break;
        }

        // Flag operations
        case 0x60: set_flag(FLAG_C, false); m_cycles += 1; break;  // CLRC
        case 0x80: set_flag(FLAG_C, true); m_cycles += 1; break;   // SETC
        case 0xED: m_psw ^= FLAG_C; m_cycles += 2; break;          // NOTC
        case 0xE0: set_flag(FLAG_V, false); set_flag(FLAG_H, false); m_cycles += 1; break;  // CLRV
        case 0x20: set_flag(FLAG_P, false); m_cycles += 1; break;  // CLRP
        case 0x40: set_flag(FLAG_P, true); m_cycles += 1; break;   // SETP
        case 0xA0: set_flag(FLAG_I, true); m_cycles += 2; break;   // EI
        case 0xC0: set_flag(FLAG_I, false); m_cycles += 2; break;  // DI

        // NOP
        case 0x00: m_cycles += 1; break;
        // SLEEP
        case 0xEF: m_cycles += 2; break;  // Just idle
        // STOP
        case 0xFF: m_cycles += 2; break;  // Just idle

        // TSET1 !abs
        case 0x0E: {
            uint16_t addr = addr_abs();
            uint8_t val = read(addr);
            update_nz(m_a - val);
            write(addr, val | m_a);
            break;
        }
        // TCLR1 !abs
        case 0x4E: {
            uint16_t addr = addr_abs();
            uint8_t val = read(addr);
            update_nz(m_a - val);
            write(addr, val & ~m_a);
            break;
        }

        // AND1 C, mem.bit
        case 0x4A: {
            uint16_t addr = addr_abs();
            int bit = (addr >> 13) & 7;
            addr &= 0x1FFF;
            bool b = (read(addr) >> bit) & 1;
            set_flag(FLAG_C, get_flag(FLAG_C) && b);
            break;
        }
        // AND1 C, /mem.bit
        case 0x6A: {
            uint16_t addr = addr_abs();
            int bit = (addr >> 13) & 7;
            addr &= 0x1FFF;
            bool b = (read(addr) >> bit) & 1;
            set_flag(FLAG_C, get_flag(FLAG_C) && !b);
            break;
        }
        // OR1 C, mem.bit
        case 0x0A: {
            uint16_t addr = addr_abs();
            int bit = (addr >> 13) & 7;
            addr &= 0x1FFF;
            bool b = (read(addr) >> bit) & 1;
            set_flag(FLAG_C, get_flag(FLAG_C) || b);
            break;
        }
        // OR1 C, /mem.bit
        case 0x2A: {
            uint16_t addr = addr_abs();
            int bit = (addr >> 13) & 7;
            addr &= 0x1FFF;
            bool b = (read(addr) >> bit) & 1;
            set_flag(FLAG_C, get_flag(FLAG_C) || !b);
            break;
        }
        // EOR1 C, mem.bit
        case 0x8A: {
            uint16_t addr = addr_abs();
            int bit = (addr >> 13) & 7;
            addr &= 0x1FFF;
            bool b = (read(addr) >> bit) & 1;
            set_flag(FLAG_C, get_flag(FLAG_C) != b);
            break;
        }
        // NOT1 mem.bit
        case 0xEA: {
            uint16_t addr = addr_abs();
            int bit = (addr >> 13) & 7;
            addr &= 0x1FFF;
            write(addr, read(addr) ^ (1 << bit));
            break;
        }
        // MOV1 C, mem.bit
        case 0xAA: {
            uint16_t addr = addr_abs();
            int bit = (addr >> 13) & 7;
            addr &= 0x1FFF;
            set_flag(FLAG_C, (read(addr) >> bit) & 1);
            break;
        }
        // MOV1 mem.bit, C
        case 0xCA: {
            uint16_t addr = addr_abs();
            int bit = (addr >> 13) & 7;
            addr &= 0x1FFF;
            uint8_t val = read(addr);
            if (get_flag(FLAG_C)) {
                val |= (1 << bit);
            } else {
                val &= ~(1 << bit);
            }
            write(addr, val);
            break;
        }

        // ADC/SBC/AND/OR/EOR/CMP dp, dp
        case 0x89: {
            uint16_t src = addr_dp();
            uint16_t dst = addr_dp();
            write(dst, op_adc(read(dst), read(src)));
            break;
        }
        case 0xA9: {
            uint16_t src = addr_dp();
            uint16_t dst = addr_dp();
            write(dst, op_sbc(read(dst), read(src)));
            break;
        }
        case 0x29: {
            uint16_t src = addr_dp();
            uint16_t dst = addr_dp();
            write(dst, op_and(read(dst), read(src)));
            break;
        }
        case 0x09: {
            uint16_t src = addr_dp();
            uint16_t dst = addr_dp();
            write(dst, op_or(read(dst), read(src)));
            break;
        }
        case 0x49: {
            uint16_t src = addr_dp();
            uint16_t dst = addr_dp();
            write(dst, op_eor(read(dst), read(src)));
            break;
        }
        case 0x69: {
            uint16_t src = addr_dp();
            uint16_t dst = addr_dp();
            op_cmp(read(dst), read(src));
            break;
        }

        // ADC/SBC/AND/OR/EOR/CMP dp, #imm
        case 0x98: {
            uint8_t imm = read(m_pc++);
            uint16_t addr = addr_dp();
            write(addr, op_adc(read(addr), imm));
            break;
        }
        case 0xB8: {
            uint8_t imm = read(m_pc++);
            uint16_t addr = addr_dp();
            write(addr, op_sbc(read(addr), imm));
            break;
        }
        case 0x38: {
            uint8_t imm = read(m_pc++);
            uint16_t addr = addr_dp();
            write(addr, op_and(read(addr), imm));
            break;
        }
        case 0x18: {
            uint8_t imm = read(m_pc++);
            uint16_t addr = addr_dp();
            write(addr, op_or(read(addr), imm));
            break;
        }
        case 0x58: {
            uint8_t imm = read(m_pc++);
            uint16_t addr = addr_dp();
            write(addr, op_eor(read(addr), imm));
            break;
        }
        case 0x78: {
            uint8_t imm = read(m_pc++);
            uint16_t addr = addr_dp();
            op_cmp(read(addr), imm);
            break;
        }

        // ADC/SBC (X), (Y)
        case 0x99: {
            uint8_t x_val = read_dp(m_x);
            uint8_t y_val = read_dp(m_y);
            write_dp(m_x, op_adc(x_val, y_val));
            m_cycles += 1;
            break;
        }
        case 0xB9: {
            uint8_t x_val = read_dp(m_x);
            uint8_t y_val = read_dp(m_y);
            write_dp(m_x, op_sbc(x_val, y_val));
            m_cycles += 1;
            break;
        }

        default:
            SNES_APU_DEBUG("Unknown SPC700 opcode: $%02X at $%04X\n", opcode, m_pc - 1);
            m_cycles += 2;
            break;
    }
}

void SPC700::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_a);
    data.push_back(m_x);
    data.push_back(m_y);
    data.push_back(m_sp);
    data.push_back(m_pc & 0xFF);
    data.push_back(m_pc >> 8);
    data.push_back(m_psw);
    data.push_back(m_control);
    data.push_back(m_ipl_rom_enabled ? 1 : 0);

    data.insert(data.end(), m_ram.begin(), m_ram.end());
    data.insert(data.end(), m_port_in.begin(), m_port_in.end());
    data.insert(data.end(), m_port_out.begin(), m_port_out.end());
    data.insert(data.end(), m_timer_target.begin(), m_timer_target.end());
    data.insert(data.end(), m_timer_counter.begin(), m_timer_counter.end());
    data.insert(data.end(), m_timer_output.begin(), m_timer_output.end());

    for (int i = 0; i < 3; i++) {
        data.push_back(m_timer_enabled[i] ? 1 : 0);
    }
}

void SPC700::load_state(const uint8_t*& data, size_t& remaining) {
    m_a = *data++; remaining--;
    m_x = *data++; remaining--;
    m_y = *data++; remaining--;
    m_sp = *data++; remaining--;
    m_pc = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
    m_psw = *data++; remaining--;
    m_control = *data++; remaining--;
    m_ipl_rom_enabled = (*data++ != 0); remaining--;

    std::memcpy(m_ram.data(), data, m_ram.size());
    data += m_ram.size(); remaining -= m_ram.size();
    std::memcpy(m_port_in.data(), data, m_port_in.size());
    data += m_port_in.size(); remaining -= m_port_in.size();
    std::memcpy(m_port_out.data(), data, m_port_out.size());
    data += m_port_out.size(); remaining -= m_port_out.size();
    std::memcpy(m_timer_target.data(), data, m_timer_target.size());
    data += m_timer_target.size(); remaining -= m_timer_target.size();
    std::memcpy(m_timer_counter.data(), data, m_timer_counter.size());
    data += m_timer_counter.size(); remaining -= m_timer_counter.size();
    std::memcpy(m_timer_output.data(), data, m_timer_output.size());
    data += m_timer_output.size(); remaining -= m_timer_output.size();

    for (int i = 0; i < 3; i++) {
        m_timer_enabled[i] = (*data++ != 0); remaining--;
    }
}

} // namespace snes
