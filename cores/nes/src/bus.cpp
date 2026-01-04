#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"
#include "mappers/mapper.hpp"
#include "debug.hpp"

#include <cstdio>
#include <cstring>

namespace nes {

Bus::Bus() {
    m_ram.fill(0);
}

Bus::~Bus() = default;

// Tick PPU and APU for one CPU cycle (3 PPU cycles, 1 APU cycle)
// This is called for every memory access in cycle-accurate mode.
//
// IMPORTANT: This implements the cycle-accurate synchronization model:
// - PPU runs at 3x CPU clock, so 3 PPU dots per CPU cycle
// - APU runs at CPU clock, so 1 APU tick per CPU cycle
// - NMI edge detection happens after each PPU step, so the CPU can "see"
//   NMI at the correct point within an instruction
// - Mapper IRQ counters are clocked via PPU A12 notifications
//
// Returns true if an NMI edge was detected during this cycle.
bool Bus::tick() {
    if (!m_cycle_accurate) return false;

    m_cpu_cycles++;
    bool nmi_detected = false;

    // Tick PPU 3 times per CPU cycle
    // NMI detection is done by PPU::step() internally which sets m_nmi_triggered
    // We check for NMI after each PPU step to detect the edge accurately
    if (m_ppu) {
        m_ppu->step();
        m_ppu->step();
        m_ppu->step();

        // Check if NMI was triggered during these PPU cycles
        // This allows the CPU to detect NMI edges mid-instruction
        //
        // For cycle-accurate NMI timing per blargg's cpu_interrupts tests:
        // - NMI is edge-triggered: we need to detect when NMI line goes high
        // - The CPU samples NMI during each CPU cycle
        // - If NMI is detected, it will fire after the current instruction
        int nmi_type = m_ppu->check_nmi();
        if (nmi_type != 0 && m_cpu) {
            nmi_detected = true;
            if (nmi_type == 1) {
                // Immediate NMI - trigger now
                m_cpu->trigger_nmi();
            } else {
                // Delayed NMI - will fire after the next instruction completes
                m_cpu->trigger_nmi_delayed();
            }
        }
    }

    // Tick APU once per CPU cycle
    // APU frame counter and channel timers advance here
    if (m_apu) {
        m_apu->step(1);
    }

    // Clock mapper for IRQ counters and expansion audio
    // Note: MMC3 A12 clocking happens via notify_ppu_address_bus during PPU step
    if (m_cartridge) {
        m_cartridge->cpu_cycle();
    }

    // Update IRQ line state for the CPU
    // This is level-triggered, so we update it every cycle
    if (m_cpu) {
        bool mapper_irq = m_cartridge && m_ppu ? m_cartridge->irq_pending(m_ppu->get_frame_cycle()) : false;
        bool apu_irq = m_apu ? m_apu->irq_pending() : false;
        m_cpu->set_irq_line(mapper_irq || apu_irq);
    }

    return nmi_detected;
}

void Bus::tick_ppu_only(int ppu_cycles) {
    if (!m_cycle_accurate) return;

    if (m_ppu) {
        for (int i = 0; i < ppu_cycles; i++) {
            m_ppu->step();
        }
    }
}

void Bus::check_interrupts() {
    if (!m_cpu || !m_ppu) return;

    // Check for NMI from PPU
    int nmi_type = m_ppu->check_nmi();
    if (nmi_type == 1) {
        m_cpu->trigger_nmi();
    } else if (nmi_type == 2) {
        m_cpu->trigger_nmi_delayed();
    }

    // Check for IRQ from mapper and APU
    bool mapper_irq = m_cartridge ? m_cartridge->irq_pending(m_ppu->get_frame_cycle()) : false;
    bool apu_irq = m_apu ? m_apu->irq_pending() : false;
    m_cpu->set_irq_line(mapper_irq || apu_irq);
}

bool Bus::poll_irq_status() {
    // Poll IRQ status from all sources
    bool mapper_irq = m_cartridge && m_ppu ? m_cartridge->irq_pending(m_ppu->get_frame_cycle()) : false;
    bool apu_irq = m_apu ? m_apu->irq_pending() : false;
    return mapper_irq || apu_irq;
}

uint8_t Bus::cpu_read(uint16_t address) {
    // Tick PPU/APU for this memory access cycle
    tick();

    return cpu_peek(address);
}

uint8_t Bus::cpu_peek(uint16_t address) const {
    if (address < 0x2000) {
        // Internal RAM (mirrored)
        return m_ram[address & 0x07FF];
    }
    else if (address < 0x4000) {
        // PPU registers (mirrored every 8 bytes)
        return m_ppu ? const_cast<PPU*>(m_ppu)->cpu_read(address & 0x0007) : 0;
    }
    else if (address < 0x4020) {
        // APU and I/O registers
        if (address == 0x4016) {
            return const_cast<Bus*>(this)->read_controller(0);
        }
        else if (address == 0x4017) {
            return const_cast<Bus*>(this)->read_controller(1);
        }
        else if (m_apu) {
            return const_cast<APU*>(m_apu)->cpu_read(address);
        }
        return 0;
    }
    else {
        // Cartridge space
        return m_cartridge ? const_cast<Cartridge*>(m_cartridge)->cpu_read(address) : 0;
    }
}

void Bus::cpu_write(uint16_t address, uint8_t value) {
    // Tick PPU/APU for this memory access cycle
    tick();

    if (address < 0x2000) {
        // Internal RAM (mirrored)
        m_ram[address & 0x07FF] = value;
    }
    else if (address < 0x4000) {
        // PPU registers (mirrored every 8 bytes)
        if (m_ppu) {
            m_ppu->cpu_write(address & 0x0007, value);
        }
    }
    else if (address < 0x4020) {
        // APU and I/O registers
        if (address == 0x4014) {
            // OAM DMA - start cycle-accurate DMA
            start_oam_dma(value);
        }
        else if (address == 0x4016) {
            // Controller strobe
            m_controller_strobe = (value & 1) != 0;
            if (m_controller_strobe) {
                m_controller_shift[0] = static_cast<uint8_t>(m_controller_state[0]);
                m_controller_shift[1] = static_cast<uint8_t>(m_controller_state[1]);
            }
        }
        else if (m_apu) {
            // Set the CPU cycle counter for accurate APU timing
            // This is critical for the $4017 jitter test - the APU needs to know
            // the exact CPU cycle when the write occurs
            m_apu->set_cpu_cycle(m_cpu_cycles);
            m_apu->cpu_write(address, value);
        }
    }
    else {
        // Cartridge space
        if (m_cartridge) {
            m_cartridge->cpu_write(address, value);
        }
    }
}

uint8_t Bus::ppu_read(uint16_t address, uint32_t frame_cycle) {
    address &= 0x3FFF;

    if (address < 0x2000) {
        // Pattern tables (CHR ROM/RAM)
        return m_cartridge ? m_cartridge->ppu_read(address, frame_cycle) : 0;
    }
    else {
        // Nametables and palettes handled by PPU
        return m_ppu ? m_ppu->ppu_read(address) : 0;
    }
}

void Bus::ppu_write(uint16_t address, uint8_t value) {
    address &= 0x3FFF;

    if (address < 0x2000) {
        // Pattern tables (CHR RAM)
        if (m_cartridge) {
            m_cartridge->ppu_write(address, value);
        }
    }
    else {
        // Nametables and palettes handled by PPU
        if (m_ppu) {
            m_ppu->ppu_write(address, value);
        }
    }
}

void Bus::set_controller_state(int controller, uint32_t buttons) {
    if (controller >= 0 && controller < 2) {
        // Map from VirtualButton format to NES format
        // VirtualButton: A=0x001, B=0x002, X=0x004, Y=0x008, L=0x010, R=0x020,
        //                Start=0x040, Select=0x080, Up=0x100, Down=0x200, Left=0x400, Right=0x800
        // NES order: A, B, Select, Start, Up, Down, Left, Right
        uint8_t nes_buttons = 0;
        if (buttons & 0x001) nes_buttons |= 0x01;  // A
        if (buttons & 0x002) nes_buttons |= 0x02;  // B
        if (buttons & 0x080) nes_buttons |= 0x04;  // Select (VirtualButton = 0x080)
        if (buttons & 0x040) nes_buttons |= 0x08;  // Start (VirtualButton = 0x040)
        if (buttons & 0x100) nes_buttons |= 0x10;  // Up (VirtualButton = 0x100)
        if (buttons & 0x200) nes_buttons |= 0x20;  // Down (VirtualButton = 0x200)
        if (buttons & 0x400) nes_buttons |= 0x40;  // Left (VirtualButton = 0x400)
        if (buttons & 0x800) nes_buttons |= 0x80;  // Right (VirtualButton = 0x800)

        m_controller_state[controller] = nes_buttons;
    }
}

uint8_t Bus::read_controller(int controller) {
    if (controller < 0 || controller >= 2) return 0;

    uint8_t data = m_controller_shift[controller] & 1;
    m_controller_shift[controller] >>= 1;
    m_controller_shift[controller] |= 0x80;  // Fill with 1s

    return data | 0x40;  // Open bus bits
}

void Bus::start_oam_dma(uint8_t page) {
    m_dma_active = true;
    m_dma_page = page;
    m_dma_cycle = 0;
    m_dma_data = 0;
    // DMA starts on an odd CPU cycle, so we may need an extra alignment cycle
    // The CPU cycle count is tracked in m_cpu_cycles
    m_dma_dummy_cycle = (m_cpu_cycles & 1) == 1;
}

void Bus::run_dma_cycle() {
    if (!m_dma_active) return;

    // DMA takes 513 or 514 cycles total:
    // - 1 or 2 dummy cycles (alignment)
    // - 256 read cycles
    // - 256 write cycles

    if (m_dma_dummy_cycle) {
        // Extra alignment cycle
        tick();
        m_dma_dummy_cycle = false;
        return;
    }

    int byte_index = m_dma_cycle / 2;
    bool is_read = (m_dma_cycle & 1) == 0;

    if (byte_index >= 256) {
        // DMA complete
        m_dma_active = false;
        return;
    }

    tick();  // Tick for this DMA cycle

    if (is_read) {
        // Read cycle
        uint16_t addr = (static_cast<uint16_t>(m_dma_page) << 8) | byte_index;
        m_dma_data = cpu_peek(addr);  // Peek, don't double-tick
    } else {
        // Write cycle
        if (m_ppu) {
            m_ppu->oam_write(byte_index, m_dma_data);
        }
    }

    m_dma_cycle++;
}

int Bus::get_pending_dma_cycles() {
    // Legacy compatibility - DMA is now handled inline
    return 0;
}

void Bus::mapper_scanline() {
    if (m_cartridge) {
        m_cartridge->scanline();
    }
}

bool Bus::mapper_irq_pending(uint32_t frame_cycle) {
    if (m_cartridge) {
        return m_cartridge->irq_pending(frame_cycle);
    }
    return false;
}

void Bus::mapper_irq_clear() {
    if (m_cartridge) {
        m_cartridge->irq_clear();
    }
}

void Bus::mapper_cpu_cycles(int count) {
    if (m_cartridge) {
        m_cartridge->cpu_cycles(count);
    }
}

void Bus::mapper_cpu_cycle() {
    if (m_cartridge) {
        m_cartridge->cpu_cycle();
    }
}

float Bus::get_mapper_audio() const {
    if (m_cartridge) {
        return m_cartridge->get_audio_output();
    }
    return 0.0f;
}

void Bus::notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr, uint32_t frame_cycle) {
    if (m_cartridge) {
        m_cartridge->notify_ppu_addr_change(old_addr, new_addr, frame_cycle);
    }
}

