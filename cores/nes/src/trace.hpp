#pragma once

#include <cstdint>
#include <cstdio>

namespace nes {

class Bus;
class CPU;
class PPU;

// nestest-style CPU instruction trace emitter.
//
// When the TRACE=1 environment variable is set, the NES core records one line
// per executed CPU instruction in the canonical Nintendulator/nestest.log
// format, e.g.:
//
//   C000  4C F5 C5  JMP $C5F5    A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 21 CYC:7
//
// Trace lines are written to a dedicated stream so they reach the test harness
// on a clean channel (uncontaminated by the binary's normal startup logging).
// The destination is, in priority order:
//   1. the file named by TRACE_FILE, if set (the testkit uses this)
//   2. otherwise stdout
//
// The disassembly is produced by peeking at memory (no PPU/APU side effects)
// using Bus::cpu_peek, so emitting a trace line never perturbs timing.
class CPUTrace {
public:
    // Returns the singleton tracer, or nullptr if TRACE is not enabled.
    static CPUTrace* instance();

    // Emit one nestest-format line describing the instruction the CPU is about
    // to execute. Must be called *before* CPU::step() reads/executes, so the
    // register/PPU/CYC columns reflect the pre-instruction state, exactly as
    // nestest.log records them.
    void emit(const CPU& cpu, const PPU& ppu, Bus& bus, uint64_t cpu_cycles);

private:
    CPUTrace() = default;
    ~CPUTrace();

    std::FILE* m_out = nullptr;   // trace destination (file or stdout)
    bool m_owns_file = false;     // true if we fopen'd m_out and must fclose it
};

} // namespace nes
