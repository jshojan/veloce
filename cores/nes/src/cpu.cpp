#include "cpu.hpp"
#include "bus.hpp"

#include <cstring>

namespace nes {

CPU::CPU(Bus& bus) : m_bus(bus) {
    reset();
}

CPU::~CPU() = default;

void CPU::reset() {
    // Read reset vector
    uint8_t lo = m_bus.cpu_read(0xFFFC);
    uint8_t hi = m_bus.cpu_read(0xFFFD);
    m_pc = (static_cast<uint16_t>(hi) << 8) | lo;

    m_a = 0;
    m_x = 0;
    m_y = 0;
    m_sp = 0xFD;
    m_status = 0x24;  // IRQ disabled, unused bit set
    m_nmi_pending = false;
    m_nmi_delayed = false;
    m_irq_pending = false;
    m_nmi_line = false;
    m_prev_nmi_line = false;
    m_prev_irq_inhibit = true;  // IRQ inhibited at reset (I flag is set)
    m_in_interrupt_sequence = false;
}

int CPU::step() {
    // Reset cycle counter for this instruction
    m_cycles = 0;

    // Handle interrupts
    // Note: NMI takes priority over IRQ
    if (m_nmi_pending) {
        m_nmi_pending = false;
        push16(m_pc);
        push(m_status & ~FLAG_B);
        set_flag(FLAG_I, true);
        uint8_t lo = read(0xFFFA);
        uint8_t hi = read(0xFFFB);
        m_pc = (static_cast<uint16_t>(hi) << 8) | lo;
        return 7;
    }

    if (m_irq_pending && !get_flag(FLAG_I)) {
        m_irq_pending = false;
        push16(m_pc);
        push(m_status & ~FLAG_B);
        set_flag(FLAG_I, true);

        // NMI hijacking: If NMI became pending during IRQ handling (e.g., during
        // the push operations), NMI takes over and uses NMI vector instead.
        uint16_t vector;
        if (m_nmi_pending) {
            m_nmi_pending = false;
            vector = 0xFFFA;  // NMI vector
        } else {
            vector = 0xFFFE;  // IRQ vector
        }

        uint8_t lo = read(vector);
        uint8_t hi = read(vector + 1);
        m_pc = (static_cast<uint16_t>(hi) << 8) | lo;
        return 7;
    }

    // Fetch opcode (cycle 1 of every instruction)
    uint8_t opcode = read(m_pc++);
    int cycles = 0;
    bool page_crossed = false;

    // Decode and execute
    switch (opcode) {
        // ADC - Add with Carry
        case 0x69: op_adc(read(addr_immediate())); cycles = 2; break;
        case 0x65: op_adc(read(addr_zero_page())); cycles = 3; break;
        case 0x75: op_adc(read(addr_zero_page_x())); cycles = 4; break;
        case 0x6D: op_adc(read(addr_absolute())); cycles = 4; break;
        case 0x7D: op_adc(read(addr_absolute_x(page_crossed))); cycles = 4 + page_crossed; break;
        case 0x79: op_adc(read(addr_absolute_y(page_crossed))); cycles = 4 + page_crossed; break;
        case 0x61: op_adc(read(addr_indirect_x())); cycles = 6; break;
        case 0x71: op_adc(read(addr_indirect_y(page_crossed))); cycles = 5 + page_crossed; break;

        // AND - Logical AND
        case 0x29: op_and(read(addr_immediate())); cycles = 2; break;
        case 0x25: op_and(read(addr_zero_page())); cycles = 3; break;
        case 0x35: op_and(read(addr_zero_page_x())); cycles = 4; break;
        case 0x2D: op_and(read(addr_absolute())); cycles = 4; break;
        case 0x3D: op_and(read(addr_absolute_x(page_crossed))); cycles = 4 + page_crossed; break;
        case 0x39: op_and(read(addr_absolute_y(page_crossed))); cycles = 4 + page_crossed; break;
        case 0x21: op_and(read(addr_indirect_x())); cycles = 6; break;
        case 0x31: op_and(read(addr_indirect_y(page_crossed))); cycles = 5 + page_crossed; break;

        // ASL - Arithmetic Shift Left
        case 0x0A: tick_internal(); op_asl_a(); cycles = 2; break;
        case 0x06: op_asl(addr_zero_page()); cycles = 5; break;
        case 0x16: op_asl(addr_zero_page_x()); cycles = 6; break;
        case 0x0E: op_asl(addr_absolute()); cycles = 6; break;
        case 0x1E: op_asl(addr_absolute_x(page_crossed, true)); cycles = 7; break;

        // BCC - Branch if Carry Clear
        case 0x90: op_branch(!get_flag(FLAG_C)); cycles = 2; break;

        // BCS - Branch if Carry Set
        case 0xB0: op_branch(get_flag(FLAG_C)); cycles = 2; break;

        // BEQ - Branch if Equal
        case 0xF0: op_branch(get_flag(FLAG_Z)); cycles = 2; break;

        // BIT - Bit Test
        case 0x24: op_bit(read(addr_zero_page())); cycles = 3; break;
        case 0x2C: op_bit(read(addr_absolute())); cycles = 4; break;

        // BMI - Branch if Minus
        case 0x30: op_branch(get_flag(FLAG_N)); cycles = 2; break;

        // BNE - Branch if Not Equal
        case 0xD0: op_branch(!get_flag(FLAG_Z)); cycles = 2; break;

        // BPL - Branch if Positive
        case 0x10: op_branch(!get_flag(FLAG_N)); cycles = 2; break;

        // BRK - Break
        case 0x00: op_brk(); cycles = 7; break;

        // BVC - Branch if Overflow Clear
        case 0x50: op_branch(!get_flag(FLAG_V)); cycles = 2; break;

        // BVS - Branch if Overflow Set
        case 0x70: op_branch(get_flag(FLAG_V)); cycles = 2; break;

        // CLC - Clear Carry Flag
        case 0x18: tick_internal(); set_flag(FLAG_C, false); cycles = 2; break;

        // CLD - Clear Decimal Mode
        case 0xD8: tick_internal(); set_flag(FLAG_D, false); cycles = 2; break;

        // CLI - Clear Interrupt Disable
        case 0x58: tick_internal(); set_flag(FLAG_I, false); cycles = 2; break;

        // CLV - Clear Overflow Flag
        case 0xB8: tick_internal(); set_flag(FLAG_V, false); cycles = 2; break;

        // CMP - Compare Accumulator
        case 0xC9: op_cmp(m_a, read(addr_immediate())); cycles = 2; break;
        case 0xC5: op_cmp(m_a, read(addr_zero_page())); cycles = 3; break;
        case 0xD5: op_cmp(m_a, read(addr_zero_page_x())); cycles = 4; break;
        case 0xCD: op_cmp(m_a, read(addr_absolute())); cycles = 4; break;
        case 0xDD: op_cmp(m_a, read(addr_absolute_x(page_crossed))); cycles = 4 + page_crossed; break;
        case 0xD9: op_cmp(m_a, read(addr_absolute_y(page_crossed))); cycles = 4 + page_crossed; break;
        case 0xC1: op_cmp(m_a, read(addr_indirect_x())); cycles = 6; break;
        case 0xD1: op_cmp(m_a, read(addr_indirect_y(page_crossed))); cycles = 5 + page_crossed; break;

        // CPX - Compare X Register
        case 0xE0: op_cmp(m_x, read(addr_immediate())); cycles = 2; break;
        case 0xE4: op_cmp(m_x, read(addr_zero_page())); cycles = 3; break;
        case 0xEC: op_cmp(m_x, read(addr_absolute())); cycles = 4; break;

        // CPY - Compare Y Register
        case 0xC0: op_cmp(m_y, read(addr_immediate())); cycles = 2; break;
        case 0xC4: op_cmp(m_y, read(addr_zero_page())); cycles = 3; break;
        case 0xCC: op_cmp(m_y, read(addr_absolute())); cycles = 4; break;

        // DEC - Decrement Memory
        case 0xC6: op_dec(addr_zero_page()); cycles = 5; break;
        case 0xD6: op_dec(addr_zero_page_x()); cycles = 6; break;
        case 0xCE: op_dec(addr_absolute()); cycles = 6; break;
        case 0xDE: op_dec(addr_absolute_x(page_crossed, true)); cycles = 7; break;

        // DEX - Decrement X
        case 0xCA: tick_internal(); m_x--; update_zero_negative(m_x); cycles = 2; break;

        // DEY - Decrement Y
        case 0x88: tick_internal(); m_y--; update_zero_negative(m_y); cycles = 2; break;

        // EOR - Exclusive OR
        case 0x49: op_eor(read(addr_immediate())); cycles = 2; break;
        case 0x45: op_eor(read(addr_zero_page())); cycles = 3; break;
        case 0x55: op_eor(read(addr_zero_page_x())); cycles = 4; break;
        case 0x4D: op_eor(read(addr_absolute())); cycles = 4; break;
        case 0x5D: op_eor(read(addr_absolute_x(page_crossed))); cycles = 4 + page_crossed; break;
        case 0x59: op_eor(read(addr_absolute_y(page_crossed))); cycles = 4 + page_crossed; break;
        case 0x41: op_eor(read(addr_indirect_x())); cycles = 6; break;
        case 0x51: op_eor(read(addr_indirect_y(page_crossed))); cycles = 5 + page_crossed; break;

        // INC - Increment Memory
        case 0xE6: op_inc(addr_zero_page()); cycles = 5; break;
        case 0xF6: op_inc(addr_zero_page_x()); cycles = 6; break;
        case 0xEE: op_inc(addr_absolute()); cycles = 6; break;
        case 0xFE: op_inc(addr_absolute_x(page_crossed, true)); cycles = 7; break;

        // INX - Increment X
        case 0xE8: tick_internal(); m_x++; update_zero_negative(m_x); cycles = 2; break;

        // INY - Increment Y
        case 0xC8: tick_internal(); m_y++; update_zero_negative(m_y); cycles = 2; break;

        // JMP - Jump
        case 0x4C: op_jmp(addr_absolute()); cycles = 3; break;
        case 0x6C: op_jmp(addr_indirect()); cycles = 5; break;

        // JSR - Jump to Subroutine
        case 0x20: op_jsr(addr_absolute()); cycles = 6; break;

        // LDA - Load Accumulator
        case 0xA9: op_lda(read(addr_immediate())); cycles = 2; break;
        case 0xA5: op_lda(read(addr_zero_page())); cycles = 3; break;
        case 0xB5: op_lda(read(addr_zero_page_x())); cycles = 4; break;
        case 0xAD: op_lda(read(addr_absolute())); cycles = 4; break;
        case 0xBD: op_lda(read(addr_absolute_x(page_crossed))); cycles = 4 + page_crossed; break;
        case 0xB9: op_lda(read(addr_absolute_y(page_crossed))); cycles = 4 + page_crossed; break;
        case 0xA1: op_lda(read(addr_indirect_x())); cycles = 6; break;
        case 0xB1: op_lda(read(addr_indirect_y(page_crossed))); cycles = 5 + page_crossed; break;

        // LDX - Load X Register
        case 0xA2: op_ldx(read(addr_immediate())); cycles = 2; break;
        case 0xA6: op_ldx(read(addr_zero_page())); cycles = 3; break;
        case 0xB6: op_ldx(read(addr_zero_page_y())); cycles = 4; break;
        case 0xAE: op_ldx(read(addr_absolute())); cycles = 4; break;
        case 0xBE: op_ldx(read(addr_absolute_y(page_crossed))); cycles = 4 + page_crossed; break;

        // LDY - Load Y Register
        case 0xA0: op_ldy(read(addr_immediate())); cycles = 2; break;
        case 0xA4: op_ldy(read(addr_zero_page())); cycles = 3; break;
        case 0xB4: op_ldy(read(addr_zero_page_x())); cycles = 4; break;
        case 0xAC: op_ldy(read(addr_absolute())); cycles = 4; break;
        case 0xBC: op_ldy(read(addr_absolute_x(page_crossed))); cycles = 4 + page_crossed; break;

        // LSR - Logical Shift Right
        case 0x4A: tick_internal(); op_lsr_a(); cycles = 2; break;
        case 0x46: op_lsr(addr_zero_page()); cycles = 5; break;
        case 0x56: op_lsr(addr_zero_page_x()); cycles = 6; break;
        case 0x4E: op_lsr(addr_absolute()); cycles = 6; break;
        case 0x5E: op_lsr(addr_absolute_x(page_crossed, true)); cycles = 7; break;

        // NOP - No Operation
        case 0xEA: tick_internal(); cycles = 2; break;

        // ORA - Logical OR
        case 0x09: op_ora(read(addr_immediate())); cycles = 2; break;
        case 0x05: op_ora(read(addr_zero_page())); cycles = 3; break;
        case 0x15: op_ora(read(addr_zero_page_x())); cycles = 4; break;
        case 0x0D: op_ora(read(addr_absolute())); cycles = 4; break;
        case 0x1D: op_ora(read(addr_absolute_x(page_crossed))); cycles = 4 + page_crossed; break;
        case 0x19: op_ora(read(addr_absolute_y(page_crossed))); cycles = 4 + page_crossed; break;
        case 0x01: op_ora(read(addr_indirect_x())); cycles = 6; break;
        case 0x11: op_ora(read(addr_indirect_y(page_crossed))); cycles = 5 + page_crossed; break;

        // PHA - Push Accumulator (3 cycles: fetch, internal, push)
        case 0x48: tick_internal(); push(m_a); cycles = 3; break;

        // PHP - Push Processor Status (3 cycles: fetch, internal, push)
        case 0x08: tick_internal(); push(m_status | FLAG_B | FLAG_U); cycles = 3; break;

        // PLA - Pull Accumulator (4 cycles: fetch, internal, dummy read, pop)
        case 0x68: tick_internal(); tick_internal(); m_a = pop(); update_zero_negative(m_a); cycles = 4; break;

        // PLP - Pull Processor Status (4 cycles: fetch, internal, dummy read, pop)
        case 0x28: tick_internal(); tick_internal(); m_status = (pop() & ~FLAG_B) | FLAG_U; cycles = 4; break;

        // ROL - Rotate Left
        case 0x2A: tick_internal(); op_rol_a(); cycles = 2; break;
        case 0x26: op_rol(addr_zero_page()); cycles = 5; break;
        case 0x36: op_rol(addr_zero_page_x()); cycles = 6; break;
        case 0x2E: op_rol(addr_absolute()); cycles = 6; break;
        case 0x3E: op_rol(addr_absolute_x(page_crossed, true)); cycles = 7; break;

        // ROR - Rotate Right
        case 0x6A: tick_internal(); op_ror_a(); cycles = 2; break;
        case 0x66: op_ror(addr_zero_page()); cycles = 5; break;
        case 0x76: op_ror(addr_zero_page_x()); cycles = 6; break;
        case 0x6E: op_ror(addr_absolute()); cycles = 6; break;
        case 0x7E: op_ror(addr_absolute_x(page_crossed, true)); cycles = 7; break;

        // RTI - Return from Interrupt
        case 0x40: op_rti(); cycles = 6; break;

        // RTS - Return from Subroutine
        case 0x60: op_rts(); cycles = 6; break;

        // SBC - Subtract with Carry
        case 0xE9: op_sbc(read(addr_immediate())); cycles = 2; break;
        case 0xE5: op_sbc(read(addr_zero_page())); cycles = 3; break;
        case 0xF5: op_sbc(read(addr_zero_page_x())); cycles = 4; break;
        case 0xED: op_sbc(read(addr_absolute())); cycles = 4; break;
        case 0xFD: op_sbc(read(addr_absolute_x(page_crossed))); cycles = 4 + page_crossed; break;
        case 0xF9: op_sbc(read(addr_absolute_y(page_crossed))); cycles = 4 + page_crossed; break;
        case 0xE1: op_sbc(read(addr_indirect_x())); cycles = 6; break;
        case 0xF1: op_sbc(read(addr_indirect_y(page_crossed))); cycles = 5 + page_crossed; break;

        // SEC - Set Carry Flag
        case 0x38: tick_internal(); set_flag(FLAG_C, true); cycles = 2; break;

        // SED - Set Decimal Flag
        case 0xF8: tick_internal(); set_flag(FLAG_D, true); cycles = 2; break;

        // SEI - Set Interrupt Disable
        case 0x78: tick_internal(); set_flag(FLAG_I, true); cycles = 2; break;

        // STA - Store Accumulator
        // Indexed addressing modes always do a dummy read for stores
        case 0x85: op_sta(addr_zero_page()); cycles = 3; break;
        case 0x95: op_sta(addr_zero_page_x()); cycles = 4; break;
        case 0x8D: op_sta(addr_absolute()); cycles = 4; break;
        case 0x9D: op_sta(addr_absolute_x(page_crossed, true)); cycles = 5; break;
        case 0x99: op_sta(addr_absolute_y(page_crossed, true)); cycles = 5; break;
        case 0x81: op_sta(addr_indirect_x()); cycles = 6; break;
        case 0x91: op_sta(addr_indirect_y(page_crossed, true)); cycles = 6; break;

        // STX - Store X Register
        case 0x86: op_stx(addr_zero_page()); cycles = 3; break;
        case 0x96: op_stx(addr_zero_page_y()); cycles = 4; break;
        case 0x8E: op_stx(addr_absolute()); cycles = 4; break;

        // STY - Store Y Register
        case 0x84: op_sty(addr_zero_page()); cycles = 3; break;
        case 0x94: op_sty(addr_zero_page_x()); cycles = 4; break;
        case 0x8C: op_sty(addr_absolute()); cycles = 4; break;

        // TAX - Transfer Accumulator to X
        case 0xAA: tick_internal(); m_x = m_a; update_zero_negative(m_x); cycles = 2; break;

        // TAY - Transfer Accumulator to Y
        case 0xA8: tick_internal(); m_y = m_a; update_zero_negative(m_y); cycles = 2; break;

        // TSX - Transfer Stack Pointer to X
        case 0xBA: tick_internal(); m_x = m_sp; update_zero_negative(m_x); cycles = 2; break;

        // TXA - Transfer X to Accumulator
        case 0x8A: tick_internal(); m_a = m_x; update_zero_negative(m_a); cycles = 2; break;

        // TXS - Transfer X to Stack Pointer
        case 0x9A: tick_internal(); m_sp = m_x; cycles = 2; break;

        // TYA - Transfer Y to Accumulator
        case 0x98: tick_internal(); m_a = m_y; update_zero_negative(m_a); cycles = 2; break;

        // ============== Unofficial/Illegal Opcodes ==============

        // LAX - Load A and X with same value
        case 0xA7: { uint8_t v = read(addr_zero_page()); m_a = m_x = v; update_zero_negative(v); cycles = 3; break; }
        case 0xB7: { uint8_t v = read(addr_zero_page_y()); m_a = m_x = v; update_zero_negative(v); cycles = 4; break; }
        case 0xAF: { uint8_t v = read(addr_absolute()); m_a = m_x = v; update_zero_negative(v); cycles = 4; break; }
        case 0xBF: { uint8_t v = read(addr_absolute_y(page_crossed)); m_a = m_x = v; update_zero_negative(v); cycles = 4 + (page_crossed ? 1 : 0); break; }
        case 0xA3: { uint8_t v = read(addr_indirect_x()); m_a = m_x = v; update_zero_negative(v); cycles = 6; break; }
        case 0xB3: { uint8_t v = read(addr_indirect_y(page_crossed)); m_a = m_x = v; update_zero_negative(v); cycles = 5 + (page_crossed ? 1 : 0); break; }

        // SAX - Store A AND X
        case 0x87: write(addr_zero_page(), m_a & m_x); cycles = 3; break;
        case 0x97: write(addr_zero_page_y(), m_a & m_x); cycles = 4; break;
        case 0x8F: write(addr_absolute(), m_a & m_x); cycles = 4; break;
        case 0x83: write(addr_indirect_x(), m_a & m_x); cycles = 6; break;

        // DCP - Decrement memory then Compare with A (RMW - includes dummy write)
        case 0xC7: { uint16_t a = addr_zero_page(); uint8_t v = read(a); write(a, v); v--; write(a, v); op_cmp(m_a, v); cycles = 5; break; }
        case 0xD7: { uint16_t a = addr_zero_page_x(); uint8_t v = read(a); write(a, v); v--; write(a, v); op_cmp(m_a, v); cycles = 6; break; }
        case 0xCF: { uint16_t a = addr_absolute(); uint8_t v = read(a); write(a, v); v--; write(a, v); op_cmp(m_a, v); cycles = 6; break; }
        case 0xDF: { uint16_t a = addr_absolute_x(page_crossed, true); uint8_t v = read(a); write(a, v); v--; write(a, v); op_cmp(m_a, v); cycles = 7; break; }
        case 0xDB: { uint16_t a = addr_absolute_y(page_crossed, true); uint8_t v = read(a); write(a, v); v--; write(a, v); op_cmp(m_a, v); cycles = 7; break; }
        case 0xC3: { uint16_t a = addr_indirect_x(); uint8_t v = read(a); write(a, v); v--; write(a, v); op_cmp(m_a, v); cycles = 8; break; }
        case 0xD3: { uint16_t a = addr_indirect_y(page_crossed, true); uint8_t v = read(a); write(a, v); v--; write(a, v); op_cmp(m_a, v); cycles = 8; break; }

        // ISB/ISC - Increment memory then Subtract from A (RMW - includes dummy write)
        case 0xE7: { uint16_t a = addr_zero_page(); uint8_t v = read(a); write(a, v); v++; write(a, v); op_sbc(v); cycles = 5; break; }
        case 0xF7: { uint16_t a = addr_zero_page_x(); uint8_t v = read(a); write(a, v); v++; write(a, v); op_sbc(v); cycles = 6; break; }
        case 0xEF: { uint16_t a = addr_absolute(); uint8_t v = read(a); write(a, v); v++; write(a, v); op_sbc(v); cycles = 6; break; }
        case 0xFF: { uint16_t a = addr_absolute_x(page_crossed, true); uint8_t v = read(a); write(a, v); v++; write(a, v); op_sbc(v); cycles = 7; break; }
        case 0xFB: { uint16_t a = addr_absolute_y(page_crossed, true); uint8_t v = read(a); write(a, v); v++; write(a, v); op_sbc(v); cycles = 7; break; }
        case 0xE3: { uint16_t a = addr_indirect_x(); uint8_t v = read(a); write(a, v); v++; write(a, v); op_sbc(v); cycles = 8; break; }
        case 0xF3: { uint16_t a = addr_indirect_y(page_crossed, true); uint8_t v = read(a); write(a, v); v++; write(a, v); op_sbc(v); cycles = 8; break; }

        // SLO - Shift Left then OR with A (RMW - includes dummy write of original value)
        case 0x07: { uint16_t a = addr_zero_page(); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x80); v <<= 1; write(a, v); m_a |= v; update_zero_negative(m_a); cycles = 5; break; }
        case 0x17: { uint16_t a = addr_zero_page_x(); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x80); v <<= 1; write(a, v); m_a |= v; update_zero_negative(m_a); cycles = 6; break; }
        case 0x0F: { uint16_t a = addr_absolute(); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x80); v <<= 1; write(a, v); m_a |= v; update_zero_negative(m_a); cycles = 6; break; }
        case 0x1F: { uint16_t a = addr_absolute_x(page_crossed, true); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x80); v <<= 1; write(a, v); m_a |= v; update_zero_negative(m_a); cycles = 7; break; }
        case 0x1B: { uint16_t a = addr_absolute_y(page_crossed, true); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x80); v <<= 1; write(a, v); m_a |= v; update_zero_negative(m_a); cycles = 7; break; }
        case 0x03: { uint16_t a = addr_indirect_x(); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x80); v <<= 1; write(a, v); m_a |= v; update_zero_negative(m_a); cycles = 8; break; }
        case 0x13: { uint16_t a = addr_indirect_y(page_crossed, true); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x80); v <<= 1; write(a, v); m_a |= v; update_zero_negative(m_a); cycles = 8; break; }

        // RLA - Rotate Left then AND with A (RMW - includes dummy write)
        case 0x27: { uint16_t a = addr_zero_page(); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 1 : 0; set_flag(FLAG_C, v & 0x80); v = (v << 1) | c; write(a, v); m_a &= v; update_zero_negative(m_a); cycles = 5; break; }
        case 0x37: { uint16_t a = addr_zero_page_x(); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 1 : 0; set_flag(FLAG_C, v & 0x80); v = (v << 1) | c; write(a, v); m_a &= v; update_zero_negative(m_a); cycles = 6; break; }
        case 0x2F: { uint16_t a = addr_absolute(); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 1 : 0; set_flag(FLAG_C, v & 0x80); v = (v << 1) | c; write(a, v); m_a &= v; update_zero_negative(m_a); cycles = 6; break; }
        case 0x3F: { uint16_t a = addr_absolute_x(page_crossed, true); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 1 : 0; set_flag(FLAG_C, v & 0x80); v = (v << 1) | c; write(a, v); m_a &= v; update_zero_negative(m_a); cycles = 7; break; }
        case 0x3B: { uint16_t a = addr_absolute_y(page_crossed, true); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 1 : 0; set_flag(FLAG_C, v & 0x80); v = (v << 1) | c; write(a, v); m_a &= v; update_zero_negative(m_a); cycles = 7; break; }
        case 0x23: { uint16_t a = addr_indirect_x(); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 1 : 0; set_flag(FLAG_C, v & 0x80); v = (v << 1) | c; write(a, v); m_a &= v; update_zero_negative(m_a); cycles = 8; break; }
        case 0x33: { uint16_t a = addr_indirect_y(page_crossed, true); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 1 : 0; set_flag(FLAG_C, v & 0x80); v = (v << 1) | c; write(a, v); m_a &= v; update_zero_negative(m_a); cycles = 8; break; }

        // SRE - Shift Right then XOR with A (RMW - includes dummy write)
        case 0x47: { uint16_t a = addr_zero_page(); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x01); v >>= 1; write(a, v); m_a ^= v; update_zero_negative(m_a); cycles = 5; break; }
        case 0x57: { uint16_t a = addr_zero_page_x(); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x01); v >>= 1; write(a, v); m_a ^= v; update_zero_negative(m_a); cycles = 6; break; }
        case 0x4F: { uint16_t a = addr_absolute(); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x01); v >>= 1; write(a, v); m_a ^= v; update_zero_negative(m_a); cycles = 6; break; }
        case 0x5F: { uint16_t a = addr_absolute_x(page_crossed, true); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x01); v >>= 1; write(a, v); m_a ^= v; update_zero_negative(m_a); cycles = 7; break; }
        case 0x5B: { uint16_t a = addr_absolute_y(page_crossed, true); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x01); v >>= 1; write(a, v); m_a ^= v; update_zero_negative(m_a); cycles = 7; break; }
        case 0x43: { uint16_t a = addr_indirect_x(); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x01); v >>= 1; write(a, v); m_a ^= v; update_zero_negative(m_a); cycles = 8; break; }
        case 0x53: { uint16_t a = addr_indirect_y(page_crossed, true); uint8_t v = read(a); write(a, v); set_flag(FLAG_C, v & 0x01); v >>= 1; write(a, v); m_a ^= v; update_zero_negative(m_a); cycles = 8; break; }

        // RRA - Rotate Right then Add with carry (RMW - includes dummy write)
        case 0x67: { uint16_t a = addr_zero_page(); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 0x80 : 0; set_flag(FLAG_C, v & 0x01); v = (v >> 1) | c; write(a, v); op_adc(v); cycles = 5; break; }
        case 0x77: { uint16_t a = addr_zero_page_x(); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 0x80 : 0; set_flag(FLAG_C, v & 0x01); v = (v >> 1) | c; write(a, v); op_adc(v); cycles = 6; break; }
        case 0x6F: { uint16_t a = addr_absolute(); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 0x80 : 0; set_flag(FLAG_C, v & 0x01); v = (v >> 1) | c; write(a, v); op_adc(v); cycles = 6; break; }
        case 0x7F: { uint16_t a = addr_absolute_x(page_crossed, true); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 0x80 : 0; set_flag(FLAG_C, v & 0x01); v = (v >> 1) | c; write(a, v); op_adc(v); cycles = 7; break; }
        case 0x7B: { uint16_t a = addr_absolute_y(page_crossed, true); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 0x80 : 0; set_flag(FLAG_C, v & 0x01); v = (v >> 1) | c; write(a, v); op_adc(v); cycles = 7; break; }
        case 0x63: { uint16_t a = addr_indirect_x(); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 0x80 : 0; set_flag(FLAG_C, v & 0x01); v = (v >> 1) | c; write(a, v); op_adc(v); cycles = 8; break; }
        case 0x73: { uint16_t a = addr_indirect_y(page_crossed, true); uint8_t v = read(a); write(a, v); uint8_t c = get_flag(FLAG_C) ? 0x80 : 0; set_flag(FLAG_C, v & 0x01); v = (v >> 1) | c; write(a, v); op_adc(v); cycles = 8; break; }

        // ANC - AND with A, copy N to C
        case 0x0B: case 0x2B: { m_a &= read(addr_immediate()); update_zero_negative(m_a); set_flag(FLAG_C, m_a & 0x80); cycles = 2; break; }

        // ALR/ASR - AND with A, then LSR
        case 0x4B: { m_a &= read(addr_immediate()); set_flag(FLAG_C, m_a & 0x01); m_a >>= 1; update_zero_negative(m_a); cycles = 2; break; }

        // ARR - AND with A, then ROR
        case 0x6B: { m_a &= read(addr_immediate()); m_a = (m_a >> 1) | (get_flag(FLAG_C) ? 0x80 : 0); update_zero_negative(m_a); set_flag(FLAG_C, m_a & 0x40); set_flag(FLAG_V, ((m_a >> 6) ^ (m_a >> 5)) & 1); cycles = 2; break; }

        // SBX/AXS - (A AND X) - imm -> X
        case 0xCB: { uint8_t v = read(addr_immediate()); uint8_t t = (m_a & m_x) - v; set_flag(FLAG_C, (m_a & m_x) >= v); m_x = t; update_zero_negative(m_x); cycles = 2; break; }

        // Unofficial SBC immediate (duplicate of 0xE9)
        case 0xEB: op_sbc(read(addr_immediate())); cycles = 2; break;

        // LAX immediate (unstable - uses (A | 0xEE) & X & imm, but many emulators use simpler behavior)
        case 0xAB: { uint8_t v = read(addr_immediate()); m_a = m_x = (m_a | 0xEE) & v; update_zero_negative(m_a); cycles = 2; break; }

        // Unofficial NOPs (various addressing modes)
        // These actually perform the addressing mode reads but discard the result
        case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: tick_internal(); cycles = 2; break;  // NOP implied
        case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2: read(m_pc++); cycles = 2; break;  // NOP immediate (2 cycles)
        case 0x04: case 0x44: case 0x64: read(addr_zero_page()); cycles = 3; break;  // NOP zero page (3 cycles)
        case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: read(addr_zero_page_x()); cycles = 4; break;  // NOP zero page,X (4 cycles)
        case 0x0C: read(addr_absolute()); cycles = 4; break;  // NOP absolute (4 cycles)
        case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: read(addr_absolute_x(page_crossed)); cycles = 4 + (page_crossed ? 1 : 0); break;  // NOP absolute,X (4+ cycles)

        // SHY - Store Y AND (high byte + 1) absolute,X (unstable)
        case 0x9C: {
            uint8_t lo = read(m_pc++);
            uint8_t hi = read(m_pc++);
            uint8_t val = m_y & (hi + 1);
            uint16_t addr = ((static_cast<uint16_t>(hi) << 8) | lo) + m_x;
            // Dummy read at uncorrected address (store instructions always do this)
            read(((static_cast<uint16_t>(hi) << 8) | ((lo + m_x) & 0xFF)));
            if ((lo + m_x) > 0xFF) {
                addr = (static_cast<uint16_t>(val) << 8) | ((lo + m_x) & 0xFF);
            }
            write(addr, val);
            cycles = 5;
            break;
        }

        // SHX - Store X AND (high byte + 1) absolute,Y (unstable)
        case 0x9E: {
            uint8_t lo = read(m_pc++);
            uint8_t hi = read(m_pc++);
            uint8_t val = m_x & (hi + 1);
            uint16_t addr = ((static_cast<uint16_t>(hi) << 8) | lo) + m_y;
            // Dummy read at uncorrected address (store instructions always do this)
            read(((static_cast<uint16_t>(hi) << 8) | ((lo + m_y) & 0xFF)));
            if ((lo + m_y) > 0xFF) {
                addr = (static_cast<uint16_t>(val) << 8) | ((lo + m_y) & 0xFF);
            }
            write(addr, val);
            cycles = 5;
            break;
        }

        // SHA/AHX - Store A AND X AND (high byte + 1) absolute,Y (unstable)
        case 0x9F: {
            uint8_t lo = read(m_pc++);
            uint8_t hi = read(m_pc++);
            uint8_t val = m_a & m_x & (hi + 1);
            uint16_t addr = ((static_cast<uint16_t>(hi) << 8) | lo) + m_y;
            // Dummy read at uncorrected address (store instructions always do this)
            read(((static_cast<uint16_t>(hi) << 8) | ((lo + m_y) & 0xFF)));
            if ((lo + m_y) > 0xFF) {
                addr = (static_cast<uint16_t>(val) << 8) | ((lo + m_y) & 0xFF);
            }
            write(addr, val);
            cycles = 5;
            break;
        }

        // SHA/AHX - Store A AND X AND (high byte + 1) (indirect),Y (unstable)
        case 0x93: {
            uint8_t ptr = read(m_pc++);
            uint8_t lo = read(ptr);
            uint8_t hi = read((ptr + 1) & 0xFF);
            uint8_t val = m_a & m_x & (hi + 1);
            uint16_t addr = ((static_cast<uint16_t>(hi) << 8) | lo) + m_y;
            // Dummy read at uncorrected address (store instructions always do this)
            read(((static_cast<uint16_t>(hi) << 8) | ((lo + m_y) & 0xFF)));
            if ((lo + m_y) > 0xFF) {
                addr = (static_cast<uint16_t>(val) << 8) | ((lo + m_y) & 0xFF);
            }
            write(addr, val);
            cycles = 6;
            break;
        }

        // TAS/SHS - Transfer A AND X to SP, then store A AND X AND (high byte + 1) (unstable)
        case 0x9B: {
            uint8_t lo = read(m_pc++);
            uint8_t hi = read(m_pc++);
            m_sp = m_a & m_x;
            uint8_t val = m_a & m_x & (hi + 1);
            uint16_t addr = ((static_cast<uint16_t>(hi) << 8) | lo) + m_y;
            // Dummy read at uncorrected address (store instructions always do this)
            read(((static_cast<uint16_t>(hi) << 8) | ((lo + m_y) & 0xFF)));
            if ((lo + m_y) > 0xFF) {
                addr = (static_cast<uint16_t>(val) << 8) | ((lo + m_y) & 0xFF);
            }
            write(addr, val);
            cycles = 5;
            break;
        }

        // LAS - Load A, X, and SP with (SP AND memory)
        case 0xBB: {
            uint16_t addr = addr_absolute_y(page_crossed);
            uint8_t val = read(addr) & m_sp;
            m_a = m_x = m_sp = val;
            update_zero_negative(val);
            cycles = 4 + (page_crossed ? 1 : 0);
            break;
        }

        // XAA/ANE - A = (A | magic) & X & imm (highly unstable, magic varies)
        case 0x8B: {
            uint8_t v = read(addr_immediate());
            m_a = (m_a | 0xEE) & m_x & v;
            update_zero_negative(m_a);
            cycles = 2;
            break;
        }

        // KIL/JAM opcodes - these halt the CPU on real hardware
        // We treat them as 2-cycle NOPs to avoid infinite loops
        case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
        case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
            cycles = 2;
            break;

        // Unknown/remaining opcodes - treat as NOP
        default: cycles = 2; break;
    }

    // At the end of each instruction, convert delayed NMI to pending
    // This makes the NMI fire BEFORE the next instruction starts
    // (but AFTER this instruction completed)
    if (m_nmi_delayed) {
        m_nmi_delayed = false;
        m_nmi_pending = true;
    }

    return cycles;
}

