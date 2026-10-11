#pragma once

// gb::TestChannel: the Game Boy side of the VELOCE-RESULT/1 test-result
// channel (docs/testing/VELOCE-RESULT.md, plugin ABI v2).
//
// Drivers, in the order they can claim a verdict:
//
//   serial      Every byte a ROM sends with SC=$81 goes to the sink raw (no
//               printable filter). A ROM built with the Veloce emitter
//               (tests/roms-src/include) prints "VELOCE 1 gb <suite>",
//               CHECK lines and its own "END p/t"; the adapters below then
//               stand down so they never add a second terminator.
//   mooneye     Tier C. LD B,B with the Fibonacci fingerprint in B..L
//               (3,5,8,13,21,34 pass; all $42 fail) -> REG dump + END.
//               Blargg ROMs also execute LD B,B, so the opcode alone is
//               never enough.
//   a000        Tier B. Blargg's cart-RAM block: $A000 status ($80 running,
//               $81 "press reset", <$80 result code), $A001-3 = $DE $B0 $61,
//               $A004.. NUL-terminated text. Probed once per frame, streamed
//               like the NES blargg6000 adapter (LOG lines, END).
//   hram        Tier B. GBMicrotest: $FF82 = $01 pass / $FF fail, $FF80 got,
//               $FF81 expected. Probed at frame end -> CHECK + END. Armed
//               only for cartridges titled "microtest" (the suite has no
//               in-RAM signature; other ROMs use HRAM freely).
//   blarggtext  Tier B. Blargg shells without the $A000 block (cpu_instrs,
//               instr_timing, mem_timing) end their serial text with a
//               "Passed..." or "Failed..." line; that line becomes END.
//
// With no sink attached every entry point returns immediately, so emulation
// is unchanged when VELOCE_TEST_OUT is unset. Nothing here reads host time or
// any other nondeterministic state.

#include "emu/emulator_plugin.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace gb {

class TestChannel {
public:
    // Reads one byte of the GB address space without side effects. The
    // adapters only look at $A000-$BFFF (cart RAM, bank 0) and $FF80-$FF82.
    using Peek = std::function<uint8_t(uint16_t)>;

    static constexpr const char* kChannels = "serial,mooneye,a000,hram,blarggtext";

    // Attach (or with nullptr detach) the application's sink. Clears all
    // adapter state; the hram arming set by set_hram_armed() is kept.
    void set_sink(emu::ITestSink* sink);
    bool active() const { return m_sink != nullptr; }

    // GBMicrotest adapter arming (cartridge title "microtest").
    void set_hram_armed(bool armed) { m_hram_armed = armed; }

    // SC written with $81: `sb` is the byte being shifted out.
    void on_serial_byte(uint8_t sb);

    // LD B,B (opcode $40) executed with these register values.
    void on_debug_breakpoint(uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t h, uint8_t l);

    // End of a frame: probe the $A000 block and the HRAM triple.
    void on_frame_end(const Peek& peek);

    // The application performed a reset the ROM asked for ($A000 = $81).
    void on_test_reset();

    // True once a terminator went out (from the ROM or an adapter).
    bool ended() const { return m_ended; }

private:
    void serial_line_done();
    void adapter_note(const char* name);
    void adapter_write(const std::string& text);   // at a line boundary
    void a000_probe(const Peek& peek);
    void hram_probe(const Peek& peek);
    bool adapters_enabled() const { return m_sink && !m_ended && !m_rom_stream; }

    emu::ITestSink* m_sink = nullptr;
    bool m_hram_armed = false;

    // Mirror of the sink's current serial line (same filter as TestFileSink)
    // so adapters can tell whether they are at a line boundary and recognise
    // a ROM-emitted header, END or blargg verdict line.
    std::string m_line;
    bool m_serial_text = false;   // any printable byte came over serial
    bool m_rom_stream = false;    // "VELOCE 1 ..." header seen on serial
    bool m_ended = false;         // an END line has gone to the sink
    std::string m_adapter;        // adapter already announced via note()

    // a000
    bool m_a000_seen = false;     // block armed at least once (owns the verdict)
    bool m_a000_running = false;  // status $80 observed (armed)
    bool m_a000_reset_sent = false;
    uint16_t m_a000_text_pos = 0; // next unread offset from $A004
    std::string m_a000_line;      // partial text line (streamed as LOG)
};

} // namespace gb
