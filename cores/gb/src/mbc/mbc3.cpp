#include "mbc3.hpp"

namespace gb {

void MBC3::reset() {
    MBC::reset();
    m_rtc_s = 0;
    m_rtc_m = 0;
    m_rtc_h = 0;
    m_rtc_dl = 0;
    m_rtc_dh = 0;
    m_latch_prev = 0xFF;
    m_rtc_selected = false;
    m_rtc_register = 0;
}

uint8_t MBC3::read_ram(uint16_t address) {
    if (!m_ram_enabled) {
        return 0xFF;
    }

    if (m_rtc_selected) {
        // Read RTC register
        switch (m_rtc_register) {
            case 0x08: return m_rtc_s_latch;
            case 0x09: return m_rtc_m_latch;
            case 0x0A: return m_rtc_h_latch;
            case 0x0B: return m_rtc_dl_latch;
            case 0x0C: return m_rtc_dh_latch;
            default: return 0xFF;
        }
    }

    return MBC::read_ram(address);
}

void MBC3::write_ram(uint16_t address, uint8_t value) {
    if (!m_ram_enabled) {
        return;
    }

    if (m_rtc_selected) {
        // Write RTC register
        switch (m_rtc_register) {
            case 0x08: m_rtc_s = value & 0x3F; break;
            case 0x09: m_rtc_m = value & 0x3F; break;
            case 0x0A: m_rtc_h = value & 0x1F; break;
            case 0x0B: m_rtc_dl = value; break;
            case 0x0C: m_rtc_dh = value & 0xC1; break;
        }
        return;
    }

    MBC::write_ram(address, value);
}

void MBC3::write(uint16_t address, uint8_t value) {
    if (address < 0x2000) {
        // RAM/RTC Enable
        m_ram_enabled = ((value & 0x0F) == 0x0A);
    } else if (address < 0x4000) {
        // ROM Bank Number
        m_rom_bank = value & 0x7F;
        if (m_rom_bank == 0) m_rom_bank = 1;
    } else if (address < 0x6000) {
        // RAM Bank Number / RTC Register Select
        if (value <= 0x03) {
            m_ram_bank = value;
            m_rtc_selected = false;
        } else if (value >= 0x08 && value <= 0x0C) {
            m_rtc_register = value;
            m_rtc_selected = true;
        }
    } else {
        // Latch Clock Data
        if (m_latch_prev == 0x00 && value == 0x01) {
            latch_rtc();
        }
        m_latch_prev = value;
    }
}

void MBC3::latch_rtc() {
    m_rtc_s_latch = m_rtc_s;
    m_rtc_m_latch = m_rtc_m;
    m_rtc_h_latch = m_rtc_h;
    m_rtc_dl_latch = m_rtc_dl;
    m_rtc_dh_latch = m_rtc_dh;
}

void MBC3::save_state(std::vector<uint8_t>& data) {
    MBC::save_state(data);

    data.push_back(m_rtc_s);
    data.push_back(m_rtc_m);
    data.push_back(m_rtc_h);
    data.push_back(m_rtc_dl);
    data.push_back(m_rtc_dh);
    data.push_back(m_rtc_s_latch);
    data.push_back(m_rtc_m_latch);
    data.push_back(m_rtc_h_latch);
    data.push_back(m_rtc_dl_latch);
    data.push_back(m_rtc_dh_latch);
    data.push_back(m_latch_prev);
    data.push_back(m_rtc_selected ? 1 : 0);
    data.push_back(m_rtc_register);
}

void MBC3::load_state(const uint8_t*& data, size_t& remaining) {
    MBC::load_state(data, remaining);

    m_rtc_s = *data++; remaining--;
    m_rtc_m = *data++; remaining--;
    m_rtc_h = *data++; remaining--;
    m_rtc_dl = *data++; remaining--;
    m_rtc_dh = *data++; remaining--;
    m_rtc_s_latch = *data++; remaining--;
    m_rtc_m_latch = *data++; remaining--;
    m_rtc_h_latch = *data++; remaining--;
    m_rtc_dl_latch = *data++; remaining--;
    m_rtc_dh_latch = *data++; remaining--;
    m_latch_prev = *data++; remaining--;
    m_rtc_selected = (*data++ != 0); remaining--;
    m_rtc_register = *data++; remaining--;
}

} // namespace gb