void CPU::trigger_nmi() {
    m_nmi_pending = true;
}

void CPU::trigger_nmi_delayed() {
    // NMI will fire after the NEXT instruction completes
    // This is used when NMI is enabled via PPUCTRL while VBL is already set
    m_nmi_delayed = true;
}

void CPU::trigger_irq() {
    m_irq_pending = true;
}

void CPU::set_irq_line(bool active) {
    // Level-triggered: IRQ is pending if the line is currently active
    // This reflects the mapper's current IRQ state, not a latched value
    m_irq_pending = active;
}

void CPU::set_nmi_line(bool active) {
    // Update NMI line state for edge detection
    // Edge detection happens in detect_nmi_edge()
    m_nmi_line = active;
}

bool CPU::detect_nmi_edge() {
    // NMI is edge-triggered: detect when line goes from low to high
    // This should be called after each PPU step to catch the edge
    bool edge_detected = false;
    if (m_nmi_line && !m_prev_nmi_line) {
        // Rising edge detected - set NMI pending
        m_nmi_pending = true;
        edge_detected = true;
    }
    m_prev_nmi_line = m_nmi_line;
    return edge_detected;
}

void CPU::poll_interrupts() {
    // Called by bus after each cycle to check for NMI edge
    // NMI is edge-triggered, so we detect when it goes from low to high
    m_bus.check_interrupts();
}

