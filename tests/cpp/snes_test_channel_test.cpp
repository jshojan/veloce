// Unit test for snes::TestChannel (the SNES side of VELOCE-RESULT/1).
//
// A recording ITestSink captures what the channel sends; the tests drive the
// channel the way the Bus and plugin do ($21FE/$21FF accesses, $6000 block
// writes, STP, S-SMP/S-CPU APU-port writes, frame ends) and check the exact
// transcript. No ROMs, no emulator. Registered with CTest as
// "snes_test_channel" (labels unit;fast).

#include "test_channel.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using snes::TestChannel;

static int g_failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++g_failures;                                                             \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                                  \
    do {                                                                                \
        auto _va = (a);                                                                 \
        auto _vb = (b);                                                                 \
        if (!(_va == _vb)) {                                                            \
            ++g_failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b  \
                      << "\n    got:  [" << _va << "]\n    want: [" << _vb << "]\n";    \
        }                                                                               \
    } while (0)

// Records the sink calls as one transcript: raw bytes verbatim, the other
// calls as "@<call> ...\n" lines.
struct RecordingSink : emu::ITestSink {
    std::string text;
    int finishes = 0;
    int32_t finish_status = 0;
    int reset_requests = 0;
    std::vector<std::string> blob_names;
    std::vector<size_t> blob_sizes;

    void write(const char* bytes, size_t n) override { text.append(bytes, n); }
    void finish(int32_t status) override {
        ++finishes;
        finish_status = status;
        text += "@finish " + std::to_string(status) + "\n";
    }
    void note(const char* key, const char* value) override {
        text += std::string("@note ") + key + "=" + value + "\n";
    }
    void blob(const char* name, const uint8_t*, size_t n) override {
        blob_names.push_back(name);
        blob_sizes.push_back(n);
        text += std::string("@blob ") + name + " " + std::to_string(n) + "\n";
    }
    void request_reset() override {
        ++reset_requests;
        text += "@reset\n";
    }
};

static void port_puts(TestChannel& ch, const char* s) {
    for (; *s; ++s) ch.port_write(TestChannel::PORT_DATA, static_cast<uint8_t>(*s));
}

// ---------------------------------------------------------------------------

static void test_inactive_without_sink() {
    TestChannel ch;
    uint8_t v = 0x12;
    CHECK(!ch.active());
    CHECK(!ch.port_read(TestChannel::PORT_CONTROL, v));
    CHECK_EQ(v, 0x12);
    // None of these may crash or arm anything without a sink.
    ch.port_write(TestChannel::PORT_DATA, 'x');
    ch.blargg_write(1, 0xDE);
    ch.arm_spcport();
    CHECK(!ch.spcport_armed());
    CHECK(!ch.stp_armed());
    ch.end_frame();
}

static void test_port_and_probe() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    uint8_t v = 0;
    CHECK(ch.port_read(TestChannel::PORT_CONTROL, v));
    CHECK_EQ(static_cast<int>(v), 0x56);
    CHECK(!ch.port_read(TestChannel::PORT_DATA, v));   // only $21FF is the probe
    CHECK(!ch.port_used());
    port_puts(ch, "VELOCE 1 snes t\nEND 1/1\n");
    ch.port_write(TestChannel::PORT_CONTROL, 0x01);
    CHECK(ch.port_used());
    CHECK_EQ(sink.text, std::string("VELOCE 1 snes t\nEND 1/1\n@finish -1\n"));
    CHECK_EQ(sink.finishes, 1);
    CHECK_EQ(sink.finish_status, -1);

    // Detach: back to inert.
    ch.set_sink(nullptr);
    CHECK(!ch.port_read(TestChannel::PORT_CONTROL, v));
}

static void blargg_signature(TestChannel& ch) {
    ch.blargg_write(0, 0x80);
    ch.blargg_write(1, 0xDE);
    ch.blargg_write(2, 0xB0);
    ch.blargg_write(3, 0x61);
}

// Mimics blargg's text_out: character at the next position, NUL after it.
static void blargg_text(TestChannel& ch, size_t& pos, const char* s) {
    for (; *s; ++s, ++pos) {
        ch.blargg_write(static_cast<uint16_t>(4 + pos), static_cast<uint8_t>(*s));
        ch.blargg_write(static_cast<uint16_t>(5 + pos), 0);
    }
}

