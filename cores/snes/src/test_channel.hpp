#pragma once

// snes::TestChannel - the SNES side of the VELOCE-RESULT/1 test-result channel
// (docs/testing/VELOCE-RESULT.md). Pure logic: it never touches the bus, the
// PPU or a file. The Bus/plugin forward the relevant events here and the
// channel turns them into bytes, notes and blobs on the application-owned
// emu::ITestSink. With no sink attached every entry point is a no-op, so
// behaviour without VELOCE_TEST_OUT is unchanged.
//
// Drivers (test_channels() = "port,blargg6000,stp,sram,spcport"):
//
//   port        $21FE write = one channel byte (B-bus, any bank, DMA-able);
//               $21FF write = control ($00 nop, $01 terminator hint);
//               $21FF read  = probe, returns $56 ('V').
//   blargg6000  The blargg $6000-$60FF block: status byte at $6000,
//               signature DE B0 61 at $6001, text at $6004+. Text is streamed
//               as LOG lines as the ROM writes it; status < $80 synthesises
//               "END 1/1" or "END 0/1 code=<n>"; status $81 asks the
//               application for a reset once per $80 -> $81 transition.
//   stp / sram  A ROM that halts with STP (the byuu/jonasquinn convention):
//               note "stp=<pbr:pc> cgram0=<hex>", blob "sram" (cartridge
//               RAM), blob "cgram" (512 bytes), then "END 1/1" when colour 0
//               is the ROM's pass colour $7C00, "END 0/1" for its fail colour
//               $001F, otherwise finish(-1) (halted, verdict unknown).
//   spcport     gilyon spctest APU-port protocol (armed only for that ROM):
//               one CHECK per S-SMP test program the S-CPU uploads, END when
//               a test fails or when the S-CPU stops uploading programs.
//
// Once the ROM speaks the port itself, the legacy adapters fall silent: a
// ROM rebuilt with the Veloce emitter must not get a second, synthesised
// verdict.

#include "emu/emulator_plugin.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace snes {

// Blargg $6000 result block, streamed through the sink as it is written.
// Layout: $6000 status ($80 running, $81 needs reset, <$80 done: 0 pass,
// else fail code), $6001-$6003 signature DE B0 61 (or DE B0 47), $6004+
// NUL-terminated text written sequentially by the ROM's text_out.
struct BlarggTestState {
    static constexpr uint16_t BLOCK_SIZE = 0x100;
    static constexpr uint16_t TEXT_START = 4;
    static constexpr uint8_t SIGNATURE_0 = 0xDE;
    static constexpr uint8_t SIGNATURE_1 = 0xB0;
    static constexpr uint8_t SIGNATURE_2 = 0x61;
    static constexpr uint8_t SIGNATURE_2_ALT = 0x47;
    static constexpr uint8_t STATUS_RUNNING = 0x80;
    static constexpr uint8_t STATUS_NEEDS_RESET = 0x81;

    std::array<uint8_t, BLOCK_SIZE> mem{};
    bool detected = false;        // signature seen
    bool completed = false;       // END emitted
    bool reset_requested = false; // one request per $80 -> $81 transition
    size_t text_pos = 0;          // next text byte ($6004 + text_pos) to stream
    bool line_start = true;       // next streamed byte starts a LOG line

    void clear() { *this = BlarggTestState{}; }
    bool signature_ok() const {
        return mem[1] == SIGNATURE_0 && mem[2] == SIGNATURE_1 &&
               (mem[3] == SIGNATURE_2 || mem[3] == SIGNATURE_2_ALT);
    }
};

// gilyon spctest: the S-SMP reports through the APU ports.
//   program start      port0 = $00
//   program passed     port2/3 = last test number, port0 = $01
//   test failed        port2/3 = test number, port1 = PSW, port0 = $02;
//                      then port1 = A, port2 = X, port3 = Y, port0 = $03
// After a pass the S-CPU releases the S-SMP into the IPL ROM and either
// uploads the next program (writes port0) or stops.
struct SpcPortState {
    // Frames to wait, after the IPL ROM reports ready ($BBAA) following a
    // passed program, for the S-CPU to start the next upload. The S-CPU polls
    // for $BBAA in a tight loop, so a real continuation starts within one
    // frame; no upload after this many frames means it has finished.
    static constexpr int QUIET_FRAMES = 16;

    bool armed = false;
    bool noted = false;
    bool completed = false;
    bool in_program = false;
    std::array<uint8_t, 4> out{};  // last value the S-SMP wrote to each port
    int programs_passed = 0;
    bool fail_pending = false;     // saw port0 = $02, waiting for $03
    uint16_t fail_test = 0;
    uint8_t fail_psw = 0;
    bool awaiting_upload = false;  // a program passed; did the S-CPU continue?
    bool ipl_ready = false;        // IPL ROM put $BBAA on the ports since then
    int quiet_frames = 0;

    void clear() { *this = SpcPortState{}; }
};

class TestChannel {
public:
    static constexpr uint16_t PORT_DATA = 0x21FE;
    static constexpr uint16_t PORT_CONTROL = 0x21FF;
    static constexpr uint8_t PROBE_VALUE = 0x56;   // 'V'
    static constexpr uint16_t STP_PASS_COLOR = 0x7C00;
    static constexpr uint16_t STP_FAIL_COLOR = 0x001F;

    static const char* channels() { return "port,blargg6000,stp,sram,spcport"; }

    // Attach (non-null) or detach (nullptr) the sink. Either way all adapter
    // state starts over.
    void set_sink(emu::ITestSink* sink);
    emu::ITestSink* sink() const { return m_sink; }
    bool active() const { return m_sink != nullptr; }

    // The application performed a ROM-requested reset: re-arm the adapters
    // that act once per power cycle (blargg reset request, STP).
    void on_test_reset();

    // ---- port ($21FE/$21FF) ----
    void port_write(uint16_t offset, uint8_t value);
    // Returns true and sets value for the probe read; false = not handled.
    bool port_read(uint16_t offset, uint8_t& value) const;
    bool port_used() const { return m_port_used; }

    // ---- blargg6000 ($6000-$60FF, offset 0-255) ----
    void blargg_write(uint16_t offset, uint8_t value);
    const BlarggTestState& blargg() const { return m_blargg; }

    // ---- stp / sram ----
    bool stp_armed() const { return active() && !m_stp_reported && !m_port_used; }
    void on_stp(uint8_t pbr, uint16_t pc, const uint8_t* cgram, size_t cgram_size,
                const uint8_t* sram, size_t sram_size);

    // ---- spcport ----
    void arm_spcport() { if (active()) m_spc.armed = true; }
    bool spcport_armed() const { return active() && m_spc.armed && !m_spc.completed; }
    // S-SMP wrote port (0-3). from_ipl: the write came from the IPL ROM.
    void spc_port_write(int port, uint8_t value, bool from_ipl);
    // S-CPU wrote $2140+port.
    void cpu_apu_write(int port, uint8_t value);
    // Called once per emulated frame.
    void end_frame();
    const SpcPortState& spcport() const { return m_spc; }

private:
    void print(const char* text);
    void blargg_emit_char(uint8_t c);
    void blargg_status(uint8_t prev, uint8_t value);
    bool legacy_enabled() const { return m_sink != nullptr && !m_port_used; }

    emu::ITestSink* m_sink = nullptr;
    bool m_port_used = false;
    BlarggTestState m_blargg;
    bool m_blargg_noted = false;
    bool m_stp_reported = false;
    SpcPortState m_spc;
};

} // namespace snes
