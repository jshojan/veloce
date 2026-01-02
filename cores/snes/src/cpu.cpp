#include "cpu.hpp"
#include "bus.hpp"
#include "debug.hpp"
#include <cstring>

namespace snes {

CPU::CPU(Bus& bus) : m_bus(bus) {
    reset();
}

CPU::~CPU() = default;

void CPU::reset() {
    m_a = 0;
    m_x = 0;
    m_y = 0;
    m_sp = 0x01FF;
    m_dp = 0;
    m_pbr = 0;
    m_dbr = 0;
    m_status = 0x34;  // M=1, X=1, I=1
    m_emulation = true;
    m_nmi_pending = false;
    m_irq_line = false;
    m_wai_waiting = false;
    m_stp_stopped = false;
    m_cycles = 0;

    // Read reset vector
    uint8_t lo = m_bus.read(VEC_RESET);
    uint8_t hi = m_bus.read(VEC_RESET + 1);
    m_pc = lo | (hi << 8);

    SNES_CPU_DEBUG("Reset: PC=$%04X\n", m_pc);
}

int CPU::step() {
    m_cycles = 0;

    // Handle stopped state
    if (m_stp_stopped) {
        m_cycles = 6;
        return m_cycles;
    }

    // Handle waiting state
    if (m_wai_waiting) {
        if (m_nmi_pending || (m_irq_line && !get_flag(FLAG_I))) {
            m_wai_waiting = false;
        } else {
            m_cycles = 6;
            return m_cycles;
        }
    }

    // Check for NMI (edge-triggered, highest priority)
    if (m_nmi_pending) {
        m_nmi_pending = false;
        do_interrupt(m_emulation ? VEC_NMI_EMU : VEC_NMI_NATIVE);
        return m_cycles;
    }

    // Check for IRQ (level-triggered)
    if (m_irq_line && !get_flag(FLAG_I)) {
        do_interrupt(m_emulation ? VEC_IRQ_BRK_EMU : VEC_IRQ_NATIVE);
        return m_cycles;
    }

    // Execute instruction
    execute();

    return m_cycles;
}

void CPU::trigger_nmi() {
    m_nmi_pending = true;
    if (m_wai_waiting) {
        m_wai_waiting = false;
    }
    static int s_nmi_count = 0;
    if (is_debug_mode() && s_nmi_count < 10) {
        SNES_CPU_DEBUG("NMI triggered! PC=$%02X:%04X\n", m_pbr, m_pc);
        s_nmi_count++;
    }
}

void CPU::trigger_irq() {
    m_irq_line = true;
}

void CPU::set_irq_line(bool active) {
    m_irq_line = active;
    if (active && m_wai_waiting && !get_flag(FLAG_I)) {
        m_wai_waiting = false;
    }
}

// Memory access
uint8_t CPU::read(uint32_t address) {
    m_cycles += 6;  // Base memory access time (can vary with FastROM)
    return m_bus.read(address);
}

void CPU::write(uint32_t address, uint8_t value) {
    m_cycles += 6;
    m_bus.write(address, value);
}

uint8_t CPU::read_pc() {
    uint32_t addr = (static_cast<uint32_t>(m_pbr) << 16) | m_pc;
    m_pc++;
    return read(addr);
}

uint16_t CPU::read_pc16() {
    uint8_t lo = read_pc();
    uint8_t hi = read_pc();
    return lo | (hi << 8);
}

uint32_t CPU::read_pc24() {
    uint8_t lo = read_pc();
    uint8_t mid = read_pc();
    uint8_t hi = read_pc();
    return lo | (mid << 8) | (hi << 16);
}

uint8_t CPU::read_db(uint16_t address) {
    uint32_t addr = (static_cast<uint32_t>(m_dbr) << 16) | address;
    return read(addr);
}

void CPU::write_db(uint16_t address, uint8_t value) {
    uint32_t addr = (static_cast<uint32_t>(m_dbr) << 16) | address;
    write(addr, value);
}

// Stack operations
void CPU::push8(uint8_t value) {
    write(m_sp, value);
    if (m_emulation) {
        m_sp = 0x0100 | ((m_sp - 1) & 0xFF);
    } else {
        m_sp--;
    }
}

uint8_t CPU::pop8() {
    if (m_emulation) {
        m_sp = 0x0100 | ((m_sp + 1) & 0xFF);
    } else {
        m_sp++;
    }
    return read(m_sp);
}

void CPU::push16(uint16_t value) {
    push8(value >> 8);
    push8(value & 0xFF);
}

uint16_t CPU::pop16() {
    uint8_t lo = pop8();
    uint8_t hi = pop8();
    return lo | (hi << 8);
}

void CPU::push24(uint32_t value) {
    push8((value >> 16) & 0xFF);
    push8((value >> 8) & 0xFF);
    push8(value & 0xFF);
}

uint32_t CPU::pop24() {
    uint8_t lo = pop8();
    uint8_t mid = pop8();
    uint8_t hi = pop8();
    return lo | (mid << 8) | (hi << 16);
}

// Addressing modes
uint32_t CPU::addr_immediate8() {
    uint32_t addr = (static_cast<uint32_t>(m_pbr) << 16) | m_pc;
    m_pc++;
    return addr;
}

uint32_t CPU::addr_immediate16() {
    uint32_t addr = (static_cast<uint32_t>(m_pbr) << 16) | m_pc;
    m_pc += 2;
    return addr;
}

uint32_t CPU::addr_immediate_m() {
    if (get_flag(FLAG_M)) {
        return addr_immediate8();
    }
    return addr_immediate16();
}

uint32_t CPU::addr_immediate_x() {
    if (get_flag(FLAG_X)) {
        return addr_immediate8();
    }
    return addr_immediate16();
}

uint32_t CPU::addr_direct() {
    uint8_t offset = read_pc();
    // Add cycle if direct page is not page-aligned
    if ((m_dp & 0xFF) != 0) m_cycles += 6;
    return (m_dp + offset) & 0xFFFF;
}

uint32_t CPU::addr_direct_x() {
    uint8_t offset = read_pc();
    if ((m_dp & 0xFF) != 0) m_cycles += 6;
    m_cycles += 6;  // Index calculation
    return (m_dp + offset + m_x) & 0xFFFF;
}

uint32_t CPU::addr_direct_y() {
    uint8_t offset = read_pc();
    if ((m_dp & 0xFF) != 0) m_cycles += 6;
    m_cycles += 6;
    return (m_dp + offset + m_y) & 0xFFFF;
}

uint32_t CPU::addr_direct_indirect() {
    uint32_t dp_addr = addr_direct();
    uint8_t lo = read(dp_addr);
    uint8_t hi = read((dp_addr + 1) & 0xFFFF);
    return (static_cast<uint32_t>(m_dbr) << 16) | (lo | (hi << 8));
}

uint32_t CPU::addr_direct_indirect_long() {
    uint32_t dp_addr = addr_direct();
    uint8_t lo = read(dp_addr);
    uint8_t mid = read((dp_addr + 1) & 0xFFFF);
    uint8_t hi = read((dp_addr + 2) & 0xFFFF);
    return lo | (mid << 8) | (hi << 16);
}

uint32_t CPU::addr_direct_x_indirect() {
    uint32_t dp_addr = addr_direct_x();
    uint8_t lo = read(dp_addr);
    uint8_t hi = read((dp_addr + 1) & 0xFFFF);
    return (static_cast<uint32_t>(m_dbr) << 16) | (lo | (hi << 8));
}

uint32_t CPU::addr_direct_indirect_y() {
    uint32_t dp_addr = addr_direct();
    uint8_t lo = read(dp_addr);
    uint8_t hi = read((dp_addr + 1) & 0xFFFF);
    uint16_t base = lo | (hi << 8);
    uint16_t result = base + m_y;
    // Page crossing penalty
    if ((base & 0xFF00) != (result & 0xFF00)) m_cycles += 6;
    return (static_cast<uint32_t>(m_dbr) << 16) | result;
}

uint32_t CPU::addr_direct_indirect_long_y() {
    uint32_t dp_addr = addr_direct();
    uint8_t lo = read(dp_addr);
    uint8_t mid = read((dp_addr + 1) & 0xFFFF);
    uint8_t hi = read((dp_addr + 2) & 0xFFFF);
    uint32_t base = lo | (mid << 8) | (hi << 16);
    return (base + m_y) & 0xFFFFFF;
}

uint32_t CPU::addr_absolute() {
    uint16_t addr = read_pc16();
    return (static_cast<uint32_t>(m_dbr) << 16) | addr;
}

uint32_t CPU::addr_absolute_x() {
    uint16_t base = read_pc16();
    uint16_t result = base + m_x;
    // Page crossing penalty (not always applied)
    if ((base & 0xFF00) != (result & 0xFF00)) m_cycles += 6;
    return (static_cast<uint32_t>(m_dbr) << 16) | result;
}

uint32_t CPU::addr_absolute_y() {
    uint16_t base = read_pc16();
    uint16_t result = base + m_y;
    if ((base & 0xFF00) != (result & 0xFF00)) m_cycles += 6;
    return (static_cast<uint32_t>(m_dbr) << 16) | result;
}

uint32_t CPU::addr_absolute_long() {
    return read_pc24();
}

uint32_t CPU::addr_absolute_long_x() {
    uint32_t base = read_pc24();
    return (base + m_x) & 0xFFFFFF;
}

uint32_t CPU::addr_absolute_indirect() {
    uint16_t ptr = read_pc16();
    uint8_t lo = read(ptr);
    uint8_t hi = read((ptr + 1) & 0xFFFF);
    return (static_cast<uint32_t>(m_pbr) << 16) | (lo | (hi << 8));
}

uint32_t CPU::addr_absolute_indirect_long() {
    uint16_t ptr = read_pc16();
    uint8_t lo = read(ptr);
    uint8_t mid = read((ptr + 1) & 0xFFFF);
    uint8_t hi = read((ptr + 2) & 0xFFFF);
    return lo | (mid << 8) | (hi << 16);
}

uint32_t CPU::addr_absolute_x_indirect() {
    uint16_t ptr = read_pc16() + m_x;
    uint32_t addr = (static_cast<uint32_t>(m_pbr) << 16) | ptr;
    uint8_t lo = read(addr);
    uint8_t hi = read((addr + 1) & 0xFFFFFF);
    return (static_cast<uint32_t>(m_pbr) << 16) | (lo | (hi << 8));
}

uint32_t CPU::addr_stack_relative() {
    uint8_t offset = read_pc();
    m_cycles += 6;
    return (m_sp + offset) & 0xFFFF;
}

uint32_t CPU::addr_stack_relative_indirect_y() {
    uint32_t sr_addr = addr_stack_relative();
    uint8_t lo = read(sr_addr);
    uint8_t hi = read((sr_addr + 1) & 0xFFFF);
    uint16_t base = lo | (hi << 8);
    return (static_cast<uint32_t>(m_dbr) << 16) | ((base + m_y) & 0xFFFF);
}

// Flag operations
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

void CPU::update_nz8(uint8_t value) {
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, (value & 0x80) != 0);
}

