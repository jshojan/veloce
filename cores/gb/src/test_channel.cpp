#include "test_channel.hpp"

#include <cstdio>
#include <cstdlib>

namespace gb {

namespace {

constexpr uint16_t kA000Text = 0xA004;
constexpr uint16_t kA000TextMax = 0xC000 - kA000Text;   // rest of the 8 KiB bank

bool starts_with(const std::string& s, const char* prefix) {
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

// Same shape the sink and the testkit accept: "END <digits>/<digits>" then
// whitespace or end of line.
bool is_end_line(const std::string& s) {
    if (!starts_with(s, "END")) return false;
    size_t i = 3;
    if (i >= s.size() || (s[i] != ' ' && s[i] != '\t')) return false;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    size_t d = i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    if (i == d || i >= s.size() || s[i] != '/') return false;
    d = ++i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    return i > d && (i == s.size() || s[i] == ' ' || s[i] == '\t');
}

std::string hex2(uint8_t v) {
    char buf[3];
    std::snprintf(buf, sizeof(buf), "%02x", v);
    return buf;
}

} // namespace

void TestChannel::set_sink(emu::ITestSink* sink) {
    m_sink = sink;
    m_line.clear();
    m_serial_text = false;
    m_rom_stream = false;
    m_ended = false;
    m_adapter.clear();
    m_a000_seen = false;
    m_a000_running = false;
    m_a000_reset_sent = false;
    m_a000_text_pos = 0;
    m_a000_line.clear();
}

// ---- serial ---------------------------------------------------------------

void TestChannel::on_serial_byte(uint8_t sb) {
    if (!m_sink) return;
    const char c = static_cast<char>(sb);
    m_sink->write(&c, 1);

    // Track the line exactly as TestFileSink builds it.
    if (sb == '\n') {
        serial_line_done();
        return;
    }
    if (sb == '\r' || (sb < 0x20 && sb != '\t') || sb == 0x7F) return;
    m_serial_text = true;
    if (m_line.size() < 255) m_line.push_back(sb >= 0x80 ? '?' : c);
}

void TestChannel::serial_line_done() {
    std::string line;
    line.swap(m_line);

    if (starts_with(line, "VELOCE 1 ")) {
        m_rom_stream = true;          // Tier A ROM: it owns its verdict
        return;
    }
    if (is_end_line(line)) {
        m_ended = true;
        return;
    }

    // blarggtext: the last line of a blargg shell without the $A000 block.
    if (!adapters_enabled() || m_a000_seen) return;
    const bool passed = starts_with(line, "Passed");
    if (!passed && !starts_with(line, "Failed")) return;
    adapter_note("blarggtext");
    std::string end = "END 1/1\n";
    if (!passed) {
        end = "END 0/1";
        // "Failed #3" carries the failing test number; "Failed 2 tests"
        // (combined ROMs) has no single code.
        size_t hash = line.find('#');
        if (hash != std::string::npos && hash + 1 < line.size() &&
            line[hash + 1] >= '0' && line[hash + 1] <= '9') {
            end += " code=" + std::to_string(std::strtol(line.c_str() + hash + 1, nullptr, 10));
        }
        end += "\n";
    }
    adapter_write(end);
    m_ended = true;
}

// ---- adapter output ---------------------------------------------------------

void TestChannel::adapter_note(const char* name) {
    if (m_adapter == name) return;
    m_adapter = name;
    m_sink->note("adapter", name);
}

void TestChannel::adapter_write(const std::string& text) {
    // Adapter lines must not be glued onto a ROM line that is still being
    // printed; close it first.
    if (!m_line.empty()) {
        m_sink->write("\n", 1);
        m_line.clear();
    }
    m_sink->write(text.data(), text.size());
}

// ---- mooneye ---------------------------------------------------------------

void TestChannel::on_debug_breakpoint(uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t h,
                                      uint8_t l) {
    if (!adapters_enabled()) return;
    const bool pass = b == 3 && c == 5 && d == 8 && e == 13 && h == 21 && l == 34;
    const bool fail = b == 0x42 && c == 0x42 && d == 0x42 && e == 0x42 && h == 0x42 && l == 0x42;
    if (!pass && !fail) return;

    adapter_note("mooneye");
    std::string out = "REG b=" + hex2(b) + " c=" + hex2(c) + " d=" + hex2(d) + " e=" + hex2(e) +
                      " h=" + hex2(h) + " l=" + hex2(l) + "\n";
    out += pass ? "END 1/1\n" : "END 0/1 code=66\n";
    adapter_write(out);
    m_ended = true;
}

// ---- per-frame probes --------------------------------------------------------

void TestChannel::on_frame_end(const Peek& peek) {
    if (!adapters_enabled() || !peek) return;
    a000_probe(peek);
    if (adapters_enabled()) hram_probe(peek);
}

void TestChannel::a000_probe(const Peek& peek) {
    if (peek(0xA001) != 0xDE || peek(0xA002) != 0xB0 || peek(0xA003) != 0x61) return;
    const uint8_t status = peek(0xA000);
    if (!m_a000_running) {
        // Arm on "running" only, so a block left in RAM by an earlier run is
        // never read as this run's result.
        if (status != 0x80) return;
        m_a000_running = true;
        m_a000_text_pos = 0;
        m_a000_line.clear();
        if (!m_a000_seen) {
            m_a000_seen = true;
            adapter_note("a000");
        }
    }

    // Stream the text block as LOG lines, unless the ROM already mirrors
    // its text to serial (the newer blargg shells do): then the raw serial
    // lines carry it and a second copy would only interleave with them.
    const bool stream_text = !m_serial_text;
    while (m_a000_text_pos < kA000TextMax) {
        uint8_t ch = peek(static_cast<uint16_t>(kA000Text + m_a000_text_pos));
        if (ch == 0) break;
        ++m_a000_text_pos;
        if (!stream_text) continue;
        if (ch == '\n') {
            adapter_write("LOG " + m_a000_line + "\n");
            m_a000_line.clear();
        } else if (ch >= 0x20 && ch < 0x7F && m_a000_line.size() < 250) {
            m_a000_line.push_back(static_cast<char>(ch));
        }
    }

    if (status == 0x80) {
        m_a000_reset_sent = false;
        return;
    }
    if (status == 0x81) {
        // "Press reset": once per $80 -> $81 transition.
        if (!m_a000_reset_sent) {
            m_a000_reset_sent = true;
            m_sink->request_reset();
        }
        return;
    }
    if (status >= 0x80) return;

    std::string out;
    if (stream_text && !m_a000_line.empty()) out = "LOG " + m_a000_line + "\n";
    m_a000_line.clear();
    out += status == 0 ? std::string("END 1/1\n")
                       : "END 0/1 code=" + std::to_string(status) + "\n";
    adapter_write(out);
    m_ended = true;
}

void TestChannel::hram_probe(const Peek& peek) {
    if (!m_hram_armed) return;
    const uint8_t result = peek(0xFF82);
    if (result != 0x01 && result != 0xFF) return;
    const bool pass = result == 0x01;
    adapter_note("hram");
    // $FF82 is the verdict; $FF80/$FF81 are informational (some tests set
    // them equal on failure).
    std::string out = std::string("CHECK 1 ") + (pass ? "PASS" : "FAIL") +
                      " hram exp=" + hex2(peek(0xFF81)) + " got=" + hex2(peek(0xFF80)) + "\n";
    out += pass ? "END 1/1\n" : "END 0/1\n";
    adapter_write(out);
    m_ended = true;
}

void TestChannel::on_test_reset() {
    // The ROM restarts: it rewrites $A000 = $80 and its text from the top.
    m_a000_running = false;
    m_a000_reset_sent = false;
    m_a000_text_pos = 0;
    m_a000_line.clear();
}

} // namespace gb