void Bus::notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle) {
    if (m_cartridge) {
        m_cartridge->notify_ppu_address_bus(address, frame_cycle);
    }
}

void Bus::notify_frame_start() {
    if (m_cartridge) {
        m_cartridge->notify_frame_start();
    }
}

int Bus::get_mirror_mode() const {
    if (m_cartridge) {
        // Return the actual mirror mode value:
        // 0=Horizontal, 1=Vertical, 2=SingleScreen0, 3=SingleScreen1, 4=FourScreen
        return static_cast<int>(m_cartridge->get_mirror_mode());
    }
    return 0;  // Default to horizontal
}

void Bus::check_test_output() {
    // Only check in debug mode
    if (!is_debug_mode()) return;

    // Check for test ROM signature: 0xDE 0xB0 0x61 at $6001-$6003
    uint8_t sig1 = cpu_peek(0x6001);
    uint8_t sig2 = cpu_peek(0x6002);
    uint8_t sig3 = cpu_peek(0x6003);

    // Debug: show what's at $6000 (every time, until signature found)
    static int check_count = 0;
    if (check_count < 10 && !(sig1 == 0xDE && sig2 == 0xB0 && sig3 == 0x61)) {
        fprintf(stderr, "Test check #%d: $6000=%02X sig=%02X %02X %02X\n",
                check_count++, cpu_peek(0x6000), sig1, sig2, sig3);
    }

    if (sig1 == 0xDE && sig2 == 0xB0 && sig3 == 0x61) {
        uint8_t status = cpu_peek(0x6000);

        // Status: 0x80 = running, 0x81 = needs reset, 0x00-0x7F = finished with result
        static bool result_printed = false;
        static uint8_t last_status = 0x80;
        if (status < 0x80 && !result_printed) {
            result_printed = true;
            last_status = status;
            fprintf(stderr, "\n=== TEST ROM RESULT ===\n");
            fprintf(stderr, "Status code: %d (%s)\n", status,
                    status == 0 ? "PASSED" : "FAILED");

            // Read text output from $6004
            fprintf(stderr, "Output: ");
            for (int i = 0; i < 200; i++) {
                uint8_t c = cpu_peek(0x6004 + i);
                if (c == 0) break;
                if (c >= 32 && c < 127) {
                    fputc(c, stderr);
                } else if (c == '\n') {
                    fputc('\n', stderr);
                }
            }
            fprintf(stderr, "\n=======================\n");
        }
    }
}