void CPU::update_nz16(uint16_t value) {
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, (value & 0x8000) != 0);
}

void CPU::update_nz_m(uint16_t value) {
    if (get_flag(FLAG_M)) {
        update_nz8(value & 0xFF);
    } else {
        update_nz16(value);
    }
}

// ALU operations
void CPU::op_adc8(uint8_t value) {
    uint8_t a = m_a & 0xFF;
    uint16_t result;

    if (get_flag(FLAG_D)) {
        // BCD mode
        uint8_t lo = (a & 0x0F) + (value & 0x0F) + (get_flag(FLAG_C) ? 1 : 0);
        if (lo > 9) lo += 6;
        uint8_t hi = (a >> 4) + (value >> 4) + (lo > 0x0F ? 1 : 0);
        set_flag(FLAG_V, (~(a ^ value) & (a ^ (hi << 4)) & 0x80) != 0);
        if (hi > 9) hi += 6;
        set_flag(FLAG_C, hi > 0x0F);
        result = ((hi & 0x0F) << 4) | (lo & 0x0F);
    } else {
        result = a + value + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(a ^ value) & (a ^ result) & 0x80) != 0);
    }

    m_a = (m_a & 0xFF00) | (result & 0xFF);
    update_nz8(result & 0xFF);
}

void CPU::op_adc16(uint16_t value) {
    uint16_t a = m_a;
    uint32_t result;

    if (get_flag(FLAG_D)) {
        // BCD mode for 16-bit
        uint32_t temp = (a & 0x000F) + (value & 0x000F) + (get_flag(FLAG_C) ? 1 : 0);
        if (temp > 0x0009) temp += 0x0006;
        temp += (a & 0x00F0) + (value & 0x00F0);
        if (temp > 0x009F) temp += 0x0060;
        temp += (a & 0x0F00) + (value & 0x0F00);
        if (temp > 0x09FF) temp += 0x0600;
        temp += (a & 0xF000) + (value & 0xF000);
        set_flag(FLAG_V, (~(a ^ value) & (a ^ temp) & 0x8000) != 0);
        if (temp > 0x9FFF) temp += 0x6000;
        set_flag(FLAG_C, temp > 0xFFFF);
        result = temp;
    } else {
        result = a + value + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFFFF);
        set_flag(FLAG_V, (~(a ^ value) & (a ^ result) & 0x8000) != 0);
    }

    m_a = result & 0xFFFF;
    update_nz16(m_a);
}

