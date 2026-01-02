#include "arm7tdmi.hpp"
#include "bus.hpp"
#include "debug.hpp"
#include <cstring>
#include <iostream>
#include <cmath>
#include <vector>

namespace gba {

ARM7TDMI::ARM7TDMI(Bus& bus) : m_bus(bus) {
    reset();
}

ARM7TDMI::~ARM7TDMI() = default;

void ARM7TDMI::reset() {
    // Clear all registers
    m_regs.fill(0);

    // Clear banked registers
    m_fiq_regs.fill(0);
    m_svc_regs.fill(0);
    m_abt_regs.fill(0);
    m_irq_regs.fill(0);
    m_und_regs.fill(0);
    m_usr_regs.fill(0);     // R8-R12 shared by User/System mode
    m_usr_sp_lr.fill(0);    // R13-R14 for User/System mode

    // Reset CPSR: ARM state, IRQ/FIQ disabled, Supervisor mode
    m_cpsr = FLAG_I | FLAG_F | static_cast<uint32_t>(ProcessorMode::Supervisor);
    m_mode = ProcessorMode::Supervisor;

    // Clear SPSRs
    m_spsr_fiq = 0;
    m_spsr_svc = 0;
    m_spsr_abt = 0;
    m_spsr_irq = 0;
    m_spsr_und = 0;

    // Set PC to reset vector
    // GBA BIOS is at 0x00000000, it will jump to cartridge
    m_regs[15] = 0x08000000;  // Start at ROM for now (skip BIOS)

    // Initialize stack pointers for each mode
    // These values match what the GBA BIOS would set up
    m_irq_regs[0] = 0x03007FA0;      // IRQ stack
    m_svc_regs[0] = 0x03007FE0;      // Supervisor stack
    m_regs[13] = 0x03007FE0;         // Current SP (in SVC mode)
    m_usr_sp_lr[0] = 0x03007F00;     // User/System stack

    // Clear pipeline
    m_pipeline[0] = 0;
    m_pipeline[1] = 0;
    m_pipeline_valid = 0;

    // Clear interrupt state
    m_irq_pending = false;
    m_halted = false;
}

int ARM7TDMI::step() {
    // Check for pending IRQ
    if (m_irq_pending && !(m_cpsr & FLAG_I)) {
        // HLE IRQ handling - simulate what the GBA BIOS does:
        // 1. Read handler address from 0x03007FFC
        // 2. If handler is valid (in executable memory), call it
        // 3. If handler is invalid, wait for handler to be set up
        uint32_t handler_addr = read32(0x03007FFC);

        // Validate handler address is in a valid executable region AND has code:
        // - 0x02xxxxxx: EWRAM (256KB)
        // - 0x03xxxxxx: IWRAM (32KB)
        // - 0x08xxxxxx-0x0Dxxxxxx: ROM
        // Also check that the handler has actual code (not just zeros).
        // Games set up the handler pointer before writing the handler code,
        // so we need to wait for the code to be written.
        bool valid_handler = false;
        uint8_t region = (handler_addr >> 24) & 0xFF;
        if (region == 0x02 || region == 0x03 ||
            (region >= 0x08 && region <= 0x0D)) {
            // Check if handler has actual code (first instruction isn't 0)
            uint32_t handler_instr = read32(handler_addr & ~3u);
            if (handler_instr != 0 && handler_instr != 0xFFFFFFFF) {
                valid_handler = true;
            }
        }

        if (!valid_handler) {
            // No valid handler yet - clear pending and continue executing
            m_irq_pending = false;
        } else {
            // Enter IRQ mode and jump to BIOS IRQ vector at 0x00000018
            // The HLE BIOS code there will save registers, call the user's
            // handler from 0x03FFFFFC, restore registers, and return from IRQ
            enter_exception(ProcessorMode::IRQ, VECTOR_IRQ);
            m_irq_pending = false;
            return 3;
        }
    }

    // If halted, just return 1 cycle
    if (m_halted) {
        return 1;
    }

    // Execute based on current state
    if (m_cpsr & FLAG_T) {
        // Thumb mode
        uint16_t instruction = fetch_thumb();
        return execute_thumb(instruction);
    } else {
        // ARM mode
        uint32_t instruction = fetch_arm();
        return execute_arm(instruction);
    }
}

void ARM7TDMI::signal_irq() {
    m_irq_pending = true;
    m_halted = false;  // IRQ wakes from halt
}

uint8_t ARM7TDMI::read8(uint32_t address) {
    return m_bus.read8(address);
}

uint16_t ARM7TDMI::read16(uint32_t address) {
    address &= ~1u;  // Force alignment
    return m_bus.read16(address);
}

uint32_t ARM7TDMI::read32(uint32_t address) {
    address &= ~3u;  // Force alignment
    return m_bus.read32(address);
}

void ARM7TDMI::write8(uint32_t address, uint8_t value) {
    m_bus.write8(address, value);
}

void ARM7TDMI::write16(uint32_t address, uint16_t value) {
    // Use unaligned write for correct SRAM byte selection
    m_bus.write16_unaligned(address, value);
}

void ARM7TDMI::write32(uint32_t address, uint32_t value) {
    // Use unaligned write for correct SRAM byte selection
    m_bus.write32_unaligned(address, value);
}

uint32_t ARM7TDMI::fetch_arm() {
    uint32_t pc = m_regs[15];
    uint32_t instruction = read32(pc);
    m_regs[15] += 4;
    return instruction;
}

uint16_t ARM7TDMI::fetch_thumb() {
    uint32_t pc = m_regs[15];
    uint16_t instruction = read16(pc);
    m_regs[15] += 2;
    return instruction;
}

void ARM7TDMI::flush_pipeline() {
    m_pipeline_valid = 0;
    // PC is already pointing to the instruction after the branch target
    // ARM: PC + 8 from current instruction
    // Thumb: PC + 4 from current instruction
}

bool ARM7TDMI::check_condition(uint32_t instruction) {
    Condition cond = static_cast<Condition>((instruction >> 28) & 0xF);

    bool n = (m_cpsr & FLAG_N) != 0;
    bool z = (m_cpsr & FLAG_Z) != 0;
    bool c = (m_cpsr & FLAG_C) != 0;
    bool v = (m_cpsr & FLAG_V) != 0;

    switch (cond) {
        case Condition::EQ: return z;
        case Condition::NE: return !z;
        case Condition::CS: return c;
        case Condition::CC: return !c;
        case Condition::MI: return n;
        case Condition::PL: return !n;
        case Condition::VS: return v;
        case Condition::VC: return !v;
        case Condition::HI: return c && !z;
        case Condition::LS: return !c || z;
        case Condition::GE: return n == v;
        case Condition::LT: return n != v;
        case Condition::GT: return !z && (n == v);
        case Condition::LE: return z || (n != v);
        case Condition::AL: return true;
        case Condition::NV: return false;  // Reserved/never
    }
    return false;
}

int ARM7TDMI::execute_arm(uint32_t instruction) {
    // Check condition first
    if (!check_condition(instruction)) {
        return 1;  // 1 cycle for skipped instruction
    }

    // Decode instruction class
    uint32_t op = (instruction >> 25) & 0x7;
    uint32_t op2 = (instruction >> 4) & 0xF;

    // Decode based on bits [27:25] and [7:4]
    switch (op) {
        case 0b000:
            if ((instruction & 0x0FFFFFF0) == 0x012FFF10) {
                return arm_branch_exchange(instruction);
            }
            if ((op2 & 0x9) == 0x9) {
                if ((instruction & 0x0FC000F0) == 0x00000090) {
                    return arm_multiply(instruction);
                }
                if ((instruction & 0x0F8000F0) == 0x00800090) {
                    return arm_multiply_long(instruction);
                }
                if ((instruction & 0x0FB00FF0) == 0x01000090) {
                    return arm_swap(instruction);
                }
                return arm_halfword_data_transfer(instruction);
            }
            if ((instruction & 0x0FBF0FFF) == 0x010F0000) {
                return arm_mrs(instruction);
            }
            if ((instruction & 0x0DB0F000) == 0x0120F000) {
                return arm_msr(instruction);
            }
            return arm_data_processing(instruction);

        case 0b001:
            if ((instruction & 0x0FBF0FFF) == 0x010F0000) {
                return arm_mrs(instruction);
            }
            if ((instruction & 0x0DB0F000) == 0x0120F000) {
                return arm_msr(instruction);
            }
            return arm_data_processing(instruction);

        case 0b010:
        case 0b011:
            if (op == 0b011 && (instruction & 0x10)) {
                return arm_undefined(instruction);
            }
            return arm_single_data_transfer(instruction);

        case 0b100:
            return arm_block_data_transfer(instruction);

        case 0b101:
            return arm_branch(instruction);

        case 0b110:
            // Coprocessor data transfer - not used on GBA
            return 1;

        case 0b111:
            if (instruction & (1 << 24)) {
                return arm_software_interrupt(instruction);
            }
            // Coprocessor operations - not used on GBA
            return 1;
    }

    return arm_undefined(instruction);
}

uint32_t ARM7TDMI::arm_shift(uint32_t value, int shift_type, int amount, bool& carry_out, bool reg_shift) {
    carry_out = (m_cpsr & FLAG_C) != 0;

    if (amount == 0 && !reg_shift) {
        // Special cases for immediate shift amount of 0
        switch (shift_type) {
            case 0:  // LSL #0 - no shift
                return value;
            case 1:  // LSR #0 means LSR #32
                carry_out = (value >> 31) & 1;
                return 0;
            case 2:  // ASR #0 means ASR #32
                carry_out = (value >> 31) & 1;
                return carry_out ? 0xFFFFFFFF : 0;
            case 3:  // ROR #0 means RRX (rotate right extended)
                carry_out = value & 1;
                return ((m_cpsr & FLAG_C) ? (1u << 31) : 0) | (value >> 1);
        }
    }

    if (amount == 0) {
        return value;
    }

    switch (shift_type) {
        case 0:  // LSL
            if (amount >= 32) {
                carry_out = (amount == 32) ? (value & 1) : false;
                return 0;
            }
            carry_out = (value >> (32 - amount)) & 1;
            return value << amount;

        case 1:  // LSR
            if (amount >= 32) {
                carry_out = (amount == 32) ? ((value >> 31) & 1) : false;
                return 0;
            }
            carry_out = (value >> (amount - 1)) & 1;
            return value >> amount;

        case 2:  // ASR
            if (amount >= 32) {
                carry_out = (value >> 31) & 1;
                return carry_out ? 0xFFFFFFFF : 0;
            }
            carry_out = (value >> (amount - 1)) & 1;
            return static_cast<uint32_t>(static_cast<int32_t>(value) >> amount);

        case 3:  // ROR
            amount &= 31;
            if (amount == 0) {
                carry_out = (value >> 31) & 1;
                return value;
            }
            carry_out = (value >> (amount - 1)) & 1;
            return ror(value, amount);
    }

    return value;
}

int ARM7TDMI::arm_branch(uint32_t instruction) {
    bool link = (instruction >> 24) & 1;
    int32_t offset = sign_extend_24(instruction & 0x00FFFFFF) << 2;

    if (link) {
        // Return address is the instruction after the branch
        // After fetch, PC is at instruction + 4, so return addr = PC
        m_regs[14] = m_regs[15];
    }

    // After fetch, PC = instruction_address + 4
    // ARM branch formula expects PC = instruction_address + 8
    // So we need to add an extra 4 to compensate
    m_regs[15] += offset + 4;
    flush_pipeline();
    return 3;  // Branch takes 3 cycles
}

int ARM7TDMI::arm_branch_exchange(uint32_t instruction) {
    uint32_t rn = instruction & 0xF;
    uint32_t addr = m_regs[rn];

    // Switch to Thumb if bit 0 is set
    if (addr & 1) {
        m_cpsr |= FLAG_T;
        m_regs[15] = addr & ~1u;
    } else {
        m_cpsr &= ~FLAG_T;
        m_regs[15] = addr & ~3u;
    }

    flush_pipeline();
    return 3;
}

int ARM7TDMI::arm_data_processing(uint32_t instruction) {
    uint32_t opcode = (instruction >> 21) & 0xF;
    bool set_flags = (instruction >> 20) & 1;
    uint32_t rn = (instruction >> 16) & 0xF;
    uint32_t rd = (instruction >> 12) & 0xF;

    // Get second operand (with shift/immediate)
    // We need to process this first to know if it's a register shift (affects PC reading)
    uint32_t op2;
    bool carry_out = (m_cpsr & FLAG_C) != 0;
    bool reg_shift = false;

    if (instruction & (1 << 25)) {
        // Immediate operand
        uint32_t imm = instruction & 0xFF;
        int rotate = ((instruction >> 8) & 0xF) * 2;
        op2 = ror(imm, rotate);
        if (rotate != 0) {
            carry_out = (op2 >> 31) & 1;
        }
    } else {
        // Register operand with shift
        uint32_t rm = instruction & 0xF;
        int shift_type = (instruction >> 5) & 3;
        int shift_amount;

        if (instruction & (1 << 4)) {
            // Shift by register
            reg_shift = true;
            uint32_t rs = (instruction >> 8) & 0xF;
            shift_amount = m_regs[rs] & 0xFF;
        } else {
            // Shift by immediate
            shift_amount = (instruction >> 7) & 0x1F;
        }

        uint32_t shift_val = m_regs[rm];
        if (rm == 15) {
            // When reading PC: normally +8, but if using register shift, it's +12
            shift_val += reg_shift ? 8 : 4;  // After fetch we have +4, so add 4 or 8
        }

        op2 = arm_shift(shift_val, shift_type, shift_amount, carry_out, reg_shift);
    }

    // Get first operand (Rn)
    uint32_t op1 = m_regs[rn];
    if (rn == 15) {
        // When reading PC: normally +8, but if using register shift, it's +12
        op1 += reg_shift ? 8 : 4;  // After fetch we have +4, so add 4 or 8
    }

    // Execute operation
    uint32_t result = 0;
    bool write_result = true;
    bool overflow = false;

    switch (opcode) {
        case 0x0:  // AND
            result = op1 & op2;
            break;
        case 0x1:  // EOR
            result = op1 ^ op2;
            break;
        case 0x2:  // SUB
            result = op1 - op2;
            carry_out = op1 >= op2;
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            break;
        case 0x3:  // RSB
            result = op2 - op1;
            carry_out = op2 >= op1;
            overflow = ((op2 ^ op1) & (op2 ^ result)) >> 31;
            break;
        case 0x4:  // ADD
            result = op1 + op2;
            carry_out = result < op1;
            overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
            break;
        case 0x5:  // ADC
            {
                uint64_t temp = static_cast<uint64_t>(op1) + op2 + ((m_cpsr & FLAG_C) ? 1 : 0);
                result = static_cast<uint32_t>(temp);
                carry_out = temp > 0xFFFFFFFF;
                overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
            }
            break;
        case 0x6:  // SBC
            {
                uint32_t borrow = (m_cpsr & FLAG_C) ? 0 : 1;
                result = op1 - op2 - borrow;
                carry_out = static_cast<uint64_t>(op1) >= (static_cast<uint64_t>(op2) + borrow);
                overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            }
            break;
        case 0x7:  // RSC
            {
                uint32_t borrow = (m_cpsr & FLAG_C) ? 0 : 1;
                result = op2 - op1 - borrow;
                carry_out = static_cast<uint64_t>(op2) >= (static_cast<uint64_t>(op1) + borrow);
                overflow = ((op2 ^ op1) & (op2 ^ result)) >> 31;
            }
            break;
        case 0x8:  // TST
            result = op1 & op2;
            write_result = false;
            break;
        case 0x9:  // TEQ
            result = op1 ^ op2;
            write_result = false;
            break;
        case 0xA:  // CMP
            result = op1 - op2;
            carry_out = op1 >= op2;
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            write_result = false;
            break;
        case 0xB:  // CMN
            result = op1 + op2;
            carry_out = result < op1;
            overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
            write_result = false;
            break;
        case 0xC:  // ORR
            result = op1 | op2;
            break;
        case 0xD:  // MOV
            result = op2;
            break;
        case 0xE:  // BIC
            result = op1 & ~op2;
            break;
        case 0xF:  // MVN
            result = ~op2;
            break;
    }

    // Write result
    if (write_result) {
        m_regs[rd] = result;
        if (rd == 15) {
            if (set_flags) {
                // Restore CPSR from SPSR
                set_cpsr(get_spsr());
            }
            flush_pipeline();
        }
    } else if (rd == 15 && set_flags) {
        // For TST/TEQ/CMP/CMN with Rd=15 and S=1:
        // Even though no result is written, CPSR is still restored from SPSR
        // This is the "TEQP/CMPP/TSTP/CMNP" behavior
        set_cpsr(get_spsr());
        // Note: no pipeline flush for these cases as PC is not modified
    }

    // Update flags
    if (set_flags && rd != 15) {
        set_nzcv_flags(result, carry_out, overflow);
    }

    return (rd == 15 && write_result) ? 3 : 1;
}

int ARM7TDMI::arm_multiply(uint32_t instruction) {
    bool accumulate = (instruction >> 21) & 1;
    bool set_flags = (instruction >> 20) & 1;
    uint32_t rd = (instruction >> 16) & 0xF;
    uint32_t rn = (instruction >> 12) & 0xF;
    uint32_t rs = (instruction >> 8) & 0xF;
    uint32_t rm = instruction & 0xF;

    uint32_t result = m_regs[rm] * m_regs[rs];
    if (accumulate) {
        result += m_regs[rn];
    }

    m_regs[rd] = result;

    if (set_flags) {
        set_nz_flags(result);
        // C flag is destroyed (unpredictable)
    }

    // Multiply timing depends on operand values
    // Simplified: 2-4 cycles
    return 3;
}

int ARM7TDMI::arm_multiply_long(uint32_t instruction) {
    bool sign = (instruction >> 22) & 1;
    bool accumulate = (instruction >> 21) & 1;
    bool set_flags = (instruction >> 20) & 1;
    uint32_t rdhi = (instruction >> 16) & 0xF;
    uint32_t rdlo = (instruction >> 12) & 0xF;
    uint32_t rs = (instruction >> 8) & 0xF;
    uint32_t rm = instruction & 0xF;

    uint64_t result;
    if (sign) {
        result = static_cast<int64_t>(static_cast<int32_t>(m_regs[rm])) *
                 static_cast<int64_t>(static_cast<int32_t>(m_regs[rs]));
    } else {
        result = static_cast<uint64_t>(m_regs[rm]) * static_cast<uint64_t>(m_regs[rs]);
    }

    if (accumulate) {
        uint64_t acc = (static_cast<uint64_t>(m_regs[rdhi]) << 32) | m_regs[rdlo];
        result += acc;
    }

    m_regs[rdlo] = static_cast<uint32_t>(result);
    m_regs[rdhi] = static_cast<uint32_t>(result >> 32);

    if (set_flags) {
        m_cpsr &= ~(FLAG_N | FLAG_Z);
        if (result == 0) m_cpsr |= FLAG_Z;
        if (result & (1ULL << 63)) m_cpsr |= FLAG_N;
    }

    return 4;  // Long multiply takes 3-5 cycles
}

int ARM7TDMI::arm_single_data_transfer(uint32_t instruction) {
    bool immediate = !((instruction >> 25) & 1);
    bool pre = (instruction >> 24) & 1;
    bool up = (instruction >> 23) & 1;
    bool byte = (instruction >> 22) & 1;
    bool writeback = (instruction >> 21) & 1;
    bool load = (instruction >> 20) & 1;
    uint32_t rn = (instruction >> 16) & 0xF;
    uint32_t rd = (instruction >> 12) & 0xF;

    // Calculate offset
    uint32_t offset;
    if (immediate) {
        offset = instruction & 0xFFF;
    } else {
        uint32_t rm = instruction & 0xF;
        int shift_type = (instruction >> 5) & 3;
        int shift_amount = (instruction >> 7) & 0x1F;
        bool carry;
        offset = arm_shift(m_regs[rm], shift_type, shift_amount, carry, false);
    }

    // Calculate address
    uint32_t base = m_regs[rn];
    if (rn == 15) base += 4;  // After fetch, PC is at instruction+4, ARM expects +8

    uint32_t addr = pre ? (up ? base + offset : base - offset) : base;

    // Perform transfer
    if (load) {
        if (byte) {
            m_regs[rd] = read8(addr);
        } else {
            m_regs[rd] = read32(addr);
            // Handle misaligned loads
            if (addr & 3) {
                m_regs[rd] = ror(m_regs[rd], (addr & 3) * 8);
            }
        }
        if (rd == 15) {
            flush_pipeline();
        }
    } else {
        uint32_t value = m_regs[rd];
        if (rd == 15) value += 8;  // STR PC stores instruction_address + 12, we have +4, so add 8
        if (byte) {
            write8(addr, static_cast<uint8_t>(value));
        } else {
            write32(addr, value);
        }
    }

    // Writeback
    if (!pre || writeback) {
        if (!pre) {
            addr = up ? base + offset : base - offset;
        }
        // For loads with Rn == Rd, the loaded value takes precedence (no writeback)
        // For stores with Rn == Rd, writeback still happens
        if (!load || rn != rd) {
            m_regs[rn] = addr;
        }
    }

    return load ? 3 : 2;
}

int ARM7TDMI::arm_halfword_data_transfer(uint32_t instruction) {
    bool pre = (instruction >> 24) & 1;
    bool up = (instruction >> 23) & 1;
    bool immediate = (instruction >> 22) & 1;
    bool writeback = (instruction >> 21) & 1;
    bool load = (instruction >> 20) & 1;
    uint32_t rn = (instruction >> 16) & 0xF;
    uint32_t rd = (instruction >> 12) & 0xF;
    uint32_t op = (instruction >> 5) & 3;

    // Calculate offset
    uint32_t offset;
    if (immediate) {
        offset = ((instruction >> 4) & 0xF0) | (instruction & 0xF);
    } else {
        offset = m_regs[instruction & 0xF];
    }

    // Calculate address
    uint32_t base = m_regs[rn];
    if (rn == 15) base += 4;  // After fetch, PC is at instruction+4, ARM expects +8

    uint32_t addr = pre ? (up ? base + offset : base - offset) : base;

    // Perform transfer
    if (load) {
        switch (op) {
            case 1:  // LDRH - unsigned halfword
                m_regs[rd] = read16(addr);
                // Misaligned halfword load rotates the value by 8 bits
                if (addr & 1) {
                    m_regs[rd] = ror(m_regs[rd], 8);
                }
                break;
            case 2:  // LDRSB - signed byte
                m_regs[rd] = static_cast<uint32_t>(sign_extend_8(read8(addr)));
                break;
            case 3:  // LDRSH - signed halfword
                // Misaligned LDRSH reads a byte and sign-extends it
                if (addr & 1) {
                    m_regs[rd] = static_cast<uint32_t>(sign_extend_8(read8(addr)));
                } else {
                    m_regs[rd] = static_cast<uint32_t>(sign_extend_16(read16(addr)));
                }
                break;
        }
    } else {
        // STRH
        write16(addr, static_cast<uint16_t>(m_regs[rd]));
    }

    // Writeback
    if (!pre || writeback) {
        if (!pre) {
            addr = up ? base + offset : base - offset;
        }
        // For loads with Rn == Rd, the loaded value takes precedence (no writeback)
        // For stores with Rn == Rd, writeback still happens
        if (!load || rn != rd) {
            m_regs[rn] = addr;
        }
    }

    return load ? 3 : 2;
}

int ARM7TDMI::arm_block_data_transfer(uint32_t instruction) {
    bool pre = (instruction >> 24) & 1;
    bool up = (instruction >> 23) & 1;
    bool psr = (instruction >> 22) & 1;
    bool writeback = (instruction >> 21) & 1;
    bool load = (instruction >> 20) & 1;
    uint32_t rn = (instruction >> 16) & 0xF;
    uint16_t reg_list = instruction & 0xFFFF;

    uint32_t base = m_regs[rn];
    int reg_count = __builtin_popcount(reg_list);

    // Handle empty register list (undocumented ARM7 behavior)
    // When reg_list is empty:
    // - Loads/stores R15 only at address calculated same as if 16 registers were transferred
    // - Base is adjusted by 0x40 (64 bytes)
    if (reg_count == 0) {
        uint32_t addr;
        if (up) {
            addr = pre ? base + 4 : base;
        } else {
            addr = pre ? base - 0x40 : base - 0x3C;  // -64 or -60
        }

        if (load) {
            m_regs[15] = read32(addr);
            flush_pipeline();
        } else {
            uint32_t value = m_regs[15] + 8;  // STM PC stores instruction_address + 12
            write32(addr, value);
        }
        if (writeback) {
            if (up) {
                m_regs[rn] = base + 0x40;
            } else {
                m_regs[rn] = base - 0x40;
            }
        }
        return 3;
    }

    // Calculate start address
    uint32_t addr;
    if (up) {
        addr = pre ? base + 4 : base;
    } else {
        addr = pre ? base - reg_count * 4 : base - reg_count * 4 + 4;
    }

    // When S bit is set and R15 is NOT in the register list:
    // - For STM: Store user mode registers
    // - For LDM: Load user mode registers
    bool user_regs = psr && !(reg_list & (1 << 15));

    // Transfer registers
    bool first = true;
    for (int i = 0; i < 16; i++) {
        if (reg_list & (1 << i)) {
            if (load) {
                uint32_t value = read32(addr);
                if (user_regs && i >= 8 && i <= 14) {
                    // Store to user bank
                    if (i <= 12) {
                        m_usr_regs[i - 8] = value;
                    } else {
                        m_usr_sp_lr[i - 13] = value;
                    }
                } else {
                    m_regs[i] = value;
                    if (i == 15) {
                        if (psr) {
                            set_cpsr(get_spsr());
                        }
                        flush_pipeline();
                    }
                }
            } else {
                uint32_t value;
                if (user_regs && i >= 8 && i <= 14) {
                    // Load from user bank
                    if (i <= 12) {
                        value = m_usr_regs[i - 8];
                    } else {
                        value = m_usr_sp_lr[i - 13];
                    }
                } else {
                    value = m_regs[i];
                    if (i == 15) value += 8;  // STM PC stores instruction_address + 12, we have +4, so add 8
                }
                write32(addr, value);
            }
            addr += 4;

            // Early writeback for first register (for correct abort behavior)
            if (first && writeback && !load) {
                if (up) {
                    m_regs[rn] = base + reg_count * 4;
                } else {
                    m_regs[rn] = base - reg_count * 4;
                }
            }
            first = false;
        }
    }

    // Writeback
    // For loads, if the base register is in the register list, the loaded value takes precedence
    bool base_in_list = (reg_list & (1 << rn)) != 0;
    if (writeback && load && !base_in_list) {
        if (up) {
            m_regs[rn] = base + reg_count * 4;
        } else {
            m_regs[rn] = base - reg_count * 4;
        }
    }

    return reg_count + (load ? 2 : 1);
}

int ARM7TDMI::arm_swap(uint32_t instruction) {
    bool byte = (instruction >> 22) & 1;
    uint32_t rn = (instruction >> 16) & 0xF;
    uint32_t rd = (instruction >> 12) & 0xF;
    uint32_t rm = instruction & 0xF;

    uint32_t addr = m_regs[rn];

    if (byte) {
        uint8_t temp = read8(addr);
        write8(addr, static_cast<uint8_t>(m_regs[rm]));
        m_regs[rd] = temp;
    } else {
        uint32_t temp = read32(addr);
        // Handle misaligned word swap - rotate like LDR
        if (addr & 3) {
            temp = ror(temp, (addr & 3) * 8);
        }
        write32(addr, m_regs[rm]);
        m_regs[rd] = temp;
    }

    return 4;
}

int ARM7TDMI::arm_software_interrupt(uint32_t instruction) {
    // GBA uses the comment field bits [23:16] for the function number in ARM mode
    uint8_t function = (instruction >> 16) & 0xFF;
    hle_bios_call(function);
    return 3;
}

int ARM7TDMI::arm_mrs(uint32_t instruction) {
    bool spsr = (instruction >> 22) & 1;
    uint32_t rd = (instruction >> 12) & 0xF;

    m_regs[rd] = spsr ? get_spsr() : m_cpsr;
    return 1;
}

int ARM7TDMI::arm_msr(uint32_t instruction) {
    bool spsr = (instruction >> 22) & 1;
    bool immediate = (instruction >> 25) & 1;
    uint32_t field_mask = (instruction >> 16) & 0xF;

    uint32_t value;
    if (immediate) {
        value = instruction & 0xFF;
        int rotate = ((instruction >> 8) & 0xF) * 2;
        value = ror(value, rotate);
    } else {
        value = m_regs[instruction & 0xF];
    }

    // Build mask from field bits
    uint32_t mask = 0;
    if (field_mask & 1) mask |= 0x000000FF;  // Control
    if (field_mask & 2) mask |= 0x0000FF00;  // Extension
    if (field_mask & 4) mask |= 0x00FF0000;  // Status
    if (field_mask & 8) mask |= 0xFF000000;  // Flags

    // In User mode, can only modify flags
    if (m_mode == ProcessorMode::User) {
        mask &= 0xF0000000;
    }

    if (spsr) {
        uint32_t spsr_val = get_spsr();
        set_spsr((spsr_val & ~mask) | (value & mask));
    } else {
        set_cpsr((m_cpsr & ~mask) | (value & mask));
    }

    return 1;
}

int ARM7TDMI::arm_undefined(uint32_t instruction) {
    (void)instruction;
    enter_exception(ProcessorMode::Undefined, VECTOR_UNDEFINED);
    return 3;
}

// Thumb instruction execution
int ARM7TDMI::execute_thumb(uint16_t instruction) {
    // Decode based on upper bits
    uint16_t op = instruction >> 13;

    switch (op) {
        case 0b000:
            if ((instruction & 0x1800) == 0x1800) {
                return thumb_add_subtract(instruction);
            }
            return thumb_move_shifted(instruction);

        case 0b001:
            return thumb_immediate(instruction);

        case 0b010:
            // Check for PC-relative load first (0x4800-0x4FFF, bit 11 set)
            if ((instruction & 0x1800) == 0x0800) {
                return thumb_pc_relative_load(instruction);
            }
            // Then check for ALU / hi-reg operations (0x4000-0x47FF)
            if ((instruction & 0x1000) == 0) {
                if ((instruction & 0x0C00) == 0x0000) {
                    return thumb_alu(instruction);
                }
                return thumb_hi_reg_bx(instruction);
            }
            // Load/store with register offset (0x5000-0x5FFF)
            if ((instruction & 0x0200) == 0) {
                return thumb_load_store_reg(instruction);
            }
            return thumb_load_store_sign(instruction);

        case 0b011:
            return thumb_load_store_imm(instruction);

        case 0b100:
            if ((instruction & 0x1000) == 0) {
                return thumb_load_store_half(instruction);
            }
            return thumb_sp_relative_load_store(instruction);

        case 0b101:
            if ((instruction & 0x1000) == 0) {
                return thumb_load_address(instruction);
            }
            if ((instruction & 0x0F00) == 0x0000) {
                return thumb_add_sp(instruction);
            }
            return thumb_push_pop(instruction);

        case 0b110:
            if ((instruction & 0x1000) == 0) {
                return thumb_multiple_load_store(instruction);
            }
            if ((instruction & 0x0F00) == 0x0F00) {
                return thumb_software_interrupt(instruction);
            }
            return thumb_conditional_branch(instruction);

        case 0b111:
            if ((instruction & 0x1800) == 0x0000) {
                return thumb_unconditional_branch(instruction);
            }
            return thumb_long_branch(instruction);
    }

    return 1;
}

int ARM7TDMI::thumb_move_shifted(uint16_t instruction) {
    uint16_t op = (instruction >> 11) & 3;
    uint16_t offset = (instruction >> 6) & 0x1F;
    uint16_t rs = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t value = m_regs[rs];
    bool carry = (m_cpsr & FLAG_C) != 0;

    switch (op) {
        case 0:  // LSL
            if (offset > 0) {
                carry = (value >> (32 - offset)) & 1;
                value <<= offset;
            }
            break;
        case 1:  // LSR
            if (offset == 0) offset = 32;
            carry = (value >> (offset - 1)) & 1;
            value = (offset < 32) ? (value >> offset) : 0;
            break;
        case 2:  // ASR
            if (offset == 0) offset = 32;
            carry = (value >> (offset - 1)) & 1;
            value = static_cast<uint32_t>(asr(static_cast<int32_t>(value), offset));
            break;
    }

    m_regs[rd] = value;
    set_nzc_flags(value, carry);
    return 1;
}

int ARM7TDMI::thumb_add_subtract(uint16_t instruction) {
    bool immediate = (instruction >> 10) & 1;
    bool subtract = (instruction >> 9) & 1;
    uint16_t rn_imm = (instruction >> 6) & 7;
    uint16_t rs = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t op1 = m_regs[rs];
    uint32_t op2 = immediate ? rn_imm : m_regs[rn_imm];
    uint32_t result;
    bool carry, overflow;

    if (subtract) {
        result = op1 - op2;
        carry = op1 >= op2;
        overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    } else {
        result = op1 + op2;
        carry = result < op1;
        overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
    }

    m_regs[rd] = result;
    set_nzcv_flags(result, carry, overflow);
    return 1;
}

int ARM7TDMI::thumb_immediate(uint16_t instruction) {
    uint16_t op = (instruction >> 11) & 3;
    uint16_t rd = (instruction >> 8) & 7;
    uint8_t imm = instruction & 0xFF;

    uint32_t value = m_regs[rd];
    uint32_t result;
    bool carry = (m_cpsr & FLAG_C) != 0;
    bool overflow = false;

    switch (op) {
        case 0:  // MOV
            result = imm;
            break;
        case 1:  // CMP
            result = value - imm;
            carry = value >= imm;
            overflow = ((value ^ imm) & (value ^ result)) >> 31;
            set_nzcv_flags(result, carry, overflow);
            return 1;  // Don't write result for CMP
        case 2:  // ADD
            result = value + imm;
            carry = result < value;
            overflow = (~(value ^ imm) & (value ^ result)) >> 31;
            break;
        case 3:  // SUB
            result = value - imm;
            carry = value >= imm;
            overflow = ((value ^ imm) & (value ^ result)) >> 31;
            break;
        default:
            result = 0;
    }

    m_regs[rd] = result;
    set_nzcv_flags(result, carry, overflow);
    return 1;
}

int ARM7TDMI::thumb_alu(uint16_t instruction) {
    uint16_t op = (instruction >> 6) & 0xF;
    uint16_t rs = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t a = m_regs[rd];
    uint32_t b = m_regs[rs];
    uint32_t result;
    bool carry = (m_cpsr & FLAG_C) != 0;
    bool overflow = (m_cpsr & FLAG_V) != 0;

    switch (op) {
        case 0x0:  // AND
            result = a & b;
            break;
        case 0x1:  // EOR
            result = a ^ b;
            break;
        case 0x2:  // LSL
            b &= 0xFF;
            if (b == 0) {
                result = a;
            } else if (b < 32) {
                carry = (a >> (32 - b)) & 1;
                result = a << b;
            } else if (b == 32) {
                carry = a & 1;
                result = 0;
            } else {
                carry = false;
                result = 0;
            }
            break;
        case 0x3:  // LSR
            b &= 0xFF;
            if (b == 0) {
                result = a;
            } else if (b < 32) {
                carry = (a >> (b - 1)) & 1;
                result = a >> b;
            } else if (b == 32) {
                carry = (a >> 31) & 1;
                result = 0;
            } else {
                carry = false;
                result = 0;
            }
            break;
        case 0x4:  // ASR
            b &= 0xFF;
            if (b == 0) {
                result = a;
            } else if (b < 32) {
                carry = (a >> (b - 1)) & 1;
                result = static_cast<uint32_t>(static_cast<int32_t>(a) >> b);
            } else {
                carry = (a >> 31) & 1;
                result = carry ? 0xFFFFFFFF : 0;
            }
            break;
        case 0x5:  // ADC
            {
                uint64_t temp = static_cast<uint64_t>(a) + b + (carry ? 1 : 0);
                result = static_cast<uint32_t>(temp);
                carry = temp > 0xFFFFFFFF;
                overflow = (~(a ^ b) & (a ^ result)) >> 31;
            }
            break;
        case 0x6:  // SBC
            {
                uint32_t borrow = carry ? 0 : 1;
                result = a - b - borrow;
                carry = static_cast<uint64_t>(a) >= (static_cast<uint64_t>(b) + borrow);
                overflow = ((a ^ b) & (a ^ result)) >> 31;
            }
            break;
        case 0x7:  // ROR
            b &= 0xFF;
            if (b == 0) {
                result = a;
            } else {
                b &= 31;
                if (b == 0) {
                    carry = (a >> 31) & 1;
                    result = a;
                } else {
                    carry = (a >> (b - 1)) & 1;
                    result = ror(a, b);
                }
            }
            break;
        case 0x8:  // TST
            result = a & b;
            set_nzc_flags(result, carry);
            return 1;
        case 0x9:  // NEG
            result = 0 - b;
            carry = b == 0;
            overflow = ((0 ^ b) & (0 ^ result)) >> 31;
            break;
        case 0xA:  // CMP
            result = a - b;
            carry = a >= b;
            overflow = ((a ^ b) & (a ^ result)) >> 31;
            set_nzcv_flags(result, carry, overflow);
            return 1;
        case 0xB:  // CMN
            result = a + b;
            carry = result < a;
            overflow = (~(a ^ b) & (a ^ result)) >> 31;
            set_nzcv_flags(result, carry, overflow);
            return 1;
        case 0xC:  // ORR
            result = a | b;
            break;
        case 0xD:  // MUL
            result = a * b;
            // C flag is destroyed
            break;
        case 0xE:  // BIC
            result = a & ~b;
            break;
        case 0xF:  // MVN
            result = ~b;
            break;
        default:
            result = 0;
    }

    m_regs[rd] = result;
    set_nzcv_flags(result, carry, overflow);
    return (op == 0xD) ? 3 : 1;  // MUL takes extra cycles
}

int ARM7TDMI::thumb_hi_reg_bx(uint16_t instruction) {
    uint16_t op = (instruction >> 8) & 3;
    bool h1 = (instruction >> 7) & 1;
    bool h2 = (instruction >> 6) & 1;
    uint16_t rs = ((instruction >> 3) & 7) | (h2 ? 8 : 0);
    uint16_t rd = (instruction & 7) | (h1 ? 8 : 0);

    switch (op) {
        case 0:  // ADD
            m_regs[rd] = m_regs[rd] + m_regs[rs];
            if (rd == 15) {
                m_regs[15] &= ~1u;
                flush_pipeline();
            }
            break;
        case 1:  // CMP
            {
                uint32_t a = m_regs[rd];
                uint32_t b = m_regs[rs];
                uint32_t result = a - b;
                bool carry = a >= b;
                bool overflow = ((a ^ b) & (a ^ result)) >> 31;
                set_nzcv_flags(result, carry, overflow);
            }
            break;
        case 2:  // MOV
            m_regs[rd] = m_regs[rs];
            if (rd == 15) {
                m_regs[15] &= ~1u;
                flush_pipeline();
            }
            break;
        case 3:  // BX
            {
                uint32_t addr = m_regs[rs];
                if (addr & 1) {
                    m_cpsr |= FLAG_T;  // Switch to Thumb mode
                    m_regs[15] = addr & ~1u;
                } else {
                    m_cpsr &= ~FLAG_T;  // Switch to ARM mode
                    m_regs[15] = addr & ~3u;
                }
                flush_pipeline();
            }
            break;
    }

    return (op == 3 || rd == 15) ? 3 : 1;
}

int ARM7TDMI::thumb_pc_relative_load(uint16_t instruction) {
    uint16_t rd = (instruction >> 8) & 7;
    uint16_t offset = (instruction & 0xFF) << 2;

    // In Thumb mode, PC-relative uses (PC+4) & ~3, where PC is the instruction address
    // After fetch, m_regs[15] = instruction_address + 2
    // So we need (m_regs[15] + 2) & ~3 to get the aligned (PC+4) value
    uint32_t addr = ((m_regs[15] + 2) & ~3u) + offset;
    uint32_t value = read32(addr);

    m_regs[rd] = value;
    return 3;
}

int ARM7TDMI::thumb_load_store_reg(uint16_t instruction) {
    bool load = (instruction >> 11) & 1;
    bool byte = (instruction >> 10) & 1;
    uint16_t ro = (instruction >> 6) & 7;
    uint16_t rb = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t addr = m_regs[rb] + m_regs[ro];

    if (load) {
        if (byte) {
            m_regs[rd] = read8(addr);
        } else {
            m_regs[rd] = read32(addr);
        }
    } else {
        if (byte) {
            write8(addr, static_cast<uint8_t>(m_regs[rd]));
        } else {
            write32(addr, m_regs[rd]);
        }
    }

    return load ? 3 : 2;
}

int ARM7TDMI::thumb_load_store_sign(uint16_t instruction) {
    uint16_t op = (instruction >> 10) & 3;
    uint16_t ro = (instruction >> 6) & 7;
    uint16_t rb = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t addr = m_regs[rb] + m_regs[ro];

    switch (op) {
        case 0:  // STRH
            write16(addr, static_cast<uint16_t>(m_regs[rd]));
            return 2;
        case 1:  // LDSB
            m_regs[rd] = static_cast<uint32_t>(sign_extend_8(read8(addr)));
            return 3;
        case 2:  // LDRH
            m_regs[rd] = read16(addr);
            return 3;
        case 3:  // LDSH
            m_regs[rd] = static_cast<uint32_t>(sign_extend_16(read16(addr)));
            return 3;
    }

    return 1;
}

int ARM7TDMI::thumb_load_store_imm(uint16_t instruction) {
    bool byte = (instruction >> 12) & 1;
    bool load = (instruction >> 11) & 1;
    uint16_t offset = (instruction >> 6) & 0x1F;
    uint16_t rb = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t addr = m_regs[rb] + (byte ? offset : (offset << 2));

    if (load) {
        if (byte) {
            m_regs[rd] = read8(addr);
        } else {
            m_regs[rd] = read32(addr);
        }
    } else {
        if (byte) {
            write8(addr, static_cast<uint8_t>(m_regs[rd]));
        } else {
            write32(addr, m_regs[rd]);
        }
    }

    return load ? 3 : 2;
}

int ARM7TDMI::thumb_load_store_half(uint16_t instruction) {
    bool load = (instruction >> 11) & 1;
    uint16_t offset = ((instruction >> 6) & 0x1F) << 1;
    uint16_t rb = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t addr = m_regs[rb] + offset;

    if (load) {
        m_regs[rd] = read16(addr);
    } else {
        write16(addr, static_cast<uint16_t>(m_regs[rd]));
    }

    return load ? 3 : 2;
}

int ARM7TDMI::thumb_sp_relative_load_store(uint16_t instruction) {
    bool load = (instruction >> 11) & 1;
    uint16_t rd = (instruction >> 8) & 7;
    uint16_t offset = (instruction & 0xFF) << 2;

    uint32_t addr = m_regs[13] + offset;

    if (load) {
        m_regs[rd] = read32(addr);
    } else {
        write32(addr, m_regs[rd]);
    }

    return load ? 3 : 2;
}

int ARM7TDMI::thumb_load_address(uint16_t instruction) {
    bool sp = (instruction >> 11) & 1;
    uint16_t rd = (instruction >> 8) & 7;
    uint16_t offset = (instruction & 0xFF) << 2;

    if (sp) {
        m_regs[rd] = m_regs[13] + offset;
    } else {
        // ADD Rd, PC, #imm uses (PC+4) & ~3
        // After fetch, m_regs[15] = instruction_address + 2
        m_regs[rd] = ((m_regs[15] + 2) & ~3u) + offset;
    }

    return 1;
}

int ARM7TDMI::thumb_add_sp(uint16_t instruction) {
    bool negative = (instruction >> 7) & 1;
    uint16_t offset = (instruction & 0x7F) << 2;

    if (negative) {
        m_regs[13] -= offset;
    } else {
        m_regs[13] += offset;
    }

    return 1;
}

int ARM7TDMI::thumb_push_pop(uint16_t instruction) {
    bool load = (instruction >> 11) & 1;
    bool pc_lr = (instruction >> 8) & 1;
    uint8_t reg_list = instruction & 0xFF;

    int reg_count = __builtin_popcount(reg_list) + (pc_lr ? 1 : 0);

    if (load) {
        // POP
        uint32_t addr = m_regs[13];
        for (int i = 0; i < 8; i++) {
            if (reg_list & (1 << i)) {
                m_regs[i] = read32(addr);
                addr += 4;
            }
        }
        if (pc_lr) {
            m_regs[15] = read32(addr) & ~1u;
            addr += 4;
            flush_pipeline();
        }
        m_regs[13] = addr;
    } else {
        // PUSH
        uint32_t addr = m_regs[13] - reg_count * 4;
        m_regs[13] = addr;
        for (int i = 0; i < 8; i++) {
            if (reg_list & (1 << i)) {
                write32(addr, m_regs[i]);
                addr += 4;
            }
        }
        if (pc_lr) {
            write32(addr, m_regs[14]);
        }
    }

    return reg_count + (load ? 2 : 1);
}

int ARM7TDMI::thumb_multiple_load_store(uint16_t instruction) {
    bool load = (instruction >> 11) & 1;
    uint16_t rb = (instruction >> 8) & 7;
    uint8_t reg_list = instruction & 0xFF;

    int reg_count = __builtin_popcount(reg_list);
    if (reg_count == 0) reg_count = 1;  // Empty list behaves specially

    uint32_t addr = m_regs[rb];

    for (int i = 0; i < 8; i++) {
        if (reg_list & (1 << i)) {
            if (load) {
                m_regs[i] = read32(addr);
            } else {
                write32(addr, m_regs[i]);
            }
            addr += 4;
        }
    }

    // Writeback (unless Rb is in the list for load)
    if (!load || !(reg_list & (1 << rb))) {
        m_regs[rb] = addr;
    }

    return reg_count + (load ? 2 : 1);
}

int ARM7TDMI::thumb_conditional_branch(uint16_t instruction) {
    Condition cond = static_cast<Condition>((instruction >> 8) & 0xF);
    int8_t offset = static_cast<int8_t>(instruction & 0xFF);

    bool take_branch = false;
    bool n = (m_cpsr & FLAG_N) != 0;
    bool z = (m_cpsr & FLAG_Z) != 0;
    bool c = (m_cpsr & FLAG_C) != 0;
    bool v = (m_cpsr & FLAG_V) != 0;

    switch (cond) {
        case Condition::EQ: take_branch = z; break;
        case Condition::NE: take_branch = !z; break;
        case Condition::CS: take_branch = c; break;
        case Condition::CC: take_branch = !c; break;
        case Condition::MI: take_branch = n; break;
        case Condition::PL: take_branch = !n; break;
        case Condition::VS: take_branch = v; break;
        case Condition::VC: take_branch = !v; break;
        case Condition::HI: take_branch = c && !z; break;
        case Condition::LS: take_branch = !c || z; break;
        case Condition::GE: take_branch = (n == v); break;
        case Condition::LT: take_branch = (n != v); break;
        case Condition::GT: take_branch = !z && (n == v); break;
        case Condition::LE: take_branch = z || (n != v); break;
        default: break;
    }

    if (take_branch) {
        // Branch target = PC + 4 + offset * 2
        // After fetch, m_regs[15] = instruction_address + 2
        // So target = m_regs[15] + 2 + offset * 2
        m_regs[15] += 2 + static_cast<int32_t>(offset) * 2;
        flush_pipeline();
        return 3;
    }

    return 1;
}

int ARM7TDMI::thumb_software_interrupt(uint16_t instruction) {
    // GBA uses the comment field bits [7:0] for the function number in Thumb mode
    uint8_t function = instruction & 0xFF;
    hle_bios_call(function);
    return 3;
}

int ARM7TDMI::thumb_unconditional_branch(uint16_t instruction) {
    // Branch target = PC + 4 + offset
    // After fetch, m_regs[15] = instruction_address + 2
    // So target = m_regs[15] + 2 + offset
    int32_t offset = static_cast<int32_t>((instruction & 0x7FF) << 21) >> 20;
    m_regs[15] += 2 + offset;
    flush_pipeline();
    return 3;
}

int ARM7TDMI::thumb_long_branch(uint16_t instruction) {
    bool second = (instruction >> 11) & 1;
    uint16_t offset = instruction & 0x7FF;

    if (!second) {
        // First instruction: set up high bits of offset in LR
        // Uses PC+4 for calculation. After fetch, m_regs[15] = instruction_address + 2
        // So PC+4 = m_regs[15] + 2
        int32_t signed_offset = static_cast<int32_t>(offset << 21) >> 9;
        m_regs[14] = (m_regs[15] + 2) + signed_offset;
        return 1;
    } else {
        // Second instruction: complete the branch
        // Return address is the instruction after this one (current PC after fetch = instruction + 2)
        // LR should have bit 0 set to indicate Thumb mode
        uint32_t next_pc = m_regs[15] | 1;
        m_regs[15] = m_regs[14] + (offset << 1);
        m_regs[14] = next_pc;

        flush_pipeline();
        return 3;
    }
}

void ARM7TDMI::switch_mode(ProcessorMode new_mode) {
    if (m_mode == new_mode) return;

    bank_registers(m_mode, new_mode);
    m_mode = new_mode;
    m_cpsr = (m_cpsr & ~0x1F) | static_cast<uint32_t>(new_mode);
}

void ARM7TDMI::enter_exception(ProcessorMode mode, uint32_t vector) {
    // Save current CPSR to new mode's SPSR
    uint32_t old_cpsr = m_cpsr;

    // Switch mode
    switch_mode(mode);

    // Save old CPSR to SPSR
    set_spsr(old_cpsr);

    // Set return address in LR
    // For IRQ: The return instruction is SUBS PC, LR, #4
    // So we need LR = address to return to + 4
    //
    // In our emulator, after executing an instruction, PC points to the next
    // instruction. When an IRQ fires:
    // - If mid-instruction (normal): PC = next_instr, LR should = PC + 4
    // - If halted: PC = next_instr to execute, LR should = PC + 4
    //
    // The ARM7TDMI manual states that for IRQ, LR_irq = PC of next instruction + 4
    // This accounts for the pipeline, and SUBS PC, LR, #4 returns to that instruction
    if (old_cpsr & FLAG_T) {
        // Thumb: instructions are 2 bytes
        m_regs[14] = m_regs[15] + 2;
    } else {
        // ARM: instructions are 4 bytes
        m_regs[14] = m_regs[15] + 4;
    }

    // Disable IRQ, clear Thumb state
    m_cpsr |= FLAG_I;
    m_cpsr &= ~FLAG_T;

    // Jump to vector
    m_regs[15] = vector;
    flush_pipeline();
}

void ARM7TDMI::bank_registers(ProcessorMode old_mode, ProcessorMode new_mode) {
    // FIQ has banked R8-R14, all other modes share R8-R12 and have banked R13-R14
    bool old_is_fiq = (old_mode == ProcessorMode::FIQ);
    bool new_is_fiq = (new_mode == ProcessorMode::FIQ);

    // Handle R8-R12 banking (only FIQ has different R8-R12)
    if (old_is_fiq && !new_is_fiq) {
        // Leaving FIQ: save FIQ R8-R12, restore User/System R8-R12
        for (int i = 0; i < 5; i++) {
            m_fiq_regs[i] = m_regs[8 + i];
            m_regs[8 + i] = m_usr_regs[i];
        }
    } else if (!old_is_fiq && new_is_fiq) {
        // Entering FIQ: save User/System R8-R12, restore FIQ R8-R12
        for (int i = 0; i < 5; i++) {
            m_usr_regs[i] = m_regs[8 + i];
            m_regs[8 + i] = m_fiq_regs[i];
        }
    }

    // Handle R13-R14 (SP/LR) banking - each mode has its own
    // Save current R13-R14 to old mode's bank
    switch (old_mode) {
        case ProcessorMode::FIQ:
            m_fiq_regs[5] = m_regs[13];
            m_fiq_regs[6] = m_regs[14];
            break;
        case ProcessorMode::Supervisor:
            m_svc_regs[0] = m_regs[13];
            m_svc_regs[1] = m_regs[14];
            break;
        case ProcessorMode::Abort:
            m_abt_regs[0] = m_regs[13];
            m_abt_regs[1] = m_regs[14];
            break;
        case ProcessorMode::IRQ:
            m_irq_regs[0] = m_regs[13];
            m_irq_regs[1] = m_regs[14];
            break;
        case ProcessorMode::Undefined:
            m_und_regs[0] = m_regs[13];
            m_und_regs[1] = m_regs[14];
            break;
        case ProcessorMode::User:
        case ProcessorMode::System:
            m_usr_sp_lr[0] = m_regs[13];
            m_usr_sp_lr[1] = m_regs[14];
            break;
    }

    // Restore R13-R14 from new mode's bank
    switch (new_mode) {
        case ProcessorMode::FIQ:
            m_regs[13] = m_fiq_regs[5];
            m_regs[14] = m_fiq_regs[6];
            break;
        case ProcessorMode::Supervisor:
            m_regs[13] = m_svc_regs[0];
            m_regs[14] = m_svc_regs[1];
            break;
        case ProcessorMode::Abort:
            m_regs[13] = m_abt_regs[0];
            m_regs[14] = m_abt_regs[1];
            break;
        case ProcessorMode::IRQ:
            m_regs[13] = m_irq_regs[0];
            m_regs[14] = m_irq_regs[1];
            break;
        case ProcessorMode::Undefined:
            m_regs[13] = m_und_regs[0];
            m_regs[14] = m_und_regs[1];
            break;
        case ProcessorMode::User:
        case ProcessorMode::System:
            m_regs[13] = m_usr_sp_lr[0];
            m_regs[14] = m_usr_sp_lr[1];
            break;
    }
}

void ARM7TDMI::set_cpsr(uint32_t value) {
    ProcessorMode new_mode = static_cast<ProcessorMode>(value & 0x1F);
    if (new_mode != m_mode) {
        switch_mode(new_mode);
    }
    m_cpsr = value;
}

uint32_t ARM7TDMI::get_spsr() const {
    switch (m_mode) {
        case ProcessorMode::FIQ:        return m_spsr_fiq;
        case ProcessorMode::Supervisor: return m_spsr_svc;
        case ProcessorMode::Abort:      return m_spsr_abt;
        case ProcessorMode::IRQ:        return m_spsr_irq;
        case ProcessorMode::Undefined:  return m_spsr_und;
        default:                        return m_cpsr;  // User/System have no SPSR
    }
}

void ARM7TDMI::set_spsr(uint32_t value) {
    switch (m_mode) {
        case ProcessorMode::FIQ:        m_spsr_fiq = value; break;
        case ProcessorMode::Supervisor: m_spsr_svc = value; break;
        case ProcessorMode::Abort:      m_spsr_abt = value; break;
        case ProcessorMode::IRQ:        m_spsr_irq = value; break;
        case ProcessorMode::Undefined:  m_spsr_und = value; break;
        default: break;
    }
}

void ARM7TDMI::set_nz_flags(uint32_t result) {
    m_cpsr &= ~(FLAG_N | FLAG_Z);
    if (result == 0) m_cpsr |= FLAG_Z;
    if (result & (1u << 31)) m_cpsr |= FLAG_N;
}

void ARM7TDMI::set_nzc_flags(uint32_t result, bool carry) {
    m_cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
    if (result == 0) m_cpsr |= FLAG_Z;
    if (result & (1u << 31)) m_cpsr |= FLAG_N;
    if (carry) m_cpsr |= FLAG_C;
}

void ARM7TDMI::set_nzcv_flags(uint32_t result, bool carry, bool overflow) {
    m_cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
    if (result == 0) m_cpsr |= FLAG_Z;
    if (result & (1u << 31)) m_cpsr |= FLAG_N;
    if (carry) m_cpsr |= FLAG_C;
    if (overflow) m_cpsr |= FLAG_V;
}

uint32_t ARM7TDMI::get_register(int reg) const {
    if (reg >= 0 && reg < 16) {
        return m_regs[reg];
    }
    return 0;
}

void ARM7TDMI::save_state(std::vector<uint8_t>& data) {
    // Save registers
    for (int i = 0; i < 16; i++) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&m_regs[i]);
        data.insert(data.end(), ptr, ptr + 4);
    }

    // Save banked registers
    for (const auto& reg : m_fiq_regs) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&reg);
        data.insert(data.end(), ptr, ptr + 4);
    }
    for (const auto& reg : m_svc_regs) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&reg);
        data.insert(data.end(), ptr, ptr + 4);
    }
    for (const auto& reg : m_abt_regs) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&reg);
        data.insert(data.end(), ptr, ptr + 4);
    }
    for (const auto& reg : m_irq_regs) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&reg);
        data.insert(data.end(), ptr, ptr + 4);
    }
    for (const auto& reg : m_und_regs) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&reg);
        data.insert(data.end(), ptr, ptr + 4);
    }

    // Save CPSR and SPSRs
    const uint8_t* cpsr_ptr = reinterpret_cast<const uint8_t*>(&m_cpsr);
    data.insert(data.end(), cpsr_ptr, cpsr_ptr + 4);

    for (uint32_t spsr : {m_spsr_fiq, m_spsr_svc, m_spsr_abt, m_spsr_irq, m_spsr_und}) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&spsr);
        data.insert(data.end(), ptr, ptr + 4);
    }

    // Save state flags
    data.push_back(m_irq_pending ? 1 : 0);
    data.push_back(m_halted ? 1 : 0);
    data.push_back(static_cast<uint8_t>(m_mode));
}

