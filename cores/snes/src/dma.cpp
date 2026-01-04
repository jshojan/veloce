#include "dma.hpp"
#include "bus.hpp"
#include "ppu.hpp"
#include "debug.hpp"
#include <cstring>

namespace snes {

DMA::DMA(Bus& bus) : m_bus(bus) {
    reset();
}

DMA::~DMA() = default;

void DMA::reset() {
    for (auto& ch : m_channels) {
        ch.dmap = 0xFF;
        ch.bbad = 0xFF;
        ch.a1t = 0xFFFF;
        ch.a1b = 0xFF;
        ch.das = 0xFFFF;
        ch.dasb = 0xFF;
        ch.a2a = 0xFFFF;
        ch.nltr = 0xFF;
        ch.hdma_do_transfer = false;
        ch.hdma_terminated = false;
        ch.hdma_line_counter = 0;
    }

    m_dma_active = false;
    m_dma_cycles = 0;
    m_hdmaen = 0;
}

uint8_t DMA::read(uint16_t address) {
    int channel = (address >> 4) & 0x07;
    int reg = address & 0x0F;

    const auto& ch = m_channels[channel];

    switch (reg) {
        case 0x00: return ch.dmap;
        case 0x01: return ch.bbad;
        case 0x02: return ch.a1t & 0xFF;
        case 0x03: return (ch.a1t >> 8) & 0xFF;
        case 0x04: return ch.a1b;
        case 0x05: return ch.das & 0xFF;
        case 0x06: return (ch.das >> 8) & 0xFF;
        case 0x07: return ch.dasb;
        case 0x08: return ch.a2a & 0xFF;
        case 0x09: return (ch.a2a >> 8) & 0xFF;
        case 0x0A: return ch.nltr;
        default: return 0xFF;  // Unused
    }
}

void DMA::write(uint16_t address, uint8_t value) {
    int channel = (address >> 4) & 0x07;
    int reg = address & 0x0F;

    auto& ch = m_channels[channel];

    // Debug: trace DMA register writes for channel 0 when setting up VRAM transfers
    static int dma_write_trace = 0;
    bool trace_this = is_debug_mode() && channel == 0 && dma_write_trace < 100;
    if (trace_this) {
        static const char* reg_names[] = {"DMAP", "BBAD", "A1TL", "A1TH", "A1B", "DASL", "DASH", "DASB", "A2AL", "A2AH", "NLTR"};
        if (reg <= 0x0A) {
            SNES_DEBUG_PRINT("DMA ch0 write $%04X (%s) = $%02X\n", 0x4300 + address, reg_names[reg], value);
        }
        dma_write_trace++;
    }

    switch (reg) {
        case 0x00: ch.dmap = value; break;
        case 0x01: ch.bbad = value; break;
        case 0x02: ch.a1t = (ch.a1t & 0xFF00) | value; break;
        case 0x03: ch.a1t = (ch.a1t & 0x00FF) | (value << 8); break;
        case 0x04: ch.a1b = value; break;
        case 0x05: ch.das = (ch.das & 0xFF00) | value; break;
        case 0x06: ch.das = (ch.das & 0x00FF) | (value << 8); break;
        case 0x07: ch.dasb = value; break;
        case 0x08: ch.a2a = (ch.a2a & 0xFF00) | value; break;
        case 0x09: ch.a2a = (ch.a2a & 0x00FF) | (value << 8); break;
        case 0x0A: ch.nltr = value; break;
    }
}

void DMA::write_mdmaen(uint8_t value) {
    if (value == 0) return;

    m_dma_active = true;
    m_dma_cycles = 8;  // DMA overhead

    // Process channels in priority order (0-7)
    for (int i = 0; i < 8; i++) {
        if (value & (1 << i)) {
            do_dma_transfer(i);
        }
    }

    m_dma_active = false;

    // Reference: bsnes dma.cpp - sets IRQ lock after DMA completion
    // This prevents NMI/IRQ from being serviced immediately after DMA,
    // which is important for timing-sensitive games.
    m_bus.set_irq_lock();
}

void DMA::write_hdmaen(uint8_t value) {
    // Detect newly-enabled channels and initialize them immediately.
    // On real hardware, HDMA channels must be enabled before hdma_init()
    // (at V=0) to participate in the frame. But games often enable HDMA
    // during their init code which runs after V=0, so we need to init
    // newly-enabled channels to work on the current frame.
    uint8_t newly_enabled = value & ~m_hdmaen;
    m_hdmaen = value;

    // Initialize any newly-enabled channels
    for (int i = 0; i < 8; i++) {
        if (newly_enabled & (1 << i)) {
            auto& ch = m_channels[i];

            ch.a2a = ch.a1t;  // Table address = A1T
            ch.hdma_terminated = false;
            ch.hdma_do_transfer = false;

            // Read first entry
            ch.nltr = hdma_read_table(i);
            ch.hdma_line_counter = ch.nltr & 0x7F;

            if (ch.nltr == 0) {
                ch.hdma_terminated = true;
            } else {
                ch.hdma_do_transfer = true;

                // For indirect mode, read indirect address
                if (ch.dmap & 0x40) {
                    ch.das = hdma_read_table(i);
                    ch.das |= hdma_read_table(i) << 8;
                }
            }
        }
    }
}

void DMA::do_dma_transfer(int channel) {
    auto& ch = m_channels[channel];

    // Get transfer mode
    int transfer_mode = ch.dmap & 0x07;
    bool direction = (ch.dmap & 0x80) != 0;  // 0 = A->B, 1 = B->A
    bool fixed = (ch.dmap & 0x08) != 0;
    bool decrement = (ch.dmap & 0x10) != 0;

    // B-bus address
    uint8_t b_addr = ch.bbad;

    // A-bus address
    uint32_t a_addr = (static_cast<uint32_t>(ch.a1b) << 16) | ch.a1t;

    // Byte count (0 = 65536)
    int count = ch.das;
    if (count == 0) count = 0x10000;

    SNES_DMA_DEBUG("DMA ch%d: mode=%d dir=%d a=$%06X b=$%02X count=%d\n",
                   channel, transfer_mode, direction, a_addr, b_addr, count);

    // Debug: Log VRAM DMAs with destination address
    if (is_debug_mode() && (b_addr == 0x18 || b_addr == 0x19)) {
        // Read VRAM address from PPU
        uint16_t vram_addr = m_bus.ppu().get_vram_addr();
        uint8_t vmain = m_bus.ppu().get_vmain();
        fprintf(stderr, "[SNES/DMA] VRAM DMA ch%d: src=$%06X -> vram=$%04X (byte $%05X) count=%d vmain=$%02X\n",
                channel, a_addr, vram_addr, vram_addr * 2, count, vmain);
        fprintf(stderr, "[SNES/DMA]   Source first 8: ");
        for (int i = 0; i < 8 && i < count; i++) {
            uint32_t src = (a_addr & 0xFF0000) | ((a_addr + i) & 0xFFFF);
            fprintf(stderr, "%02X ", m_bus.read(src));
        }
        fprintf(stderr, "\n");
    }

    // Debug: Log all CGDATA DMAs for palette tracking
    if (is_debug_mode() && b_addr == 0x22) {
        // Read a few source bytes to show what's being transferred
        fprintf(stderr, "[SNES/DMA] CGDATA DMA from $%06X (%d bytes), first 8: ", a_addr, count);
        for (int i = 0; i < 8 && i < count; i++) {
            uint32_t src = (a_addr & 0xFF0000) | ((a_addr + i) & 0xFFFF);
            fprintf(stderr, "%02X ", m_bus.read(src));
        }
        fprintf(stderr, "\n");
    }

    // Transfer patterns for each mode
    // Mode 0: 1 byte  (p)
    // Mode 1: 2 bytes (p, p+1)
    // Mode 2: 2 bytes (p, p)
    // Mode 3: 4 bytes (p, p, p+1, p+1)
    // Mode 4: 4 bytes (p, p+1, p+2, p+3)
    // Mode 5: 4 bytes (p, p+1, p, p+1) - same as mode 1 repeated
    // Mode 6: 2 bytes (p, p) - same as mode 2
    // Mode 7: 4 bytes (p, p, p+1, p+1) - same as mode 3

    static const int transfer_size[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    static const int b_offset[8][4] = {
        {0, 0, 0, 0},  // Mode 0
        {0, 1, 0, 1},  // Mode 1
        {0, 0, 0, 0},  // Mode 2
        {0, 0, 1, 1},  // Mode 3
        {0, 1, 2, 3},  // Mode 4
        {0, 1, 0, 1},  // Mode 5
        {0, 0, 0, 0},  // Mode 6
        {0, 0, 1, 1}   // Mode 7
    };

    int size = transfer_size[transfer_mode];
    int transferred = 0;

    while (transferred < count) {
        for (int i = 0; i < size && transferred < count; i++) {
            uint8_t b = b_addr + b_offset[transfer_mode][i];
            uint16_t b_full = 0x2100 + b;

            if (direction) {
                // B -> A
                uint8_t value = m_bus.read(b_full);
                m_bus.write(a_addr, value);
            } else {
                // A -> B
                uint8_t value = m_bus.read(a_addr);
                m_bus.write(b_full, value);
            }

            // Update A-bus address
            if (!fixed) {
                if (decrement) {
                    a_addr = ((a_addr & 0xFF0000) | ((a_addr - 1) & 0xFFFF));
                } else {
                    a_addr = ((a_addr & 0xFF0000) | ((a_addr + 1) & 0xFFFF));
                }
            }

            transferred++;
            m_dma_cycles += 8;  // Each byte takes 8 master cycles
        }
    }

    // Update channel registers
    ch.a1t = a_addr & 0xFFFF;
    ch.das = 0;  // Count becomes 0 after transfer
}

void DMA::hdma_init() {
    // Initialize HDMA at the start of frame (V=0)
    for (int i = 0; i < 8; i++) {
        if (m_hdmaen & (1 << i)) {
            auto& ch = m_channels[i];

            ch.a2a = ch.a1t;  // Table address = A1T
            ch.hdma_terminated = false;
            ch.hdma_do_transfer = false;

            // Read first entry
            ch.nltr = hdma_read_table(i);
            ch.hdma_line_counter = ch.nltr & 0x7F;

            if (ch.nltr == 0) {
                ch.hdma_terminated = true;
            } else {
                ch.hdma_do_transfer = true;

                // For indirect mode, read indirect address
                if (ch.dmap & 0x40) {
                    ch.das = hdma_read_table(i);
                    ch.das |= hdma_read_table(i) << 8;
                }
            }
        }
    }
}

void DMA::hdma_transfer() {
    // Process HDMA at the start of each H-blank
    for (int i = 0; i < 8; i++) {
        if ((m_hdmaen & (1 << i)) && !m_channels[i].hdma_terminated) {
            do_hdma_channel(i);
        }
    }
}

void DMA::do_hdma_channel(int channel) {
    auto& ch = m_channels[channel];

    if (ch.hdma_terminated) return;

    // bsnes-accurate HDMA timing:
    // 1. Check if line_counter == 0 FIRST - if so, reload next table entry
    // 2. Do transfer if do_transfer flag is set
    // 3. Decrement line_counter
    // 4. Update do_transfer based on new line_counter value

    // Step 1: Check for reload BEFORE transfer (bsnes hdmaAdvance checks first)
    if (ch.hdma_line_counter == 0) {
        // Read next table entry
        ch.nltr = hdma_read_table(channel);

        if (ch.nltr == 0) {
            ch.hdma_terminated = true;
            return;  // Terminate - no transfer on this scanline
        }

        ch.hdma_line_counter = ch.nltr & 0x7F;
        ch.hdma_do_transfer = true;

        // For indirect mode, read new indirect address
        if (ch.dmap & 0x40) {
            ch.das = hdma_read_table(channel);
            ch.das |= hdma_read_table(channel) << 8;
        }
    }

    // Step 2: Do transfer if needed
    if (ch.hdma_do_transfer) {
        int transfer_mode = ch.dmap & 0x07;
        bool indirect = (ch.dmap & 0x40) != 0;

        // Get source address
        uint32_t src_addr;
        if (indirect) {
            src_addr = (static_cast<uint32_t>(ch.dasb) << 16) | ch.das;
        } else {
            src_addr = (static_cast<uint32_t>(ch.a1b) << 16) | ch.a2a;
        }

        // B-bus address
        uint8_t b_addr = ch.bbad;

        // Transfer pattern based on mode
        static const int transfer_size[8] = {1, 2, 2, 4, 4, 4, 2, 4};
        static const int b_offset[8][4] = {
            {0, 0, 0, 0},
            {0, 1, 0, 1},
            {0, 0, 0, 0},
            {0, 0, 1, 1},
            {0, 1, 2, 3},
            {0, 1, 0, 1},
            {0, 0, 0, 0},
            {0, 0, 1, 1}
        };

        int size = transfer_size[transfer_mode];

        for (int i = 0; i < size; i++) {
            uint8_t value = m_bus.read(src_addr);
            uint16_t b_full = 0x2100 + b_addr + b_offset[transfer_mode][i];
            m_bus.write(b_full, value);
            src_addr++;
        }

        // Update address pointer after transfer
        if (indirect) {
            ch.das = src_addr & 0xFFFF;
        } else {
            // For direct mode, update table pointer A2A since data comes from table
            ch.a2a = src_addr & 0xFFFF;
        }
    }

    // Step 3: Decrement line counter
    ch.hdma_line_counter--;

    // Step 4: Update do_transfer for next scanline
    // If line_counter is now 0, set do_transfer = true (reload will happen next time)
    // Otherwise, use repeat flag
    if (ch.hdma_line_counter == 0) {
        ch.hdma_do_transfer = true;  // Force transfer on reload scanline
    } else {
        ch.hdma_do_transfer = (ch.nltr & 0x80) != 0;  // Use repeat flag
    }
}

uint8_t DMA::hdma_read_table(int channel) {
    auto& ch = m_channels[channel];
    uint32_t addr = (static_cast<uint32_t>(ch.a1b) << 16) | ch.a2a;
    uint8_t value = m_bus.read(addr);
    ch.a2a++;
    return value;
}

void DMA::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_hdmaen);