void CPU::op_sbc8(uint8_t value) {
    uint8_t a = m_a & 0xFF;
    uint16_t result;

    if (get_flag(FLAG_D)) {
        // BCD mode
        int lo = (a & 0x0F) - (value & 0x0F) - (get_flag(FLAG_C) ? 0 : 1);
        int hi = (a >> 4) - (value >> 4);
        if (lo < 0) { lo -= 6; hi--; }
        if (hi < 0) hi -= 6;
        result = ((hi & 0x0F) << 4) | (lo & 0x0F);
        set_flag(FLAG_C, (a - value - (get_flag(FLAG_C) ? 0 : 1)) >= 0);
    } else {
        result = a - value - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
    }

    set_flag(FLAG_V, ((a ^ value) & (a ^ result) & 0x80) != 0);
    m_a = (m_a & 0xFF00) | (result & 0xFF);
    update_nz8(result & 0xFF);
}

void CPU::op_sbc16(uint16_t value) {
    uint16_t a = m_a;
    uint32_t result;

    if (get_flag(FLAG_D)) {
        // BCD mode for 16-bit
        int32_t temp = (a & 0x000F) - (value & 0x000F) - (get_flag(FLAG_C) ? 0 : 1);
        if (temp < 0) temp -= 0x0006;
        temp += (a & 0x00F0) - (value & 0x00F0);
        if (temp < 0) temp -= 0x0060;
        temp += (a & 0x0F00) - (value & 0x0F00);
        if (temp < 0) temp -= 0x0600;
        temp += (a & 0xF000) - (value & 0xF000);
        if (temp < 0) temp -= 0x6000;
        result = temp;
        set_flag(FLAG_C, (static_cast<int32_t>(a) - value - (get_flag(FLAG_C) ? 0 : 1)) >= 0);
    } else {
        result = a - value - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFFFF);
    }

    set_flag(FLAG_V, ((a ^ value) & (a ^ result) & 0x8000) != 0);
    m_a = result & 0xFFFF;
    update_nz16(m_a);
}

void CPU::op_and8(uint8_t value) {
    m_a = (m_a & 0xFF00) | ((m_a & value) & 0xFF);
    update_nz8(m_a & 0xFF);
}

void CPU::op_and16(uint16_t value) {
    m_a &= value;
    update_nz16(m_a);
}

void CPU::op_ora8(uint8_t value) {
    m_a = (m_a & 0xFF00) | ((m_a | value) & 0xFF);
    update_nz8(m_a & 0xFF);
}

void CPU::op_ora16(uint16_t value) {
    m_a |= value;
    update_nz16(m_a);
}

void CPU::op_eor8(uint8_t value) {
    m_a = (m_a & 0xFF00) | ((m_a ^ value) & 0xFF);
    update_nz8(m_a & 0xFF);
}

void CPU::op_eor16(uint16_t value) {
    m_a ^= value;
    update_nz16(m_a);
}

void CPU::op_cmp8(uint8_t reg, uint8_t value) {
    uint16_t result = reg - value;
    set_flag(FLAG_C, reg >= value);
    update_nz8(result & 0xFF);
}

void CPU::op_cmp16(uint16_t reg, uint16_t value) {
    uint32_t result = reg - value;
    set_flag(FLAG_C, reg >= value);
    update_nz16(result & 0xFFFF);
}

void CPU::op_bit8(uint8_t value) {
    set_flag(FLAG_Z, (m_a & value & 0xFF) == 0);
    set_flag(FLAG_N, (value & 0x80) != 0);
    set_flag(FLAG_V, (value & 0x40) != 0);
}

void CPU::op_bit16(uint16_t value) {
    set_flag(FLAG_Z, (m_a & value) == 0);
    set_flag(FLAG_N, (value & 0x8000) != 0);
    set_flag(FLAG_V, (value & 0x4000) != 0);
}

void CPU::op_bit_imm8(uint8_t value) {
    set_flag(FLAG_Z, (m_a & value & 0xFF) == 0);
}

void CPU::op_bit_imm16(uint16_t value) {
    set_flag(FLAG_Z, (m_a & value) == 0);
}

