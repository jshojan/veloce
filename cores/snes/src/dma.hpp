#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace snes {

class Bus;

// SNES DMA and HDMA Controller
// 8 DMA channels, each can do general purpose DMA or HDMA
class DMA {
public:
    explicit DMA(Bus& bus);
    ~DMA();

    void reset();

    // Register access ($43x0-$43xF)
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    // DMA control registers ($420B, $420C)
    void write_mdmaen(uint8_t value);  // Start general purpose DMA
    void write_hdmaen(uint8_t value);  // Enable HDMA channels

    // HDMA processing (called at start of each scanline)
    void hdma_init();      // Called at V=0
    void hdma_transfer();  // Called at start of each H-blank

    // Check if DMA is currently active
    bool is_dma_active() const { return m_dma_active; }
    int get_dma_cycles() const { return m_dma_cycles; }
    void clear_dma_cycles() { m_dma_cycles = 0; }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    void do_dma_transfer(int channel);
    void do_hdma_channel(int channel);
    uint8_t hdma_read_table(int channel);

    Bus& m_bus;

    // DMA channel state
    struct Channel {
        uint8_t dmap;       // $43x0 - DMA parameters
        uint8_t bbad;       // $43x1 - B-bus address
        uint16_t a1t;       // $43x2-$43x3 - A-bus address
        uint8_t a1b;        // $43x4 - A-bus bank
        uint16_t das;       // $43x5-$43x6 - DMA byte count / HDMA indirect address
        uint8_t dasb;       // $43x7 - HDMA indirect bank
        uint16_t a2a;       // $43x8-$43x9 - HDMA table address
        uint8_t nltr;       // $43xA - HDMA line counter

        // HDMA state
        bool hdma_do_transfer;
        bool hdma_terminated;
        uint8_t hdma_line_counter;
    };
    std::array<Channel, 8> m_channels;

    // Active DMA state
    bool m_dma_active = false;
    int m_dma_cycles = 0;

    // HDMA state
    uint8_t m_hdmaen = 0;  // HDMA enable bits
};

} // namespace snes