void ARM7TDMI::load_state(const uint8_t*& data, size_t& remaining) {
    // Load registers
    for (int i = 0; i < 16; i++) {
        std::memcpy(&m_regs[i], data, 4);
        data += 4;
        remaining -= 4;
    }

    // Load banked registers
    for (auto& reg : m_fiq_regs) {
        std::memcpy(&reg, data, 4);
        data += 4;
        remaining -= 4;
    }
    for (auto& reg : m_svc_regs) {
        std::memcpy(&reg, data, 4);
        data += 4;
        remaining -= 4;
    }
    for (auto& reg : m_abt_regs) {
        std::memcpy(&reg, data, 4);
        data += 4;
        remaining -= 4;
    }
    for (auto& reg : m_irq_regs) {
        std::memcpy(&reg, data, 4);
        data += 4;
        remaining -= 4;
    }
    for (auto& reg : m_und_regs) {
        std::memcpy(&reg, data, 4);
        data += 4;
        remaining -= 4;
    }

    // Load CPSR and SPSRs
    std::memcpy(&m_cpsr, data, 4);
    data += 4;
    remaining -= 4;

    std::memcpy(&m_spsr_fiq, data, 4); data += 4; remaining -= 4;
    std::memcpy(&m_spsr_svc, data, 4); data += 4; remaining -= 4;
    std::memcpy(&m_spsr_abt, data, 4); data += 4; remaining -= 4;
    std::memcpy(&m_spsr_irq, data, 4); data += 4; remaining -= 4;
    std::memcpy(&m_spsr_und, data, 4); data += 4; remaining -= 4;

    // Load state flags
    m_irq_pending = *data++ != 0; remaining--;
    m_halted = *data++ != 0; remaining--;
    m_mode = static_cast<ProcessorMode>(*data++); remaining--;
}

