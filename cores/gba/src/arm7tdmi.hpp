#pragma once

#include "types.hpp"
#include <cstdint>
#include <array>
#include <vector>

namespace gba {

class Bus;

// ARM7TDMI CPU emulator
// Supports both ARM (32-bit) and Thumb (16-bit) instruction sets
class ARM7TDMI {
public:
    explicit ARM7TDMI(Bus& bus);
    ~ARM7TDMI();

    // Reset the CPU to initial state
    void reset();

    // Execute one instruction, return cycles consumed
    int step();

    // Signal an IRQ (level-triggered)
    void signal_irq();

    // Save/load state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

    // Debug access
    uint32_t get_register(int reg) const;
    uint32_t get_cpsr() const { return m_cpsr; }
    uint32_t get_pc() const { return m_regs[15]; }
    bool is_thumb_mode() const { return m_cpsr & FLAG_T; }
    bool is_halted() const { return m_halted; }

private:
    // Memory access with proper bus timing
    uint8_t read8(uint32_t address);
    uint16_t read16(uint32_t address);
    uint32_t read32(uint32_t address);
    void write8(uint32_t address, uint8_t value);
    void write16(uint32_t address, uint16_t value);
    void write32(uint32_t address, uint32_t value);

    // Instruction fetch with pipeline emulation
    uint32_t fetch_arm();
    uint16_t fetch_thumb();
    void flush_pipeline();

    // ARM instruction execution
    int execute_arm(uint32_t instruction);
    bool check_condition(uint32_t instruction);

    // ARM instruction handlers
    int arm_branch(uint32_t instruction);
    int arm_branch_exchange(uint32_t instruction);
    int arm_data_processing(uint32_t instruction);
    int arm_multiply(uint32_t instruction);
    int arm_multiply_long(uint32_t instruction);
    int arm_single_data_transfer(uint32_t instruction);
    int arm_halfword_data_transfer(uint32_t instruction);
    int arm_block_data_transfer(uint32_t instruction);
    int arm_swap(uint32_t instruction);
    int arm_software_interrupt(uint32_t instruction);
    int arm_mrs(uint32_t instruction);
    int arm_msr(uint32_t instruction);
    int arm_undefined(uint32_t instruction);

    // ARM data processing operand calculation
    uint32_t arm_shift(uint32_t value, int shift_type, int amount, bool& carry_out, bool reg_shift);

    // Thumb instruction execution
    int execute_thumb(uint16_t instruction);

    // Thumb instruction handlers
    int thumb_move_shifted(uint16_t instruction);
    int thumb_add_subtract(uint16_t instruction);
    int thumb_immediate(uint16_t instruction);
    int thumb_alu(uint16_t instruction);
    int thumb_hi_reg_bx(uint16_t instruction);
    int thumb_pc_relative_load(uint16_t instruction);
    int thumb_load_store_reg(uint16_t instruction);
    int thumb_load_store_sign(uint16_t instruction);
    int thumb_load_store_imm(uint16_t instruction);
    int thumb_load_store_half(uint16_t instruction);
    int thumb_sp_relative_load_store(uint16_t instruction);
    int thumb_load_address(uint16_t instruction);
    int thumb_add_sp(uint16_t instruction);
    int thumb_push_pop(uint16_t instruction);
    int thumb_multiple_load_store(uint16_t instruction);
    int thumb_conditional_branch(uint16_t instruction);
    int thumb_software_interrupt(uint16_t instruction);
    int thumb_unconditional_branch(uint16_t instruction);
    int thumb_long_branch(uint16_t instruction);

    // Mode switching
    void switch_mode(ProcessorMode new_mode);
    void enter_exception(ProcessorMode mode, uint32_t vector);

