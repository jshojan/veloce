#include "arm7tdmi.hpp"
#include "bus.hpp"
#include "debug.hpp"
#include <cstring>
#include <iostream>
#include <cmath>
#include <vector>

namespace gba {

// Helper to check if a PC value points to valid executable memory
static inline bool is_valid_pc(uint32_t pc) {
    // Valid GBA executable memory regions:
    // BIOS: 0x00000000 - 0x00003FFF
    // EWRAM: 0x02000000 - 0x0203FFFF
    // IWRAM: 0x03000000 - 0x03007FFF
    // ROM: 0x08000000 - 0x09FFFFFF (and mirrors)
    if (pc < 0x00004000) return true;  // BIOS
    if (pc >= 0x02000000 && pc < 0x02040000) return true;  // EWRAM
    if (pc >= 0x03000000 && pc < 0x03008000) return true;  // IWRAM
    if (pc >= 0x08000000 && pc < 0x0E000000) return true;  // ROM + mirrors
    return false;
}

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

    // Clear SPSRs
    m_spsr_fiq = 0;
    m_spsr_svc = 0;
    m_spsr_abt = 0;
    m_spsr_irq = 0;
    m_spsr_und = 0;

    // Initialize stack pointers for each mode (matching mGBA's GBAReset)
    // The BIOS sets up different stack pointers for each mode

    // Set up IRQ mode stack
    m_cpsr = static_cast<uint32_t>(ProcessorMode::IRQ);
    m_mode = ProcessorMode::IRQ;
    m_irq_regs[0] = 0x03007FA0;      // SP_irq

    // Set up Supervisor mode stack
    m_cpsr = static_cast<uint32_t>(ProcessorMode::Supervisor);
    m_mode = ProcessorMode::Supervisor;
    m_svc_regs[0] = 0x03007FE0;      // SP_svc

    // Set up User/System mode stack
    m_usr_sp_lr[0] = 0x03007F00;     // SP_usr/SP_sys

    // Final state: System mode with IRQ enabled, FIQ disabled
    // This matches the state after the real BIOS completes
    // mGBA ends reset in System mode
    m_cpsr = static_cast<uint32_t>(ProcessorMode::System);  // No FLAG_I - IRQs enabled
    m_mode = ProcessorMode::System;
    m_regs[13] = 0x03007F00;         // Current SP (in System mode)

    // Set PC to ROM entry point (skip BIOS)
    m_regs[15] = 0x08000000;

    // Clear pipeline
    m_pipeline[0] = 0;
    m_pipeline[1] = 0;
    m_pipeline_valid = 0;

    // Clear interrupt state
    m_irq_pending = false;
    m_irq_delay = 0;
    m_halted = false;
    m_in_thumb_bl = false;

    // Clear IntrWait state
    m_in_intr_wait = false;
    m_intr_wait_flags = 0;
    m_intr_wait_return_pc = 0;
    m_intr_wait_return_cpsr = 0;

    // Reset prefetch buffer and sequential tracking
    m_prefetch.reset();
    m_last_fetch_addr = 0xFFFFFFFF;
    m_last_data_addr = 0xFFFFFFFF;
    m_next_fetch_nonseq = true;  // First fetch after reset is non-sequential

    GBA_DEBUG_PRINT("CPU Reset: PC=0x%08X, CPSR=0x%08X (mode=%s, IRQ=%s)\n",
                    m_regs[15], m_cpsr,
                    (m_mode == ProcessorMode::System) ? "System" : "Other",
                    (m_cpsr & FLAG_I) ? "disabled" : "enabled");
}

int ARM7TDMI::step() {
    int cycles = 1;  // Default cycle count

    // If halted (either from Halt SWI or IntrWait), just pass time
    // The CPU will be woken by signal_irq() when an interrupt arrives
    if (m_halted) {
        // Decrement IRQ delay during halt (cycles still pass)
        if (m_irq_delay > 0) {
            m_irq_delay--;
        }
        return 1;
    }

    // Check for pending IRQ - only service if:
    // 1. IRQ is pending
    // 2. IRQs are enabled (I flag clear)
    // 3. The IRQ delay has elapsed (m_irq_delay <= 0)
    // 4. We're not in the middle of a Thumb BL instruction (which is pseudo-atomic)
    if (m_irq_pending && !(m_cpsr & FLAG_I) && m_irq_delay <= 0 && !m_in_thumb_bl) {
        // Enter IRQ mode and jump to BIOS IRQ vector at 0x00000018
        // The game's IRQ handler will be called via the handler address at 0x03007FFC
        enter_exception(ProcessorMode::IRQ, VECTOR_IRQ);
        m_irq_pending = false;
        return 3;
    }

    // If we're in IntrWait mode and not halted, check if the waited interrupt occurred
    // This happens after an IRQ woke us and the IRQ handler ran
    if (m_in_intr_wait) {
        // Read the BIOS interrupt flags at 0x03007FF8
        // The game's IRQ handler should have ORed the acknowledged interrupt flags here
        uint16_t bios_flags = read16(0x03007FF8);
        uint16_t matched = bios_flags & m_intr_wait_flags;

        GBA_DEBUG_PRINT("IntrWait: Checking flags, BIOS_IF=0x%04X, waiting=0x%04X, matched=0x%04X\n",
                        bios_flags, m_intr_wait_flags, matched);

        if (matched != 0) {
            // The interrupt we were waiting for occurred!
            // Clear the matched flags from BIOS interrupt flags
            write16(0x03007FF8, bios_flags & ~matched);

            GBA_DEBUG_PRINT("IntrWait: Complete! Cleared flags, returning to PC=0x%08X\n",
                            m_intr_wait_return_pc);

            // Exit IntrWait state
            m_in_intr_wait = false;
            m_intr_wait_flags = 0;

            // IntrWait returns normally - the SWI already set up the return
            // We just continue execution from where the SWI was called
        } else {
            // Interrupt we were waiting for hasn't occurred yet
            // Go back to halt state and wait for next interrupt
            GBA_DEBUG_PRINT("IntrWait: Flag not set, halting again\n");
            m_halted = true;
            return 1;
        }
    }


    // Execute instruction based on current state
    if (m_cpsr & FLAG_T) {
        // Thumb mode
        uint32_t fetch_addr = m_regs[15];

        // Use prefetch buffer for ROM fetches, otherwise normal wait states
        int fetch_wait = prefetch_read(fetch_addr, 16);

        uint16_t instruction = fetch_thumb();
        int exec_cycles = execute_thumb(instruction);
        cycles = exec_cycles + fetch_wait;

        // Advance prefetch buffer during execution cycles
        prefetch_step(exec_cycles);

        // Update last fetch address (m_regs[15] was incremented by fetch_thumb)
        m_last_fetch_addr = fetch_addr;
    } else {
        // ARM mode
        uint32_t fetch_addr = m_regs[15];

        // Use prefetch buffer for ROM fetches, otherwise normal wait states
        int fetch_wait = prefetch_read(fetch_addr, 32);

        uint32_t instruction = fetch_arm();
        int exec_cycles = execute_arm(instruction);
        cycles = exec_cycles + fetch_wait;

        // Advance prefetch buffer during execution cycles
        prefetch_step(exec_cycles);

        // Update last fetch address
        m_last_fetch_addr = fetch_addr;
    }

    // Decrement IRQ delay by cycles consumed
    if (m_irq_delay > 0) {
        m_irq_delay -= cycles;
    }

    return cycles;
}