// ============================================================================
// HLE BIOS Functions
// ============================================================================

void ARM7TDMI::hle_bios_call(uint8_t function) {
    switch (function) {
        case 0x00:  // SoftReset
            // Reset CPU state and jump to ROM
            reset();
            break;

        case 0x01:  // RegisterRamReset
            // R0 contains flags for what to reset
            // For now, do nothing (games usually handle their own init)
            break;

        case 0x02:  // Halt
            m_halted = true;
            break;

        case 0x03:  // Stop
            m_halted = true;
            break;

        case 0x04:  // IntrWait
            // R0: 1 = discard old flags, 0 = check existing
            // R1: interrupt flags to wait for
            // For proper HLE, we need to:
            // 1. If R0=1, clear the flags from the BIOS IRQ mirror at 0x03007FF8
            // 2. Wait until the requested interrupt fires
            // For now, we implement a simplified version that just halts
            // and the game's IRQ handler will update the BIOS IRQ flags
            if (m_regs[0] != 0) {
                // Clear the requested flags from BIOS IRQ mirror
                uint16_t flags = read16(0x03007FF8);
                flags &= ~static_cast<uint16_t>(m_regs[1]);
                write16(0x03007FF8, flags);
            }
            m_halted = true;
            // Store the wait flags for later checking (we use IWRAM location)
            m_intr_wait_flags = m_regs[1] & 0x3FFF;
            break;

        case 0x05:  // VBlankIntrWait
            // Equivalent to IntrWait(1, 1) - wait for VBlank
            {
                // Clear VBlank flag from BIOS IRQ mirror
                uint16_t flags = read16(0x03007FF8);
                flags &= ~0x0001;  // Clear VBlank flag
                write16(0x03007FF8, flags);
            }
            m_halted = true;
            m_intr_wait_flags = 0x0001;  // Wait for VBlank
            break;

        case 0x06:  // Div
            bios_div();
            break;

        case 0x07:  // DivArm
            // Same as Div but with swapped R0/R1
            {
                uint32_t temp = m_regs[0];
                m_regs[0] = m_regs[1];
                m_regs[1] = temp;
                bios_div();
            }
            break;

        case 0x08:  // Sqrt
            bios_sqrt();
            break;

        case 0x09:  // ArcTan
            bios_arctan();
            break;

        case 0x0A:  // ArcTan2
            bios_arctan2();
            break;

        case 0x0B:  // CpuSet
            bios_cpu_set();
            break;

        case 0x0C:  // CpuFastSet
            bios_cpu_fast_set();
            break;

        case 0x0D:  // GetBiosChecksum
            // Return BIOS checksum (fixed value for GBA)
            m_regs[0] = 0xBAAE187F;
            break;

        case 0x0E:  // BgAffineSet
            // Background affine transformation - implement as needed
            break;

        case 0x0F:  // ObjAffineSet
            bios_obj_affine_set();
            break;

        case 0x10:  // BitUnPack
            bios_bit_unpack();
            break;

        case 0x11:  // LZ77UnCompWram
            bios_lz77_uncomp_wram();
            break;

        case 0x12:  // LZ77UnCompVram
            bios_lz77_uncomp_vram();
            break;

        case 0x13:  // HuffUnComp
            bios_huff_uncomp();
            break;

        case 0x14:  // RLUnCompWram
            bios_rl_uncomp_wram();
            break;

        case 0x15:  // RLUnCompVram
            bios_rl_uncomp_vram();
            break;

        case 0x16:  // Diff8bitUnFilterWram
        case 0x17:  // Diff8bitUnFilterVram
        case 0x18:  // Diff16bitUnFilter
            // Differential filters - implement as needed
            break;

        case 0x19:  // SoundBias
            // Sound bias adjustment - not critical for most games
            break;

        case 0x1F:  // MidiKey2Freq
            // MIDI key to frequency conversion
            // R0 = WaveData pointer, R1 = mk (MIDI key), R2 = fp (fine pitch)
            // Return frequency in R0
            {
                // Simplified implementation
                uint32_t freq = 8013;  // Base frequency
                m_regs[0] = freq << 10;
            }
            break;

        default:
            // Unknown BIOS function - log and continue
            break;
    }

    // After SWI call, update the BIOS protection value to simulate
    // the value at address 0x188+8=0x190: 0xE3A02004 (mov r2, #4)
    // This is what real BIOS would have in its prefetch after returning from SWI
    m_bus.set_last_bios_read(0xE3A02004);
}

