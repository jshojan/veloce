#pragma once

#include <cstdint>
#include <vector>

namespace snes {

class Bus;

// Ricoh 5A22 CPU (65816 core) - 16-bit processor with 8-bit compatibility
// Reference: 65816 Programming Manual, anomie's SNES docs
class CPU {
public:
    explicit CPU(Bus& bus);
    ~CPU();

    // Reset the CPU to power-on state
    void reset();

    // Execute one instruction, return master cycles consumed
    // Master clock is 21.477 MHz, CPU runs at 3.58 MHz (6 master cycles per CPU cycle)
    int step();

    // Interrupts
    void trigger_nmi();
    void trigger_irq();
    void set_irq_line(bool active);

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

    // Register access (for debugging)
    uint16_t get_pc() const { return m_pc; }
    uint16_t get_a() const { return m_a; }
    uint16_t get_x() const { return m_x; }
    uint16_t get_y() const { return m_y; }
    uint16_t get_sp() const { return m_sp; }
    uint16_t get_dp() const { return m_dp; }
    uint8_t get_pbr() const { return m_pbr; }
    uint8_t get_dbr() const { return m_dbr; }
    uint8_t get_status() const { return m_status; }
    bool is_emulation_mode() const { return m_emulation; }
    bool get_interrupt_disable() const { return get_flag(FLAG_I); }

    // Get full 24-bit address
    uint32_t get_full_pc() const { return (static_cast<uint32_t>(m_pbr) << 16) | m_pc; }

private:
    // Memory access (adds appropriate cycles)
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t value);

    // Read from program bank
    uint8_t read_pc();
    uint16_t read_pc16();
    uint32_t read_pc24();

    // Read from data bank
    uint8_t read_db(uint16_t address);
    void write_db(uint16_t address, uint8_t value);

    // Stack operations
    void push8(uint8_t value);
    uint8_t pop8();
    void push16(uint16_t value);
    uint16_t pop16();
    void push24(uint32_t value);
    uint32_t pop24();

    // Addressing modes - return effective address
    uint32_t addr_immediate8();
    uint32_t addr_immediate16();
    uint32_t addr_immediate_m();  // 8 or 16 bit depending on M flag
    uint32_t addr_immediate_x();  // 8 or 16 bit depending on X flag
    uint32_t addr_direct();
    uint32_t addr_direct_x();
    uint32_t addr_direct_y();
    uint32_t addr_direct_indirect();
    uint32_t addr_direct_indirect_long();
    uint32_t addr_direct_x_indirect();
    uint32_t addr_direct_indirect_y();
    uint32_t addr_direct_indirect_long_y();
    uint32_t addr_absolute();
    uint32_t addr_absolute_x();
    uint32_t addr_absolute_y();
    uint32_t addr_absolute_long();
    uint32_t addr_absolute_long_x();
    uint32_t addr_absolute_indirect();
    uint32_t addr_absolute_indirect_long();
    uint32_t addr_absolute_x_indirect();
    uint32_t addr_stack_relative();
    uint32_t addr_stack_relative_indirect_y();

    // Flag operations
    void set_flag(uint8_t flag, bool value);
    bool get_flag(uint8_t flag) const;
    void update_nz8(uint8_t value);
    void update_nz16(uint16_t value);
    void update_nz_m(uint16_t value);  // Uses M flag to determine width

    // ALU operations
    void op_adc8(uint8_t value);
    void op_adc16(uint16_t value);
    void op_sbc8(uint8_t value);
    void op_sbc16(uint16_t value);
    void op_and8(uint8_t value);
    void op_and16(uint16_t value);
    void op_ora8(uint8_t value);
    void op_ora16(uint16_t value);
    void op_eor8(uint8_t value);
    void op_eor16(uint16_t value);
    void op_cmp8(uint8_t reg, uint8_t value);
    void op_cmp16(uint16_t reg, uint16_t value);
    void op_bit8(uint8_t value);
    void op_bit16(uint16_t value);
    void op_bit_imm8(uint8_t value);
    void op_bit_imm16(uint16_t value);

