#include "test_channel.hpp"

#include <cstdio>

namespace snes {

void TestChannel::set_sink(emu::ITestSink* sink) {
    m_sink = sink;
    m_port_used = false;
    m_blargg.clear();
    m_blargg_noted = false;
    m_stp_reported = false;
    m_spc.clear();
}

void TestChannel::on_test_reset() {
    m_blargg.reset_requested = false;
    m_stp_reported = false;
}

void TestChannel::print(const char* text) {
    if (m_sink) m_sink->print(text);
}

// ---------------------------------------------------------------------------
// port
// ---------------------------------------------------------------------------

void TestChannel::port_write(uint16_t offset, uint8_t value) {
    if (!m_sink) return;
    if (offset == PORT_DATA) {
        m_port_used = true;
        const char c = static_cast<char>(value);
        m_sink->write(&c, 1);
    } else if (offset == PORT_CONTROL) {
        // Completion carries no correctness verdict. Preserve the owned
        // observation protocol even when the guest emits no END line.
        m_port_used = true;
        if (value == 1) m_sink->finish(-1);
    }
}

bool TestChannel::port_read(uint16_t offset, uint8_t& value) const {
    if (!m_sink || offset != PORT_CONTROL) return false;
    value = PROBE_VALUE;
    return true;
}

// ---------------------------------------------------------------------------
// blargg6000
// ---------------------------------------------------------------------------

void TestChannel::blargg_emit_char(uint8_t c) {
    if (m_blargg.line_start) {
        print(c == '\n' ? "LOG" : "LOG ");
        m_blargg.line_start = false;
    }
    if (c == '\n') {
        print("\n");
        m_blargg.line_start = true;
        return;
    }
    const char ch = static_cast<char>(c);
    m_sink->write(&ch, 1);
}

void TestChannel::blargg_write(uint16_t offset, uint8_t value) {
    if (!legacy_enabled() || offset >= BlarggTestState::BLOCK_SIZE) return;
    BlarggTestState& b = m_blargg;
    const uint8_t prev = b.mem[offset];
    b.mem[offset] = value;
    if (b.completed) return;

    if (!b.detected) {
        if (offset < 1 || offset > 3 || !b.signature_ok()) return;
        b.detected = true;
        if (!m_blargg_noted) {
            m_sink->note("adapter", "blargg6000");
            m_blargg_noted = true;
        }
        // Text the ROM wrote before its signature.
        while (b.text_pos < BlarggTestState::BLOCK_SIZE - BlarggTestState::TEXT_START &&
               b.mem[BlarggTestState::TEXT_START + b.text_pos] != 0) {
            blargg_emit_char(b.mem[BlarggTestState::TEXT_START + b.text_pos]);
            ++b.text_pos;
        }
        return;
    }

    if (offset == 0) {
        blargg_status(prev, value);
        return;
    }
    // The ROM's text_out writes each character at the next position and a
    // NUL after it; stream a character when it lands at the stream position.
    if (offset >= BlarggTestState::TEXT_START && value != 0 &&
        offset - BlarggTestState::TEXT_START == b.text_pos) {
        blargg_emit_char(value);
        ++b.text_pos;
    }
}

void TestChannel::blargg_status(uint8_t prev, uint8_t value) {
    BlarggTestState& b = m_blargg;
    if (value == BlarggTestState::STATUS_NEEDS_RESET) {
        if (!b.reset_requested) {
            b.reset_requested = true;
            m_sink->request_reset();
        }
        return;
    }
    if (value >= BlarggTestState::STATUS_RUNNING) return;
    // Done: only a status that follows "running" / "needs reset" counts.
    if (prev != BlarggTestState::STATUS_RUNNING && prev != BlarggTestState::STATUS_NEEDS_RESET) return;

    while (b.text_pos < BlarggTestState::BLOCK_SIZE - BlarggTestState::TEXT_START &&
           b.mem[BlarggTestState::TEXT_START + b.text_pos] != 0) {
        blargg_emit_char(b.mem[BlarggTestState::TEXT_START + b.text_pos]);
        ++b.text_pos;
    }
    if (!b.line_start) blargg_emit_char('\n');
    b.completed = true;
    if (value == 0) {
        print("END 1/1\n");
    } else {
        char line[32];
        std::snprintf(line, sizeof(line), "END 0/1 code=%u\n", static_cast<unsigned>(value));
        print(line);
    }
}

// ---------------------------------------------------------------------------
// stp / sram
// ---------------------------------------------------------------------------

void TestChannel::on_stp(uint8_t pbr, uint16_t pc, const uint8_t* cgram, size_t cgram_size,
                         const uint8_t* sram, size_t sram_size) {
    if (!stp_armed()) return;
    m_stp_reported = true;

    const uint16_t color0 = (cgram && cgram_size >= 2)
        ? static_cast<uint16_t>(cgram[0] | (cgram[1] << 8)) : 0;
    char value[48];
    std::snprintf(value, sizeof(value), "%02x:%04x cgram0=%04x",
                  static_cast<unsigned>(pbr), static_cast<unsigned>(pc),
                  static_cast<unsigned>(color0));
    m_sink->note("adapter", "stp");
    m_sink->note("stp", value);
    if (sram && sram_size) m_sink->blob("sram", sram, sram_size);
    if (cgram && cgram_size) m_sink->blob("cgram", cgram, cgram_size);

    if (color0 == STP_PASS_COLOR) {
        print("END 1/1\n");
    } else if (color0 == STP_FAIL_COLOR) {
        print("END 0/1\n");
    } else {
        // Halted for good, but not with either convention colour.
        m_sink->finish(-1);
    }
}

// ---------------------------------------------------------------------------
// spcport
// ---------------------------------------------------------------------------

void TestChannel::spc_port_write(int port, uint8_t value, bool from_ipl) {
    if (!spcport_armed() || m_port_used || port < 0 || port > 3) return;
    SpcPortState& s = m_spc;
    s.out[port] = value;

    if (from_ipl) {
        if (s.awaiting_upload && s.out[0] == 0xAA && s.out[1] == 0xBB) s.ipl_ready = true;
        return;
    }
    if (port != 0) return;

    if (!s.noted) {
        m_sink->note("adapter", "spcport");
        s.noted = true;
    }
    char line[96];
    switch (value) {
        case 0x00:  // a test program started
            s.in_program = true;
            s.fail_pending = false;
            s.awaiting_upload = false;
            s.ipl_ready = false;
            break;
        case 0x01:  // the whole program passed
            if (!s.in_program) break;
            s.in_program = false;
            ++s.programs_passed;
            std::snprintf(line, sizeof(line), "CHECK %d PASS spc_program_%d\nREG last_test=%04x\n",
                          s.programs_passed, s.programs_passed,
                          static_cast<unsigned>(s.out[2] | (s.out[3] << 8)));
            print(line);
            s.awaiting_upload = true;
            s.ipl_ready = false;
            s.quiet_frames = 0;
            break;
        case 0x02:  // a test failed: test number + PSW
            if (!s.in_program) break;
            s.fail_pending = true;
            s.fail_test = static_cast<uint16_t>(s.out[2] | (s.out[3] << 8));
            s.fail_psw = s.out[1];
            break;
        case 0x03: {  // failed test's A/X/Y
            if (!s.fail_pending) break;
            const int id = s.programs_passed + 1;
            std::snprintf(line, sizeof(line),
                          "REG test=%04x a=%02x x=%02x y=%02x p=%02x\n"
                          "CHECK %d FAIL spc_program_%d at=%04x\n",
                          static_cast<unsigned>(s.fail_test), s.out[1], s.out[2], s.out[3],
                          s.fail_psw, id, id, static_cast<unsigned>(s.fail_test));
            print(line);
            std::snprintf(line, sizeof(line), "END %d/%d\n", s.programs_passed, id);
            print(line);
            s.completed = true;
            break;
        }
        default:
            break;
    }
}

void TestChannel::cpu_apu_write(int port, uint8_t value) {
    (void)value;
    if (!spcport_armed()) return;
    // Writing port0 is how the S-CPU starts the next IPL upload.
    if (port == 0 && m_spc.awaiting_upload) {
        m_spc.awaiting_upload = false;
        m_spc.ipl_ready = false;
    }
}

void TestChannel::end_frame() {
    if (!spcport_armed() || m_port_used) return;
    SpcPortState& s = m_spc;
    if (!s.awaiting_upload || !s.ipl_ready) return;
    if (++s.quiet_frames < SpcPortState::QUIET_FRAMES) return;
    char line[32];
    std::snprintf(line, sizeof(line), "END %d/%d\n", s.programs_passed, s.programs_passed);
    print(line);
    s.completed = true;
}

} // namespace snes