void ARM7TDMI::bios_div() {
    // R0 = numerator, R1 = denominator
    // Returns: R0 = quotient, R1 = remainder, R3 = abs(quotient)
    int32_t num = static_cast<int32_t>(m_regs[0]);
    int32_t den = static_cast<int32_t>(m_regs[1]);

    if (den == 0) {
        // Division by zero - undefined behavior, return something reasonable
        m_regs[0] = (num < 0) ? 1 : -1;
        m_regs[1] = num;
        m_regs[3] = 1;
        return;
    }

    int32_t quot = num / den;
    int32_t rem = num % den;

    m_regs[0] = static_cast<uint32_t>(quot);
    m_regs[1] = static_cast<uint32_t>(rem);
    m_regs[3] = static_cast<uint32_t>(quot < 0 ? -quot : quot);
}

void ARM7TDMI::bios_sqrt() {
    // R0 = input value
    // Returns: R0 = sqrt(R0)
    uint32_t val = m_regs[0];

    if (val == 0) {
        m_regs[0] = 0;
        return;
    }

    // Integer square root using Newton's method
    uint32_t result = val;
    uint32_t prev;
    do {
        prev = result;
        result = (result + val / result) >> 1;
    } while (result < prev);

    m_regs[0] = prev;
}

void ARM7TDMI::bios_arctan() {
    // R0 = tan value (signed 16-bit fixed point, 1.14)
    // Returns: R0 = arctan result (-0x4000 to 0x4000)
    int16_t tan = static_cast<int16_t>(m_regs[0]);

    // Polynomial approximation
    // arctan(x) ≈ x - x³/3 + x⁵/5 - ...
    // Using fixed point math
    int32_t x = tan;
    int32_t x2 = (x * x) >> 14;
    int32_t x3 = (x2 * x) >> 14;
    int32_t x5 = (x3 * x2) >> 14;

    int32_t result = x - (x3 / 3) + (x5 / 5);

    // Clamp to valid range
    if (result > 0x4000) result = 0x4000;
    if (result < -0x4000) result = -0x4000;

    m_regs[0] = static_cast<uint32_t>(static_cast<int32_t>(result));
}

