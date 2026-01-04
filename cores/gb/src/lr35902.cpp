#include "lr35902.hpp"
#include "bus.hpp"
#include <cstring>

namespace gb {

// Cycle counts for main instructions (in M-cycles, 1 M-cycle = 4 T-cycles)
const uint8_t LR35902::s_cycle_table[256] = {
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    1, 3, 2, 2, 1, 1, 2, 1, 5, 2, 2, 2, 1, 1, 2, 1,  // 0x00
    1, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,  // 0x10
    2, 3, 2, 2, 1, 1, 2, 1, 2, 2, 2, 2, 1, 1, 2, 1,  // 0x20
    2, 3, 2, 2, 3, 3, 3, 1, 2, 2, 2, 2, 1, 1, 2, 1,  // 0x30
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,  // 0x40
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,  // 0x50
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,  // 0x60
    2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1,  // 0x70
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,  // 0x80
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,  // 0x90
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,  // 0xA0
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,  // 0xB0
    2, 3, 3, 4, 3, 4, 2, 4, 2, 4, 3, 1, 3, 6, 2, 4,  // 0xC0
    2, 3, 3, 0, 3, 4, 2, 4, 2, 4, 3, 0, 3, 0, 2, 4,  // 0xD0
    3, 3, 2, 0, 0, 4, 2, 4, 4, 1, 4, 0, 0, 0, 2, 4,  // 0xE0
    3, 3, 2, 1, 0, 4, 2, 4, 3, 2, 4, 1, 0, 0, 2, 4,  // 0xF0
};

// Cycle counts for CB-prefixed instructions
const uint8_t LR35902::s_cb_cycle_table[256] = {
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0x00 RLC
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0x10 RRC
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0x20 RL
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0x30 RR
    2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2,  // 0x40 BIT (HL) is 3
    2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2,  // 0x50 BIT
    2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2,  // 0x60 BIT
    2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2,  // 0x70 BIT
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0x80 RES
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0x90 RES
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0xA0 RES
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0xB0 RES
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0xC0 SET
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0xD0 SET
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0xE0 SET
    2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,  // 0xF0 SET
};

LR35902::LR35902(Bus& bus) : m_bus(bus) {
    reset();
}

LR35902::~LR35902() = default;

void LR35902::reset() {
    // Post-boot ROM state (simulating boot ROM execution)
    m_a = 0x01;  // 0x11 for GBC
    m_f = 0xB0;
    m_b = 0x00;
    m_c = 0x13;
    m_d = 0x00;
    m_e = 0xD8;
    m_h = 0x01;
    m_l = 0x4D;
    m_sp = 0xFFFE;
    m_pc = 0x0100;  // Entry point after boot ROM

    m_ime = false;
    m_ime_pending = false;
    m_halted = false;
    m_halt_bug = false;
}

uint8_t LR35902::read(uint16_t address) {
    // Read first, then tick - the read samples current state, then time advances
    // This matches: the memory operation sees the state at the END of the previous cycle
    uint8_t value = m_bus.read(address);
    m_bus.tick_m_cycle();
    return value;
}

void LR35902::write(uint16_t address, uint8_t value) {
    // Write first, then tick - the write affects current state, then time advances
    m_bus.write(address, value);
    m_bus.tick_m_cycle();
}

uint8_t LR35902::fetch() {
    uint8_t value = read(m_pc);
    if (!m_halt_bug) {
        m_pc++;
    }
    m_halt_bug = false;
    return value;
}

void LR35902::internal_cycle() {
    // An internal cycle with no memory access - still takes 1 M-cycle
    m_bus.tick_m_cycle();
}

void LR35902::check_oam_bug(uint16_t addr, bool is_read) {
    // DMG OAM corruption bug: triggered by 16-bit register pair operations
    // when the register contains an address in the OAM range (0xFE00-0xFEFF)
    // during PPU mode 2 (OAM scan)
    if (addr >= 0xFE00 && addr < 0xFF00) {
        m_bus.trigger_oam_bug(addr, is_read);
    }
}

uint16_t LR35902::fetch16() {
    uint8_t lo = fetch();
    uint8_t hi = fetch();
    return make_u16(lo, hi);
}

void LR35902::push(uint16_t value) {
    // OAM bug: decrementing SP when it points to OAM range triggers corruption
    // PUSH decrements SP by 2 before writing
    check_oam_bug(m_sp, false);
    m_sp -= 2;
    write(m_sp, value & 0xFF);
    write(m_sp + 1, value >> 8);
}

uint16_t LR35902::pop() {
    uint16_t value = read(m_sp) | (read(m_sp + 1) << 8);
    // OAM bug: incrementing SP when it points to OAM range triggers corruption
    // POP increments SP by 2 after reading
    check_oam_bug(m_sp, true);
    m_sp += 2;
    return value;
}

void LR35902::handle_interrupts(uint8_t pending) {
    if (!m_ime && !m_halted) return;

    // Wake from HALT even if IME is disabled
    if (m_halted && pending) {
        m_halted = false;
    }

    if (!m_ime) return;

    // Check interrupts in priority order
    static const struct {
        uint8_t bit;
        uint16_t vector;
    } interrupts[] = {
        {0x01, 0x0040},  // VBlank
        {0x02, 0x0048},  // LCD STAT
        {0x04, 0x0050},  // Timer
        {0x08, 0x0058},  // Serial
        {0x10, 0x0060},  // Joypad
    };

    for (const auto& irq : interrupts) {
        if (pending & irq.bit) {
            m_ime = false;
            m_bus.clear_interrupt(irq.bit);
            push(m_pc);
            m_pc = irq.vector;
            break;  // Only handle one interrupt
        }
    }
}

int LR35902::step() {
    // Handle pending EI
    if (m_ime_pending) {
        m_ime_pending = false;
        m_ime = true;
    }

    // If halted, just consume 1 cycle
    if (m_halted) {
        return 1;
    }

    uint8_t opcode = fetch();
    int cycles = s_cycle_table[opcode];

    // Execute instruction
    switch (opcode) {
        // NOP
        case 0x00: break;

        // LD BC, nn
        case 0x01: set_bc(fetch16()); break;

        // LD (BC), A
        case 0x02: write(get_bc(), m_a); break;

        // INC BC (2 cycles: fetch + internal)
        case 0x03: check_oam_bug(get_bc(), false); set_bc(get_bc() + 1); internal_cycle(); break;

        // INC B
        case 0x04: m_b = alu_inc(m_b); break;

        // DEC B
        case 0x05: m_b = alu_dec(m_b); break;

        // LD B, n
        case 0x06: m_b = fetch(); break;

        // RLCA
        case 0x07:
            m_a = rlc(m_a);
            set_flag_z(false);  // RLCA always clears Z
            break;

        // LD (nn), SP
        case 0x08: {
            uint16_t addr = fetch16();
            write(addr, m_sp & 0xFF);
            write(addr + 1, m_sp >> 8);
            break;
        }

        // ADD HL, BC (2 cycles: fetch + internal)
        case 0x09: {
            uint32_t result = get_hl() + get_bc();
            set_flag_n(false);
            set_flag_h((get_hl() & 0xFFF) + (get_bc() & 0xFFF) > 0xFFF);
            set_flag_c(result > 0xFFFF);
            set_hl(result & 0xFFFF);
            internal_cycle();
            break;
        }

        // LD A, (BC)
        case 0x0A: m_a = read(get_bc()); break;

        // DEC BC (2 cycles: fetch + internal)
        case 0x0B: check_oam_bug(get_bc(), false); set_bc(get_bc() - 1); internal_cycle(); break;

        // INC C
        case 0x0C: m_c = alu_inc(m_c); break;

        // DEC C
        case 0x0D: m_c = alu_dec(m_c); break;

        // LD C, n
        case 0x0E: m_c = fetch(); break;

        // RRCA
        case 0x0F:
            m_a = rrc(m_a);
            set_flag_z(false);
            break;

        // STOP
        case 0x10:
            fetch();  // Skip next byte
            // TODO: Handle double-speed mode switch for CGB
            break;

        // LD DE, nn
        case 0x11: set_de(fetch16()); break;

        // LD (DE), A
        case 0x12: write(get_de(), m_a); break;

        // INC DE (2 cycles: fetch + internal)
        case 0x13: check_oam_bug(get_de(), false); set_de(get_de() + 1); internal_cycle(); break;

        // INC D
        case 0x14: m_d = alu_inc(m_d); break;

        // DEC D
        case 0x15: m_d = alu_dec(m_d); break;

        // LD D, n
        case 0x16: m_d = fetch(); break;

        // RLA
        case 0x17:
            m_a = rl(m_a);
            set_flag_z(false);
            break;

        // JR n (3 cycles: fetch opcode + fetch offset + internal)
        case 0x18: {
            int8_t offset = static_cast<int8_t>(fetch());
            m_pc += offset;
            internal_cycle();
            break;
        }

        // ADD HL, DE (2 cycles: fetch + internal)
        case 0x19: {
            uint32_t result = get_hl() + get_de();
            set_flag_n(false);
            set_flag_h((get_hl() & 0xFFF) + (get_de() & 0xFFF) > 0xFFF);
            set_flag_c(result > 0xFFFF);
            set_hl(result & 0xFFFF);
            internal_cycle();
            break;
        }

        // LD A, (DE)
        case 0x1A: m_a = read(get_de()); break;

        // DEC DE (2 cycles: fetch + internal)
        case 0x1B: check_oam_bug(get_de(), false); set_de(get_de() - 1); internal_cycle(); break;

        // INC E
        case 0x1C: m_e = alu_inc(m_e); break;

        // DEC E
        case 0x1D: m_e = alu_dec(m_e); break;

        // LD E, n
        case 0x1E: m_e = fetch(); break;

        // RRA
        case 0x1F:
            m_a = rr(m_a);
            set_flag_z(false);
            break;

        // JR NZ, n (2 cycles if not taken, 3 if taken)
        case 0x20: {
            int8_t offset = static_cast<int8_t>(fetch());
            if (!get_flag_z()) {
                m_pc += offset;
                internal_cycle();
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }

        // LD HL, nn
        case 0x21: set_hl(fetch16()); break;

        // LD (HL+), A - OAM bug triggers on the increment (is_read=false for write+inc)
        case 0x22: write(get_hl(), m_a); check_oam_bug(get_hl(), false); set_hl(get_hl() + 1); break;

        // INC HL (2 cycles: fetch + internal)
        case 0x23: check_oam_bug(get_hl(), false); set_hl(get_hl() + 1); internal_cycle(); break;

        // INC H
        case 0x24: m_h = alu_inc(m_h); break;

        // DEC H
        case 0x25: m_h = alu_dec(m_h); break;

        // LD H, n
        case 0x26: m_h = fetch(); break;

        // DAA
        case 0x27: {
            uint16_t a = m_a;
            if (get_flag_n()) {
                if (get_flag_h()) a -= 0x06;
                if (get_flag_c()) a -= 0x60;
            } else {
                if (get_flag_h() || (a & 0x0F) > 0x09) a += 0x06;
                if (get_flag_c() || a > 0x9F) {
                    a += 0x60;
                    set_flag_c(true);
                }
            }
            m_a = a & 0xFF;
            set_flag_z(m_a == 0);
            set_flag_h(false);
            break;
        }

        // JR Z, n (2 cycles if not taken, 3 if taken)
        case 0x28: {
            int8_t offset = static_cast<int8_t>(fetch());
            if (get_flag_z()) {
                m_pc += offset;
                internal_cycle();
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }

        // ADD HL, HL (2 cycles: fetch + internal)
        case 0x29: {
            uint32_t result = get_hl() + get_hl();
            set_flag_n(false);
            set_flag_h((get_hl() & 0xFFF) + (get_hl() & 0xFFF) > 0xFFF);
            set_flag_c(result > 0xFFFF);
            set_hl(result & 0xFFFF);
            internal_cycle();
            break;
        }

        // LD A, (HL+) - OAM bug triggers on the increment (is_read=true for read+inc)
        case 0x2A: m_a = read(get_hl()); check_oam_bug(get_hl(), true); set_hl(get_hl() + 1); break;

        // DEC HL (2 cycles: fetch + internal)
        case 0x2B: check_oam_bug(get_hl(), false); set_hl(get_hl() - 1); internal_cycle(); break;

        // INC L
        case 0x2C: m_l = alu_inc(m_l); break;

        // DEC L
        case 0x2D: m_l = alu_dec(m_l); break;

        // LD L, n
        case 0x2E: m_l = fetch(); break;

        // CPL
        case 0x2F:
            m_a = ~m_a;
            set_flag_n(true);
            set_flag_h(true);
            break;

        // JR NC, n (2 cycles if not taken, 3 if taken)
        case 0x30: {
            int8_t offset = static_cast<int8_t>(fetch());
            if (!get_flag_c()) {
                m_pc += offset;
                internal_cycle();
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }

        // LD SP, nn
        case 0x31: m_sp = fetch16(); break;

        // LD (HL-), A - OAM bug triggers on the decrement
        case 0x32: write(get_hl(), m_a); check_oam_bug(get_hl(), false); set_hl(get_hl() - 1); break;

        // INC SP (2 cycles: fetch + internal) - SP can point to OAM
        case 0x33: check_oam_bug(m_sp, false); m_sp++; internal_cycle(); break;

        // INC (HL)
        case 0x34: write(get_hl(), alu_inc(read(get_hl()))); break;

        // DEC (HL)
        case 0x35: write(get_hl(), alu_dec(read(get_hl()))); break;

        // LD (HL), n
        case 0x36: write(get_hl(), fetch()); break;

        // SCF
        case 0x37:
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(true);
            break;

        // JR C, n (2 cycles if not taken, 3 if taken)
        case 0x38: {
            int8_t offset = static_cast<int8_t>(fetch());
            if (get_flag_c()) {
                m_pc += offset;
                internal_cycle();
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }

        // ADD HL, SP (2 cycles: fetch + internal)
        case 0x39: {
            uint32_t result = get_hl() + m_sp;
            set_flag_n(false);
            set_flag_h((get_hl() & 0xFFF) + (m_sp & 0xFFF) > 0xFFF);
            set_flag_c(result > 0xFFFF);
            set_hl(result & 0xFFFF);
            internal_cycle();
            break;
        }

        // LD A, (HL-) - OAM bug triggers on the decrement
        case 0x3A: m_a = read(get_hl()); check_oam_bug(get_hl(), true); set_hl(get_hl() - 1); break;

        // DEC SP (2 cycles: fetch + internal) - SP can point to OAM
        case 0x3B: check_oam_bug(m_sp, false); m_sp--; internal_cycle(); break;

        // INC A
        case 0x3C: m_a = alu_inc(m_a); break;

        // DEC A
        case 0x3D: m_a = alu_dec(m_a); break;

        // LD A, n
        case 0x3E: m_a = fetch(); break;

        // CCF
        case 0x3F:
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(!get_flag_c());
            break;

        // LD B, r
        case 0x40: break;  // LD B, B
        case 0x41: m_b = m_c; break;
        case 0x42: m_b = m_d; break;
        case 0x43: m_b = m_e; break;
        case 0x44: m_b = m_h; break;
        case 0x45: m_b = m_l; break;
        case 0x46: m_b = read(get_hl()); break;
        case 0x47: m_b = m_a; break;

        // LD C, r
        case 0x48: m_c = m_b; break;
        case 0x49: break;  // LD C, C
        case 0x4A: m_c = m_d; break;
        case 0x4B: m_c = m_e; break;
        case 0x4C: m_c = m_h; break;
        case 0x4D: m_c = m_l; break;
        case 0x4E: m_c = read(get_hl()); break;
        case 0x4F: m_c = m_a; break;

        // LD D, r
        case 0x50: m_d = m_b; break;
        case 0x51: m_d = m_c; break;
        case 0x52: break;  // LD D, D
        case 0x53: m_d = m_e; break;
        case 0x54: m_d = m_h; break;
        case 0x55: m_d = m_l; break;
        case 0x56: m_d = read(get_hl()); break;
        case 0x57: m_d = m_a; break;

        // LD E, r
        case 0x58: m_e = m_b; break;
        case 0x59: m_e = m_c; break;
        case 0x5A: m_e = m_d; break;
        case 0x5B: break;  // LD E, E
        case 0x5C: m_e = m_h; break;
        case 0x5D: m_e = m_l; break;
        case 0x5E: m_e = read(get_hl()); break;
        case 0x5F: m_e = m_a; break;

        // LD H, r
        case 0x60: m_h = m_b; break;
        case 0x61: m_h = m_c; break;
        case 0x62: m_h = m_d; break;
        case 0x63: m_h = m_e; break;
        case 0x64: break;  // LD H, H
        case 0x65: m_h = m_l; break;
        case 0x66: m_h = read(get_hl()); break;
        case 0x67: m_h = m_a; break;

        // LD L, r
        case 0x68: m_l = m_b; break;
        case 0x69: m_l = m_c; break;
        case 0x6A: m_l = m_d; break;
        case 0x6B: m_l = m_e; break;
        case 0x6C: m_l = m_h; break;
        case 0x6D: break;  // LD L, L
        case 0x6E: m_l = read(get_hl()); break;
        case 0x6F: m_l = m_a; break;

        // LD (HL), r
        case 0x70: write(get_hl(), m_b); break;
        case 0x71: write(get_hl(), m_c); break;
        case 0x72: write(get_hl(), m_d); break;
        case 0x73: write(get_hl(), m_e); break;
        case 0x74: write(get_hl(), m_h); break;
        case 0x75: write(get_hl(), m_l); break;

        // HALT
        case 0x76:
            m_halted = true;
            // HALT bug: if IME=0 and IE&IF!=0, PC doesn't increment on next fetch
            if (!m_ime && m_bus.get_pending_interrupts()) {
                m_halt_bug = true;
                m_halted = false;
            }
            break;

        case 0x77: write(get_hl(), m_a); break;

        // LD A, r
        case 0x78: m_a = m_b; break;
        case 0x79: m_a = m_c; break;
        case 0x7A: m_a = m_d; break;
        case 0x7B: m_a = m_e; break;
        case 0x7C: m_a = m_h; break;
        case 0x7D: m_a = m_l; break;
        case 0x7E: m_a = read(get_hl()); break;
        case 0x7F: break;  // LD A, A

        // ADD A, r
        case 0x80: alu_add(m_b, false); break;
        case 0x81: alu_add(m_c, false); break;
        case 0x82: alu_add(m_d, false); break;
        case 0x83: alu_add(m_e, false); break;
        case 0x84: alu_add(m_h, false); break;
        case 0x85: alu_add(m_l, false); break;
        case 0x86: alu_add(read(get_hl()), false); break;
        case 0x87: alu_add(m_a, false); break;

        // ADC A, r
        case 0x88: alu_add(m_b, true); break;
        case 0x89: alu_add(m_c, true); break;
        case 0x8A: alu_add(m_d, true); break;
        case 0x8B: alu_add(m_e, true); break;
        case 0x8C: alu_add(m_h, true); break;
        case 0x8D: alu_add(m_l, true); break;
        case 0x8E: alu_add(read(get_hl()), true); break;
        case 0x8F: alu_add(m_a, true); break;

        // SUB r
        case 0x90: alu_sub(m_b, false); break;
        case 0x91: alu_sub(m_c, false); break;
        case 0x92: alu_sub(m_d, false); break;
        case 0x93: alu_sub(m_e, false); break;
        case 0x94: alu_sub(m_h, false); break;
        case 0x95: alu_sub(m_l, false); break;
        case 0x96: alu_sub(read(get_hl()), false); break;
        case 0x97: alu_sub(m_a, false); break;

        // SBC A, r
        case 0x98: alu_sub(m_b, true); break;
        case 0x99: alu_sub(m_c, true); break;
        case 0x9A: alu_sub(m_d, true); break;
        case 0x9B: alu_sub(m_e, true); break;
        case 0x9C: alu_sub(m_h, true); break;
        case 0x9D: alu_sub(m_l, true); break;
        case 0x9E: alu_sub(read(get_hl()), true); break;
        case 0x9F: alu_sub(m_a, true); break;

        // AND r
        case 0xA0: alu_and(m_b); break;
        case 0xA1: alu_and(m_c); break;
        case 0xA2: alu_and(m_d); break;
        case 0xA3: alu_and(m_e); break;
        case 0xA4: alu_and(m_h); break;
        case 0xA5: alu_and(m_l); break;
        case 0xA6: alu_and(read(get_hl())); break;
        case 0xA7: alu_and(m_a); break;

        // XOR r
        case 0xA8: alu_xor(m_b); break;
        case 0xA9: alu_xor(m_c); break;
        case 0xAA: alu_xor(m_d); break;
        case 0xAB: alu_xor(m_e); break;
        case 0xAC: alu_xor(m_h); break;
        case 0xAD: alu_xor(m_l); break;
        case 0xAE: alu_xor(read(get_hl())); break;
        case 0xAF: alu_xor(m_a); break;

        // OR r
        case 0xB0: alu_or(m_b); break;
        case 0xB1: alu_or(m_c); break;
        case 0xB2: alu_or(m_d); break;
        case 0xB3: alu_or(m_e); break;
        case 0xB4: alu_or(m_h); break;
        case 0xB5: alu_or(m_l); break;
        case 0xB6: alu_or(read(get_hl())); break;
        case 0xB7: alu_or(m_a); break;

        // CP r
        case 0xB8: alu_cp(m_b); break;
        case 0xB9: alu_cp(m_c); break;
        case 0xBA: alu_cp(m_d); break;
        case 0xBB: alu_cp(m_e); break;
        case 0xBC: alu_cp(m_h); break;
        case 0xBD: alu_cp(m_l); break;
        case 0xBE: alu_cp(read(get_hl())); break;
        case 0xBF: alu_cp(m_a); break;

        // RET NZ (2 cycles if not taken, 5 if taken: internal check + pop lo + pop hi + internal + internal)
        case 0xC0:
            internal_cycle();  // Condition check cycle
            if (!get_flag_z()) {
                m_pc = pop();
                internal_cycle();  // Internal before jumping
                cycles = 5;
            } else {
                cycles = 2;
            }
            break;

        // POP BC (3 cycles: fetch + pop lo + pop hi)
        case 0xC1: set_bc(pop()); break;

        // JP NZ, nn (3 cycles if not taken, 4 if taken: fetch + lo + hi + internal if taken)
        case 0xC2: {
            uint16_t addr = fetch16();
            if (!get_flag_z()) {
                m_pc = addr;
                internal_cycle();
                cycles = 4;
            } else {
                cycles = 3;
            }
            break;
        }

        // JP nn (4 cycles: fetch + lo + hi + internal)
        case 0xC3: m_pc = fetch16(); internal_cycle(); break;

        // CALL NZ, nn (3 cycles if not taken, 6 if taken: fetch + lo + hi + [internal + push hi + push lo])
        case 0xC4: {
            uint16_t addr = fetch16();
            if (!get_flag_z()) {
                internal_cycle();
                push(m_pc);
                m_pc = addr;
                cycles = 6;
            } else {
                cycles = 3;
            }
            break;
        }

        // PUSH BC (4 cycles: fetch + internal + push hi + push lo)
        case 0xC5: internal_cycle(); push(get_bc()); break;

        // ADD A, n
        case 0xC6: alu_add(fetch(), false); break;

        // RST 00 (4 cycles: fetch + internal + push hi + push lo)
        case 0xC7: internal_cycle(); push(m_pc); m_pc = 0x00; break;

        // RET Z (2 cycles if not taken, 5 if taken)
        case 0xC8:
            internal_cycle();  // Condition check cycle
            if (get_flag_z()) {
                m_pc = pop();
                internal_cycle();  // Internal before jumping
                cycles = 5;
            } else {
                cycles = 2;
            }
            break;

        // RET (4 cycles: fetch + pop lo + pop hi + internal)
        case 0xC9: m_pc = pop(); internal_cycle(); break;

        // JP Z, nn (3 cycles if not taken, 4 if taken)
        case 0xCA: {
            uint16_t addr = fetch16();
            if (get_flag_z()) {
                m_pc = addr;
                internal_cycle();
                cycles = 4;
            } else {
                cycles = 3;
            }
            break;
        }

        // CB prefix
        case 0xCB: cycles = execute_cb(); break;

        // CALL Z, nn (3 cycles if not taken, 6 if taken)
        case 0xCC: {
            uint16_t addr = fetch16();
            if (get_flag_z()) {
                internal_cycle();
                push(m_pc);
                m_pc = addr;
                cycles = 6;
            } else {
                cycles = 3;
            }
            break;
        }

        // CALL nn (6 cycles: fetch + lo + hi + internal + push hi + push lo)
        case 0xCD: {
            uint16_t addr = fetch16();
            internal_cycle();
            push(m_pc);
            m_pc = addr;
            break;
        }

        // ADC A, n
        case 0xCE: alu_add(fetch(), true); break;

        // RST 08 (4 cycles: fetch + internal + push hi + push lo)
        case 0xCF: internal_cycle(); push(m_pc); m_pc = 0x08; break;

        // RET NC (2 cycles if not taken, 5 if taken)
        case 0xD0:
            internal_cycle();  // Condition check
            if (!get_flag_c()) {
                m_pc = pop();
                internal_cycle();
                cycles = 5;
            } else {
                cycles = 2;
            }
            break;

        // POP DE (3 cycles: fetch + pop lo + pop hi)
        case 0xD1: set_de(pop()); break;

        // JP NC, nn (3 cycles if not taken, 4 if taken)
        case 0xD2: {
            uint16_t addr = fetch16();
            if (!get_flag_c()) {
                m_pc = addr;
                internal_cycle();
                cycles = 4;
            } else {
                cycles = 3;
            }
            break;
        }

        // CALL NC, nn (3 cycles if not taken, 6 if taken)
        case 0xD4: {
            uint16_t addr = fetch16();
            if (!get_flag_c()) {
                internal_cycle();
                push(m_pc);
                m_pc = addr;
                cycles = 6;
            } else {
                cycles = 3;
            }
            break;
        }

        // PUSH DE (4 cycles: fetch + internal + push hi + push lo)
        case 0xD5: internal_cycle(); push(get_de()); break;

        // SUB n
        case 0xD6: alu_sub(fetch(), false); break;

        // RST 10 (4 cycles: fetch + internal + push hi + push lo)
        case 0xD7: internal_cycle(); push(m_pc); m_pc = 0x10; break;

        // RET C (2 cycles if not taken, 5 if taken)
        case 0xD8:
            internal_cycle();  // Condition check
            if (get_flag_c()) {
                m_pc = pop();
                internal_cycle();
                cycles = 5;
            } else {
                cycles = 2;
            }
            break;

        // RETI (4 cycles: fetch + pop lo + pop hi + internal)
        case 0xD9:
            m_pc = pop();
            internal_cycle();
            m_ime = true;
            break;

        // JP C, nn (3 cycles if not taken, 4 if taken)
        case 0xDA: {
            uint16_t addr = fetch16();
            if (get_flag_c()) {
                m_pc = addr;
                internal_cycle();
                cycles = 4;
            } else {
                cycles = 3;
            }
            break;
        }

        // CALL C, nn (3 cycles if not taken, 6 if taken)
        case 0xDC: {
            uint16_t addr = fetch16();
            if (get_flag_c()) {
                internal_cycle();
                push(m_pc);
                m_pc = addr;
                cycles = 6;
            } else {
                cycles = 3;
            }
            break;
        }

        // SBC A, n
        case 0xDE: alu_sub(fetch(), true); break;

        // RST 18 (4 cycles: fetch + internal + push hi + push lo)
        case 0xDF: internal_cycle(); push(m_pc); m_pc = 0x18; break;

        // LD (FF00+n), A
        case 0xE0: write(0xFF00 + fetch(), m_a); break;

        // POP HL (3 cycles: fetch + pop lo + pop hi)
        case 0xE1: set_hl(pop()); break;

        // LD (FF00+C), A
        case 0xE2: write(0xFF00 + m_c, m_a); break;

        // PUSH HL (4 cycles: fetch + internal + push hi + push lo)
        case 0xE5: internal_cycle(); push(get_hl()); break;

        // AND n
        case 0xE6: alu_and(fetch()); break;

        // RST 20 (4 cycles: fetch + internal + push hi + push lo)
        case 0xE7: internal_cycle(); push(m_pc); m_pc = 0x20; break;

        // ADD SP, n (4 cycles: fetch + n + internal + internal)
        case 0xE8: {
            int8_t n = static_cast<int8_t>(fetch());
            uint32_t result = m_sp + n;
            set_flag_z(false);
            set_flag_n(false);
            set_flag_h((m_sp & 0x0F) + (n & 0x0F) > 0x0F);
            set_flag_c((m_sp & 0xFF) + (n & 0xFF) > 0xFF);
            m_sp = result & 0xFFFF;
            internal_cycle();
            internal_cycle();
            break;
        }

        // JP HL (1 cycle - just fetch, no internal needed)
        case 0xE9: m_pc = get_hl(); break;

        // LD (nn), A
        case 0xEA: write(fetch16(), m_a); break;

        // XOR n
        case 0xEE: alu_xor(fetch()); break;

        // RST 28 (4 cycles: fetch + internal + push hi + push lo)
        case 0xEF: internal_cycle(); push(m_pc); m_pc = 0x28; break;

        // LD A, (FF00+n)
        case 0xF0: m_a = read(0xFF00 + fetch()); break;

        // POP AF (3 cycles: fetch + pop lo + pop hi)
        case 0xF1: set_af(pop()); break;

        // LD A, (FF00+C)
        case 0xF2: m_a = read(0xFF00 + m_c); break;

        // DI
        case 0xF3: m_ime = false; break;

        // PUSH AF (4 cycles: fetch + internal + push hi + push lo)
        case 0xF5: internal_cycle(); push(get_af()); break;

        // OR n
        case 0xF6: alu_or(fetch()); break;

        // RST 30 (4 cycles: fetch + internal + push hi + push lo)
        case 0xF7: internal_cycle(); push(m_pc); m_pc = 0x30; break;

        // LD HL, SP+n (3 cycles: fetch + n + internal)
        case 0xF8: {
            int8_t n = static_cast<int8_t>(fetch());
            uint32_t result = m_sp + n;
            set_flag_z(false);
            set_flag_n(false);
            set_flag_h((m_sp & 0x0F) + (n & 0x0F) > 0x0F);
            set_flag_c((m_sp & 0xFF) + (n & 0xFF) > 0xFF);
            set_hl(result & 0xFFFF);
            internal_cycle();
            break;
        }

        // LD SP, HL (2 cycles: fetch + internal)
        case 0xF9: m_sp = get_hl(); internal_cycle(); break;

        // LD A, (nn)
        case 0xFA: m_a = read(fetch16()); break;

        // EI
        case 0xFB: m_ime_pending = true; break;

        // CP n
        case 0xFE: alu_cp(fetch()); break;

        // RST 38 (4 cycles: fetch + internal + push hi + push lo)
        case 0xFF: internal_cycle(); push(m_pc); m_pc = 0x38; break;

        default:
            // Undefined opcodes (0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD)
            // Behave as NOP on real hardware
            break;
    }

    return cycles;
}

int LR35902::execute_cb() {
    uint8_t opcode = fetch();
    int cycles = s_cb_cycle_table[opcode];

    // Get register based on lower 3 bits
    auto get_reg = [this](int r) -> uint8_t {
        switch (r) {
            case 0: return m_b;
            case 1: return m_c;
            case 2: return m_d;
            case 3: return m_e;
            case 4: return m_h;
            case 5: return m_l;
            case 6: return read(get_hl());
            case 7: return m_a;
        }
        return 0;
    };

    auto set_reg = [this](int r, uint8_t value) {
        switch (r) {
            case 0: m_b = value; break;
            case 1: m_c = value; break;
            case 2: m_d = value; break;
            case 3: m_e = value; break;
            case 4: m_h = value; break;
            case 5: m_l = value; break;
            case 6: write(get_hl(), value); break;
            case 7: m_a = value; break;
        }
    };

    int reg = opcode & 0x07;
    int bit_num = (opcode >> 3) & 0x07;
    int op = opcode >> 6;

    uint8_t value = get_reg(reg);
    uint8_t result;

    switch (op) {
        case 0:  // Rotate/shift operations
            switch (bit_num) {
                case 0: result = rlc(value); break;
                case 1: result = rrc(value); break;
                case 2: result = rl(value); break;
                case 3: result = rr(value); break;
                case 4: result = sla(value); break;
                case 5: result = sra(value); break;
                case 6: result = swap(value); break;
                case 7: result = srl(value); break;
                default: result = value;
            }
            set_reg(reg, result);
            break;

        case 1:  // BIT
            bit(bit_num, value);
            break;

        case 2:  // RES
            set_reg(reg, res(bit_num, value));
            break;

        case 3:  // SET
            set_reg(reg, set(bit_num, value));
            break;
    }

    return cycles;
}

// ALU operations
void LR35902::alu_add(uint8_t value, bool with_carry) {
    int carry = (with_carry && get_flag_c()) ? 1 : 0;
    int result = m_a + value + carry;

    set_flag_z((result & 0xFF) == 0);
    set_flag_n(false);
    set_flag_h((m_a & 0x0F) + (value & 0x0F) + carry > 0x0F);
    set_flag_c(result > 0xFF);

    m_a = result & 0xFF;
}

void LR35902::alu_sub(uint8_t value, bool with_carry) {
    int carry = (with_carry && get_flag_c()) ? 1 : 0;
    int result = m_a - value - carry;

    set_flag_z((result & 0xFF) == 0);
    set_flag_n(true);
    set_flag_h((m_a & 0x0F) < (value & 0x0F) + carry);
    set_flag_c(result < 0);

    m_a = result & 0xFF;
}

void LR35902::alu_and(uint8_t value) {
    m_a &= value;
    set_flag_z(m_a == 0);
    set_flag_n(false);
    set_flag_h(true);
    set_flag_c(false);
}

void LR35902::alu_or(uint8_t value) {
    m_a |= value;
    set_flag_z(m_a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
}

void LR35902::alu_xor(uint8_t value) {
    m_a ^= value;
    set_flag_z(m_a == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
}

void LR35902::alu_cp(uint8_t value) {
    int result = m_a - value;
    set_flag_z((result & 0xFF) == 0);
    set_flag_n(true);
    set_flag_h((m_a & 0x0F) < (value & 0x0F));
    set_flag_c(result < 0);
}

uint8_t LR35902::alu_inc(uint8_t value) {
    uint8_t result = value + 1;
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h((value & 0x0F) == 0x0F);
    return result;
}

uint8_t LR35902::alu_dec(uint8_t value) {
    uint8_t result = value - 1;
    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((value & 0x0F) == 0);
    return result;
}

// Rotate/shift operations
uint8_t LR35902::rlc(uint8_t value) {
    bool bit7 = value & 0x80;
    uint8_t result = (value << 1) | (bit7 ? 1 : 0);
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(bit7);
    return result;
}

uint8_t LR35902::rrc(uint8_t value) {
    bool bit0 = value & 0x01;
    uint8_t result = (value >> 1) | (bit0 ? 0x80 : 0);
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(bit0);
    return result;
}

uint8_t LR35902::rl(uint8_t value) {
    bool old_carry = get_flag_c();
    bool bit7 = value & 0x80;
    uint8_t result = (value << 1) | (old_carry ? 1 : 0);
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(bit7);
    return result;
}

uint8_t LR35902::rr(uint8_t value) {
    bool old_carry = get_flag_c();
    bool bit0 = value & 0x01;
    uint8_t result = (value >> 1) | (old_carry ? 0x80 : 0);
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(bit0);
    return result;
}

uint8_t LR35902::sla(uint8_t value) {
    bool bit7 = value & 0x80;
    uint8_t result = value << 1;
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(bit7);
    return result;
}

uint8_t LR35902::sra(uint8_t value) {
    bool bit0 = value & 0x01;
    uint8_t result = (value >> 1) | (value & 0x80);  // Preserve bit 7
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(bit0);
    return result;
}

uint8_t LR35902::swap(uint8_t value) {
    uint8_t result = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
    return result;
}

uint8_t LR35902::srl(uint8_t value) {
    bool bit0 = value & 0x01;
    uint8_t result = value >> 1;
    set_flag_z(result == 0);
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(bit0);
    return result;
}

void LR35902::bit(int n, uint8_t value) {
    set_flag_z(!(value & (1 << n)));
    set_flag_n(false);
    set_flag_h(true);
}

uint8_t LR35902::res(int n, uint8_t value) {
    return value & ~(1 << n);
}

uint8_t LR35902::set(int n, uint8_t value) {
    return value | (1 << n);
}

void LR35902::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_a);
    data.push_back(m_f);
    data.push_back(m_b);
    data.push_back(m_c);
    data.push_back(m_d);
    data.push_back(m_e);
    data.push_back(m_h);
    data.push_back(m_l);

    data.push_back(m_sp & 0xFF);
    data.push_back(m_sp >> 8);
    data.push_back(m_pc & 0xFF);
    data.push_back(m_pc >> 8);

    data.push_back(m_ime ? 1 : 0);
    data.push_back(m_ime_pending ? 1 : 0);
    data.push_back(m_halted ? 1 : 0);
    data.push_back(m_halt_bug ? 1 : 0);
}

void LR35902::load_state(const uint8_t*& data, size_t& remaining) {
    m_a = *data++; remaining--;
    m_f = *data++; remaining--;
    m_b = *data++; remaining--;
    m_c = *data++; remaining--;
    m_d = *data++; remaining--;
    m_e = *data++; remaining--;
    m_h = *data++; remaining--;
    m_l = *data++; remaining--;

    m_sp = *data++; remaining--;
    m_sp |= (*data++ << 8); remaining--;
    m_pc = *data++; remaining--;
    m_pc |= (*data++ << 8); remaining--;

    m_ime = (*data++ != 0); remaining--;
    m_ime_pending = (*data++ != 0); remaining--;
    m_halted = (*data++ != 0); remaining--;
    m_halt_bug = (*data++ != 0); remaining--;
}

} // namespace gb
