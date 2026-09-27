// Unit test for nes::TestChannel (the NES side of the plugin ABI v2 test
// channel): the $401E/$401F port, the $401F probe and the blargg6000 adapter.
//
// A recording ITestSink captures what the core would hand the application;
// the test replays the exact write sequences blargg's shell performs
// (common/text_out.s init_text_out / write_text_out, build_rom.s post_exit,
// run_at_reset.s prompt_to_reset). No ROMs, no emulator.
// Registered with CTest as "nes_test_channel" (labels unit;fast).

#include "test_channel.hpp"
#include "emu/emulator_plugin.hpp"

#include <iostream>
#include <string>

using nes::TestChannel;

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

struct RecordingSink final : emu::ITestSink {
    std::string out;        // write() bytes and notes, in call order
    int finishes = 0;
    int resets = 0;

    void write(const char* bytes, size_t n) override { out.append(bytes, n); }
    void finish(int32_t) override { ++finishes; }
    void note(const char* key, const char* value) override {
        out += "#VELOCE ";
        out += key;
        out += "=";
        out += value;
        out += "\n";
    }
    void blob(const char*, const uint8_t*, size_t) override {}
    void request_reset() override { ++resets; }
};

// blargg shell emulation ------------------------------------------------------
struct BlarggRom {
    TestChannel& ch;
    uint16_t addr = 0x6004;

    void init_text_out() {
        ch.cart_write(0x6004, 0);          // "put valid data first"
        ch.cart_write(0x6000, 0x80);       // set_final_result #$80
        ch.cart_write(0x6001, 0xDE);
        ch.cart_write(0x6002, 0xB0);
        ch.cart_write(0x6003, 0x61);
        addr = 0x6004;
    }
    void print(const std::string& s) {
        for (char c : s) {                 // write_text_out: terminator first
            ch.cart_write(static_cast<uint16_t>(addr + 1), 0);
            ch.cart_write(addr, static_cast<uint8_t>(c));
            ++addr;
        }
    }
    void exit(uint8_t code) { ch.cart_write(0x6000, code); }
};

static void test_detached_is_inert() {
    TestChannel ch;
    uint8_t v = 0xAA;
    CHECK(!ch.attached());
    CHECK(!ch.port_write(0x401E, 'X'));
    CHECK(!ch.port_write(0x401F, 0x01));
    CHECK(!ch.port_read(0x401F, v));
    CHECK_EQ(v, 0xAA);
    ch.cart_write(0x6001, 0xDE);           // must not crash without a sink
    ch.cart_write(0x6000, 0x00);
}

static void test_port_and_probe() {
    TestChannel ch;
    RecordingSink sink;
    ch.set_sink(&sink);
    uint8_t v = 0;
    CHECK(ch.port_read(0x401F, v));
    CHECK_EQ(static_cast<int>(v), 0x56);
    CHECK(!ch.port_read(0x401E, v));       // data register has no read side
    CHECK(!ch.port_read(0x4015, v));
    CHECK(!ch.port_write(0x4015, 0x0F));   // APU writes are not consumed
    for (char c : std::string("END 1/1\n")) CHECK(ch.port_write(0x401E, static_cast<uint8_t>(c)));
    CHECK(ch.port_write(0x401F, 0x01));
    CHECK_EQ(sink.out, std::string("END 1/1\n"));
    CHECK_EQ(sink.finishes, 0);            // the sink parses END itself
    CHECK_EQ(std::string(TestChannel::channels()), std::string("port,blargg6000"));
}

static void test_blargg_pass() {
    TestChannel ch;
    RecordingSink sink;
    ch.set_sink(&sink);
    BlarggRom rom{ch};
    rom.init_text_out();
    CHECK_EQ(sink.out, std::string(""));   // no output (and no note) before text
    rom.print("\n01-basics\n\nPassed\n");
    rom.exit(0);
    CHECK_EQ(sink.out, std::string("#VELOCE adapter=blargg6000\n"
                                   "LOG\n"
                                   "LOG 01-basics\n"
                                   "LOG\n"
                                   "LOG Passed\n"
                                   "END 1/1\n"));
    rom.exit(3);                           // only the first terminator counts
    rom.print("more");
    CHECK_EQ(sink.out.find("END 0/1"), std::string::npos);
    CHECK_EQ(sink.out.find("more"), std::string::npos);
    CHECK_EQ(sink.resets, 0);
}