void ARM7TDMI::bios_arctan2() {
    // R0 = x, R1 = y (both signed 16-bit)
    // Returns: R0 = arctan2(y, x) (0x0000 to 0xFFFF for full circle)
    int16_t x = static_cast<int16_t>(m_regs[0]);
    int16_t y = static_cast<int16_t>(m_regs[1]);

    if (x == 0 && y == 0) {
        m_regs[0] = 0;
        return;
    }

    // Simple implementation using lookup
    // Full implementation would use CORDIC or table lookup
    double angle = atan2(static_cast<double>(y), static_cast<double>(x));
    // Convert from radians (-PI to PI) to GBA format (0 to 0xFFFF)
    int32_t result = static_cast<int32_t>((angle / 3.14159265358979) * 0x8000);
    if (result < 0) result += 0x10000;

    m_regs[0] = static_cast<uint32_t>(result);
}

void ARM7TDMI::bios_cpu_set() {
    // R0 = source, R1 = destination, R2 = length/mode
    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];
    uint32_t cnt = m_regs[2];

    bool fixed_src = (cnt & (1 << 24)) != 0;
    bool is_32bit = (cnt & (1 << 26)) != 0;
    uint32_t count = cnt & 0x1FFFFF;

    if (is_32bit) {
        for (uint32_t i = 0; i < count; i++) {
            uint32_t val = read32(src);
            write32(dst, val);
            if (!fixed_src) src += 4;
            dst += 4;
        }
    } else {
        for (uint32_t i = 0; i < count; i++) {
            uint16_t val = read16(src);
            write16(dst, val);
            if (!fixed_src) src += 2;
            dst += 2;
        }
    }
}