    // HLE BIOS functions
    void hle_bios_call(uint8_t function);
    void bios_div();
    void bios_sqrt();
    void bios_arctan();
    void bios_arctan2();
    void bios_cpu_set();
    void bios_cpu_fast_set();
    void bios_obj_affine_set();
    void bios_bit_unpack();
    void bios_lz77_uncomp_wram();
    void bios_lz77_uncomp_vram();
    void bios_huff_uncomp();
    void bios_rl_uncomp_wram();
    void bios_rl_uncomp_vram();

    // Register bank access
    void bank_registers(ProcessorMode old_mode, ProcessorMode new_mode);

    // CPSR/SPSR access
    void set_cpsr(uint32_t value);
    uint32_t get_spsr() const;
    void set_spsr(uint32_t value);

    // Flag operations
    void set_nz_flags(uint32_t result);
    void set_nzc_flags(uint32_t result, bool carry);
    void set_nzcv_flags(uint32_t result, bool carry, bool overflow);

    // Bus reference
    Bus& m_bus;

    // General purpose registers (R0-R15)
    // R13 = SP, R14 = LR, R15 = PC
    std::array<uint32_t, 16> m_regs;

    // Banked registers for each mode
    // FIQ has R8-R14 banked, others have R13-R14 banked
    std::array<uint32_t, 7> m_fiq_regs;     // R8_fiq - R14_fiq
    std::array<uint32_t, 5> m_usr_regs;     // R8-R12 for non-FIQ modes (shared by User/System/IRQ/SVC/ABT/UND)
    std::array<uint32_t, 2> m_svc_regs;     // R13_svc, R14_svc
    std::array<uint32_t, 2> m_abt_regs;     // R13_abt, R14_abt
    std::array<uint32_t, 2> m_irq_regs;     // R13_irq, R14_irq
    std::array<uint32_t, 2> m_und_regs;     // R13_und, R14_und
    std::array<uint32_t, 2> m_usr_sp_lr;    // R13_usr, R14_usr (for User/System mode)

    // Current Program Status Register
    uint32_t m_cpsr = 0;

    // Saved Program Status Registers (one per exception mode)
    uint32_t m_spsr_fiq = 0;
    uint32_t m_spsr_svc = 0;
    uint32_t m_spsr_abt = 0;
    uint32_t m_spsr_irq = 0;
    uint32_t m_spsr_und = 0;

    // Pipeline state
    uint32_t m_pipeline[2] = {0, 0};  // 2-stage prefetch
    int m_pipeline_valid = 0;

    // IRQ state
    bool m_irq_pending = false;

    // Halt state (for low-power wait)
    bool m_halted = false;

    // IntrWait flags - which interrupts we're waiting for
    uint16_t m_intr_wait_flags = 0;

    // Current processor mode
    ProcessorMode m_mode = ProcessorMode::Supervisor;

    // CPSR flag bits
    static constexpr uint32_t FLAG_N = 1u << 31;  // Negative
    static constexpr uint32_t FLAG_Z = 1u << 30;  // Zero
    static constexpr uint32_t FLAG_C = 1u << 29;  // Carry
    static constexpr uint32_t FLAG_V = 1u << 28;  // Overflow
    static constexpr uint32_t FLAG_I = 1u << 7;   // IRQ disable
    static constexpr uint32_t FLAG_F = 1u << 6;   // FIQ disable
    static constexpr uint32_t FLAG_T = 1u << 5;   // Thumb state

    // Exception vectors
    static constexpr uint32_t VECTOR_RESET     = 0x00000000;
    static constexpr uint32_t VECTOR_UNDEFINED = 0x00000004;
    static constexpr uint32_t VECTOR_SWI       = 0x00000008;
    static constexpr uint32_t VECTOR_PREFETCH  = 0x0000000C;
    static constexpr uint32_t VECTOR_DATA      = 0x00000010;
    static constexpr uint32_t VECTOR_IRQ       = 0x00000018;
    static constexpr uint32_t VECTOR_FIQ       = 0x0000001C;
};

} // namespace gba
