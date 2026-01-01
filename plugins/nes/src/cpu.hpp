#pragma once

#include <cstdint>
#include <vector>

namespace nes {

class Bus;

// 6502 CPU emulator (cycle-accurate)
class CPU {
public:
    explicit CPU(Bus& bus);
    ~CPU();

    // Reset the CPU
    void reset();

    // Execute one instruction, return cycles consumed
    int step();

    // Interrupts
    void trigger_nmi();
    void trigger_nmi_delayed();  // NMI will fire after NEXT instruction
    void trigger_irq();  // Edge-triggered (BRK, etc.)
    void set_irq_line(bool active);  // Level-triggered (mapper IRQ)

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

    // Register access (for debugging)
    uint16_t get_pc() const { return m_pc; }
    uint8_t get_a() const { return m_a; }
    uint8_t get_x() const { return m_x; }
    uint8_t get_y() const { return m_y; }
    uint8_t get_sp() const { return m_sp; }
    uint8_t get_status() const { return m_status; }

private:
    // Memory access
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    // Stack operations
    void push(uint8_t value);
    uint8_t pop();
    void push16(uint16_t value);
    uint16_t pop16();

    // Addressing modes - return address and add to cycles if page crossed
    uint16_t addr_immediate();
    uint16_t addr_zero_page();
    uint16_t addr_zero_page_x();
    uint16_t addr_zero_page_y();
    uint16_t addr_absolute();
    uint16_t addr_absolute_x(bool& page_crossed);
    uint16_t addr_absolute_y(bool& page_crossed);
    uint16_t addr_indirect();
    uint16_t addr_indirect_x();
    uint16_t addr_indirect_y(bool& page_crossed);

    // Flag operations
    void set_flag(uint8_t flag, bool value);
    bool get_flag(uint8_t flag) const;
    void update_zero_negative(uint8_t value);

    // Instructions
    void op_adc(uint8_t value);
    void op_and(uint8_t value);
    void op_asl(uint16_t address);
    void op_asl_a();
    void op_bit(uint8_t value);
    int op_branch(bool condition);  // Returns extra cycles (0, 1, or 2)
    void op_brk();
    void op_cmp(uint8_t reg, uint8_t value);
    void op_dec(uint16_t address);
    void op_eor(uint8_t value);
    void op_inc(uint16_t address);
    void op_jmp(uint16_t address);
    void op_jsr(uint16_t address);
    void op_lda(uint8_t value);
    void op_ldx(uint8_t value);
    void op_ldy(uint8_t value);
    void op_lsr(uint16_t address);
    void op_lsr_a();
    void op_ora(uint8_t value);
    void op_rol(uint16_t address);
    void op_rol_a();
    void op_ror(uint16_t address);
    void op_ror_a();
    void op_rti();
    void op_rts();
    void op_sbc(uint8_t value);
    void op_sta(uint16_t address);
    void op_stx(uint16_t address);
    void op_sty(uint16_t address);

    // Bus reference
    Bus& m_bus;

    // Registers
    uint16_t m_pc = 0;      // Program counter
    uint8_t m_a = 0;        // Accumulator
    uint8_t m_x = 0;        // X index register
    uint8_t m_y = 0;        // Y index register
    uint8_t m_sp = 0xFD;    // Stack pointer
    uint8_t m_status = 0x24; // Status register

    // Interrupt flags
    bool m_nmi_pending = false;
    bool m_nmi_delayed = false;  // NMI will fire after next instruction
    bool m_irq_pending = false;

    // Status register flags
    static constexpr uint8_t FLAG_C = 0x01;  // Carry
    static constexpr uint8_t FLAG_Z = 0x02;  // Zero
    static constexpr uint8_t FLAG_I = 0x04;  // Interrupt disable
    static constexpr uint8_t FLAG_D = 0x08;  // Decimal (unused on NES)
    static constexpr uint8_t FLAG_B = 0x10;  // Break
    static constexpr uint8_t FLAG_U = 0x20;  // Unused (always 1)
    static constexpr uint8_t FLAG_V = 0x40;  // Overflow
    static constexpr uint8_t FLAG_N = 0x80;  // Negative
};

} // namespace nes