void ARM7TDMI::bios_cpu_fast_set() {
    // R0 = source, R1 = destination, R2 = length/mode
    // Like CpuSet but always 32-bit and copies 8 words at a time
    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];
    uint32_t cnt = m_regs[2];

    bool fixed_src = (cnt & (1 << 24)) != 0;
    uint32_t count = cnt & 0x1FFFFF;

    // Round up to multiple of 8
    count = (count + 7) & ~7u;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t val = read32(src);
        write32(dst, val);
        if (!fixed_src) src += 4;
        dst += 4;
    }
}

void ARM7TDMI::bios_obj_affine_set() {
    // R0 = source data pointer
    // R1 = destination OAM pointer
    // R2 = number of calculations
    // R3 = offset between destination entries (usually 8 for OAM)
    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];
    uint32_t count = m_regs[2];
    uint32_t offset = m_regs[3];

    for (uint32_t i = 0; i < count; i++) {
        // Read source data: sx, sy, angle (each 16-bit)
        int16_t sx = static_cast<int16_t>(read16(src));
        int16_t sy = static_cast<int16_t>(read16(src + 2));
        uint16_t angle = read16(src + 4);
        src += 8;

        // Calculate sin/cos from angle (angle is 0-0xFFFF for full circle)
        double rad = (angle / 65536.0) * 2.0 * 3.14159265358979;
        double sin_val = sin(rad);
        double cos_val = cos(rad);

        // Calculate matrix: pa = sx*cos, pb = -sx*sin, pc = sy*sin, pd = sy*cos
        int16_t pa = static_cast<int16_t>((cos_val * 256.0 * 256.0) / sx);
        int16_t pb = static_cast<int16_t>((-sin_val * 256.0 * 256.0) / sx);
        int16_t pc = static_cast<int16_t>((sin_val * 256.0 * 256.0) / sy);
        int16_t pd = static_cast<int16_t>((cos_val * 256.0 * 256.0) / sy);

        // Write to OAM (pa, pb, pc, pd at offsets 6, 14, 22, 30 relative to base)
        write16(dst + 6, static_cast<uint16_t>(pa));
        write16(dst + 14, static_cast<uint16_t>(pb));
        write16(dst + 22, static_cast<uint16_t>(pc));
        write16(dst + 30, static_cast<uint16_t>(pd));

        dst += offset;
    }
}

