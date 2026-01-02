#pragma once

#include "mbc.hpp"

namespace gb {

// MBC3 - Supports RTC (Real Time Clock)
// Used by Pokemon Gold/Silver/Crystal
class MBC3 : public MBC {
public:
    using MBC::MBC;

    void reset() override;
    uint8_t read_ram(uint16_t address) override;
    void write_ram(uint16_t address, uint8_t value) override;
    void write(uint16_t address, uint8_t value) override;

    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

private:
    void latch_rtc();

    // RTC registers
    uint8_t m_rtc_s = 0;    // Seconds
    uint8_t m_rtc_m = 0;    // Minutes
    uint8_t m_rtc_h = 0;    // Hours
    uint8_t m_rtc_dl = 0;   // Day counter low
    uint8_t m_rtc_dh = 0;   // Day counter high / control

    // Latched RTC values
    uint8_t m_rtc_s_latch = 0;
    uint8_t m_rtc_m_latch = 0;
    uint8_t m_rtc_h_latch = 0;
    uint8_t m_rtc_dl_latch = 0;
    uint8_t m_rtc_dh_latch = 0;

    uint8_t m_latch_prev = 0xFF;
    bool m_rtc_selected = false;
    uint8_t m_rtc_register = 0;
};

} // namespace gb