void ARM7TDMI::signal_irq() {
    // Only start the delay if we don't already have an IRQ pending
    // This prevents resetting the delay counter on every call
    if (!m_irq_pending) {
        m_irq_pending = true;
        // When waking from HALT, use shorter delay (~2 cycles)
        // During normal execution, use standard delay (~3 cycles)
        if (m_halted) {
            m_irq_delay = IRQ_DELAY_FROM_HALT;
        } else {
            m_irq_delay = IRQ_DELAY_CYCLES;
        }
    }

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
    m_bus.write16_unaligned(address, value);
}

void ARM7TDMI::write32(uint32_t address, uint32_t value) {
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

    // Debug: check if we're branching to an invalid address
    if (!is_valid_pc(m_regs[15])) {
        GBA_DEBUG_PRINT("=== BRANCH TO INVALID PC ===\n");
        GBA_DEBUG_PRINT("  PC=0x%08X, CPSR=0x%08X, mode=%s\n",
                        m_regs[15], m_cpsr,
                        m_mode == ProcessorMode::System ? "System" :
                        m_mode == ProcessorMode::User ? "User" :
                        m_mode == ProcessorMode::IRQ ? "IRQ" :
                        m_mode == ProcessorMode::FIQ ? "FIQ" :
                        m_mode == ProcessorMode::Supervisor ? "SVC" :
                        m_mode == ProcessorMode::Abort ? "ABT" :
                        m_mode == ProcessorMode::Undefined ? "UND" : "???");
        GBA_DEBUG_PRINT("  LR=0x%08X, SP=0x%08X\n", m_regs[14], m_regs[13]);
    }

    // Reset sequential tracking - next fetch after a branch is non-sequential
    m_last_fetch_addr = 0xFFFFFFFF;
    m_last_data_addr = 0xFFFFFFFF;
    m_next_fetch_nonseq = true;  // Force next fetch to be non-sequential

    // Invalidate prefetch buffer on branch (non-sequential access)
    prefetch_invalidate();
}

bool ARM7TDMI::is_rom_address(uint32_t address) const {
    // ROM region: 0x08000000 - 0x0DFFFFFF (WS0, WS1, WS2)
    uint32_t region = address >> 24;
    return region >= 0x08 && region <= 0x0D;
}

void ARM7TDMI::prefetch_invalidate() {
    m_prefetch.invalidate();
}

void ARM7TDMI::prefetch_step(int cycles) {
    // Don't fill if prefetch is disabled
    if (!m_bus.is_prefetch_enabled()) {
        return;
    }

    // Need a valid next_address to prefetch from
    if (!m_prefetch.active) {
        return;
    }

    // Only prefetch from ROM regions (0x08-0x0D)
    if (!is_rom_address(m_prefetch.next_address)) {
        m_prefetch.active = false;
        return;
    }

    // Buffer is full (8 halfwords)
    if (m_prefetch.count >= 8) {
        return;
    }

    // Get the duty cycle (S wait states) for the current ROM region
    int duty = m_bus.get_prefetch_duty(m_prefetch.next_address);

    // Use the cycles to fill the buffer
    m_prefetch.countdown -= cycles;

    while (m_prefetch.countdown <= 0 && m_prefetch.count < 8) {
        // Check for 128KB boundary crossing
        // The GBA forces non-sequential timing at each 128KB ROM boundary
        // The prefetcher stops at these boundaries (acts as full)
        uint32_t current_block = m_prefetch.next_address & 0x1FFFF;  // Within 128KB block
        if (current_block == 0 && m_prefetch.count > 0) {
            // We've reached a 128KB boundary, stop prefetching
            m_prefetch.countdown = 0;
            break;
        }

        // One halfword filled
        m_prefetch.count++;
        m_prefetch.next_address += 2;

        // Check if next address crosses 128KB boundary
        uint32_t next_block = m_prefetch.next_address & 0x1FFFF;

        // Reset countdown for next halfword if buffer not full and valid
        if (m_prefetch.count < 8 && is_rom_address(m_prefetch.next_address) && next_block != 0) {
            m_prefetch.countdown += duty;
        } else if (m_prefetch.count < 8 && is_rom_address(m_prefetch.next_address) && next_block == 0) {
            // Stop at boundary - don't continue prefetching
            m_prefetch.countdown = 0;
            break;
        } else {
            m_prefetch.countdown = 0;
            break;
        }
    }
}

