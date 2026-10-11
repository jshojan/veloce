#include "test_channel.hpp"

#include "emu/emulator_plugin.hpp"

#include <cstdio>
#include <cstring>

namespace nes {

namespace {
constexpr uint16_t kBlarggBase   = 0x6000;   // status
constexpr uint16_t kBlarggText   = 0x6004;   // zero-terminated text
constexpr uint16_t kCartRamEnd   = 0x8000;
constexpr uint8_t  kStatusRun    = 0x80;
constexpr uint8_t  kStatusReset  = 0x81;
}

void TestChannel::set_sink(emu::ITestSink* sink) {
    m_sink = sink;
    m_port_used = false;
    std::memset(m_head, 0, sizeof(m_head));
    m_text_tail = 0;
    m_line_open = false;
    m_noted = false;
    m_ended = false;
    m_reset_armed = true;
}

bool TestChannel::port_write(uint16_t address, uint8_t value) {
    if (!m_sink) return false;
    if (address == kPortData) {
        if (!m_port_used) {
            m_port_used = true;
            adapter_close_line();            // never splice into an adapter LOG line
        }
        const char c = static_cast<char>(value);
        m_sink->write(&c, 1);
        return true;
    }
    if (address == kPortControl) {
        // Raw observation corpora use a completion hint without a guest verdict.
        if (!m_port_used) {
            m_port_used = true;
            adapter_close_line();
        }
        if (value == 1) m_sink->finish(-1);
        return true;
    }
    return false;
}

bool TestChannel::port_read(uint16_t address, uint8_t& value) const {
    if (!m_sink || address != kPortControl) return false;
    value = kProbeValue;
    return true;
}

void TestChannel::cart_write(uint16_t address, uint8_t value) {
    if (!m_sink || address < kBlarggBase || address >= kCartRamEnd) return;
    if (address < kBlarggText) {
        m_head[address - kBlarggBase] = value;
        if (address == kBlarggBase) blargg_status(value);
        return;
    }
    blargg_text(static_cast<uint16_t>(address - kBlarggText), value);
}

void TestChannel::on_reset() {
    m_reset_armed = true;
}

bool TestChannel::signature_ok() const {
    return m_head[1] == 0xDE && m_head[2] == 0xB0 && m_head[3] == 0x61;
}

bool TestChannel::adapter_live() const {
    return m_sink && !m_port_used && !m_ended && signature_ok();
}

void TestChannel::blargg_status(uint8_t status) {
    if (!signature_ok()) return;               // init writes $80 before the signature
    if (status == kStatusRun) {
        m_reset_armed = true;
        return;
    }
    if (status == kStatusReset) {
        if (m_reset_armed && !m_ended) {
            m_reset_armed = false;
            m_sink->request_reset();
        }
        return;
    }
    if (status > kStatusReset || !adapter_live()) return;

    adapter_note();
    adapter_close_line();
    char line[32];
    if (status == 0) {
        std::snprintf(line, sizeof(line), "END 1/1\n");
    } else {
        std::snprintf(line, sizeof(line), "END 0/1 code=%u\n", static_cast<unsigned>(status));
    }
    write(line);
    m_ended = true;
}

void TestChannel::blargg_text(uint16_t offset, uint8_t value) {
    // blargg's write_text_out stores the new terminator at tail+1 first, then
    // the character at tail. init_text_out (every boot, including after a
    // requested reset) stores 0 at $6004, which rewinds the tail.
    if (offset < m_text_tail) {
        adapter_close_line();
        m_text_tail = offset;
    }
    if (offset != m_text_tail || value == 0) return;
    ++m_text_tail;
    if (adapter_live()) adapter_char(static_cast<char>(value));
}

void TestChannel::adapter_char(char c) {
    if (c == '\n') {
        adapter_note();
        write(m_line_open ? "\n" : "LOG\n");
        m_line_open = false;
        return;
    }
    const unsigned char u = static_cast<unsigned char>(c);
    if (u < 0x20 || u == 0x7F) return;        // console control codes
    adapter_note();
    if (!m_line_open) {
        write("LOG ");
        m_line_open = true;
    }
    write(&c, 1);
}

void TestChannel::adapter_close_line() {
    if (!m_line_open) return;
    write("\n");
    m_line_open = false;
}

void TestChannel::adapter_note() {
    if (m_noted || !m_sink) return;
    m_noted = true;
    m_sink->note("adapter", "blargg6000");
}

void TestChannel::write(const char* s, size_t n) {
    if (m_sink) m_sink->write(s, n);
}

void TestChannel::write(const char* s) {
    write(s, std::strlen(s));
}

} // namespace nes