    // Shift/rotate operations
    uint8_t op_asl8(uint8_t value);
    uint16_t op_asl16(uint16_t value);
    uint8_t op_lsr8(uint8_t value);
    uint16_t op_lsr16(uint16_t value);
    uint8_t op_rol8(uint8_t value);
    uint16_t op_rol16(uint16_t value);
    uint8_t op_ror8(uint8_t value);
    uint16_t op_ror16(uint16_t value);

    // Increment/decrement
    uint8_t op_inc8(uint8_t value);
    uint16_t op_inc16(uint16_t value);
    uint8_t op_dec8(uint8_t value);
    uint16_t op_dec16(uint16_t value);

    // Test and set/reset bits
    uint8_t op_tsb8(uint8_t value);
    uint16_t op_tsb16(uint16_t value);
    uint8_t op_trb8(uint8_t value);
    uint16_t op_trb16(uint16_t value);

    // Branch helper
    void branch(bool condition);

    // Interrupt handling
    void do_interrupt(uint16_t vector, bool is_brk = false);

    // Execute single instruction
    void execute();

    // Bus reference
    Bus& m_bus;

    // Registers
    uint16_t m_a = 0;       // Accumulator (16-bit, or 8-bit in emulation mode)
    uint16_t m_x = 0;       // X index register (16-bit, or 8-bit with X flag)
    uint16_t m_y = 0;       // Y index register (16-bit, or 8-bit with X flag)
    uint16_t m_sp = 0x01FF; // Stack pointer (16-bit, forced to $01xx in emulation)
    uint16_t m_dp = 0;      // Direct page register (zero page relocation)
    uint16_t m_pc = 0;      // Program counter
    uint8_t m_pbr = 0;      // Program bank register (K)
    uint8_t m_dbr = 0;      // Data bank register (B)
    uint8_t m_status = 0x34; // Processor status (flags)
    bool m_emulation = true; // Emulation mode flag (E)

    // Interrupt state
    bool m_nmi_pending = false;
    bool m_irq_line = false;
    bool m_wai_waiting = false;  // Waiting for interrupt (WAI instruction)
    bool m_stp_stopped = false;  // Processor stopped (STP instruction)

    // Cycle counter for current instruction
    int m_cycles = 0;

    // Status register flags
    static constexpr uint8_t FLAG_C = 0x01;  // Carry
    static constexpr uint8_t FLAG_Z = 0x02;  // Zero
    static constexpr uint8_t FLAG_I = 0x04;  // IRQ disable
    static constexpr uint8_t FLAG_D = 0x08;  // Decimal mode
    static constexpr uint8_t FLAG_X = 0x10;  // Index register size (0=16-bit, 1=8-bit) / B in emulation
    static constexpr uint8_t FLAG_M = 0x20;  // Accumulator size (0=16-bit, 1=8-bit) / unused in emulation
    static constexpr uint8_t FLAG_V = 0x40;  // Overflow
    static constexpr uint8_t FLAG_N = 0x80;  // Negative

    // In emulation mode, bit 4 is B (break) flag, bit 5 is always 1
    static constexpr uint8_t FLAG_B = 0x10;  // Break (emulation mode only)

    // Interrupt vectors
    static constexpr uint16_t VEC_COP_NATIVE    = 0xFFE4;
    static constexpr uint16_t VEC_BRK_NATIVE    = 0xFFE6;
    static constexpr uint16_t VEC_ABORT_NATIVE  = 0xFFE8;
    static constexpr uint16_t VEC_NMI_NATIVE    = 0xFFEA;
    static constexpr uint16_t VEC_IRQ_NATIVE    = 0xFFEE;
    static constexpr uint16_t VEC_COP_EMU       = 0xFFF4;
    static constexpr uint16_t VEC_ABORT_EMU     = 0xFFF8;
    static constexpr uint16_t VEC_NMI_EMU       = 0xFFFA;
    static constexpr uint16_t VEC_RESET         = 0xFFFC;
    static constexpr uint16_t VEC_IRQ_BRK_EMU   = 0xFFFE;
};

} // namespace snes