static void test_blargg_pass_streams_text() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    blargg_signature(ch);
    size_t pos = 0;
    blargg_text(ch, pos, "01-basics\n\n");
    // Streamed before the verdict (a hung ROM still leaves its text).
    CHECK_EQ(sink.text, std::string("@note adapter=blargg6000\nLOG 01-basics\nLOG\n"));
    blargg_text(ch, pos, "Passed");
    ch.blargg_write(0, 0x00);
    CHECK_EQ(sink.text, std::string("@note adapter=blargg6000\nLOG 01-basics\nLOG\nLOG Passed\nEND 1/1\n"));
    // Later writes are ignored once the verdict is out.
    ch.blargg_write(0, 0x05);
    blargg_text(ch, pos, "more");
    CHECK_EQ(sink.text, std::string("@note adapter=blargg6000\nLOG 01-basics\nLOG\nLOG Passed\nEND 1/1\n"));
}

static void test_blargg_fail_code_and_text_before_signature() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    size_t pos = 0;
    ch.blargg_write(0, 0x80);
    blargg_text(ch, pos, "early\n");   // before the signature: caught up on detection
    ch.blargg_write(1, 0xDE);
    ch.blargg_write(2, 0xB0);
    ch.blargg_write(3, 0x47);           // alternate signature byte
    blargg_text(ch, pos, "Failed #3");
    ch.blargg_write(0, 0x03);
    CHECK_EQ(sink.text,
             std::string("@note adapter=blargg6000\nLOG early\nLOG Failed #3\nEND 0/1 code=3\n"));
}

static void test_blargg_needs_signature_and_running() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    // No signature: a plain SRAM write at $6000 means nothing.
    ch.blargg_write(0, 0x80);
    ch.blargg_write(0, 0x00);
    CHECK_EQ(sink.text, std::string(""));
    // Signature, but the status never said "running": no verdict.
    RecordingSink sink2;
    TestChannel ch2;
    ch2.set_sink(&sink2);
    ch2.blargg_write(1, 0xDE);
    ch2.blargg_write(2, 0xB0);
    ch2.blargg_write(3, 0x61);
    ch2.blargg_write(0, 0x00);
    CHECK_EQ(sink2.text, std::string("@note adapter=blargg6000\n"));
}

static void test_blargg_reset_request_once_per_transition() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    blargg_signature(ch);
    ch.blargg_write(0, 0x81);
    ch.blargg_write(0, 0x81);          // repeated: still one request
    CHECK_EQ(sink.reset_requests, 1);
    ch.on_test_reset();                // the application performed the reset
    ch.blargg_write(0, 0x80);
    ch.blargg_write(0, 0x81);
    CHECK_EQ(sink.reset_requests, 2);
    ch.on_test_reset();
    ch.blargg_write(0, 0x80);
    ch.blargg_write(0, 0x00);
    CHECK_EQ(sink.text, std::string("@note adapter=blargg6000\n@reset\n@reset\nEND 1/1\n"));
}

static void test_port_silences_legacy_adapters() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    port_puts(ch, "END 1/1\n");
    blargg_signature(ch);
    ch.blargg_write(0, 0x00);
    uint8_t cgram[512] = {0x00, 0x7C};
    CHECK(!ch.stp_armed());
    ch.on_stp(0, 0x8000, cgram, sizeof(cgram), nullptr, 0);
    CHECK_EQ(sink.text, std::string("END 1/1\n"));
}

static void test_stp_pass_fail_unknown() {
    uint8_t sram[16] = {1, 2, 3};
    {
        RecordingSink sink;
        TestChannel ch;
        ch.set_sink(&sink);
        uint8_t cgram[512] = {0x00, 0x7C};   // $7C00: the ROMs' pass() colour
        CHECK(ch.stp_armed());
        ch.on_stp(0x80, 0x833F, cgram, sizeof(cgram), sram, sizeof(sram));
        CHECK_EQ(sink.text, std::string("@note adapter=stp\n@note stp=80:833f cgram0=7c00\n"
                                        "@blob sram 16\n@blob cgram 512\nEND 1/1\n"));
        CHECK(!ch.stp_armed());              // once per power cycle
        ch.on_test_reset();
        CHECK(ch.stp_armed());
    }
    {
        RecordingSink sink;
        TestChannel ch;
        ch.set_sink(&sink);
        uint8_t cgram[512] = {0x1F, 0x00};   // $001F: fail() colour
        ch.on_stp(0x00, 0xC0AF, cgram, sizeof(cgram), nullptr, 0);   // no cart RAM: no sram blob
        CHECK_EQ(sink.text, std::string("@note adapter=stp\n@note stp=00:c0af cgram0=001f\n"
                                        "@blob cgram 512\nEND 0/1\n"));
    }
    {
        RecordingSink sink;
        TestChannel ch;
        ch.set_sink(&sink);
        uint8_t cgram[512] = {0xE0, 0x03};   // any other colour: halted, verdict unknown
        ch.on_stp(0x00, 0x8123, cgram, sizeof(cgram), sram, sizeof(sram));
        CHECK_EQ(sink.finishes, 1);
        CHECK_EQ(sink.finish_status, -1);
        CHECK(sink.text.find("END") == std::string::npos);
    }
}