void ARM7TDMI::bios_bit_unpack() {
    // R0 = source, R1 = destination, R2 = unpack info pointer
    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];
    uint32_t info = m_regs[2];

    uint16_t src_len = read16(info);
    uint8_t src_width = read8(info + 2);
    uint8_t dst_width = read8(info + 3);
    uint32_t data_offset = read32(info + 4);

    bool zero_flag = (data_offset >> 31) != 0;
    data_offset &= 0x7FFFFFFF;

    int src_bits_left = 0;
    uint32_t src_buffer = 0;
    int dst_bits_filled = 0;
    uint32_t dst_buffer = 0;

    for (uint16_t i = 0; i < src_len; i++) {
        // Read source byte
        src_buffer = read8(src++);
        src_bits_left = 8;

        while (src_bits_left >= src_width) {
            // Extract bits
            uint32_t val = src_buffer & ((1 << src_width) - 1);
            src_buffer >>= src_width;
            src_bits_left -= src_width;

            // Apply offset if non-zero or zero_flag is set
            if (val != 0 || zero_flag) {
                val += data_offset;
            }

            // Pack into destination
            dst_buffer |= (val & ((1 << dst_width) - 1)) << dst_bits_filled;
            dst_bits_filled += dst_width;

            // Flush when we have 32 bits
            if (dst_bits_filled >= 32) {
                write32(dst, dst_buffer);
                dst += 4;
                dst_buffer = 0;
                dst_bits_filled = 0;
            }
        }
    }

    // Flush remaining bits
    if (dst_bits_filled > 0) {
        write32(dst, dst_buffer);
    }
}