static void test_blargg_fail_closes_partial_line() {
    TestChannel ch;
    RecordingSink sink;
    ch.set_sink(&sink);
    BlarggRom rom{ch};
    rom.init_text_out();
    rom.print("03-dummy_reads\nFailed #3");
    rom.exit(3);
    CHECK_EQ(sink.out, std::string("#VELOCE adapter=blargg6000\n"
                                   "LOG 03-dummy_reads\n"
                                   "LOG Failed #3\n"
                                   "END 0/1 code=3\n"));
}

static void test_status_needs_signature() {
    TestChannel ch;
    RecordingSink sink;
    ch.set_sink(&sink);
    ch.cart_write(0x6000, 0x00);           // e.g. a game clearing its WRAM
    ch.cart_write(0x6000, 0x81);
    ch.cart_write(0x6004, 'x');
    CHECK_EQ(sink.out, std::string(""));
    CHECK_EQ(sink.resets, 0);
}

static void test_reset_handshake() {
    TestChannel ch;
    RecordingSink sink;
    ch.set_sink(&sink);
    BlarggRom rom{ch};
    rom.init_text_out();
    rom.print("4015_cleared\n\nPress reset AFTER this message\ndisappears\n");
    rom.exit(0x81);
    CHECK_EQ(sink.resets, 1);
    rom.exit(0x81);                        // no 0x80 in between: not re-armed
    CHECK_EQ(sink.resets, 1);

    ch.on_reset();                         // app reset the console
    rom.init_text_out();                   // shell boots again, text rewinds
    rom.print("4015_cleared\n\nPassed\n");
    rom.exit(0);
    CHECK_EQ(sink.out, std::string("#VELOCE adapter=blargg6000\n"
                                   "LOG 4015_cleared\n"
                                   "LOG\n"
                                   "LOG Press reset AFTER this message\n"
                                   "LOG disappears\n"
                                   "LOG 4015_cleared\n"
                                   "LOG\n"
                                   "LOG Passed\n"
                                   "END 1/1\n"));

    // A $80 -> $81 transition re-arms without on_reset() as well.
    TestChannel ch2;
    RecordingSink sink2;
    ch2.set_sink(&sink2);
    BlarggRom rom2{ch2};
    rom2.init_text_out();
    rom2.exit(0x81);
    rom2.exit(0x80);
    rom2.exit(0x81);
    CHECK_EQ(sink2.resets, 2);
}

static void test_port_silences_adapter() {
    TestChannel ch;
    RecordingSink sink;
    ch.set_sink(&sink);
    BlarggRom rom{ch};
    rom.init_text_out();
    rom.print("half");                     // adapter line in progress
    for (char c : std::string("VELOCE 1 nes x\n")) ch.port_write(0x401E, static_cast<uint8_t>(c));
    rom.print(" line\n");
    rom.exit(0x81);                        // reset handshake still works
    rom.exit(0);                           // no second END from the adapter
    CHECK_EQ(sink.out, std::string("#VELOCE adapter=blargg6000\n"
                                   "LOG half\n"
                                   "VELOCE 1 nes x\n"));
    CHECK_EQ(sink.resets, 1);

    // Port first (the patched-shell order): the adapter never speaks.
    TestChannel ch2;
    RecordingSink sink2;
    ch2.set_sink(&sink2);
    BlarggRom rom2{ch2};
    rom2.init_text_out();
    ch2.port_write(0x401F, 0x00);
    rom2.print("text\n");
    rom2.exit(0);
    CHECK_EQ(sink2.out, std::string(""));
}

static void test_reattach_resets_state() {
    TestChannel ch;
    RecordingSink a, b;
    ch.set_sink(&a);
    BlarggRom rom{ch};
    rom.init_text_out();
    ch.port_write(0x401E, 'x');
    ch.set_sink(nullptr);
    CHECK(!ch.attached());
    ch.set_sink(&b);
    rom.init_text_out();
    rom.print("ok\n");
    rom.exit(0);
    CHECK_EQ(b.out, std::string("#VELOCE adapter=blargg6000\nLOG ok\nEND 1/1\n"));
}

int main() {
    test_detached_is_inert();
    test_port_and_probe();
    test_blargg_pass();
    test_blargg_fail_closes_partial_line();
    test_status_needs_signature();
    test_reset_handshake();
    test_port_silences_adapter();
    test_reattach_resets_state();
    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "nes_test_channel: all checks passed\n";
    return 0;
}
