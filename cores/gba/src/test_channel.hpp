#pragma once

// VELOCE-RESULT/1 test-result channel for the GBA core
// (docs/testing/VELOCE-RESULT.md, section 7).
//
// Two drivers feed the application-owned emu::ITestSink:
//
//   mgba  (Tier A) - every line a ROM flushes through the mGBA debug
//                    registers (0x4FFF600 string, 0x4FFF700 flags) is
//                    forwarded verbatim, one sink line per flush. The mGBA
//                    log level is recorded as a "#VELOCE level=<n>" note
//                    whenever it changes; it is never a filter.
//   r12   (Tier C) - legacy adapter for unmodified jsmolka/alyosha-style
//                    ROMs that park in a one-instruction `b .` loop with the
//                    failing test number in a register (0 = pass). Only
//                    consulted while no END line has come over the mgba
//                    channel. Emits "END 1/1" or "END 0/1 code=<n>".
//
// Nothing here runs without a sink attached, so a normal (non-test) run is
// unaffected. All state is plain members reset by reset(): no statics, no
// host time.

#include <cstddef>
#include <cstdint>

namespace emu { struct ITestSink; }

namespace gba {

class ARM7TDMI;
class Bus;

class TestChannel {
public:
    // Channel names for IEmulatorPlugin::test_channels().
    static constexpr const char* kChannels = "mgba,r12";

    void set_sink(emu::ITestSink* sink) { m_sink = sink; reset(); }
    void complete_observations();
    bool attached() const { return m_sink != nullptr; }

    // Re-arm the adapters (power-on / app-performed reset).
    void reset();

    // mgba: one REG_DEBUG_FLAGS flush. `text` is the debug string up to its
    // first NUL (at most 256 bytes); `level` is the flags' low 3 bits.
    void debug_flush(const char* text, size_t n, int level);

    // r12: called once at the end of every emulated frame.
    void end_of_frame(const ARM7TDMI& cpu, Bus& bus);

private:
    // Side-effect-free code fetch for the adapter's pattern checks: only
    // EWRAM, IWRAM and cartridge ROM (no BIOS latch, no save-chip state).
    static bool peek_ok(uint32_t address);
    static uint32_t peek32(Bus& bus, uint32_t address);
    static uint16_t peek16(Bus& bus, uint32_t address);

    // Register that jsmolka/alyosha's m_test_eval macro evaluated, found by
    // walking back from the `ldmfd sp!,{r0-r12}; b .` epilogue to its
    // `stmfd sp!,{r0-r12}; movs r12,rX` prologue. -1 when absent.
    static int find_eval_register(Bus& bus, uint32_t idle_pc);

    // True when the sink will read this line as an END terminator.
    static bool is_end_line(const char* line, size_t n);

    emu::ITestSink* m_sink = nullptr;

    // mgba
    int m_last_level = -1;      // last level noted (-1: none yet)
    bool m_end_seen = false;    // an END line came over the mgba channel

    // r12
    bool m_r12_done = false;
    uint32_t m_last_pc = 0;
    int m_same_pc_frames = 0;
};

} // namespace gba
