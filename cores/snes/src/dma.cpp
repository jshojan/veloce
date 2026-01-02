#include "dma.hpp"
#include "bus.hpp"
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
}

void DMA::write_hdmaen(uint8_t value) {
    m_hdmaen = value;
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

    // For VRAM DMAs (b=$18 or $19), also show the current VRAM address
    if (is_debug_mode() && (b_addr == 0x18 || b_addr == 0x19)) {
        // Read PPU's VRAM address via $2116/$2117 state
        // We can't directly access PPU, but we can note this is a VRAM DMA
        fprintf(stderr, "[SNES/DMA] VRAM DMA ch%d: mode=%d a=$%06X count=%d (b=$%02X)\n",
                channel, transfer_mode, a_addr, count, b_addr);
    }
    // For CGRAM DMAs (b=$22), trace the transfer - always show frame number
    static int cgram_dma_trace = 0;
    if (is_debug_mode() && b_addr == 0x22) {
        // Read first few bytes from source to see what's being transferred
        uint8_t src_bytes[8];
        for (int i = 0; i < 8; i++) {
            src_bytes[i] = m_bus.read((a_addr + i) & 0xFFFFFF);
        }
        // Always trace if src_bytes are different from proper palette or all zero
        bool all_zero = (src_bytes[2] == 0);  // Palette color 1 should be non-zero ($FF $7F)
        // Note: We can't easily access PPU frame from here, so trace unconditionally
        fprintf(stderr, "[SNES/DMA] CGRAM DMA ch%d: a=$%06X count=%d, src=[%02X %02X %02X %02X %02X %02X %02X %02X]%s (trace #%d)\n",
                channel, a_addr, count,
                src_bytes[0], src_bytes[1], src_bytes[2], src_bytes[3],
                src_bytes[4], src_bytes[5], src_bytes[6], src_bytes[7],
                all_zero ? " **BAD**" : "", cgram_dma_trace);
        cgram_dma_trace++;
    }
    SNES_DMA_DEBUG("DMA ch%d: mode=%d dir=%d a=$%06X b=$%02X count=%d\n",
                   channel, transfer_mode, direction, a_addr, b_addr, count);

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

    // Do transfer if needed
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

        // Debug: trace HDMA to CGRAM ($22)
        static int hdma_cgram_trace = 0;
        if (is_debug_mode() && b_addr == 0x22 && hdma_cgram_trace < 20) {
            uint8_t first_byte = m_bus.read(src_addr);
            fprintf(stderr, "[HDMA] CGRAM ch%d: mode=%d src=$%06X first_byte=$%02X\n",
                channel, transfer_mode, src_addr, first_byte);
            hdma_cgram_trace++;
        }

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

        // Update indirect address if used
        if (indirect) {
            ch.das = src_addr & 0xFFFF;
        }
    }

    // Decrement line counter
    ch.hdma_line_counter--;

    // Check if we need to reload
    if (ch.hdma_line_counter == 0) {
        // Read next table entry
        ch.nltr = hdma_read_table(channel);

        if (ch.nltr == 0) {
            ch.hdma_terminated = true;
            return;
        }

        ch.hdma_line_counter = ch.nltr & 0x7F;
        ch.hdma_do_transfer = true;

        // For indirect mode, read new indirect address
        if (ch.dmap & 0x40) {
            ch.das = hdma_read_table(channel);
            ch.das |= hdma_read_table(channel) << 8;
        }
    } else {
        // Check repeat flag
        ch.hdma_do_transfer = (ch.nltr & 0x80) != 0;
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