void ARM7TDMI::bios_lz77_uncomp_wram() {
    // R0 = source, R1 = destination
    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];

    // Read header
    uint32_t header = read32(src);
    src += 4;

    uint32_t decomp_size = header >> 8;
    uint32_t decomp_end = dst + decomp_size;

    while (dst < decomp_end) {
        uint8_t flags = read8(src++);

        for (int i = 0; i < 8 && dst < decomp_end; i++) {
            if (flags & 0x80) {
                // Compressed - read offset/length
                uint8_t b1 = read8(src++);
                uint8_t b2 = read8(src++);

                uint32_t len = ((b1 >> 4) & 0xF) + 3;
                uint32_t offset = ((b1 & 0xF) << 8) | b2;

                uint32_t src_ptr = dst - offset - 1;
                for (uint32_t j = 0; j < len && dst < decomp_end; j++) {
                    write8(dst++, read8(src_ptr++));
                }
            } else {
                // Uncompressed
                write8(dst++, read8(src++));
            }
            flags <<= 1;
        }
    }
}

void ARM7TDMI::bios_lz77_uncomp_vram() {
    // Same as WRAM version but writes in 16-bit units to VRAM
    // To handle back-references correctly, we decompress to a local buffer first
    uint32_t src = m_regs[0];
    uint32_t dst_start = m_regs[1];

    uint32_t header = read32(src);
    src += 4;

    uint32_t decomp_size = header >> 8;

    // Allocate temporary buffer for decompression
    // For safety, limit to reasonable size (16MB should cover any GBA graphics)
    if (decomp_size > 0x1000000) {
        return;  // Too large, bail out
    }

    std::vector<uint8_t> temp_buffer(decomp_size);
    uint32_t dst_pos = 0;

    while (dst_pos < decomp_size) {
        uint8_t flags = read8(src++);

        for (int i = 0; i < 8 && dst_pos < decomp_size; i++) {
            if (flags & 0x80) {
                // Compressed - read offset/length
                uint8_t b1 = read8(src++);
                uint8_t b2 = read8(src++);

                uint32_t len = ((b1 >> 4) & 0xF) + 3;
                uint32_t offset = ((b1 & 0xF) << 8) | b2;

                uint32_t src_ptr = dst_pos - offset - 1;
                for (uint32_t j = 0; j < len && dst_pos < decomp_size; j++) {
                    temp_buffer[dst_pos++] = temp_buffer[src_ptr++];
                }
            } else {
                // Uncompressed
                temp_buffer[dst_pos++] = read8(src++);
            }
            flags <<= 1;
        }
    }

    // Now write to VRAM in 16-bit units
    uint32_t dst = dst_start;
    for (uint32_t i = 0; i + 1 < decomp_size; i += 2) {
        write16(dst, temp_buffer[i] | (temp_buffer[i + 1] << 8));
        dst += 2;
    }
    // Handle odd byte if present
    if (decomp_size & 1) {
        // Last odd byte - write as 16-bit with 0 padding (hardware behavior)
        write16(dst, temp_buffer[decomp_size - 1]);
    }
}

void ARM7TDMI::bios_huff_uncomp() {
    // Huffman decompression - complex, implement basic version
    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];

    uint32_t header = read32(src);
    src += 4;

    uint8_t data_size = header & 0xF;  // Bits per symbol (4 or 8)
    uint32_t decomp_size = header >> 8;

    // Read tree size and tree data
    uint8_t tree_size = read8(src++);
    uint32_t tree_start = src;
    src = tree_start + (tree_size + 1) * 2;

    // Decompress
    uint32_t bits = 0;
    int bits_left = 0;
    uint32_t out_buffer = 0;
    int out_bits = 0;

    for (uint32_t written = 0; written < decomp_size;) {
        // Refill bit buffer
        while (bits_left < 16 && src < m_regs[0] + 0x10000) {
            bits |= read8(src++) << bits_left;
            bits_left += 8;
        }

        // Traverse tree
        uint32_t node_offset = tree_start;
        while (true) {
            uint8_t node = read8(node_offset);
            bool is_data = (node & ((bits & 1) ? 0x80 : 0x40)) != 0;
            uint8_t offset = node & 0x3F;

            bits >>= 1;
            bits_left--;

            if (is_data) {
                // Read data
                uint8_t data = read8(tree_start + (offset + 1) * 2 + ((bits & 1) ? 1 : 0));
                out_buffer |= data << out_bits;
                out_bits += data_size;

                if (out_bits >= 32) {
                    write32(dst, out_buffer);
                    dst += 4;
                    written += 4;
                    out_buffer = 0;
                    out_bits = 0;
                }
                break;
            } else {
                node_offset = tree_start + (offset + 1) * 2;
            }
        }
    }
}

void ARM7TDMI::bios_rl_uncomp_wram() {
    // Run-length decompression
    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];

    uint32_t header = read32(src);
    src += 4;

    uint32_t decomp_size = header >> 8;
    uint32_t decomp_end = dst + decomp_size;

    while (dst < decomp_end) {
        uint8_t flag = read8(src++);

        if (flag & 0x80) {
            // Compressed run
            uint8_t len = (flag & 0x7F) + 3;
            uint8_t data = read8(src++);
            for (int i = 0; i < len && dst < decomp_end; i++) {
                write8(dst++, data);
            }
        } else {
            // Uncompressed run
            uint8_t len = (flag & 0x7F) + 1;
            for (int i = 0; i < len && dst < decomp_end; i++) {
                write8(dst++, read8(src++));
            }
        }
    }
}

void ARM7TDMI::bios_rl_uncomp_vram() {
    // Same as WRAM but writes 16-bit at a time
    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];

    uint32_t header = read32(src);
    src += 4;

    uint32_t decomp_size = header >> 8;
    uint32_t decomp_end = dst + decomp_size;

    uint8_t buffer[2];
    int buf_pos = 0;

    while (dst < decomp_end) {
        uint8_t flag = read8(src++);

        if (flag & 0x80) {
            uint8_t len = (flag & 0x7F) + 3;
            uint8_t data = read8(src++);
            for (int i = 0; i < len && dst < decomp_end; i++) {
                buffer[buf_pos++] = data;
                if (buf_pos == 2) {
                    write16(dst, buffer[0] | (buffer[1] << 8));
                    dst += 2;
                    buf_pos = 0;
                }
            }
        } else {
            uint8_t len = (flag & 0x7F) + 1;
            for (int i = 0; i < len && dst < decomp_end; i++) {
                buffer[buf_pos++] = read8(src++);
                if (buf_pos == 2) {
                    write16(dst, buffer[0] | (buffer[1] << 8));
                    dst += 2;
                    buf_pos = 0;
                }
            }
        }
    }
}

} // namespace gba
