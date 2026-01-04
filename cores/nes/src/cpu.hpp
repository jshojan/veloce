#pragma once

#include <cstdint>
#include <vector>

namespace nes {

class Bus;

// 6502 CPU emulator (cycle-accurate)
// Memory accesses tick PPU/APU through the Bus
class CPU {
public:
    explicit CPU(Bus& bus);
    ~CPU();

    // Reset the CPU
    void reset();

    // Execute one instruction
    // Returns the number of cycles consumed (for statistics only - PPU/APU
    // are already ticked during memory accesses)
    int step();

    // Interrupts
    void trigger_nmi();
    void trigger_nmi_delayed();  // NMI will fire after NEXT instruction
    void trigger_irq();  // Edge-triggered (BRK, etc.)
    void set_irq_line(bool active);  // Level-triggered (mapper IRQ)
    void set_nmi_line(bool active);  // NMI line state for edge detection

    // Check if NMI is pending (for cycle-accurate detection)
    bool is_nmi_pending() const { return m_nmi_pending; }

    // Poll interrupts during instruction execution
    // Called during the penultimate cycle of each instruction
    void poll_interrupts();

    // Detect NMI edge (called after each PPU step via bus)
    // Returns true if NMI edge was detected
    bool detect_nmi_edge();

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
    // Memory access (these tick PPU/APU via the bus)
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    // Internal cycle (tick PPU/APU without memory access)
    // Used for implied mode operations, branch penalty cycles, etc.
    void tick_internal();

    // Stack operations
    void push(uint8_t value);
    uint8_t pop();
    void push16(uint16_t value);
    uint16_t pop16();

    // Addressing modes - return address and add to cycles if page crossed
    // For write operations, set is_write=true to always do the dummy read
    uint16_t addr_immediate();
    uint16_t addr_zero_page();
    uint16_t addr_zero_page_x();
    uint16_t addr_zero_page_y();
    uint16_t addr_absolute();
    uint16_t addr_absolute_x(bool& page_crossed, bool is_write = false);
    uint16_t addr_absolute_y(bool& page_crossed, bool is_write = false);
    uint16_t addr_indirect();
    uint16_t addr_indirect_x();
    uint16_t addr_indirect_y(bool& page_crossed, bool is_write = false);

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
    void op_branch(bool condition);  // Handles its own internal cycles
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

    // NMI edge detection - tracks whether we've seen the edge
    // NMI is edge-triggered: we detect when it goes from low to high
    bool m_nmi_line = false;      // Current NMI line state
    bool m_prev_nmi_line = false; // Previous NMI line state (for edge detection)

    // CLI/SEI latency: The I flag state from before the previous instruction
    // is what's used for IRQ polling. This simulates the fact that interrupt
    // polling happens during the second-to-last cycle of each instruction,
    // and CLI/SEI change the flag AFTER that polling occurs.
    bool m_prev_irq_inhibit = true;  // Start with IRQ inhibited (matches reset I=1)

    // Track if we're currently in an interrupt sequence
    bool m_in_interrupt_sequence = false;

    // Cycle counter (for statistics)
    int m_cycles = 0;

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