int ARM7TDMI::prefetch_read(uint32_t address, int size) {
    // Check if this is a ROM address and prefetch is enabled
    if (!is_rom_address(address) || !m_bus.is_prefetch_enabled()) {
        bool is_sequential = !m_next_fetch_nonseq && (address == m_last_fetch_addr + (size / 8));
        m_next_fetch_nonseq = false;  // Clear the flag after use
        return m_bus.get_wait_states(address, is_sequential, size);
    }

    // Calculate how many halfwords we need (1 for Thumb/16-bit, 2 for ARM/32-bit)
    int halfwords_needed = (size == 32) ? 2 : 1;

    // Check if the requested address is within the prefetch buffer range
    // Buffer covers: [head_address, head_address + count * 2)
    bool hit = false;
    if (m_prefetch.count >= halfwords_needed) {
        uint32_t buffer_end = m_prefetch.head_address + (m_prefetch.count * 2);
        // Check if address falls within [head_address, buffer_end - size_in_bytes)
        if (address >= m_prefetch.head_address &&
            address + (size / 8) <= buffer_end) {
            hit = true;
        }
    }

    // After a branch, even if we have a prefetch hit, the timing is different
    // The first fetch after a branch is always non-sequential on the ROM bus
    // But if we have a prefetch hit, we still get the benefit of 1S timing
    bool forced_nonseq = m_next_fetch_nonseq;
    m_next_fetch_nonseq = false;  // Clear the flag

    if (hit && !forced_nonseq) {
        // Prefetch hit - consume from buffer
        // Calculate how many halfwords to consume (from head to address + size)
        uint32_t consumed_end = address + (size / 8);
        int consumed = (consumed_end - m_prefetch.head_address) / 2;

        // Update buffer state
        m_prefetch.head_address = consumed_end;
        m_prefetch.count -= consumed;

        // The prefetcher continues from where it was
        // If it was idle, restart it
        if (!m_prefetch.active && m_prefetch.count < 8) {
            m_prefetch.active = true;
            m_prefetch.countdown = m_bus.get_prefetch_duty(m_prefetch.next_address);
        }

        // Prefetch hit: 1 cycle (1S) instead of normal wait states
        return 1;
    }

    // Prefetch miss or forced non-sequential (after branch)
    // Use normal wait states and restart prefetch from this address
    bool is_sequential = !forced_nonseq && (address == m_last_fetch_addr + (size / 8));
    int wait = m_bus.get_wait_states(address, is_sequential, size);

    // Restart prefetch buffer from after this access
    m_prefetch.head_address = address + (size / 8);
    m_prefetch.next_address = m_prefetch.head_address;
    m_prefetch.count = 0;
    m_prefetch.active = true;
    m_prefetch.countdown = m_bus.get_prefetch_duty(m_prefetch.next_address);

    return wait;
}

