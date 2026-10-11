// Unit test for gb::TestChannel (cores/gb/src/test_channel.cpp): the Game Boy
// serial driver and its mooneye / a000 / hram / blarggtext adapters, driven
// through a recording ITestSink. No emulator, no ROMs.
//
// Registered with CTest as "gb_test_channel" (labels unit;fast).

#include "test_channel.hpp"
#include "emu/emulator_plugin.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <string>

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

// Records the sink calls as one transcript: raw bytes verbatim, notes as
// "{key=value}", reset requests as "{reset}", finish as "{finish n}".
struct RecordingSink : emu::ITestSink {
    std::string out;
    int resets = 0;
    void write(const char* bytes, size_t n) override { out.append(bytes, n); }
    void finish(int32_t s) override { out += "{finish " + std::to_string(s) + "}"; }
    void note(const char* k, const char* v) override {
        out += std::string("{") + k + "=" + v + "}";
    }
    void blob(const char* name, const uint8_t*, size_t n) override {
        out += std::string("{blob ") + name + " " + std::to_string(n) + "}";
    }
    void request_reset() override { ++resets; out += "{reset}"; }
};

struct Memory {
    std::map<uint16_t, uint8_t> bytes;
    gb::TestChannel::Peek peek() {
        return [this](uint16_t a) -> uint8_t {
            auto it = bytes.find(a);
            return it == bytes.end() ? 0 : it->second;
        };
    }
    void a000(uint8_t status, const std::string& text) {
        bytes[0xA000] = status;
        bytes[0xA001] = 0xDE;
        bytes[0xA002] = 0xB0;
        bytes[0xA003] = 0x61;
        for (size_t i = 0; i <= text.size(); ++i)
            bytes[static_cast<uint16_t>(0xA004 + i)] = i < text.size() ? text[i] : 0;
    }
};

static void serial(gb::TestChannel& ch, const std::string& s) {
    for (unsigned char c : s) ch.on_serial_byte(c);
}

static void test_serial_raw() {
    std::cerr << "serial_raw\n";
    gb::TestChannel ch;
    serial(ch, "dropped without a sink\n");     // no sink: nothing happens
    RecordingSink sink;
    ch.set_sink(&sink);
    std::string bytes = "VELOCE 1 gb smoke\n";
    bytes += std::string("\x03\x05\x08\x0d\x15\x22\x7f\x80\n", 9);
    bytes += "CHECK 1 PASS x\nEND 1/1\n";
    serial(ch, bytes);
    CHECK_EQ(sink.out, bytes);                     // every byte, unfiltered
    CHECK(ch.ended());
    // A Tier A stream owns its verdict: adapters stand down.
    ch.on_debug_breakpoint(3, 5, 8, 13, 21, 34);
    CHECK_EQ(sink.out, bytes);
}

static void test_rom_stream_blocks_adapters() {
    std::cerr << "rom_stream_blocks_adapters\n";
    gb::TestChannel ch;
    RecordingSink sink;
    ch.set_sink(&sink);
    serial(ch, "VELOCE 1 gb cpu\nCHECK 1 PASS a\n");
    ch.on_debug_breakpoint(3, 5, 8, 13, 21, 34);
    serial(ch, "Passed\n");
    CHECK(!ch.ended());
    CHECK_EQ(sink.out, std::string("VELOCE 1 gb cpu\nCHECK 1 PASS a\nPassed\n"));
}

static void test_mooneye() {
    std::cerr << "mooneye\n";
    {
        gb::TestChannel ch;
        RecordingSink sink;
        ch.set_sink(&sink);
        ch.on_debug_breakpoint(1, 2, 3, 4, 5, 6);   // incidental LD B,B (blargg)
        CHECK_EQ(sink.out, std::string());
        ch.on_debug_breakpoint(3, 5, 8, 13, 21, 34);
        CHECK_EQ(sink.out, std::string("{adapter=mooneye}REG b=03 c=05 d=08 e=0d h=15 l=22\n"
                                       "END 1/1\n"));
        ch.on_debug_breakpoint(0x42, 0x42, 0x42, 0x42, 0x42, 0x42);   // only once
        CHECK(sink.out.find("code=66") == std::string::npos);
    }
    {
        gb::TestChannel ch;
        RecordingSink sink;
        ch.set_sink(&sink);
        serial(ch, "partial");                      // mid-line: closed first
        ch.on_debug_breakpoint(0x42, 0x42, 0x42, 0x42, 0x42, 0x42);
        CHECK_EQ(sink.out, std::string("partial{adapter=mooneye}\nREG b=42 c=42 d=42 e=42 h=42 "
                                       "l=42\nEND 0/1 code=66\n"));
    }
}