    for (const auto& ch : m_channels) {
        data.push_back(ch.dmap);
        data.push_back(ch.bbad);
        data.push_back(ch.a1t & 0xFF);
        data.push_back((ch.a1t >> 8) & 0xFF);
        data.push_back(ch.a1b);
        data.push_back(ch.das & 0xFF);
        data.push_back((ch.das >> 8) & 0xFF);
        data.push_back(ch.dasb);
        data.push_back(ch.a2a & 0xFF);
        data.push_back((ch.a2a >> 8) & 0xFF);
        data.push_back(ch.nltr);
        data.push_back(ch.hdma_do_transfer ? 1 : 0);
        data.push_back(ch.hdma_terminated ? 1 : 0);
        data.push_back(ch.hdma_line_counter);
    }
}

void DMA::load_state(const uint8_t*& data, size_t& remaining) {
    m_hdmaen = *data++; remaining--;

    for (auto& ch : m_channels) {
        ch.dmap = *data++; remaining--;
        ch.bbad = *data++; remaining--;
        ch.a1t = data[0] | (data[1] << 8);
        data += 2; remaining -= 2;
        ch.a1b = *data++; remaining--;
        ch.das = data[0] | (data[1] << 8);
        data += 2; remaining -= 2;
        ch.dasb = *data++; remaining--;
        ch.a2a = data[0] | (data[1] << 8);
        data += 2; remaining -= 2;
        ch.nltr = *data++; remaining--;
        ch.hdma_do_transfer = (*data++ != 0); remaining--;
        ch.hdma_terminated = (*data++ != 0); remaining--;
        ch.hdma_line_counter = *data++; remaining--;
    }
}

} // namespace snes