int ARM7TDMI::data_access_cycles(uint32_t address, int access_size, bool is_write) {
    // Calculate wait states for data memory access
    // During non-ROM accesses, the prefetch buffer can fill
    //
    // Key insight from mGBA: when CPU accesses non-ROM memory (EWRAM, VRAM, etc.),
    // the prefetch buffer continues filling during that memory stall time.

    // Determine if this is a sequential access
    // Data accesses are sequential if they follow the previous data access
    bool is_sequential = (address == m_last_data_addr + (access_size / 8));
    m_last_data_addr = address;

    // Get the wait states for this memory region
    int wait = m_bus.get_wait_states(address, is_sequential, access_size);

    // If we're accessing non-ROM memory and prefetch is enabled,
    // the prefetch buffer can fill during the memory stall cycles
    if (!is_rom_address(address) && m_bus.is_prefetch_enabled() && m_prefetch.active) {
        // Advance prefetch buffer during the memory stall
        prefetch_step(wait);
    }

    return wait;
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

int ARM7TDMI::multiply_cycles(uint32_t rs) {
    // ARM7TDMI multiply timing depends on the significant bits of the Rs operand.
    // The multiplier array processes 8 bits at a time, terminating early when
    // all remaining bits are zeros or ones (sign extension for signed multiply).
    //
    // m = 1 if bits [31:8] are all zeros or all ones
    // m = 2 if bits [31:16] are all zeros or all ones
    // m = 3 if bits [31:24] are all zeros or all ones
    // m = 4 otherwise
    //
    // Total cycles for MUL: m, MLA: m+1, UMULL/UMLAL: m+1, SMULL/SMLAL: m+2

    uint32_t mask = rs & 0xFFFFFF00;
    if (mask == 0 || mask == 0xFFFFFF00) return 1;

    mask = rs & 0xFFFF0000;
    if (mask == 0 || mask == 0xFFFF0000) return 2;

    mask = rs & 0xFF000000;
    if (mask == 0 || mask == 0xFF000000) return 3;

    return 4;
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

    // ARM7TDMI multiply timing:
    // MUL: m cycles, MLA: m+1 cycles
    // where m is 1-4 based on Rs significant bits
    int m = multiply_cycles(m_regs[rs]);
    return accumulate ? m + 1 : m;
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

    // ARM7TDMI long multiply timing:
    // UMULL: m+1, UMLAL: m+2, SMULL: m+2, SMLAL: m+3
    // where m is 1-4 based on Rs significant bits
    int m = multiply_cycles(m_regs[rs]);
    int cycles = m + 1;           // Base for long multiply
    if (sign) cycles++;           // +1 for signed
    if (accumulate) cycles++;     // +1 for accumulate
    return cycles;
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

    // Calculate memory access timing (this also advances prefetch during non-ROM stalls)
    int access_size = byte ? 8 : 32;
    int mem_cycles = data_access_cycles(addr, access_size, !load);

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

    // Base timing: 1S (internal) + memory wait states
    // LDR: 1S + 1N + 1I = 3 cycles minimum + memory wait
    // STR: 1S + 1N = 2 cycles minimum + memory wait
    int base_cycles = load ? 1 : 1;  // Internal cycles
    return base_cycles + mem_cycles;
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

    // Determine access size for timing
    int access_size = (op == 2) ? 8 : 16;  // LDRSB is 8-bit, others are 16-bit
    int mem_cycles = data_access_cycles(addr, access_size, !load);

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

    // Base timing: 1S (internal) + memory wait states
    int base_cycles = load ? 1 : 1;
    return base_cycles + mem_cycles;
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
                    // Debug: if loading garbage into SP, dump the context
                    if (i == 13 && (value < 0x03000000 || value >= 0x03008000)) {
                        static bool sp_ldm_logged = false;
                        if (!sp_ldm_logged) {
                            sp_ldm_logged = true;
                            GBA_DEBUG_PRINT("=== LDM loading invalid SP ===\n");
                            GBA_DEBUG_PRINT("  Instruction: 0x%08X (P=%d, U=%d, S=%d, W=%d, L=%d)\n",
                                           instruction, pre, up, psr, writeback, load);
                            GBA_DEBUG_PRINT("  Loading SP from addr=0x%08X, got value=0x%08X\n", addr, value);
                            GBA_DEBUG_PRINT("  PC=0x%08X, reg_list=0x%04X, base Rn=R%d=0x%08X, reg_count=%d\n",
                                           m_regs[15], reg_list, rn, base, reg_count);
                            // Print memory both before and after base
                            GBA_DEBUG_PRINT("  Stack contents (base-16 to base+28):\n");
                            for (int j = -4; j < 8; j++) {
                                uint32_t a = base + j * 4;
                                uint32_t v = read32(a);
                                GBA_DEBUG_PRINT("    [0x%08X] = 0x%08X%s%s\n", a, v,
                                    (a == addr) ? " <-- SP loaded from here" : "",
                                    (v >= 0x08000000 && v < 0x0E000000) ? " (ROM)" :
                                    (v >= 0x03000000 && v < 0x03008000) ? " (IWRAM)" :
                                    (v >= 0x02000000 && v < 0x02040000) ? " (EWRAM)" : "");
                            }
                        }
                    }
                    if (i == 15) {
                        // Debug: if loading garbage into PC, dump the context
                        if (!is_valid_pc(value & ~3u)) {
                            GBA_DEBUG_PRINT("=== LDM loading invalid PC ===\n");
                            GBA_DEBUG_PRINT("  Loading PC from addr=0x%08X, got value=0x%08X\n", addr - 4, value);
                            GBA_DEBUG_PRINT("  Current regs: PC=0x%08X SP=0x%08X LR=0x%08X\n",
                                           m_regs[15], m_regs[13], m_regs[14]);
                            GBA_DEBUG_PRINT("  Base was Rn=R%d=0x%08X, reg_list=0x%04X\n",
                                           rn, base, reg_list);
                        }
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
                    // ARM7TDMI STM behavior when Rn (base) is in the register list:
                    // - If Rn is the FIRST (lowest numbered) register in the list: store OLD base
                    // - If Rn is NOT the first register: store NEW (updated) base
                    // Reference: GBATEK and ARM7TDMI documentation
                    if (static_cast<uint32_t>(i) == rn) {
                        // Find the lowest set bit in register list to check if rn is first
                        int lowest_reg = __builtin_ctz(reg_list);  // Count trailing zeros = lowest register
                        if (static_cast<int>(rn) == lowest_reg) {
                            // Base register is FIRST in the list - store OLD base
                            value = base;
                        } else {
                            // Base register is NOT first - store NEW (updated) base
                            // The new base has already been written to m_regs[rn] via early writeback
                            value = m_regs[rn];
                        }
                    } else {
                        value = m_regs[i];
                    }
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
        uint32_t rm = instruction & 0xF;
        value = m_regs[rm];
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
        uint32_t new_val = (spsr_val & ~mask) | (value & mask);
        set_spsr(new_val);
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
    // Thumb MUL timing: m cycles based on Rs significant bits
    return (op == 0xD) ? multiply_cycles(b) : 1;
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

    // Calculate memory access timing
    int mem_cycles = data_access_cycles(addr, 32, false);

    uint32_t value = read32(addr);
    m_regs[rd] = value;

    return 1 + mem_cycles;
}

int ARM7TDMI::thumb_load_store_reg(uint16_t instruction) {
    bool load = (instruction >> 11) & 1;
    bool byte = (instruction >> 10) & 1;
    uint16_t ro = (instruction >> 6) & 7;
    uint16_t rb = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t addr = m_regs[rb] + m_regs[ro];

    // Calculate memory access timing
    int access_size = byte ? 8 : 32;
    int mem_cycles = data_access_cycles(addr, access_size, !load);

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

    return 1 + mem_cycles;
}

int ARM7TDMI::thumb_load_store_sign(uint16_t instruction) {
    uint16_t op = (instruction >> 10) & 3;
    uint16_t ro = (instruction >> 6) & 7;
    uint16_t rb = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t addr = m_regs[rb] + m_regs[ro];

    // Determine access size and if it's a write
    int access_size = (op == 1) ? 8 : 16;  // LDSB is 8-bit, others are 16-bit
    bool is_write = (op == 0);  // STRH

    // Calculate memory access timing
    int mem_cycles = data_access_cycles(addr, access_size, is_write);

    switch (op) {
        case 0:  // STRH
            write16(addr, static_cast<uint16_t>(m_regs[rd]));
            break;
        case 1:  // LDSB
            m_regs[rd] = static_cast<uint32_t>(sign_extend_8(read8(addr)));
            break;
        case 2:  // LDRH
            m_regs[rd] = read16(addr);
            break;
        case 3:  // LDSH
            m_regs[rd] = static_cast<uint32_t>(sign_extend_16(read16(addr)));
            break;
    }

    return 1 + mem_cycles;
}

int ARM7TDMI::thumb_load_store_imm(uint16_t instruction) {
    bool byte = (instruction >> 12) & 1;
    bool load = (instruction >> 11) & 1;
    uint16_t offset = (instruction >> 6) & 0x1F;
    uint16_t rb = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t addr = m_regs[rb] + (byte ? offset : (offset << 2));

    // Calculate memory access timing
    int access_size = byte ? 8 : 32;
    int mem_cycles = data_access_cycles(addr, access_size, !load);

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

    return 1 + mem_cycles;
}

int ARM7TDMI::thumb_load_store_half(uint16_t instruction) {
    bool load = (instruction >> 11) & 1;
    uint16_t offset = ((instruction >> 6) & 0x1F) << 1;
    uint16_t rb = (instruction >> 3) & 7;
    uint16_t rd = instruction & 7;

    uint32_t addr = m_regs[rb] + offset;

    // Calculate memory access timing
    int mem_cycles = data_access_cycles(addr, 16, !load);

    if (load) {
        m_regs[rd] = read16(addr);
    } else {
        write16(addr, static_cast<uint16_t>(m_regs[rd]));
    }

    return 1 + mem_cycles;
}

int ARM7TDMI::thumb_sp_relative_load_store(uint16_t instruction) {
    bool load = (instruction >> 11) & 1;
    uint16_t rd = (instruction >> 8) & 7;
    uint16_t offset = (instruction & 0xFF) << 2;

    uint32_t addr = m_regs[13] + offset;

    // Calculate memory access timing
    int mem_cycles = data_access_cycles(addr, 32, !load);

    if (load) {
        m_regs[rd] = read32(addr);
    } else {
        write32(addr, m_regs[rd]);
    }

    return 1 + mem_cycles;
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
            uint32_t new_pc = read32(addr) & ~1u;
            m_regs[15] = new_pc;
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

    // Debug: trace conditional branches near timer polling area
    uint32_t pc = m_regs[15] - 2;  // PC of this instruction
    if (pc >= 0x081E34F0 && pc <= 0x081E3510) {
        uint32_t target = m_regs[15] + 2 + static_cast<int32_t>(offset) * 2;
        GBA_DEBUG_PRINT("BRANCH @ 0x%08X: cond=%d, N=%d Z=%d C=%d V=%d, take=%d, offset=%d, target=0x%08X\n",
                       pc, static_cast<int>(cond), n, z, c, v, take_branch, offset, target);
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
        // Mark that we're in the middle of BL - defer IRQ until second half
        m_in_thumb_bl = true;
        return 1;
    } else {
        // Second instruction: complete the branch
        // Return address is the instruction after this one (current PC after fetch = instruction + 2)
        // LR should have bit 0 set to indicate Thumb mode
        uint32_t next_pc = m_regs[15] | 1;
        m_regs[15] = m_regs[14] + (offset << 1);
        m_regs[14] = next_pc;

        // BL complete - allow IRQs again
        m_in_thumb_bl = false;

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

    // Warning if IRQ fires before game has set up handler
    if (mode == ProcessorMode::IRQ) {
        uint32_t user_handler = read32(0x03007FFC);
        if (user_handler == 0) {
            GBA_DEBUG_PRINT("WARNING: IRQ fired but game handler at 0x03007FFC is NULL!\n");
        }
    }

    // Switch mode
    switch_mode(mode);

    // Save old CPSR to SPSR
    set_spsr(old_cpsr);

    // Set return address in LR
    // For IRQ/FIQ: The return instruction is SUBS PC, LR, #4
    // ARM7TDMI manual: LR_irq = "address of next instruction to be executed" + 4
    //
    // In our emulator, at IRQ check time (before fetch), m_regs[15] = address of
    // next instruction to be executed. So LR = m_regs[15] + 4.
    //
    // This is the SAME for both ARM and Thumb modes because SUBS PC, LR, #4
    // is always executed in ARM mode (T bit is cleared on exception entry).
    // When CPSR is restored from SPSR, the correct Thumb/ARM state resumes.
    //
    // Reference implementations:
    // - mGBA: LR = PC - instructionWidth + WORD_SIZE_ARM (effectively PC for ARM, PC+2 for Thumb
    //         when their PC is instruction_addr + width, but timing differs from ours)
    // - SkyEmu: LR = PC + 4 (direct)
    // - NanoBoyAdvance: Similar approach
    //
    // For SWI/Undefined, the return instruction is MOVS PC, LR so LR = next_instruction.
    if (mode == ProcessorMode::IRQ || mode == ProcessorMode::FIQ) {
        // IRQ/FIQ: LR = next_instruction + 4 so SUBS PC, LR, #4 returns correctly
        m_regs[14] = m_regs[15] + 4;
    } else {
        // SWI/Undefined/Abort: LR = next_instruction so MOVS PC, LR returns correctly
        // (or SUBS PC, LR, #4 for Data Abort which returns to retry the instruction)
        if (mode == ProcessorMode::Abort) {
            // Data/Prefetch Abort: LR = instruction_address + 8 (to retry after SUBS PC, LR, #8)
            m_regs[14] = m_regs[15] + 8;
        } else {
            // SWI/Undefined: LR = next_instruction
            m_regs[14] = m_regs[15];
        }
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

    // Validate mode - ARM7TDMI only has specific valid modes
    bool valid_mode = (new_mode == ProcessorMode::User ||
                       new_mode == ProcessorMode::FIQ ||
                       new_mode == ProcessorMode::IRQ ||
                       new_mode == ProcessorMode::Supervisor ||
                       new_mode == ProcessorMode::Abort ||
                       new_mode == ProcessorMode::Undefined ||
                       new_mode == ProcessorMode::System);

    if (!valid_mode) {
        GBA_DEBUG_PRINT("=== INVALID CPSR MODE ===\n");
        GBA_DEBUG_PRINT("  Attempting to set CPSR=0x%08X (mode=0x%02X)\n", value, value & 0x1F);
        GBA_DEBUG_PRINT("  Current PC=0x%08X, CPSR=0x%08X, mode=%s\n",
                        m_regs[15], m_cpsr,
                        m_mode == ProcessorMode::System ? "System" :
                        m_mode == ProcessorMode::User ? "User" :
                        m_mode == ProcessorMode::IRQ ? "IRQ" :
                        m_mode == ProcessorMode::FIQ ? "FIQ" :
                        m_mode == ProcessorMode::Supervisor ? "SVC" :
                        m_mode == ProcessorMode::Abort ? "ABT" :
                        m_mode == ProcessorMode::Undefined ? "UND" : "???");
        GBA_DEBUG_PRINT("  Current SPSR=%08X\n", get_spsr());
        // Don't apply invalid mode - this would crash
        return;
    }

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
    // Validate the mode bits in the SPSR value being written
    // If the mode is invalid (0x00), preserve the current valid SPSR
    // This helps prevent corruption when game code incorrectly uses SPSR operations
    uint8_t mode_bits = value & 0x1F;
    bool valid_mode = (mode_bits == 0x10 || mode_bits == 0x11 || mode_bits == 0x12 ||
                       mode_bits == 0x13 || mode_bits == 0x17 || mode_bits == 0x1B ||
                       mode_bits == 0x1F);

    switch (m_mode) {
        case ProcessorMode::FIQ:
            if (valid_mode || value == m_spsr_fiq) m_spsr_fiq = value;
            break;
        case ProcessorMode::Supervisor:
            if (valid_mode || value == m_spsr_svc) m_spsr_svc = value;
            break;
        case ProcessorMode::Abort:
            if (valid_mode || value == m_spsr_abt) m_spsr_abt = value;
            break;
        case ProcessorMode::IRQ:
            if (valid_mode || value == m_spsr_irq) m_spsr_irq = value;
            break;
        case ProcessorMode::Undefined:
            if (valid_mode || value == m_spsr_und) m_spsr_und = value;
            break;
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

    // Save IRQ delay counter (added for 7-cycle IRQ delay implementation)
    data.push_back(static_cast<uint8_t>(m_irq_delay));

    // Save IntrWait state
    data.push_back(m_in_intr_wait ? 1 : 0);
    const uint8_t* flags_ptr = reinterpret_cast<const uint8_t*>(&m_intr_wait_flags);
    data.insert(data.end(), flags_ptr, flags_ptr + 2);
    const uint8_t* pc_ptr = reinterpret_cast<const uint8_t*>(&m_intr_wait_return_pc);
    data.insert(data.end(), pc_ptr, pc_ptr + 4);
    const uint8_t* cpsr_ptr2 = reinterpret_cast<const uint8_t*>(&m_intr_wait_return_cpsr);
    data.insert(data.end(), cpsr_ptr2, cpsr_ptr2 + 4);

    // Save prefetch buffer state
    const uint8_t* prefetch_head = reinterpret_cast<const uint8_t*>(&m_prefetch.head_address);
    data.insert(data.end(), prefetch_head, prefetch_head + 4);
    const uint8_t* prefetch_next = reinterpret_cast<const uint8_t*>(&m_prefetch.next_address);
    data.insert(data.end(), prefetch_next, prefetch_next + 4);
    data.push_back(static_cast<uint8_t>(m_prefetch.count));
    data.push_back(static_cast<uint8_t>(m_prefetch.countdown));
    data.push_back(m_prefetch.active ? 1 : 0);

    // Save last fetch address
    const uint8_t* last_fetch = reinterpret_cast<const uint8_t*>(&m_last_fetch_addr);
    data.insert(data.end(), last_fetch, last_fetch + 4);
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

    // Load IRQ delay counter (added for 7-cycle IRQ delay implementation)
    if (remaining >= 1) {
        m_irq_delay = *data++; remaining--;
    } else {
        m_irq_delay = 0;
    }

    // Load IntrWait state (check if data is available for backwards compatibility)
    if (remaining >= 11) {
        m_in_intr_wait = *data++ != 0; remaining--;
        std::memcpy(&m_intr_wait_flags, data, 2); data += 2; remaining -= 2;
        std::memcpy(&m_intr_wait_return_pc, data, 4); data += 4; remaining -= 4;
        std::memcpy(&m_intr_wait_return_cpsr, data, 4); data += 4; remaining -= 4;
    } else {
        // Old save state without IntrWait data
        m_in_intr_wait = false;
        m_intr_wait_flags = 0;
        m_intr_wait_return_pc = 0;
        m_intr_wait_return_cpsr = 0;
    }

    // Load prefetch buffer state (check if data is available for backwards compatibility)
    if (remaining >= 15) {
        // New format with next_address and active
        std::memcpy(&m_prefetch.head_address, data, 4); data += 4; remaining -= 4;
        std::memcpy(&m_prefetch.next_address, data, 4); data += 4; remaining -= 4;
        m_prefetch.count = *data++; remaining--;
        m_prefetch.countdown = static_cast<int8_t>(*data++); remaining--;
        m_prefetch.active = *data++ != 0; remaining--;
        std::memcpy(&m_last_fetch_addr, data, 4); data += 4; remaining -= 4;
    } else if (remaining >= 10) {
        // Old format without next_address and active
        std::memcpy(&m_prefetch.head_address, data, 4); data += 4; remaining -= 4;
        m_prefetch.next_address = m_prefetch.head_address;
        m_prefetch.count = *data++; remaining--;
        m_prefetch.countdown = static_cast<int8_t>(*data++); remaining--;
        m_prefetch.active = m_prefetch.count > 0;
        std::memcpy(&m_last_fetch_addr, data, 4); data += 4; remaining -= 4;
    } else {
        // Old save state without prefetch data
        m_prefetch.reset();
        m_last_fetch_addr = 0xFFFFFFFF;
    }
}

// ============================================================================
// HLE BIOS Functions
// ============================================================================

void ARM7TDMI::hle_bios_call(uint8_t function) {
    GBA_DEBUG_PRINT("BIOS call: 0x%02X at PC=0x%08X\n", function, m_regs[15]);
    switch (function) {
        case 0x00:  // SoftReset
            bios_soft_reset();
            break;

        case 0x01:  // RegisterRamReset
        {
            // R0 contains flags for what to reset
            uint32_t flags = m_regs[0];
            GBA_DEBUG_PRINT("BIOS call: RegisterRamReset flags=0x%02X\n", flags);

            // bit 0 - Clear 256K EWRAM (0x02000000-0x0203FFFF)
            if (flags & 0x01) {
                for (uint32_t addr = 0x02000000; addr < 0x02040000; addr += 4) {
                    m_bus.write32(addr, 0);
                }
            }
            // bit 1 - Clear 32K IWRAM (0x03000000-0x03007FFF), except last 512 bytes (stack area)
            if (flags & 0x02) {
                for (uint32_t addr = 0x03000000; addr < 0x03007E00; addr += 4) {
                    m_bus.write32(addr, 0);
                }
            }
            // bit 2 - Clear Palette (0x05000000-0x050003FF)
            if (flags & 0x04) {
                for (uint32_t addr = 0x05000000; addr < 0x05000400; addr += 4) {
                    m_bus.write32(addr, 0);
                }
            }
            // bit 3 - Clear VRAM (0x06000000-0x06017FFF)
            if (flags & 0x08) {
                for (uint32_t addr = 0x06000000; addr < 0x06018000; addr += 4) {
                    m_bus.write32(addr, 0);
                }
            }
            // bit 4 - Clear OAM (0x07000000-0x070003FF)
            if (flags & 0x10) {
                for (uint32_t addr = 0x07000000; addr < 0x07000400; addr += 4) {
                    m_bus.write32(addr, 0);
                }
            }
            // bits 5-7: SIO, Sound, other registers - not implemented for now
            break;
        }

        case 0x02:  // Halt
            m_halted = true;
            break;

        case 0x03:  // Stop
            m_halted = true;
            break;

        case 0x04:  // IntrWait
            // R0: 1 = discard old flags, 0 = check existing
            // R1: interrupt flags to wait for
            //
            // The real BIOS implements this as a polling loop:
            // 1. If R0=1, clear the requested flags from 0x03007FF8
            // 2. Halt the CPU (write to HALTCNT)
            // 3. When IRQ wakes CPU, let IRQ handler run
            // 4. After IRQ handler returns, check if flag is set in 0x03007FF8
            // 5. If set, clear it and return; otherwise, halt again
            {
                GBA_DEBUG_PRINT("IntrWait: Called with R0=%u, R1=0x%04X\n",
                                m_regs[0], m_regs[1] & 0x3FFF);

                m_intr_wait_flags = m_regs[1] & 0x3FFF;

                // If R0 != 0, discard old flags (clear them from BIOS mirror)
                if (m_regs[0] != 0) {
                    uint16_t flags = read16(0x03007FF8);
                    flags &= ~m_intr_wait_flags;
                    write16(0x03007FF8, flags);
                    GBA_DEBUG_PRINT("IntrWait: Cleared old flags, BIOS_IF now=0x%04X\n", flags);
                } else {
                    // R0 == 0: Check if flag is already set
                    uint16_t flags = read16(0x03007FF8);
                    if (flags & m_intr_wait_flags) {
                        // Flag already set, clear it and return immediately
                        write16(0x03007FF8, flags & ~m_intr_wait_flags);
                        GBA_DEBUG_PRINT("IntrWait: Flag already set! Returning immediately\n");
                        break;
                    }
                }

                // Enter IntrWait state - we'll poll after each IRQ
                m_in_intr_wait = true;
                m_halted = true;

                GBA_DEBUG_PRINT("IntrWait: Entering halt, waiting for flags=0x%04X\n",
                                m_intr_wait_flags);
            }
            break;

        case 0x05:  // VBlankIntrWait
            // Equivalent to IntrWait(1, 1) - wait for VBlank
            // Always discards old flags and waits for a fresh VBlank
            {
                GBA_DEBUG_PRINT("VBlankIntrWait: Called\n");

                // Clear VBlank flag from BIOS IRQ mirror
                uint16_t flags = read16(0x03007FF8);
                flags &= ~0x0001;  // Clear VBlank flag
                write16(0x03007FF8, flags);

                // Enter IntrWait state waiting for VBlank
                m_in_intr_wait = true;
                m_intr_wait_flags = 0x0001;  // Wait for VBlank
                m_halted = true;

                GBA_DEBUG_PRINT("VBlankIntrWait: Entering halt, BIOS_IF=0x%04X\n", flags);
            }
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
            bios_bg_affine_set();
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
            bios_diff8bit_unfilter_wram();
            break;

        case 0x17:  // Diff8bitUnFilterVram
            bios_diff8bit_unfilter_vram();
            break;

        case 0x18:  // Diff16bitUnFilter
            bios_diff16bit_unfilter();
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

void ARM7TDMI::bios_soft_reset() {
    // SoftReset (SWI 0x00) - Full implementation per GBATEK
    // 1. Read return address flag from 0x03007FFA before clearing IWRAM
    // 2. Clear top 0x200 bytes of IWRAM (0x03007E00-0x03007FFF)
    // 3. Reset stack pointers: sp_svc=0x03007FE0, sp_irq=0x03007FA0, sp_sys=0x03007F00
    // 4. Clear R0-R12, LR_svc, SPSR_svc, LR_irq, SPSR_irq
    // 5. Enter System mode
    // 6. Jump to 0x08000000 (if flag==0) or 0x02000000 (if flag!=0)

    // Read the return address flag before clearing memory
    uint8_t return_flag = m_bus.read8(0x03007FFA);
    uint32_t return_address = (return_flag == 0) ? 0x08000000 : 0x02000000;

    // Clear top 0x200 bytes of IWRAM (stacks and BIOS IRQ area)
    for (uint32_t addr = 0x03007E00; addr < 0x03008000; addr += 4) {
        m_bus.write32(addr, 0);
    }

    // Clear R0-R12
    for (int i = 0; i <= 12; i++) {
        m_regs[i] = 0;
    }

    // Set stack pointers for each mode
    m_svc_regs[0] = 0x03007FE0;  // SP_svc
    m_svc_regs[1] = 0;           // LR_svc = 0
    m_irq_regs[0] = 0x03007FA0;  // SP_irq
    m_irq_regs[1] = 0;           // LR_irq = 0
    m_usr_sp_lr[0] = 0x03007F00; // SP_usr/sys
    m_usr_sp_lr[1] = 0;          // LR_usr/sys (will be overwritten below)

    // Clear SPSRs
    m_spsr_svc = 0;
    m_spsr_irq = 0;

    // Enter System mode (same as User but privileged)
    m_mode = ProcessorMode::System;
    m_cpsr = (m_cpsr & ~0x1F) | static_cast<uint32_t>(ProcessorMode::System);
    m_cpsr &= ~FLAG_T;  // Ensure ARM mode

    // Update current SP to System mode's SP
    m_regs[13] = m_usr_sp_lr[0];

    // Set LR to return address and jump
    m_regs[14] = return_address;
    m_regs[15] = return_address;

    // Flush pipeline for the mode switch
    flush_pipeline();
}

void ARM7TDMI::bios_bg_affine_set() {
    // R0 = source data pointer (20 bytes per entry)
    // R1 = destination pointer (16 bytes per entry)
    // R2 = number of calculations
    //
    // Source structure (20 bytes):
    //   s32 orig_center_x   (8.8 fixed point)
    //   s32 orig_center_y   (8.8 fixed point)
    //   s16 display_center_x
    //   s16 display_center_y
    //   s16 scale_x         (8.8 fixed point)
    //   s16 scale_y         (8.8 fixed point)
    //   u16 angle           (0-0xFFFF = 0-360 degrees)
    //   2 bytes padding
    //
    // Destination structure (16 bytes):
    //   s16 PA (dx)
    //   s16 PB (dmx)
    //   s16 PC (dy)
    //   s16 PD (dmy)
    //   s32 start_x
    //   s32 start_y

    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];
    uint32_t count = m_regs[2];

    for (uint32_t i = 0; i < count; i++) {
        // Read source data
        int32_t orig_center_x = static_cast<int32_t>(read32(src));
        int32_t orig_center_y = static_cast<int32_t>(read32(src + 4));
        int16_t display_center_x = static_cast<int16_t>(read16(src + 8));
        int16_t display_center_y = static_cast<int16_t>(read16(src + 10));
        int16_t scale_x = static_cast<int16_t>(read16(src + 12));
        int16_t scale_y = static_cast<int16_t>(read16(src + 14));
        uint16_t angle = read16(src + 16);
        src += 20;

        // Calculate sin/cos from angle (angle is 0-0xFFFF for full circle)
        // GBA BIOS only uses the upper 8 bits for the angle
        double rad = (angle / 65536.0) * 2.0 * 3.14159265358979;
        double sin_val = sin(rad);
        double cos_val = cos(rad);

        // Calculate affine matrix parameters (8.8 fixed point)
        // PA = cos(angle) / scaleX, PB = sin(angle) / scaleX
        // PC = -sin(angle) / scaleY, PD = cos(angle) / scaleY
        int16_t pa, pb, pc, pd;
        if (scale_x != 0) {
            pa = static_cast<int16_t>((cos_val * 256.0 * 256.0) / scale_x);
            pb = static_cast<int16_t>((sin_val * 256.0 * 256.0) / scale_x);
        } else {
            pa = 0;
            pb = 0;
        }
        if (scale_y != 0) {
            pc = static_cast<int16_t>((-sin_val * 256.0 * 256.0) / scale_y);
            pd = static_cast<int16_t>((cos_val * 256.0 * 256.0) / scale_y);
        } else {
            pc = 0;
            pd = 0;
        }

        // Calculate starting position (19.8 fixed point for backgrounds)
        // start_x = orig_center_x - (display_center_x * PA + display_center_y * PB)
        // start_y = orig_center_y - (display_center_x * PC + display_center_y * PD)
        int32_t start_x = orig_center_x - (display_center_x * pa + display_center_y * pb);
        int32_t start_y = orig_center_y - (display_center_x * pc + display_center_y * pd);

        // Write destination data
        write16(dst, static_cast<uint16_t>(pa));
        write16(dst + 2, static_cast<uint16_t>(pb));
        write16(dst + 4, static_cast<uint16_t>(pc));
        write16(dst + 6, static_cast<uint16_t>(pd));
        write32(dst + 8, static_cast<uint32_t>(start_x));
        write32(dst + 12, static_cast<uint32_t>(start_y));
        dst += 16;
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
        // sx and sy are 8.8 fixed point, output is also 8.8 fixed point
        int16_t pa = static_cast<int16_t>((cos_val * 256.0 * 256.0) / sx);
        int16_t pb = static_cast<int16_t>((-sin_val * 256.0 * 256.0) / sx);
        int16_t pc = static_cast<int16_t>((sin_val * 256.0 * 256.0) / sy);
        int16_t pd = static_cast<int16_t>((cos_val * 256.0 * 256.0) / sy);

        // Write affine parameters using R3 as the offset between each parameter
        // For standard OAM: offset=8 (writes to OAM+6, OAM+14, OAM+22, OAM+30)
        // For custom buffer: offset=2 (writes consecutive 16-bit values)
        write16(dst, static_cast<uint16_t>(pa));
        write16(dst + offset, static_cast<uint16_t>(pb));
        write16(dst + offset * 2, static_cast<uint16_t>(pc));
        write16(dst + offset * 3, static_cast<uint16_t>(pd));

        dst += offset * 4;  // Move to next group of 4 parameters
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

void ARM7TDMI::bios_diff8bit_unfilter_wram() {
    // Diff8bitUnFilterWram - SWI 0x16
    // R0: Source address (compressed data)
    // R1: Destination address (decompressed data)
    //
    // Data format:
    // - Header (4 bytes): Bit 4-7 = type (0x80), Bit 8-31 = decompressed size
    // - Data: Differential 8-bit values

    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];

    uint32_t header = read32(src);
    src += 4;

    uint32_t decomp_size = header >> 8;
    uint32_t decomp_end = dst + decomp_size;

    if (decomp_size == 0) return;

    // First byte is the base value
    uint8_t running_sum = read8(src++);
    write8(dst++, running_sum);

    // Each subsequent byte is a difference to add to the running sum
    while (dst < decomp_end) {
        uint8_t diff = read8(src++);
        running_sum += diff;
        write8(dst++, running_sum);
    }
}

void ARM7TDMI::bios_diff8bit_unfilter_vram() {
    // Diff8bitUnFilterVram - SWI 0x17
    // Same as Wram but writes 16-bit at a time for VRAM compatibility
    // VRAM only supports 16-bit writes

    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];

    uint32_t header = read32(src);
    src += 4;

    uint32_t decomp_size = header >> 8;

    if (decomp_size == 0) return;

    // First byte is the base value
    uint8_t running_sum = read8(src++);
    uint32_t bytes_processed = 1;

    // Process in pairs - always write 16 bits at a time
    while (bytes_processed <= decomp_size) {
        uint8_t lo = running_sum;  // Current value

        // Get next byte if available
        uint8_t hi = 0;
        if (bytes_processed < decomp_size) {
            uint8_t diff = read8(src++);
            running_sum += diff;
            hi = running_sum;
            bytes_processed++;
        }

        write16(dst, lo | (hi << 8));
        dst += 2;

        // Prepare next low byte
        if (bytes_processed < decomp_size) {
            uint8_t diff = read8(src++);
            running_sum += diff;
            bytes_processed++;
        } else {
            break;
        }
    }
}

void ARM7TDMI::bios_diff16bit_unfilter() {
    // Diff16bitUnFilter - SWI 0x18
    // R0: Source address (compressed data)
    // R1: Destination address (decompressed data)
    //
    // Data format:
    // - Header (4 bytes): Bit 4-7 = type (0x81), Bit 8-31 = decompressed size
    // - Data: Differential 16-bit values

    uint32_t src = m_regs[0];
    uint32_t dst = m_regs[1];

    uint32_t header = read32(src);
    src += 4;

    uint32_t decomp_size = header >> 8;
    uint32_t decomp_end = dst + decomp_size;

    if (decomp_size == 0) return;

    // First halfword is the base value
    uint16_t running_sum = read16(src);
    src += 2;
    write16(dst, running_sum);
    dst += 2;

    // Each subsequent halfword is a difference to add to the running sum
    while (dst < decomp_end) {
        uint16_t diff = read16(src);
        src += 2;
        running_sum += diff;
        write16(dst, running_sum);
        dst += 2;
    }
}

} // namespace gba