static void test_blarggtext() {
    std::cerr << "blarggtext\n";
    {
        gb::TestChannel ch;
        RecordingSink sink;
        ch.set_sink(&sink);
        serial(ch, "01-special\n\n\nPassed\n");
        CHECK_EQ(sink.out, std::string("01-special\n\n\nPassed\n{adapter=blarggtext}END 1/1\n"));
        CHECK(ch.ended());
    }
    {
        gb::TestChannel ch;
        RecordingSink sink;
        ch.set_sink(&sink);
        serial(ch, "02-interrupts\n\nTimer doesn't work\nFailed #4\n");
        CHECK(sink.out.find("{adapter=blarggtext}END 0/1 code=4\n") != std::string::npos);
    }
    {
        gb::TestChannel ch;
        RecordingSink sink;
        ch.set_sink(&sink);
        serial(ch, "cpu_instrs\n\n01:ok  02:04\n\nFailed 1 tests.\n");
        CHECK(sink.out.find("{adapter=blarggtext}END 0/1\n") != std::string::npos);
    }
}

static void test_a000() {
    std::cerr << "a000\n";
    {   // text not mirrored to serial: streamed as LOG
        gb::TestChannel ch;
        RecordingSink sink;
        Memory mem;
        ch.set_sink(&sink);
        mem.a000(0x00, "stale\n");                 // left over: not yet armed
        ch.on_frame_end(mem.peek());
        CHECK(!ch.ended());
        mem.a000(0x80, "dmg_sound\n\n01:ok");
        ch.on_frame_end(mem.peek());
        mem.a000(0x80, "dmg_sound\n\n01:ok 02:ok\n");
        ch.on_frame_end(mem.peek());
        mem.a000(0x03, "dmg_sound\n\n01:ok 02:ok\n\nFailed #3");
        ch.on_frame_end(mem.peek());
        CHECK_EQ(sink.out, std::string("{adapter=a000}LOG dmg_sound\nLOG \nLOG 01:ok 02:ok\n"
                                       "LOG \nLOG Failed #3\nEND 0/1 code=3\n"));
        CHECK(ch.ended());
    }
    {   // text mirrored to serial: only the verdict; serial "Passed" does not double up
        gb::TestChannel ch;
        RecordingSink sink;
        Memory mem;
        ch.set_sink(&sink);
        mem.a000(0x80, "");
        ch.on_frame_end(mem.peek());
        serial(ch, "oam_bug\n\nPassed\n");
        mem.a000(0x00, "oam_bug\n\nPassed\n");
        ch.on_frame_end(mem.peek());
        CHECK_EQ(sink.out, std::string("{adapter=a000}oam_bug\n\nPassed\nEND 1/1\n"));
    }
    {   // reset handshake: one request per $80 -> $81 transition
        gb::TestChannel ch;
        RecordingSink sink;
        Memory mem;
        ch.set_sink(&sink);
        mem.a000(0x80, "");
        ch.on_frame_end(mem.peek());
        mem.a000(0x81, "");
        ch.on_frame_end(mem.peek());
        ch.on_frame_end(mem.peek());
        CHECK_EQ(sink.resets, 1);
        ch.on_test_reset();
        mem.a000(0x81, "");                        // not re-armed until $80 again
        ch.on_frame_end(mem.peek());
        CHECK_EQ(sink.resets, 1);
        mem.a000(0x80, "");
        ch.on_frame_end(mem.peek());
        mem.a000(0x81, "");
        ch.on_frame_end(mem.peek());
        CHECK_EQ(sink.resets, 2);
    }
}

static void test_hram() {
    std::cerr << "hram\n";
    {   // not armed (not a GBMicrotest cartridge): HRAM is ordinary RAM
        gb::TestChannel ch;
        RecordingSink sink;
        Memory mem;
        ch.set_sink(&sink);
        mem.bytes[0xFF82] = 0x01;
        ch.on_frame_end(mem.peek());
        CHECK_EQ(sink.out, std::string());
    }
    {
        gb::TestChannel ch;
        RecordingSink sink;
        Memory mem;
        ch.set_hram_armed(true);
        ch.set_sink(&sink);
        ch.on_frame_end(mem.peek());               // $FF82 still 0: running
        CHECK_EQ(sink.out, std::string());
        mem.bytes[0xFF80] = 0x12;
        mem.bytes[0xFF81] = 0x34;
        mem.bytes[0xFF82] = 0xFF;
        ch.on_frame_end(mem.peek());
        CHECK_EQ(sink.out, std::string("{adapter=hram}CHECK 1 FAIL hram exp=34 got=12\nEND 0/1\n"));
    }
    {
        gb::TestChannel ch;
        RecordingSink sink;
        Memory mem;
        ch.set_hram_armed(true);
        ch.set_sink(&sink);
        mem.bytes[0xFF80] = 0x05;
        mem.bytes[0xFF81] = 0x05;
        mem.bytes[0xFF82] = 0x01;
        ch.on_frame_end(mem.peek());
        CHECK_EQ(sink.out, std::string("{adapter=hram}CHECK 1 PASS hram exp=05 got=05\nEND 1/1\n"));
    }
}

int main() {
    test_serial_raw();
    test_rom_stream_blocks_adapters();
    test_mooneye();
    test_blarggtext();
    test_a000();
    test_hram();
    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cerr << "all gb_test_channel checks passed\n";
    return 0;
}