// One gilyon spctest program as the S-SMP reports it.
static void spc_program_pass(TestChannel& ch, uint16_t last_test) {
    ch.spc_port_write(0, 0x00, false);                      // program start
    ch.spc_port_write(2, last_test & 0xFF, false);          // running test numbers
    ch.spc_port_write(3, last_test >> 8, false);
    ch.spc_port_write(0, 0x01, false);                      // all passed
}

static void spc_ipl_ready(TestChannel& ch) {
    ch.spc_port_write(0, 0xAA, true);
    ch.spc_port_write(1, 0xBB, true);
}

static void test_spcport_pass_then_quiet() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    ch.arm_spcport();
    CHECK(ch.spcport_armed());
    // IPL upload echoes on port0 (values 0,1,2...) must not count.
    ch.spc_port_write(0, 0x00, true);
    ch.spc_port_write(0, 0x01, true);
    spc_program_pass(ch, 0x01f3);
    spc_ipl_ready(ch);
    ch.end_frame();
    ch.cpu_apu_write(0, 0xCC);         // S-CPU starts the next upload
    for (int i = 0; i < 40; ++i) ch.end_frame();
    spc_program_pass(ch, 0x0527);
    // Not "done" until the IPL is back and the S-CPU stays quiet.
    for (int i = 0; i < 40; ++i) ch.end_frame();
    CHECK(sink.text.find("END") == std::string::npos);
    spc_ipl_ready(ch);
    for (int i = 0; i < snes::SpcPortState::QUIET_FRAMES - 1; ++i) ch.end_frame();
    CHECK(sink.text.find("END") == std::string::npos);
    ch.end_frame();
    CHECK_EQ(sink.text, std::string("@note adapter=spcport\n"
                                    "CHECK 1 PASS spc_program_1\nREG last_test=01f3\n"
                                    "CHECK 2 PASS spc_program_2\nREG last_test=0527\n"
                                    "END 2/2\n"));
    CHECK(!ch.spcport_armed());
}

static void test_spcport_fail() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    ch.arm_spcport();
    spc_program_pass(ch, 0x0100);
    spc_ipl_ready(ch);
    ch.cpu_apu_write(0, 0xCC);
    ch.spc_port_write(0, 0x00, false);
    ch.spc_port_write(2, 0x34, false);   // failing test $0234
    ch.spc_port_write(3, 0x02, false);
    ch.spc_port_write(1, 0x4A, false);   // PSW
    ch.spc_port_write(0, 0x02, false);
    ch.spc_port_write(1, 0x11, false);   // A
    ch.spc_port_write(2, 0x22, false);   // X
    ch.spc_port_write(3, 0x33, false);   // Y
    ch.spc_port_write(0, 0x03, false);
    CHECK_EQ(sink.text, std::string("@note adapter=spcport\n"
                                    "CHECK 1 PASS spc_program_1\nREG last_test=0100\n"
                                    "REG test=0234 a=11 x=22 y=33 p=4a\n"
                                    "CHECK 2 FAIL spc_program_2 at=0234\n"
                                    "END 1/2\n"));
}

static void test_spcport_not_armed() {
    RecordingSink sink;
    TestChannel ch;
    ch.set_sink(&sink);
    spc_program_pass(ch, 0x0100);
    spc_ipl_ready(ch);
    for (int i = 0; i < 40; ++i) ch.end_frame();
    CHECK_EQ(sink.text, std::string(""));
}

int main() {
    test_inactive_without_sink();
    test_port_and_probe();
    test_blargg_pass_streams_text();
    test_blargg_fail_code_and_text_before_signature();
    test_blargg_needs_signature_and_running();
    test_blargg_reset_request_once_per_transition();
    test_port_silences_legacy_adapters();
    test_stp_pass_fail_unknown();
    test_spcport_pass_then_quiet();
    test_spcport_fail();
    test_spcport_not_armed();
    if (g_failures) {
        std::cerr << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cerr << "ALL PASS\n";
    return 0;
}