void CPU::tick_internal() {
    // Tick PPU/APU for one CPU cycle without a memory access
    // Used for internal operation cycles (implied mode, branch taken penalty, etc.)
    m_bus.tick();
    m_cycles++;
}

uint8_t CPU::read(uint16_t address) {
    m_cycles++;
    return m_bus.cpu_read(address);
}

void CPU::write(uint16_t address, uint8_t value) {
    m_cycles++;
    m_bus.cpu_write(address, value);
}

void CPU::push(uint8_t value) {
    write(0x0100 + m_sp, value);
    m_sp--;
}

uint8_t CPU::pop() {
    m_sp++;
    return read(0x0100 + m_sp);
}

void CPU::push16(uint16_t value) {
    push(static_cast<uint8_t>(value >> 8));
    push(static_cast<uint8_t>(value & 0xFF));
}

uint16_t CPU::pop16() {
    uint8_t lo = pop();
    uint8_t hi = pop();
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

// Addressing modes
uint16_t CPU::addr_immediate() {
    return m_pc++;
}

uint16_t CPU::addr_zero_page() {
    return read(m_pc++);
}

uint16_t CPU::addr_zero_page_x() {
    // Zero Page,X is 4 cycles for reads:
    // 1. Fetch opcode (already done)
    // 2. Fetch zero page address
    uint8_t addr = read(m_pc++);
    // 3. Read from address (dummy read while adding X)
    read(addr);
    // 4. Read from effective address (done by caller)
    return (addr + m_x) & 0xFF;
}

uint16_t CPU::addr_zero_page_y() {
    // Zero Page,Y is 4 cycles for reads:
    // 1. Fetch opcode (already done)
    // 2. Fetch zero page address
    uint8_t addr = read(m_pc++);
    // 3. Read from address (dummy read while adding Y)
    read(addr);
    // 4. Read from effective address (done by caller)
    return (addr + m_y) & 0xFF;
}

uint16_t CPU::addr_absolute() {
    uint8_t lo = read(m_pc++);
    uint8_t hi = read(m_pc++);
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

uint16_t CPU::addr_absolute_x(bool& page_crossed, bool is_write) {
    uint8_t lo = read(m_pc++);
    uint8_t hi = read(m_pc++);
    uint16_t addr = (static_cast<uint16_t>(hi) << 8) | lo;
    uint16_t result = addr + m_x;
    page_crossed = (addr & 0xFF00) != (result & 0xFF00);
    // Dummy read at the "wrong" address (before page crossing is corrected).
    // For read operations, this only happens on page cross.
    // For write operations, this ALWAYS happens (stores take fixed cycles).
    if (page_crossed || is_write) {
        uint16_t dummy_addr = (static_cast<uint16_t>(hi) << 8) | ((lo + m_x) & 0xFF);
        read(dummy_addr);
    }
    return result;
}

uint16_t CPU::addr_absolute_y(bool& page_crossed, bool is_write) {
    uint8_t lo = read(m_pc++);
    uint8_t hi = read(m_pc++);
    uint16_t addr = (static_cast<uint16_t>(hi) << 8) | lo;
    uint16_t result = addr + m_y;
    page_crossed = (addr & 0xFF00) != (result & 0xFF00);
    // Dummy read: for reads only on page cross, for writes always
    if (page_crossed || is_write) {
        uint16_t dummy_addr = (static_cast<uint16_t>(hi) << 8) | ((lo + m_y) & 0xFF);
        read(dummy_addr);
    }
    return result;
}

uint16_t CPU::addr_indirect() {
    uint8_t ptr_lo = read(m_pc++);
    uint8_t ptr_hi = read(m_pc++);
    uint16_t ptr = (static_cast<uint16_t>(ptr_hi) << 8) | ptr_lo;

    // 6502 bug: if ptr is on page boundary, wrap around
    uint16_t ptr_next = (ptr & 0xFF00) | ((ptr + 1) & 0x00FF);

    uint8_t lo = read(ptr);
    uint8_t hi = read(ptr_next);
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

uint16_t CPU::addr_indirect_x() {
    // (Indirect,X) is 6 cycles for reads, 6 for writes:
    // 1. Fetch opcode (already done)
    // 2. Fetch pointer base
    uint8_t base = read(m_pc++);
    // 3. Read from base address (dummy read while adding X)
    read(base);
    // 4-5. Read low and high bytes from effective pointer
    uint8_t ptr = (base + m_x) & 0xFF;
    uint8_t lo = read(ptr);
    uint8_t hi = read((ptr + 1) & 0xFF);
    // 6. Read from effective address (done by caller)
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

uint16_t CPU::addr_indirect_y(bool& page_crossed, bool is_write) {
    uint8_t ptr = read(m_pc++);
    uint8_t lo = read(ptr);
    uint8_t hi = read((ptr + 1) & 0xFF);
    uint16_t addr = (static_cast<uint16_t>(hi) << 8) | lo;
    uint16_t result = addr + m_y;
    page_crossed = (addr & 0xFF00) != (result & 0xFF00);
    // Dummy read: for reads only on page cross, for writes always
    if (page_crossed || is_write) {
        uint16_t dummy_addr = (static_cast<uint16_t>(hi) << 8) | ((lo + m_y) & 0xFF);
        read(dummy_addr);
    }
    return result;
}

void CPU::set_flag(uint8_t flag, bool value) {
    if (value) {
        m_status |= flag;
    } else {
        m_status &= ~flag;
    }
}

bool CPU::get_flag(uint8_t flag) const {
    return (m_status & flag) != 0;
}

void CPU::update_zero_negative(uint8_t value) {
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, (value & 0x80) != 0);
}

// Instructions
void CPU::op_adc(uint8_t value) {
    uint16_t sum = m_a + value + (get_flag(FLAG_C) ? 1 : 0);
    set_flag(FLAG_C, sum > 0xFF);
    set_flag(FLAG_V, (~(m_a ^ value) & (m_a ^ sum) & 0x80) != 0);
    m_a = static_cast<uint8_t>(sum);
    update_zero_negative(m_a);
}

void CPU::op_and(uint8_t value) {
    m_a &= value;
    update_zero_negative(m_a);
}

void CPU::op_asl(uint16_t address) {
    uint8_t value = read(address);
    write(address, value);  // Dummy write of original value (RMW behavior)
    set_flag(FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    write(address, value);
    update_zero_negative(value);
}

void CPU::op_asl_a() {
    set_flag(FLAG_C, (m_a & 0x80) != 0);
    m_a <<= 1;
    update_zero_negative(m_a);
}

void CPU::op_bit(uint8_t value) {
    set_flag(FLAG_Z, (m_a & value) == 0);
    set_flag(FLAG_N, (value & 0x80) != 0);
    set_flag(FLAG_V, (value & 0x40) != 0);
}

void CPU::op_branch(bool condition) {
    int8_t offset = static_cast<int8_t>(read(m_pc++));
    if (condition) {
        // Branch taken: 1 cycle penalty for calculating new PC
        tick_internal();

        uint16_t old_pc = m_pc;
        m_pc += offset;

        // Page crossing adds another cycle
        if ((old_pc & 0xFF00) != (m_pc & 0xFF00)) {
            tick_internal();
        }
    }
    // Branch not taken: no extra cycles (just the 2 cycles for fetch + operand)
}

void CPU::op_brk() {
    // BRK is 7 cycles:
    // 1. Fetch opcode (already done in step())
    // 2. Read next byte and throw away (padding byte)
    read(m_pc);  // Dummy read of padding byte
    m_pc++;
    // 3-4. Push PCH, PCL
    push16(m_pc);
    // 5. Push status with B flag set
    push(m_status | FLAG_B | FLAG_U);
    set_flag(FLAG_I, true);

    // NMI hijacking: If NMI is pending when we're about to read the vector,
    // NMI takes over and uses the NMI vector instead of the IRQ/BRK vector.
    // The B flag is still set on stack (from BRK), but we jump to NMI handler.
    uint16_t vector;
    if (m_nmi_pending) {
        m_nmi_pending = false;
        vector = 0xFFFA;  // NMI vector
    } else {
        vector = 0xFFFE;  // IRQ/BRK vector
    }

    // 6-7. Read vector low, high
    uint8_t lo = read(vector);
    uint8_t hi = read(vector + 1);
    m_pc = (static_cast<uint16_t>(hi) << 8) | lo;
}

void CPU::op_cmp(uint8_t reg, uint8_t value) {
    set_flag(FLAG_C, reg >= value);
    update_zero_negative(reg - value);
}

void CPU::op_dec(uint16_t address) {
    uint8_t value = read(address);
    write(address, value);  // Dummy write of original value (RMW behavior)
    value--;
    write(address, value);
    update_zero_negative(value);
}

void CPU::op_eor(uint8_t value) {
    m_a ^= value;
    update_zero_negative(m_a);
}

void CPU::op_inc(uint16_t address) {
    uint8_t value = read(address);
    write(address, value);  // Dummy write of original value (RMW behavior)
    value++;
    write(address, value);
    update_zero_negative(value);
}

void CPU::op_jmp(uint16_t address) {
    m_pc = address;
}

void CPU::op_jsr(uint16_t address) {
    // JSR is 6 cycles:
    // 1. Fetch opcode (already done)
    // 2-3. Fetch address low/high (done by addr_absolute)
    // 4. Internal operation
    tick_internal();
    // 5-6. Push PCH, PCL
    push16(m_pc - 1);
    m_pc = address;
}

void CPU::op_lda(uint8_t value) {
    m_a = value;
    update_zero_negative(m_a);
}

void CPU::op_ldx(uint8_t value) {
    m_x = value;
    update_zero_negative(m_x);
}

void CPU::op_ldy(uint8_t value) {
    m_y = value;
    update_zero_negative(m_y);
}

void CPU::op_lsr(uint16_t address) {
    uint8_t value = read(address);
    write(address, value);  // Dummy write of original value (RMW behavior)
    set_flag(FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    write(address, value);
    update_zero_negative(value);
}

void CPU::op_lsr_a() {
    set_flag(FLAG_C, (m_a & 0x01) != 0);
    m_a >>= 1;
    update_zero_negative(m_a);
}

void CPU::op_ora(uint8_t value) {
    m_a |= value;
    update_zero_negative(m_a);
}

void CPU::op_rol(uint16_t address) {
    uint8_t value = read(address);
    write(address, value);  // Dummy write of original value (RMW behavior)
    bool carry = get_flag(FLAG_C);
    set_flag(FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (carry ? 1 : 0);
    write(address, value);
    update_zero_negative(value);
}

void CPU::op_rol_a() {
    bool carry = get_flag(FLAG_C);
    set_flag(FLAG_C, (m_a & 0x80) != 0);
    m_a = (m_a << 1) | (carry ? 1 : 0);
    update_zero_negative(m_a);
}

void CPU::op_ror(uint16_t address) {
    uint8_t value = read(address);
    write(address, value);  // Dummy write of original value (RMW behavior)
    bool carry = get_flag(FLAG_C);
    set_flag(FLAG_C, (value & 0x01) != 0);
    value = (value >> 1) | (carry ? 0x80 : 0);
    write(address, value);
    update_zero_negative(value);
}

void CPU::op_ror_a() {
    bool carry = get_flag(FLAG_C);
    set_flag(FLAG_C, (m_a & 0x01) != 0);
    m_a = (m_a >> 1) | (carry ? 0x80 : 0);
    update_zero_negative(m_a);
}

void CPU::op_rti() {
    // RTI is 6 cycles:
    // 1. Fetch opcode (already done in step())
    // 2. Read next instruction byte (discarded)
    read(m_pc);
    // 3. Dummy read from stack (increment SP internal operation)
    read(0x0100 + m_sp);
    // 4. Pull status (1 cycle via pop)
    m_status = (pop() & ~FLAG_B) | FLAG_U;
    // 5-6. Pull PCL, PCH (2 cycles via pop16)
    m_pc = pop16();
}

void CPU::op_rts() {
    // RTS is 6 cycles:
    // 1. Fetch opcode (already done in step())
    // 2. Read next instruction byte (discarded)
    read(m_pc);
    // 3. Dummy read from stack (increment SP internal operation)
    read(0x0100 + m_sp);
    // 4-5. Pull PCL, PCH (2 cycles via pop16)
    m_pc = pop16();
    // 6. Increment PC (internal operation)
    tick_internal();
    m_pc++;
}

void CPU::op_sbc(uint8_t value) {
    // SBC is the same as ADC with inverted value
    op_adc(~value);
}

void CPU::op_sta(uint16_t address) {
    write(address, m_a);
}

void CPU::op_stx(uint16_t address) {
    write(address, m_x);
}

void CPU::op_sty(uint16_t address) {
    write(address, m_y);
}

// Save state serialization helpers
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
}

void CPU::save_state(std::vector<uint8_t>& data) {
    write_value(data, m_pc);
    write_value(data, m_a);
    write_value(data, m_x);
    write_value(data, m_y);
    write_value(data, m_sp);
    write_value(data, m_status);
    write_value(data, static_cast<uint8_t>(m_nmi_pending ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_nmi_delayed ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_irq_pending ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_prev_irq_inhibit ? 1 : 0));
    // New fields for cycle-accurate NMI edge detection
    write_value(data, static_cast<uint8_t>(m_nmi_line ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_prev_nmi_line ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_in_interrupt_sequence ? 1 : 0));
}

void CPU::load_state(const uint8_t*& data, size_t& remaining) {
    read_value(data, remaining, m_pc);
    read_value(data, remaining, m_a);
    read_value(data, remaining, m_x);
    read_value(data, remaining, m_y);
    read_value(data, remaining, m_sp);
    read_value(data, remaining, m_status);
    uint8_t nmi, nmi_delayed, irq;
    read_value(data, remaining, nmi);
    read_value(data, remaining, nmi_delayed);
    read_value(data, remaining, irq);
    m_nmi_pending = nmi != 0;
    m_nmi_delayed = nmi_delayed != 0;
    m_irq_pending = irq != 0;
    // Load prev_irq_inhibit if present (for backwards compatibility)
    if (remaining >= 1) {
        uint8_t prev_inhibit;
        read_value(data, remaining, prev_inhibit);
        m_prev_irq_inhibit = prev_inhibit != 0;
    } else {
        // Old save state format - initialize from current I flag
        m_prev_irq_inhibit = get_flag(FLAG_I);
    }
    // Load new NMI edge detection fields if present
    if (remaining >= 3) {
        uint8_t nmi_line, prev_nmi_line, in_int_seq;
        read_value(data, remaining, nmi_line);
        read_value(data, remaining, prev_nmi_line);
        read_value(data, remaining, in_int_seq);
        m_nmi_line = nmi_line != 0;
        m_prev_nmi_line = prev_nmi_line != 0;
        m_in_interrupt_sequence = in_int_seq != 0;
    } else {
        // Old save state - initialize to safe defaults
        m_nmi_line = false;
        m_prev_nmi_line = false;
        m_in_interrupt_sequence = false;
    }
}

} // namespace nes
