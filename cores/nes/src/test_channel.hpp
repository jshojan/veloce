#pragma once

// NES test-result channel (plugin ABI v2, docs/testing/VELOCE-RESULT.md).
//
// Two drivers feed the application-owned ITestSink:
//
//  port        $401E write = one channel byte (VELOCE-RESULT/1 text),
//              $401F write = control ($00 no-op, $01 terminator hint),
//              $401F read  = $56 ('V') probe.
//              $4018-$401F are the 2A03's disabled CPU test registers: writes
//              are ignored and reads are open bus on retail hardware, and no
//              mapper decodes below $4020, so a ROM can use the port and still
//              run on a console.
//
//  blargg6000  Legacy adapter for unmodified blargg ROMs. It watches CPU
//              writes to cart RAM: $6000 status, $6001-$6003 signature
//              DE B0 61, $6004+ zero-terminated text. The text is streamed as
//              "LOG" lines as the ROM writes it; a status below $80 becomes
//              "END 1/1" or "END 0/1 code=<n>"; status $81 ("press reset") is
//              forwarded to ITestSink::request_reset() once per $80->$81
//              transition. It follows the values the ROM *wrote*, never a
//              peek of cart RAM, so a stale battery save cannot fake a verdict.
//
// Precedence: the first write to $401E/$401F marks the ROM as port-aware and
// silences the adapter's LOG/END output for the rest of the session (a patched
// blargg shell keeps its $6000 block for other emulators and must not produce
// a second END). The $81 reset request stays active, because the port has no
// reset handshake of its own.
//
// Everything is a no-op while no sink is attached, so behaviour without
// VELOCE_TEST_OUT is unchanged. The channel holds no host-derived state
// (no clocks, no randomness); it is not part of save states.

#include <cstddef>
#include <cstdint>

namespace emu { struct ITestSink; }

namespace nes {

class TestChannel {
public:
    static constexpr uint16_t kPortData    = 0x401E;
    static constexpr uint16_t kPortControl = 0x401F;
    static constexpr uint8_t  kProbeValue  = 0x56;   // 'V'

    // Channel drivers, for IEmulatorPlugin::test_channels().
    static const char* channels() { return "port,blargg6000"; }

    // Attach (non-null) or detach (nullptr) the sink. Either way the adapter
    // state starts over.
    void set_sink(emu::ITestSink* sink);
    bool attached() const { return m_sink != nullptr; }

    // CPU write in $4000-$401F. Returns true when the write was consumed by
    // the port (only with a sink attached); the bus then skips APU dispatch.
    bool port_write(uint16_t address, uint8_t value);

    // CPU read in $4000-$401F. Returns true (and sets value) for the $401F
    // probe while a sink is attached.
    bool port_read(uint16_t address, uint8_t& value) const;

    // CPU write in $6000-$7FFF (after the cartridge saw it): blargg6000.
    void cart_write(uint16_t address, uint8_t value);

    // The application performed a requested reset: re-arm the $81 handshake.
    void on_reset();

private:
    bool signature_ok() const;
    bool adapter_live() const;
    void blargg_status(uint8_t status);
    void blargg_text(uint16_t offset, uint8_t value);
    void adapter_char(char c);
    void adapter_close_line();
    void adapter_note();
    void write(const char* s, size_t n);
    void write(const char* s);

    emu::ITestSink* m_sink = nullptr;

    bool m_port_used = false;          // ROM spoke on $401E/$401F

    // blargg6000 adapter state
    uint8_t  m_head[4] = {0, 0, 0, 0}; // $6000-$6003 as last written
    uint16_t m_text_tail = 0;          // next expected text offset from $6004
    bool m_line_open = false;          // a "LOG ..." line is in progress
    bool m_noted = false;              // "#VELOCE adapter=blargg6000" written
    bool m_ended = false;              // END synthesised
    bool m_reset_armed = true;         // next $81 requests a reset
};

} // namespace nes