// Shift/rotate operations
uint8_t CPU::op_asl8(uint8_t value) {
    set_flag(FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    update_nz8(value);
    return value;
}

uint16_t CPU::op_asl16(uint16_t value) {
    set_flag(FLAG_C, (value & 0x8000) != 0);
    value <<= 1;
    update_nz16(value);
    return value;
}

uint8_t CPU::op_lsr8(uint8_t value) {
    set_flag(FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    update_nz8(value);
    return value;
}

uint16_t CPU::op_lsr16(uint16_t value) {
    set_flag(FLAG_C, (value & 0x0001) != 0);
    value >>= 1;
    update_nz16(value);
    return value;
}

uint8_t CPU::op_rol8(uint8_t value) {
    bool c = get_flag(FLAG_C);
    set_flag(FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (c ? 1 : 0);
    update_nz8(value);
    return value;
}

uint16_t CPU::op_rol16(uint16_t value) {
    bool c = get_flag(FLAG_C);
    set_flag(FLAG_C, (value & 0x8000) != 0);
    value = (value << 1) | (c ? 1 : 0);
    update_nz16(value);
    return value;
}

uint8_t CPU::op_ror8(uint8_t value) {
    bool c = get_flag(FLAG_C);
    set_flag(FLAG_C, (value & 0x01) != 0);
    value = (value >> 1) | (c ? 0x80 : 0);
    update_nz8(value);
    return value;
}

uint16_t CPU::op_ror16(uint16_t value) {
    bool c = get_flag(FLAG_C);
    set_flag(FLAG_C, (value & 0x0001) != 0);
    value = (value >> 1) | (c ? 0x8000 : 0);
    update_nz16(value);
    return value;
}

// Increment/decrement
uint8_t CPU::op_inc8(uint8_t value) {
    value++;
    update_nz8(value);
    return value;
}

uint16_t CPU::op_inc16(uint16_t value) {
    value++;
    update_nz16(value);
    return value;
}

uint8_t CPU::op_dec8(uint8_t value) {
    value--;
    update_nz8(value);
    return value;
}

uint16_t CPU::op_dec16(uint16_t value) {
    value--;
    update_nz16(value);
    return value;
}

// Test and set/reset bits
uint8_t CPU::op_tsb8(uint8_t value) {
    set_flag(FLAG_Z, (m_a & value & 0xFF) == 0);
    return value | (m_a & 0xFF);
}

uint16_t CPU::op_tsb16(uint16_t value) {
    set_flag(FLAG_Z, (m_a & value) == 0);
    return value | m_a;
}

uint8_t CPU::op_trb8(uint8_t value) {
    set_flag(FLAG_Z, (m_a & value & 0xFF) == 0);
    return value & ~(m_a & 0xFF);
}

uint16_t CPU::op_trb16(uint16_t value) {
    set_flag(FLAG_Z, (m_a & value) == 0);
    return value & ~m_a;
}

// Branch helper
void CPU::branch(bool condition) {
    int8_t offset = static_cast<int8_t>(read_pc());
    if (condition) {
        m_cycles += 6;  // Branch taken
        uint16_t old_pc = m_pc;
        m_pc += offset;
        // Extra cycle if page crossed in emulation mode
        if (m_emulation && ((old_pc & 0xFF00) != (m_pc & 0xFF00))) {
            m_cycles += 6;
        }
    }
}

// Interrupt handling
void CPU::do_interrupt(uint16_t vector, bool is_brk) {
    m_cycles += 6;  // Internal operation

    if (!m_emulation) {
        push8(m_pbr);
    }
    push16(m_pc);

    if (m_emulation) {
        uint8_t p = m_status;
        if (is_brk) p |= FLAG_B;
        push8(p | 0x20);  // Set unused bit
    } else {
        push8(m_status);
    }

    set_flag(FLAG_I, true);
    set_flag(FLAG_D, false);
    m_pbr = 0;

    uint8_t lo = read(vector);
    uint8_t hi = read(vector + 1);
    m_pc = lo | (hi << 8);
}

// Main execution
void CPU::execute() {
    static int s_trace_count = 0;
    static uint16_t s_last_pc = 0xFFFF;

    uint16_t current_pc = m_pc;
    uint8_t opcode = read_pc();

    // Trace first 100 unique instructions or if stuck in a loop
    if (is_debug_mode() && (s_trace_count < 100 || current_pc == s_last_pc)) {
        if (current_pc != s_last_pc || s_trace_count < 10) {
            fprintf(stderr, "[SNES/CPU] %02X:%04X op=%02X A=%04X X=%04X Y=%04X SP=%04X P=%02X%s\n",
                m_pbr, current_pc, opcode, m_a, m_x, m_y, m_sp, m_status,
                m_emulation ? " (E)" : "");
            s_trace_count++;
        }
    }
    s_last_pc = current_pc;

    switch (opcode) {
        // ADC - Add with Carry
        case 0x69:  // ADC #imm
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0x65: { // ADC dp
            uint32_t addr = addr_direct();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x75: { // ADC dp,X
            uint32_t addr = addr_direct_x();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x6D: { // ADC abs
            uint32_t addr = addr_absolute();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x7D: { // ADC abs,X
            uint32_t addr = addr_absolute_x();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x79: { // ADC abs,Y
            uint32_t addr = addr_absolute_y();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x6F: { // ADC long
            uint32_t addr = addr_absolute_long();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x7F: { // ADC long,X
            uint32_t addr = addr_absolute_long_x();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x72: { // ADC (dp)
            uint32_t addr = addr_direct_indirect();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x67: { // ADC [dp]
            uint32_t addr = addr_direct_indirect_long();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x61: { // ADC (dp,X)
            uint32_t addr = addr_direct_x_indirect();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x71: { // ADC (dp),Y
            uint32_t addr = addr_direct_indirect_y();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x77: { // ADC [dp],Y
            uint32_t addr = addr_direct_indirect_long_y();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x63: { // ADC sr,S
            uint32_t addr = addr_stack_relative();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }
        case 0x73: { // ADC (sr,S),Y
            uint32_t addr = addr_stack_relative_indirect_y();
            if (get_flag(FLAG_M)) {
                op_adc8(read(addr));
            } else {
                op_adc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        }

        // AND - Logical AND
        case 0x29:  // AND #imm
            if (get_flag(FLAG_M)) {
                op_and8(read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_and16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0x25: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x35: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x2D: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x3D: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x39: { uint32_t a = addr_absolute_y(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x2F: { uint32_t a = addr_absolute_long(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x3F: { uint32_t a = addr_absolute_long_x(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x32: { uint32_t a = addr_direct_indirect(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x27: { uint32_t a = addr_direct_indirect_long(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x21: { uint32_t a = addr_direct_x_indirect(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x31: { uint32_t a = addr_direct_indirect_y(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x37: { uint32_t a = addr_direct_indirect_long_y(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x23: { uint32_t a = addr_stack_relative(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }
        case 0x33: { uint32_t a = addr_stack_relative_indirect_y(); if (get_flag(FLAG_M)) op_and8(read(a)); else op_and16(read(a)|(read(a+1)<<8)); break; }

        // ASL - Arithmetic Shift Left
        case 0x0A:  // ASL A
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | op_asl8(m_a & 0xFF);
            } else {
                m_a = op_asl16(m_a);
            }
            break;
        case 0x06: { uint32_t a = addr_direct(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_asl8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_asl16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x16: { uint32_t a = addr_direct_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_asl8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_asl16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x0E: { uint32_t a = addr_absolute(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_asl8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_asl16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x1E: { uint32_t a = addr_absolute_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_asl8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_asl16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }

        // BCC/BCS/BEQ/BMI/BNE/BPL/BVC/BVS - Branches
        case 0x90: branch(!get_flag(FLAG_C)); break;  // BCC
        case 0xB0: branch(get_flag(FLAG_C)); break;   // BCS
        case 0xF0: branch(get_flag(FLAG_Z)); break;   // BEQ
        case 0x30: branch(get_flag(FLAG_N)); break;   // BMI
        case 0xD0: branch(!get_flag(FLAG_Z)); break;  // BNE
        case 0x10: branch(!get_flag(FLAG_N)); break;  // BPL
        case 0x50: branch(!get_flag(FLAG_V)); break;  // BVC
        case 0x70: branch(get_flag(FLAG_V)); break;   // BVS
        case 0x80: branch(true); break;               // BRA (always)

        // BRL - Branch Long
        case 0x82: {
            int16_t offset = static_cast<int16_t>(read_pc16());
            m_cycles += 6;
            m_pc += offset;
            break;
        }

        // BIT - Bit Test
        case 0x89:  // BIT #imm
            if (get_flag(FLAG_M)) {
                op_bit_imm8(read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_bit_imm16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0x24: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) op_bit8(read(a)); else op_bit16(read(a)|(read(a+1)<<8)); break; }
        case 0x34: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) op_bit8(read(a)); else op_bit16(read(a)|(read(a+1)<<8)); break; }
        case 0x2C: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) op_bit8(read(a)); else op_bit16(read(a)|(read(a+1)<<8)); break; }
        case 0x3C: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) op_bit8(read(a)); else op_bit16(read(a)|(read(a+1)<<8)); break; }

        // BRK - Break
        case 0x00:
            read_pc();  // Padding byte
            do_interrupt(m_emulation ? VEC_IRQ_BRK_EMU : VEC_BRK_NATIVE, true);
            break;

        // CLC/CLD/CLI/CLV - Clear flags
        case 0x18: m_cycles += 6; set_flag(FLAG_C, false); break;
        case 0xD8: m_cycles += 6; set_flag(FLAG_D, false); break;
        case 0x58: m_cycles += 6; set_flag(FLAG_I, false); break;
        case 0xB8: m_cycles += 6; set_flag(FLAG_V, false); break;

        // CMP - Compare Accumulator
        case 0xC9:
            if (get_flag(FLAG_M)) {
                op_cmp8(m_a & 0xFF, read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_cmp16(m_a, read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0xC5: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xD5: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xCD: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xDD: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xD9: { uint32_t a = addr_absolute_y(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xCF: { uint32_t a = addr_absolute_long(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xDF: { uint32_t a = addr_absolute_long_x(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xD2: { uint32_t a = addr_direct_indirect(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xC7: { uint32_t a = addr_direct_indirect_long(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xC1: { uint32_t a = addr_direct_x_indirect(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xD1: { uint32_t a = addr_direct_indirect_y(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xD7: { uint32_t a = addr_direct_indirect_long_y(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xC3: { uint32_t a = addr_stack_relative(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }
        case 0xD3: { uint32_t a = addr_stack_relative_indirect_y(); if (get_flag(FLAG_M)) op_cmp8(m_a&0xFF, read(a)); else op_cmp16(m_a, read(a)|(read(a+1)<<8)); break; }

        // COP - Coprocessor
        case 0x02:
            read_pc();  // Signature byte
            do_interrupt(m_emulation ? VEC_COP_EMU : VEC_COP_NATIVE);
            break;

        // CPX - Compare X
        case 0xE0:
            if (get_flag(FLAG_X)) {
                op_cmp8(m_x & 0xFF, read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_cmp16(m_x, read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0xE4: { uint32_t a = addr_direct(); if (get_flag(FLAG_X)) op_cmp8(m_x&0xFF, read(a)); else op_cmp16(m_x, read(a)|(read(a+1)<<8)); break; }
        case 0xEC: { uint32_t a = addr_absolute(); if (get_flag(FLAG_X)) op_cmp8(m_x&0xFF, read(a)); else op_cmp16(m_x, read(a)|(read(a+1)<<8)); break; }

        // CPY - Compare Y
        case 0xC0:
            if (get_flag(FLAG_X)) {
                op_cmp8(m_y & 0xFF, read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_cmp16(m_y, read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0xC4: { uint32_t a = addr_direct(); if (get_flag(FLAG_X)) op_cmp8(m_y&0xFF, read(a)); else op_cmp16(m_y, read(a)|(read(a+1)<<8)); break; }
        case 0xCC: { uint32_t a = addr_absolute(); if (get_flag(FLAG_X)) op_cmp8(m_y&0xFF, read(a)); else op_cmp16(m_y, read(a)|(read(a+1)<<8)); break; }

        // DEC - Decrement
        case 0x3A:  // DEC A
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | op_dec8(m_a & 0xFF);
            } else {
                m_a = op_dec16(m_a);
            }
            break;
        case 0xC6: { uint32_t a = addr_direct(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_dec8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_dec16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0xD6: { uint32_t a = addr_direct_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_dec8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_dec16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0xCE: { uint32_t a = addr_absolute(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_dec8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_dec16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0xDE: { uint32_t a = addr_absolute_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_dec8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_dec16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }

        // DEX/DEY - Decrement X/Y
        case 0xCA:
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                m_x = (m_x & 0xFF00) | op_dec8(m_x & 0xFF);
            } else {
                m_x = op_dec16(m_x);
            }
            break;
        case 0x88:
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                m_y = (m_y & 0xFF00) | op_dec8(m_y & 0xFF);
            } else {
                m_y = op_dec16(m_y);
            }
            break;

        // EOR - Exclusive OR
        case 0x49:
            if (get_flag(FLAG_M)) {
                op_eor8(read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_eor16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0x45: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x55: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x4D: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x5D: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x59: { uint32_t a = addr_absolute_y(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x4F: { uint32_t a = addr_absolute_long(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x5F: { uint32_t a = addr_absolute_long_x(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x52: { uint32_t a = addr_direct_indirect(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x47: { uint32_t a = addr_direct_indirect_long(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x41: { uint32_t a = addr_direct_x_indirect(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x51: { uint32_t a = addr_direct_indirect_y(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x57: { uint32_t a = addr_direct_indirect_long_y(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x43: { uint32_t a = addr_stack_relative(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }
        case 0x53: { uint32_t a = addr_stack_relative_indirect_y(); if (get_flag(FLAG_M)) op_eor8(read(a)); else op_eor16(read(a)|(read(a+1)<<8)); break; }

        // INC - Increment
        case 0x1A:  // INC A
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | op_inc8(m_a & 0xFF);
            } else {
                m_a = op_inc16(m_a);
            }
            break;
        case 0xE6: { uint32_t a = addr_direct(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_inc8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_inc16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0xF6: { uint32_t a = addr_direct_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_inc8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_inc16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0xEE: { uint32_t a = addr_absolute(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_inc8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_inc16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0xFE: { uint32_t a = addr_absolute_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_inc8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_inc16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }

        // INX/INY - Increment X/Y
        case 0xE8:
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                m_x = (m_x & 0xFF00) | op_inc8(m_x & 0xFF);
            } else {
                m_x = op_inc16(m_x);
            }
            break;
        case 0xC8:
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                m_y = (m_y & 0xFF00) | op_inc8(m_y & 0xFF);
            } else {
                m_y = op_inc16(m_y);
            }
            break;

        // JMP - Jump
        case 0x4C: {  // JMP abs
            m_pc = read_pc16();
            break;
        }
        case 0x6C: {  // JMP (abs)
            m_pc = addr_absolute_indirect() & 0xFFFF;
            break;
        }
        case 0x7C: {  // JMP (abs,X)
            m_pc = addr_absolute_x_indirect() & 0xFFFF;
            break;
        }
        case 0x5C: {  // JMP long
            uint32_t addr = read_pc24();
            m_pbr = (addr >> 16) & 0xFF;
            m_pc = addr & 0xFFFF;
            break;
        }
        case 0xDC: {  // JMP [abs]
            uint32_t addr = addr_absolute_indirect_long();
            m_pbr = (addr >> 16) & 0xFF;
            m_pc = addr & 0xFFFF;
            break;
        }

        // JSR/JSL - Jump to Subroutine
        case 0x20: {  // JSR abs
            uint16_t addr = read_pc16();
            m_cycles += 6;
            push16(m_pc - 1);
            m_pc = addr;
            break;
        }
        case 0xFC: {  // JSR (abs,X)
            push16(m_pc + 1);
            m_pc = addr_absolute_x_indirect() & 0xFFFF;
            break;
        }
        case 0x22: {  // JSL long
            uint32_t addr = read_pc24();
            push8(m_pbr);
            m_cycles += 6;
            push16(m_pc - 1);
            m_pbr = (addr >> 16) & 0xFF;
            m_pc = addr & 0xFFFF;
            break;
        }

        // LDA - Load Accumulator
        case 0xA9:
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | read(addr_immediate8());
                update_nz8(m_a & 0xFF);
            } else {
                uint32_t addr = addr_immediate16();
                m_a = read(addr) | (read(addr + 1) << 8);
                update_nz16(m_a);
            }
            break;
        case 0xA5: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xB5: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xAD: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xBD: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xB9: { uint32_t a = addr_absolute_y(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xAF: { uint32_t a = addr_absolute_long(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xBF: { uint32_t a = addr_absolute_long_x(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xB2: { uint32_t a = addr_direct_indirect(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xA7: { uint32_t a = addr_direct_indirect_long(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xA1: { uint32_t a = addr_direct_x_indirect(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xB1: { uint32_t a = addr_direct_indirect_y(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xB7: { uint32_t a = addr_direct_indirect_long_y(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xA3: { uint32_t a = addr_stack_relative(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }
        case 0xB3: { uint32_t a = addr_stack_relative_indirect_y(); if (get_flag(FLAG_M)) { m_a = (m_a&0xFF00)|read(a); update_nz8(m_a&0xFF); } else { m_a = read(a)|(read(a+1)<<8); update_nz16(m_a); } break; }

        // LDX - Load X
        case 0xA2:
            if (get_flag(FLAG_X)) {
                m_x = read(addr_immediate8());
                update_nz8(m_x & 0xFF);
            } else {
                uint32_t addr = addr_immediate16();
                m_x = read(addr) | (read(addr + 1) << 8);
                update_nz16(m_x);
            }
            break;
        case 0xA6: { uint32_t a = addr_direct(); if (get_flag(FLAG_X)) { m_x = read(a); update_nz8(m_x&0xFF); } else { m_x = read(a)|(read(a+1)<<8); update_nz16(m_x); } break; }
        case 0xB6: { uint32_t a = addr_direct_y(); if (get_flag(FLAG_X)) { m_x = read(a); update_nz8(m_x&0xFF); } else { m_x = read(a)|(read(a+1)<<8); update_nz16(m_x); } break; }
        case 0xAE: { uint32_t a = addr_absolute(); if (get_flag(FLAG_X)) { m_x = read(a); update_nz8(m_x&0xFF); } else { m_x = read(a)|(read(a+1)<<8); update_nz16(m_x); } break; }
        case 0xBE: { uint32_t a = addr_absolute_y(); if (get_flag(FLAG_X)) { m_x = read(a); update_nz8(m_x&0xFF); } else { m_x = read(a)|(read(a+1)<<8); update_nz16(m_x); } break; }

        // LDY - Load Y
        case 0xA0:
            if (get_flag(FLAG_X)) {
                m_y = read(addr_immediate8());
                update_nz8(m_y & 0xFF);
            } else {
                uint32_t addr = addr_immediate16();
                m_y = read(addr) | (read(addr + 1) << 8);
                update_nz16(m_y);
            }
            break;
        case 0xA4: { uint32_t a = addr_direct(); if (get_flag(FLAG_X)) { m_y = read(a); update_nz8(m_y&0xFF); } else { m_y = read(a)|(read(a+1)<<8); update_nz16(m_y); } break; }
        case 0xB4: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_X)) { m_y = read(a); update_nz8(m_y&0xFF); } else { m_y = read(a)|(read(a+1)<<8); update_nz16(m_y); } break; }
        case 0xAC: { uint32_t a = addr_absolute(); if (get_flag(FLAG_X)) { m_y = read(a); update_nz8(m_y&0xFF); } else { m_y = read(a)|(read(a+1)<<8); update_nz16(m_y); } break; }
        case 0xBC: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_X)) { m_y = read(a); update_nz8(m_y&0xFF); } else { m_y = read(a)|(read(a+1)<<8); update_nz16(m_y); } break; }

        // LSR - Logical Shift Right
        case 0x4A:  // LSR A
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | op_lsr8(m_a & 0xFF);
            } else {
                m_a = op_lsr16(m_a);
            }
            break;
        case 0x46: { uint32_t a = addr_direct(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_lsr8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_lsr16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x56: { uint32_t a = addr_direct_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_lsr8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_lsr16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x4E: { uint32_t a = addr_absolute(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_lsr8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_lsr16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x5E: { uint32_t a = addr_absolute_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_lsr8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_lsr16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }

        // MVN/MVP - Block Move
        case 0x54: {  // MVN (Move Negative/Increment)
            uint8_t dst_bank = read_pc();
            uint8_t src_bank = read_pc();
            m_dbr = dst_bank;
            uint32_t src = (static_cast<uint32_t>(src_bank) << 16) | m_x;
            uint32_t dst = (static_cast<uint32_t>(dst_bank) << 16) | m_y;
            write(dst, read(src));
            m_x++;
            m_y++;
            if (get_flag(FLAG_X)) { m_x &= 0xFF; m_y &= 0xFF; }
            m_a--;
            if (m_a != 0xFFFF) m_pc -= 3;  // Repeat
            m_cycles += 6;
            break;
        }
        case 0x44: {  // MVP (Move Positive/Decrement)
            uint8_t dst_bank = read_pc();
            uint8_t src_bank = read_pc();
            m_dbr = dst_bank;
            uint32_t src = (static_cast<uint32_t>(src_bank) << 16) | m_x;
            uint32_t dst = (static_cast<uint32_t>(dst_bank) << 16) | m_y;
            write(dst, read(src));
            m_x--;
            m_y--;
            if (get_flag(FLAG_X)) { m_x &= 0xFF; m_y &= 0xFF; }
            m_a--;
            if (m_a != 0xFFFF) m_pc -= 3;
            m_cycles += 6;
            break;
        }

        // NOP
        case 0xEA:
            m_cycles += 6;
            break;

        // ORA - Logical OR
        case 0x09:
            if (get_flag(FLAG_M)) {
                op_ora8(read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_ora16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0x05: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x15: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x0D: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x1D: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x19: { uint32_t a = addr_absolute_y(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x0F: { uint32_t a = addr_absolute_long(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x1F: { uint32_t a = addr_absolute_long_x(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x12: { uint32_t a = addr_direct_indirect(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x07: { uint32_t a = addr_direct_indirect_long(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x01: { uint32_t a = addr_direct_x_indirect(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x11: { uint32_t a = addr_direct_indirect_y(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x17: { uint32_t a = addr_direct_indirect_long_y(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x03: { uint32_t a = addr_stack_relative(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }
        case 0x13: { uint32_t a = addr_stack_relative_indirect_y(); if (get_flag(FLAG_M)) op_ora8(read(a)); else op_ora16(read(a)|(read(a+1)<<8)); break; }

        // PEA/PEI/PER - Push Effective Address
        case 0xF4: {  // PEA abs
            push16(read_pc16());
            break;
        }
        case 0xD4: {  // PEI (dp)
            uint32_t addr = addr_direct();
            uint16_t val = read(addr) | (read(addr + 1) << 8);
            push16(val);
            break;
        }
        case 0x62: {  // PER rel
            int16_t offset = static_cast<int16_t>(read_pc16());
            push16(m_pc + offset);
            break;
        }

        // PHA/PHB/PHD/PHK/PHP/PHX/PHY - Push
        case 0x48:  // PHA
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                push8(m_a & 0xFF);
            } else {
                push16(m_a);
            }
            break;
        case 0x8B: m_cycles += 6; push8(m_dbr); break;  // PHB
        case 0x0B: m_cycles += 6; push16(m_dp); break;  // PHD
        case 0x4B: m_cycles += 6; push8(m_pbr); break;  // PHK
        case 0x08: m_cycles += 6; push8(m_status); break;  // PHP
        case 0xDA:  // PHX
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                push8(m_x & 0xFF);
            } else {
                push16(m_x);
            }
            break;
        case 0x5A:  // PHY
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                push8(m_y & 0xFF);
            } else {
                push16(m_y);
            }
            break;

        // PLA/PLB/PLD/PLP/PLX/PLY - Pull
        case 0x68:  // PLA
            m_cycles += 12;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | pop8();
                update_nz8(m_a & 0xFF);
            } else {
                m_a = pop16();
                update_nz16(m_a);
            }
            break;
        case 0xAB:  // PLB
            m_cycles += 12;
            m_dbr = pop8();
            update_nz8(m_dbr);
            break;
        case 0x2B:  // PLD
            m_cycles += 12;
            m_dp = pop16();
            update_nz16(m_dp);
            break;
        case 0x28:  // PLP
            m_cycles += 12;
            m_status = pop8();
            if (m_emulation) {
                m_status |= FLAG_M | FLAG_X;
            }
            if (get_flag(FLAG_X)) {
                m_x &= 0xFF;
                m_y &= 0xFF;
            }
            break;
        case 0xFA:  // PLX
            m_cycles += 12;
            if (get_flag(FLAG_X)) {
                m_x = pop8();
                update_nz8(m_x & 0xFF);
            } else {
                m_x = pop16();
                update_nz16(m_x);
            }
            break;
        case 0x7A:  // PLY
            m_cycles += 12;
            if (get_flag(FLAG_X)) {
                m_y = pop8();
                update_nz8(m_y & 0xFF);
            } else {
                m_y = pop16();
                update_nz16(m_y);
            }
            break;

        // REP - Reset Processor Status Bits
        case 0xC2: {
            uint8_t mask = read_pc();
            m_cycles += 6;
            m_status &= ~mask;
            if (m_emulation) {
                m_status |= FLAG_M | FLAG_X;
            }
            if (get_flag(FLAG_X)) {
                m_x &= 0xFF;
                m_y &= 0xFF;
            }
            break;
        }

        // ROL - Rotate Left
        case 0x2A:  // ROL A
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | op_rol8(m_a & 0xFF);
            } else {
                m_a = op_rol16(m_a);
            }
            break;
        case 0x26: { uint32_t a = addr_direct(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_rol8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_rol16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x36: { uint32_t a = addr_direct_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_rol8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_rol16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x2E: { uint32_t a = addr_absolute(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_rol8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_rol16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x3E: { uint32_t a = addr_absolute_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_rol8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_rol16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }

        // ROR - Rotate Right
        case 0x6A:  // ROR A
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | op_ror8(m_a & 0xFF);
            } else {
                m_a = op_ror16(m_a);
            }
            break;
        case 0x66: { uint32_t a = addr_direct(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_ror8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_ror16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x76: { uint32_t a = addr_direct_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_ror8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_ror16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x6E: { uint32_t a = addr_absolute(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_ror8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_ror16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x7E: { uint32_t a = addr_absolute_x(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_ror8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_ror16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }

        // RTI - Return from Interrupt
        case 0x40:
            m_cycles += 12;
            m_status = pop8();
            if (m_emulation) {
                m_status |= FLAG_M | FLAG_X;
            }
            m_pc = pop16();
            if (!m_emulation) {
                m_pbr = pop8();
            }
            if (get_flag(FLAG_X)) {
                m_x &= 0xFF;
                m_y &= 0xFF;
            }
            break;

        // RTL - Return from Subroutine Long
        case 0x6B:
            m_cycles += 12;
            m_pc = pop16() + 1;
            m_pbr = pop8();
            break;

        // RTS - Return from Subroutine
        case 0x60:
            m_cycles += 18;
            m_pc = pop16() + 1;
            break;

        // SBC - Subtract with Carry
        case 0xE9:
            if (get_flag(FLAG_M)) {
                op_sbc8(read(addr_immediate8()));
            } else {
                uint32_t addr = addr_immediate16();
                op_sbc16(read(addr) | (read(addr + 1) << 8));
            }
            break;
        case 0xE5: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xF5: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xED: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xFD: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xF9: { uint32_t a = addr_absolute_y(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xEF: { uint32_t a = addr_absolute_long(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xFF: { uint32_t a = addr_absolute_long_x(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xF2: { uint32_t a = addr_direct_indirect(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xE7: { uint32_t a = addr_direct_indirect_long(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xE1: { uint32_t a = addr_direct_x_indirect(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xF1: { uint32_t a = addr_direct_indirect_y(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xF7: { uint32_t a = addr_direct_indirect_long_y(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xE3: { uint32_t a = addr_stack_relative(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }
        case 0xF3: { uint32_t a = addr_stack_relative_indirect_y(); if (get_flag(FLAG_M)) op_sbc8(read(a)); else op_sbc16(read(a)|(read(a+1)<<8)); break; }

        // SEC/SED/SEI - Set flags
        case 0x38: m_cycles += 6; set_flag(FLAG_C, true); break;
        case 0xF8: m_cycles += 6; set_flag(FLAG_D, true); break;
        case 0x78: m_cycles += 6; set_flag(FLAG_I, true); break;

        // SEP - Set Processor Status Bits
        case 0xE2: {
            uint8_t mask = read_pc();
            m_cycles += 6;
            m_status |= mask;
            if (get_flag(FLAG_X)) {
                m_x &= 0xFF;
                m_y &= 0xFF;
            }
            break;
        }

        // STA - Store Accumulator
        case 0x85: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x95: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x8D: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x9D: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x99: { uint32_t a = addr_absolute_y(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x8F: { uint32_t a = addr_absolute_long(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x9F: { uint32_t a = addr_absolute_long_x(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x92: { uint32_t a = addr_direct_indirect(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x87: { uint32_t a = addr_direct_indirect_long(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x81: { uint32_t a = addr_direct_x_indirect(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x91: { uint32_t a = addr_direct_indirect_y(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x97: { uint32_t a = addr_direct_indirect_long_y(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x83: { uint32_t a = addr_stack_relative(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }
        case 0x93: { uint32_t a = addr_stack_relative_indirect_y(); if (get_flag(FLAG_M)) write(a, m_a&0xFF); else { write(a, m_a&0xFF); write(a+1, m_a>>8); } break; }

        // STP - Stop Processor
        case 0xDB:
            m_cycles += 6;
            m_stp_stopped = true;
            break;

        // STX - Store X
        case 0x86: { uint32_t a = addr_direct(); if (get_flag(FLAG_X)) write(a, m_x&0xFF); else { write(a, m_x&0xFF); write(a+1, m_x>>8); } break; }
        case 0x96: { uint32_t a = addr_direct_y(); if (get_flag(FLAG_X)) write(a, m_x&0xFF); else { write(a, m_x&0xFF); write(a+1, m_x>>8); } break; }
        case 0x8E: { uint32_t a = addr_absolute(); if (get_flag(FLAG_X)) write(a, m_x&0xFF); else { write(a, m_x&0xFF); write(a+1, m_x>>8); } break; }

        // STY - Store Y
        case 0x84: { uint32_t a = addr_direct(); if (get_flag(FLAG_X)) write(a, m_y&0xFF); else { write(a, m_y&0xFF); write(a+1, m_y>>8); } break; }
        case 0x94: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_X)) write(a, m_y&0xFF); else { write(a, m_y&0xFF); write(a+1, m_y>>8); } break; }
        case 0x8C: { uint32_t a = addr_absolute(); if (get_flag(FLAG_X)) write(a, m_y&0xFF); else { write(a, m_y&0xFF); write(a+1, m_y>>8); } break; }

        // STZ - Store Zero
        case 0x64: { uint32_t a = addr_direct(); if (get_flag(FLAG_M)) write(a, 0); else { write(a, 0); write(a+1, 0); } break; }
        case 0x74: { uint32_t a = addr_direct_x(); if (get_flag(FLAG_M)) write(a, 0); else { write(a, 0); write(a+1, 0); } break; }
        case 0x9C: { uint32_t a = addr_absolute(); if (get_flag(FLAG_M)) write(a, 0); else { write(a, 0); write(a+1, 0); } break; }
        case 0x9E: { uint32_t a = addr_absolute_x(); if (get_flag(FLAG_M)) write(a, 0); else { write(a, 0); write(a+1, 0); } break; }

        // TAX/TAY/TCD/TCS/TDC/TSC/TSX/TXA/TXS/TXY/TYA/TYX - Transfers
        case 0xAA:  // TAX
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                m_x = m_a & 0xFF;
                update_nz8(m_x & 0xFF);
            } else {
                m_x = m_a;
                update_nz16(m_x);
            }
            break;
        case 0xA8:  // TAY
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                m_y = m_a & 0xFF;
                update_nz8(m_y & 0xFF);
            } else {
                m_y = m_a;
                update_nz16(m_y);
            }
            break;
        case 0x5B:  // TCD
            m_cycles += 6;
            m_dp = m_a;
            update_nz16(m_dp);
            break;
        case 0x1B:  // TCS
            m_cycles += 6;
            m_sp = m_a;
            if (m_emulation) m_sp = 0x0100 | (m_sp & 0xFF);
            break;
        case 0x7B:  // TDC
            m_cycles += 6;
            m_a = m_dp;
            update_nz16(m_a);
            break;
        case 0x3B:  // TSC
            m_cycles += 6;
            m_a = m_sp;
            update_nz16(m_a);
            break;
        case 0xBA:  // TSX
            m_cycles += 6;
            if (get_flag(FLAG_X)) {
                m_x = m_sp & 0xFF;
                update_nz8(m_x & 0xFF);
            } else {
                m_x = m_sp;
                update_nz16(m_x);
            }
            break;
        case 0x8A:  // TXA
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | (m_x & 0xFF);
                update_nz8(m_a & 0xFF);
            } else {
                m_a = m_x;
                update_nz16(m_a);
            }
            break;
        case 0x9A:  // TXS
            m_cycles += 6;
            m_sp = m_x;
            if (m_emulation) m_sp = 0x0100 | (m_sp & 0xFF);
            break;
        case 0x9B:  // TXY
            m_cycles += 6;
            m_y = m_x;
            if (get_flag(FLAG_X)) {
                update_nz8(m_y & 0xFF);
            } else {
                update_nz16(m_y);
            }
            break;
        case 0x98:  // TYA
            m_cycles += 6;
            if (get_flag(FLAG_M)) {
                m_a = (m_a & 0xFF00) | (m_y & 0xFF);
                update_nz8(m_a & 0xFF);
            } else {
                m_a = m_y;
                update_nz16(m_a);
            }
            break;
        case 0xBB:  // TYX
            m_cycles += 6;
            m_x = m_y;
            if (get_flag(FLAG_X)) {
                update_nz8(m_x & 0xFF);
            } else {
                update_nz16(m_x);
            }
            break;

        // TRB - Test and Reset Bits
        case 0x14: { uint32_t a = addr_direct(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_trb8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_trb16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x1C: { uint32_t a = addr_absolute(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_trb8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_trb16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }

        // TSB - Test and Set Bits
        case 0x04: { uint32_t a = addr_direct(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_tsb8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_tsb16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }
        case 0x0C: { uint32_t a = addr_absolute(); m_cycles += 6; if (get_flag(FLAG_M)) write(a, op_tsb8(read(a))); else { uint16_t v = read(a)|(read(a+1)<<8); v = op_tsb16(v); write(a, v&0xFF); write(a+1, v>>8); } break; }

        // WAI - Wait for Interrupt
        case 0xCB:
            m_cycles += 6;
            m_wai_waiting = true;
            break;

        // WDM - Reserved (2-byte NOP)
        case 0x42:
            read_pc();  // Skip signature byte
            m_cycles += 6;
            break;

        // XBA - Exchange B and A
        case 0xEB:
            m_cycles += 6;
            m_a = ((m_a & 0xFF) << 8) | ((m_a >> 8) & 0xFF);
            update_nz8(m_a & 0xFF);
            break;

        // XCE - Exchange Carry and Emulation
        case 0xFB: {
            m_cycles += 6;
            bool old_c = get_flag(FLAG_C);
            set_flag(FLAG_C, m_emulation);
            m_emulation = old_c;
            if (m_emulation) {
                m_status |= FLAG_M | FLAG_X;
                m_x &= 0xFF;
                m_y &= 0xFF;
                m_sp = 0x0100 | (m_sp & 0xFF);
            }
            break;
        }

        default:
            SNES_CPU_DEBUG("Unknown opcode: $%02X at $%02X:%04X\n", opcode, m_pbr, m_pc - 1);
            m_cycles += 6;
            break;
    }
}

void CPU::save_state(std::vector<uint8_t>& data) {
    auto write16 = [&](uint16_t v) { data.push_back(v & 0xFF); data.push_back(v >> 8); };
    auto write8 = [&](uint8_t v) { data.push_back(v); };

    write16(m_a);
    write16(m_x);
    write16(m_y);
    write16(m_sp);
    write16(m_dp);
    write16(m_pc);
    write8(m_pbr);
    write8(m_dbr);
    write8(m_status);
    write8(m_emulation ? 1 : 0);
    write8(m_nmi_pending ? 1 : 0);
    write8(m_irq_line ? 1 : 0);
    write8(m_wai_waiting ? 1 : 0);
    write8(m_stp_stopped ? 1 : 0);
}

void CPU::load_state(const uint8_t*& data, size_t& remaining) {
    auto read16 = [&]() -> uint16_t {
        uint16_t v = data[0] | (data[1] << 8);
        data += 2; remaining -= 2;
        return v;
    };
    auto read8 = [&]() -> uint8_t {
        uint8_t v = *data++;
        remaining--;
        return v;
    };

    m_a = read16();
    m_x = read16();
    m_y = read16();
    m_sp = read16();
    m_dp = read16();
    m_pc = read16();
    m_pbr = read8();
    m_dbr = read8();
    m_status = read8();
    m_emulation = read8() != 0;
    m_nmi_pending = read8() != 0;
    m_irq_line = read8() != 0;
    m_wai_waiting = read8() != 0;
    m_stp_stopped = read8() != 0;
}

} // namespace snes
