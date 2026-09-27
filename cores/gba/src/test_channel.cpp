#include "test_channel.hpp"

#include "arm7tdmi.hpp"
#include "bus.hpp"
#include "emu/emulator_plugin.hpp"

#include <cstdio>

namespace gba {

namespace {

// A ROM that reached its verdict parks in a one-instruction loop. The PC must
// sit there for this many consecutive frame boundaries before the r12 adapter
// reads the verdict (unchanged from the pre-channel detector).
constexpr int kStableFrames = 10;

constexpr uint32_t kArmBranchSelf   = 0xEAFFFFFEu;  // b .
constexpr uint16_t kThumbBranchSelf = 0xE7FEu;      // b .
constexpr uint32_t kArmLdmfdR0R12   = 0xE8BD1FFFu;  // ldmfd sp!,{r0-r12}
constexpr uint32_t kArmStmfdR0R12   = 0xE92D1FFFu;  // stmfd sp!,{r0-r12}
constexpr uint32_t kArmMovsR12Mask  = 0xFFFFFFF0u;  // movs r12, rX
constexpr uint32_t kArmMovsR12      = 0xE1B0C000u;

} // namespace

void TestChannel::reset() {
    m_last_level = -1;
    m_end_seen = false;
    m_r12_done = false;
    m_last_pc = 0;
    m_same_pc_frames = 0;
}

// ---- mgba -----------------------------------------------------------------

bool TestChannel::is_end_line(const char* line, size_t n) {
    // Mirror what the sink parses: control bytes (other than tab) are
    // dropped, then the line must be "END" followed by a blank or nothing.
    char head[4];
    size_t k = 0;
    for (size_t i = 0; i < n && k < sizeof(head); ++i) {
        unsigned char c = static_cast<unsigned char>(line[i]);
        if ((c < 0x20 && c != '\t') || c == 0x7F) continue;
        head[k++] = static_cast<char>(c);
    }
    if (k < 3 || head[0] != 'E' || head[1] != 'N' || head[2] != 'D') return false;
    return k == 3 || head[3] == ' ' || head[3] == '\t';
}

void TestChannel::debug_flush(const char* text, size_t n, int level) {
    if (!m_sink || !text || n == 0) return;

    if (level != m_last_level) {
        char value[12];
        std::snprintf(value, sizeof(value), "%d", level);
        m_sink->note("level", value);
        m_last_level = level;
    }

    // One flush is one line; a string with embedded newlines is several.
    m_sink->write(text, n);
    if (text[n - 1] != '\n') m_sink->write("\n", 1);

    if (!m_end_seen) {
        size_t start = 0;
        for (size_t i = 0; i <= n; ++i) {
            if (i == n || text[i] == '\n') {
                if (is_end_line(text + start, i - start)) {
                    m_end_seen = true;
                    break;
                }
                start = i + 1;
            }
        }
    }
}

// ---- r12 ------------------------------------------------------------------

bool TestChannel::peek_ok(uint32_t address) {
    switch (address >> 24) {
        case 0x02:                          // EWRAM
        case 0x03:                          // IWRAM
        case 0x08: case 0x09:               // ROM WS0
        case 0x0A: case 0x0B:               // ROM WS1
        case 0x0C:                          // ROM WS2 (0x0D may be EEPROM)
            return true;
        default:
            return false;
    }
}

uint32_t TestChannel::peek32(Bus& bus, uint32_t address) {
    return peek_ok(address) ? bus.read32(address) : 0;
}

uint16_t TestChannel::peek16(Bus& bus, uint32_t address) {
    return peek_ok(address) ? bus.read16(address) : 0;
}

int TestChannel::find_eval_register(Bus& bus, uint32_t idle_pc) {
    // jsmolka/alyosha gba-tests' shared m_test_eval macro (lib/macros.inc)
    // is `stmfd sp!,{r0-r12} / movs r12,rX / ... / ldmfd sp!,{r0-r12}`
    // followed by `idle: b idle`. The ldmfd restores r12 to whatever it held
    // *before* the eval, so r12 at the idle loop is the verdict only when the
    // ROM evaluates r12 itself (`m_test_eval r12`); ROMs that evaluate
    // another register (thumb.gba: r7, the LDM suite: r0 after a mode
    // switch, start_up_vbl_irq*: r7) leave an unrelated r12 behind, which was
    // reported as a false PASS (gba-01). The same ldmfd restores rX to its
    // pre-eval value -- which *is* the evaluated result, since `movs r12,rX`
    // does not modify rX -- so rX at the idle loop is exactly what the macro
    // tested.
    //
    // Recognise the epilogue near the idle PC, walk back to the macro's
    // stmfd/movs pair to learn X. If the pattern is absent (nba-hw-test,
    // FuzzARM, non-macro ROMs) the caller keeps the plain R12 convention.
    //
    // The .failed path's decimal digits at IWRAM 0x03000000/4/8 are
    // deliberately not consulted: a passing run never writes them, so they
    // hold whatever the test left there (memory.gba's IWRAM-mirror test
    // stores 1 at 0x03000000; the prefetcher_boundary ROMs put the stack at
    // IWRAM+16 so the stmfd itself lands r9-r11 there).
    uint32_t base = idle_pc & ~3u;
    uint32_t idle = 0;
    for (int off = -8; off <= 8 && !idle; off += 4) {
        uint32_t a = base + static_cast<uint32_t>(off);
        if (a >= 0x100 &&
            peek32(bus, a) == kArmBranchSelf &&
            peek32(bus, a - 4) == kArmLdmfdR0R12) {
            idle = a;
        }
    }
    if (!idle) return -1;

    // The macro body (two text paths) is ~100 ARM instructions; 2 KiB is
    // ample and cannot reach another m_test_eval (each ROM has exactly one).
    for (uint32_t p = idle - 8; p + 0x800 >= idle && p >= 0x100; p -= 4) {
        uint32_t w0 = peek32(bus, p);
        uint32_t w1 = peek32(bus, p + 4);
        if (w0 == kArmStmfdR0R12 && (w1 & kArmMovsR12Mask) == kArmMovsR12) {
            return static_cast<int>(w1 & 0xF);
        }
    }
    return -1;
}

void TestChannel::end_of_frame(const ARM7TDMI& cpu, Bus& bus) {
    // A ROM that reports over the mgba channel owns its verdict.
    if (!m_sink || m_r12_done || m_end_seen) return;

    uint32_t pc = cpu.get_pc();
    if (pc != m_last_pc) {
        m_last_pc = pc;
        m_same_pc_frames = 0;
        return;
    }
    if (++m_same_pc_frames < kStableFrames) return;

    // A stable PC alone is not a verdict: a crashed ROM (e.g. spinning in an
    // exception vector) or a CPU halted in an IntrWait loop also sits still.
    // Only a one-instruction self-branch counts as "parked on its result".
    // get_pc() is the address of the next instruction to execute.
    bool thumb = (cpu.get_cpsr() & 0x20) != 0;
    bool parked = thumb ? peek16(bus, pc & ~1u) == kThumbBranchSelf
                        : peek32(bus, pc & ~3u) == kArmBranchSelf;
    if (!parked) return;

    m_r12_done = true;

    uint32_t r12 = cpu.get_register(12);
    int eval_reg = thumb ? -1 : find_eval_register(bus, pc);  // the macro is ARM code
    uint32_t verdict = eval_reg >= 0 ? cpu.get_register(eval_reg) : r12;

    m_sink->note("adapter", "r12");

    char line[96];
    if (eval_reg >= 0 && eval_reg != 12) {
        std::snprintf(line, sizeof(line), "REG pc=%08x r12=%08x r%d=%08x\n",
                      pc, r12, eval_reg, verdict);
    } else {
        std::snprintf(line, sizeof(line), "REG pc=%08x r12=%08x\n", pc, r12);
    }
    m_sink->print(line);

    if (eval_reg >= 0) {
        std::snprintf(line, sizeof(line), "LOG verdict from m_test_eval register r%d\n", eval_reg);
    } else {
        std::snprintf(line, sizeof(line), "LOG m_test_eval epilogue not found; verdict from r12\n");
    }
    m_sink->print(line);

    if (verdict == 0) {
        m_sink->print("END 1/1\n");
    } else {
        std::snprintf(line, sizeof(line), "END 0/1 code=%u\n", verdict);
        m_sink->print(line);
    }
}

} // namespace gba