// Save state serialization
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

    void write_array(std::vector<uint8_t>& data, const uint8_t* arr, size_t size) {
        data.insert(data.end(), arr, arr + size);
    }

    bool read_array(const uint8_t*& data, size_t& remaining, uint8_t* arr, size_t size) {
        if (remaining < size) return false;
        std::memcpy(arr, data, size);
        data += size;
        remaining -= size;
        return true;
    }
}

void Bus::save_state(std::vector<uint8_t>& data) {
    // Save RAM
    write_array(data, m_ram.data(), m_ram.size());

    // Save controller state
    write_value(data, m_controller_state[0]);
    write_value(data, m_controller_state[1]);
    write_value(data, m_controller_shift[0]);
    write_value(data, m_controller_shift[1]);
    write_value(data, static_cast<uint8_t>(m_controller_strobe ? 1 : 0));

    // Save DMA state
    write_value(data, static_cast<uint8_t>(m_dma_active ? 1 : 0));
    write_value(data, m_dma_page);
    write_value(data, m_dma_cycle);
    write_value(data, m_dma_data);
    write_value(data, static_cast<uint8_t>(m_dma_dummy_cycle ? 1 : 0));

    // Save cycle counter
    write_value(data, m_cpu_cycles);
}

void Bus::load_state(const uint8_t*& data, size_t& remaining) {
    // Load RAM
    read_array(data, remaining, m_ram.data(), m_ram.size());

    // Load controller state
    read_value(data, remaining, m_controller_state[0]);
    read_value(data, remaining, m_controller_state[1]);
    read_value(data, remaining, m_controller_shift[0]);
    read_value(data, remaining, m_controller_shift[1]);
    uint8_t strobe;
    read_value(data, remaining, strobe);
    m_controller_strobe = strobe != 0;

    // Load DMA state (if present - new format)
    if (remaining >= sizeof(uint8_t) + sizeof(uint8_t) + sizeof(int) + sizeof(uint8_t) + sizeof(uint8_t)) {
        uint8_t dma_active;
        read_value(data, remaining, dma_active);
        m_dma_active = dma_active != 0;
        read_value(data, remaining, m_dma_page);
        read_value(data, remaining, m_dma_cycle);
        read_value(data, remaining, m_dma_data);
        uint8_t dma_dummy;
        read_value(data, remaining, dma_dummy);
        m_dma_dummy_cycle = dma_dummy != 0;
    } else {
        m_dma_active = false;
    }

    // Load cycle counter (if present - new format)
    if (remaining >= sizeof(uint64_t)) {
        read_value(data, remaining, m_cpu_cycles);
    }
}

} // namespace nes
