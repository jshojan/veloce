#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include "emu/input_types.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using emulator_test::require;
using n64::sign32;
constexpr uint64_t start = 0xffffffff80000400ull;
static uint32_t r(unsigned fn, unsigned rs = 0, unsigned rt = 0, unsigned rd = 0, unsigned sa = 0) {
    return rs << 21 | rt << 16 | rd << 11 | sa << 6 | fn;
}
static uint32_t i(unsigned op, unsigned rs, unsigned rt, uint16_t imm = 0) {
    return op << 26 | rs << 21 | rt << 16 | imm;
}
static void program(n64::Machine& m, std::initializer_list<uint32_t> instructions) {
    m.reset();
    uint64_t pc = start;
    for (auto ins : instructions) { require(m.write(pc, 4, ins), "instruction store failed"); pc += 4; }
    m.cpu.reset(start);
}
static void control(n64::Machine& m) {
    for (bool take : {false, true}) {
        for (bool likely : {false, true}) {
            for (bool unequal : {false, true}) {
                program(m, {i(unequal ? (likely ? 21 : 5) : (likely ? 20 : 4), 1, 2, 2),
                            i(9, 0, 3, 7), i(9, 0, 4, 9), i(9, 0, 5, 11)});
                m.cpu.state.gpr[1] = 1;
                m.cpu.state.gpr[2] = (take != unequal) ? 1 : 2;
                m.cpu.step();
                require(m.cpu.state.pc == start + ((likely && !take) ? 8 : 4), "branch/likely PC wrong");
                if (!(likely && !take)) {
                    require(m.cpu.state.delay_slot, "untaken ordinary branch still has delay slot");
                    m.cpu.step();
                    require(m.cpu.state.gpr[3] == 7, "branch delay did not execute");
                }
                require(m.cpu.state.pc == start + (take ? 12 : 8), "branch target/fallthrough wrong");
                m.cpu.step();
                require(take ? m.cpu.state.gpr[5] == 11 && m.cpu.state.gpr[4] == 0 : m.cpu.state.gpr[4] == 9,
                        "branch path wrong");
                require(m.cpu.state.gpr[3] == ((likely && !take) ? 0u : 7u), "likely annulment wrong");
            }
        }
    }
    program(m, {r(9, 1, 0, 1), i(9, 0, 2, 4)}); // JALR with source/destination alias
    m.cpu.state.gpr[1] = start + 0x40;
    m.cpu.step(); m.cpu.step();
    require(m.cpu.state.pc == start + 0x40 && m.cpu.state.gpr[1] == start + 8 && m.cpu.state.gpr[2] == 4,
            "JALR old target/link/delay wrong");
    program(m, {(3u << 26) | (uint32_t(start + 0x40) >> 2 & 0x03ffffff), 0});
    m.cpu.step(); m.cpu.step();
    require(m.cpu.state.pc == start + 0x40 && m.cpu.state.gpr[31] == start + 8, "JAL failed");
    program(m, {i(9, 0, 0, 12)}); m.cpu.step();
    require(m.cpu.state.gpr[0] == 0, "$zero changed");
}
static void exceptions(n64::Machine& m) {
    for (bool wide : {false, true}) {
        for (bool subtract : {false, true}) {
            for (bool delay : {false, true}) {
                const auto ins = r(wide ? (subtract ? 0x2e : 0x2c) : (subtract ? 0x22 : 0x20), 1, 2, 3);
                if (delay) program(m, {i(4, 0, 0, 3), ins}); else program(m, {ins});
                m.cpu.state.gpr[1] = subtract ? (wide ? 0x8000000000000000ull : sign32(0x80000000))
                                            : (wide ? 0x7fffffffffffffffull : 0x7fffffffull);
                m.cpu.state.gpr[2] = 1; m.cpu.state.gpr[3] = 123;
                if (delay) m.cpu.step();
                m.cpu.step();
                auto& s = m.cpu.state;
                require((s.cp0[13] & 0x7c) == 48 && s.cp0[14] == start && (s.cp0[12] & 2), "overflow exception context wrong");
                require(bool(s.cp0[13] & 0x80000000) == delay && s.gpr[3] == 123, "overflow BD or destination wrong");
                require(s.pc == 0xffffffff80000180ull, "general exception vector wrong");
                // Nested exception preserves outer EPC/BD while EXL is set.
                require(m.write(s.pc, 4, r(0x0d)), "nested exception code write failed");
                m.cpu.step();
                require(s.cp0[14] == start && bool(s.cp0[13] & 0x80000000) == delay, "nested exception lost EPC/BD");
                require(m.write(s.pc, 4, 0x42000018), "ERET code write failed");
                s.cp0[14] = start + 0x80;
                m.cpu.step();
                require(s.pc == start + 0x80 && !(s.cp0[12] & 2) && !s.delay_slot, "ERET failed");
            }
        }
    }
    for (auto code : {8u, 9u, 10u}) {
        program(m, {code == 8 ? r(0x0c) : code == 9 ? r(0x0d) : 0x7c000000u});
        m.cpu.step(); require((m.cpu.state.cp0[13] & 0x7c) == code * 4, "SYSCALL/BREAK/RI wrong");
    }
    program(m, {i(35, 1, 2, 1)}); m.cpu.state.gpr[1] = start;
    m.cpu.step(); require((m.cpu.state.cp0[13] & 0x7c) == 16 && m.cpu.state.cp0[8] == start + 1, "unaligned load exception wrong");
    program(m, {i(43, 1, 2, 1)}); m.cpu.state.gpr[1] = start;
    m.cpu.step(); require((m.cpu.state.cp0[13] & 0x7c) == 20 && m.cpu.state.cp0[8] == start + 1, "unaligned store exception wrong");
    program(m, {0}); m.cpu.state.pc = start + 1;
    m.cpu.step(); require((m.cpu.state.cp0[13] & 0x7c) == 16 && m.cpu.state.cp0[14] == start + 1, "fetch alignment wrong");
}
static void memory(n64::Machine& m) {
    program(m, {i(32, 1, 2), i(36, 1, 3), i(33, 1, 4), i(37, 1, 5),
                i(35, 1, 6), i(39, 1, 7), i(55, 1, 8), i(63, 1, 8, 8)});
    const uint64_t base = 0xffffffff80002000ull;
    require(m.write(base, 8, 0x89abcdef01234567ull), "doubleword write failed");
    uint64_t value = 0;
    require(m.read(0xffffffffa0002000ull, 8, value) && value == 0x89abcdef01234567ull, "KSEG aliases/big endian wrong");
    m.cpu.state.gpr[1] = base;
    for (unsigned n = 0; n < 8; ++n) m.cpu.step();
    const auto& g = m.cpu.state.gpr;
    require(g[2] == 0xffffffffffffff89ull && g[3] == 0x89 && g[4] == 0xffffffffffff89abull && g[5] == 0x89ab,
            "byte/halfword sign extension wrong");
    require(g[6] == 0xffffffff89abcdefull && g[7] == 0x89abcdef && g[8] == 0x89abcdef01234567ull,
            "word/doubleword load wrong");
    require(m.read(base + 8, 8, value) && value == g[8], "SD failed");
    require(!m.write(0xffffffffb0000000ull, 4, 0), "cartridge write accepted");
    require(!m.read(0x80002000, 4, value), "noncanonical address accepted");
    require(!m.read(0xffffffff80000000ull + m.ram_size, 4, value), "RAM overrun accepted");
}
static void merge_memory(n64::Machine& m) {
    const uint8_t original[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                                0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    for (const uint64_t base : {0xffffffff80003000ull, 0xffffffffa4001000ull}) {
        for (unsigned width : {4u, 8u}) {
            for (unsigned lane = 0; lane < width; ++lane) {
                for (bool reverse : {false, true}) {
                    const unsigned left_load = width == 4 ? 0x22 : 0x1a, right_load = width == 4 ? 0x26 : 0x1b;
                    const unsigned left_store = width == 4 ? 0x2a : 0x2c, right_store = width == 4 ? 0x2e : 0x2d;
                    const auto load_left = i(left_load, 1, 2, lane), load_right = i(right_load, 1, 2, lane + width - 1);
                    program(m, {reverse ? load_right : load_left, reverse ? load_left : load_right});
                    for (unsigned n = 0; n < 16; ++n) require(m.write(base + n, 1, original[n]), "merge fixture failed");
                    m.cpu.state.gpr[1] = base; m.cpu.state.gpr[2] = 0xa5a5a5a5a5a5a5a5ull;
                    uint64_t expected = 0;
                    for (unsigned n = 0; n < width; ++n) expected = expected * 256 + original[lane + n];
                    if (width == 4) expected = sign32(uint32_t(expected));
                    m.cpu.step(); m.cpu.step();
                    require(m.cpu.state.gpr[2] == expected && !(m.cpu.state.cp0[12] & 2), "paired merge load/order failed");
                    const auto store_left = i(left_store, 1, 2, lane), store_right = i(right_store, 1, 2, lane + width - 1);
                    program(m, {reverse ? store_right : store_left, reverse ? store_left : store_right});
                    for (unsigned n = 0; n < 16; ++n) m.write(base + n, 1, original[n]);
                    const uint64_t reg = 0x8badf00dfedcba98ull;
                    m.cpu.state.gpr[1] = base; m.cpu.state.gpr[2] = reg;
                    m.cpu.step(); m.cpu.step();
                    for (unsigned n = 0; n < 16; ++n) {
                        uint64_t got = 0;
                        require(m.read(base + n, 1, got), "paired store readback failed");
                        const uint8_t want = n >= lane && n < lane + width ? uint8_t(reg >> (8 * (width - 1 - (n - lane)))) : original[n];
                        require(got == want, "paired merge store damaged bytes or order failed");
                    }
                    require(m.cpu.state.gpr[2] == reg && !(m.cpu.state.cp0[12] & 2), "merge store changed source or faulted");
                }
            }
        }
    }
    const uint64_t base = 0xffffffff80003000ull;
    program(m, {i(0x1a, 1, 1, 1)}); // base and target alias: use old base.
    m.write(base, 8, 0x0123456789abcdefull); m.cpu.state.gpr[1] = base;
    m.cpu.step(); require(m.cpu.state.gpr[1] == 0x23456789abcdef00ull, "merge base/target alias failed");
    program(m, {i(0x1b, 1, 0, 2)}); m.cpu.state.gpr[1] = base;
    m.cpu.step(); require(m.cpu.state.gpr[0] == 0 && !(m.cpu.state.cp0[12] & 2), "$zero merge load failed");
    program(m, {i(0x1a, 1, 2, uint16_t(-1))});
    m.write(base, 8, 0x0123456789abcdefull); m.cpu.state.gpr[1] = base + 1;
    m.cpu.step(); require(m.cpu.state.gpr[2] == 0x0123456789abcdefull, "negative merge offset failed");
    program(m, {i(0x2c, 1, 2, 3)}); m.cpu.state.gpr[1] = 0xffffffffb0000000ull;
    auto rom = m.rom; m.cpu.step();
    require(m.rom == rom && (m.cpu.state.cp0[13] & 0x7c) == 28 && m.cpu.state.cp0[8] == 0,
            "protected merge store/fault address wrong");
    m.write(base, 8, 0x0123456789abcdefull);
    require(!m.write_masked(base, 4, 0, 0x80) && !m.write_masked(base + 1, 8, 0, 0xff), "invalid mask/alignment accepted");
    uint64_t unchanged = 0; m.read(base, 8, unchanged);
    require(unchanged == 0x0123456789abcdefull, "rejected masked store changed storage");
    // Saving between two dependent merge loads retains the partially merged GPR.
    program(m, {i(0x1a, 1, 2, 3), i(0x1b, 1, 2, 10)});
    m.write(base, 8, 0x0123456789abcdefull); m.write(base + 8, 8, 0xfedcba9876543210ull);
    m.cpu.state.gpr[1] = base; m.cpu.state.gpr[2] = 0xa5a5a5a5a5a5a5a5ull;
    m.cpu.step();
    std::vector<uint8_t> partial, baseline, repeated;
    m.save(partial); m.cpu.step(); m.save(baseline);
    require(m.restore(partial), "partial merge state rejected");
    m.cpu.step(); m.save(repeated);
    require(repeated == baseline && m.cpu.state.gpr[2] == 0x6789abcdeffedcbaull, "merge state continuation failed");
}
struct StoreTrace : n64::Memory {
    n64::Machine& m;
    unsigned data_reads = 0, masked_writes = 0;
    StoreTrace(n64::Machine& machine) : m(machine) {}
    bool read(uint64_t address, unsigned bytes, uint64_t& value) override {
        if (address == 0xffffffff80003000ull) ++data_reads;
        return m.read(address, bytes, value);
    }
    bool write(uint64_t address, unsigned bytes, uint64_t value) override { return m.write(address, bytes, value); }
    bool write_masked(uint64_t address, unsigned bytes, uint64_t value, uint8_t mask) override {
        ++masked_writes; return m.write_masked(address, bytes, value, mask);
    }
};
static void masked_store_transaction(n64::Machine& m) {
    program(m, {i(0x2d, 1, 2, 2)});
    StoreTrace trace(m); n64::VR4300 cpu(trace); cpu.reset(start);
    m.write(0xffffffff80003000ull, 8, 0x0123456789abcdefull);
    cpu.state.gpr[1] = 0xffffffff80003000ull; cpu.state.gpr[2] = 0xfedcba9876543210ull;
    cpu.step();
    uint64_t value = 0; m.read(0xffffffff80003000ull, 8, value);
    require(trace.data_reads == 0 && trace.masked_writes == 1 && value == 0x5432106789abcdefull,
            "merge store performed RMW read or incorrect transaction");
}
static void multiply_divide(n64::Machine& m) {
    constexpr uint64_t max = 0xffffffffffffffffull, min = 0x8000000000000000ull;
    struct Reference { unsigned fn; uint64_t a, b, lo, hi; };
    const Reference cases[] = {
        {0x1c, max, max, 1, 0}, {0x1d, max, max, 1, max - 1},
        {0x1c, min, 2, 0, max}, {0x1c, min, max, min, 0},
        {0x1d, 0x100000000ull, 0x100000000ull, 0, 1},
        {0x1d, max, 2, max - 1, 1},
        {0x1e, min, 3, 0xd555555555555556ull, max - 1},
        {0x1e, max - 6, 3, max - 1, max},
        {0x1e, 7, max - 2, max - 1, 1},
        {0x1f, max, 2, 0x7fffffffffffffffull, 1},
        // Deterministic undefined-input conventions; not hardware accuracy scores.
        {0x1e, min, max, min, 0}, {0x1e, min, 0, 1, min},
        {0x1e, 7, 0, max, 7}, {0x1f, max, 0, max, max},
    };
    for (const auto& test : cases) {
        program(m, {r(test.fn, 1, 2), r(0x12, 0, 0, 3), r(0x10, 0, 0, 4)});
        m.cpu.state.gpr[1] = test.a; m.cpu.state.gpr[2] = test.b;
        m.cpu.state.hi = 0x1234; m.cpu.state.lo = 0x5678;
        for (unsigned n = 0; n < 3; ++n) m.cpu.step();
        require(m.cpu.state.gpr[3] == test.lo && m.cpu.state.gpr[4] == test.hi,
                "doubleword multiply/divide reference failed");
        require(!(m.cpu.state.cp0[12] & 2), "doubleword multiply/divide raised exception");
    }
    program(m, {r(0x1d, 1, 2), r(0x1f, 1, 2)});
    m.cpu.state.gpr[1] = max; m.cpu.state.gpr[2] = max;
    m.cpu.step();
    std::vector<uint8_t> saved, baseline, repeated;
    require(m.save(saved), "wide product state save failed");
    m.cpu.step(); m.save(baseline);
    require(m.restore(saved) && m.cpu.state.hi == max - 1 && m.cpu.state.lo == 1,
            "128-bit product state lost high bits");
    m.cpu.step(); m.save(repeated);
    require(repeated == baseline, "wide product state continuation diverged");
}
struct InterruptBus : n64::Memory {
    n64::Machine& machine;
    uint8_t levels = 0;
    unsigned reads = 0;
    explicit InterruptBus(n64::Machine& m) : machine(m) {}
    uint8_t interrupt_pending() const override { return levels; }
    bool read(uint64_t a, unsigned n, uint64_t& v) override { ++reads; return machine.read(a,n,v); }
    bool write(uint64_t a, unsigned n, uint64_t v) override { return machine.write(a,n,v); }
    bool write_masked(uint64_t a, unsigned n, uint64_t v, uint8_t mask) override { return machine.write_masked(a,n,v,mask); }
};
static void interrupts(n64::Machine& m) {
    m.reset();
    require(m.write(start,4,i(9,0,3,123)) && m.write(start+4,4,i(9,0,3,123)), "interrupt probe store failed");
    InterruptBus bus(m); n64::VR4300 cpu(bus);
    // All eight request bits, all masks, all IE/EXL/ERL states, both BD/vector choices.
    // External pins are injected into the CPU component here; only SI exists on the board.
    for (unsigned pending=0; pending<256; ++pending) for (unsigned mask=0; mask<256; ++mask)
        for (unsigned control=0; control<8; ++control) for (bool delay : {false,true}) for (bool bev : {false,true}) {
            cpu.reset(start); auto& c=cpu.state;
            if (delay) { c.pc=start+4; c.next_pc=start+0x40; c.delay_slot=true; }
            c.cp0[12]=(mask<<8)|control|(bev ? 1u<<22 : 0);
            c.cp0[13]=(pending<<8)|0x7c; c.cp0[14]=0x12345678;
            bus.levels=uint8_t(pending); bus.reads=0;
            cpu.step();
            const bool take=control==1 && (mask & pending);
            if (take) {
                require(c.pc==(bev ? 0xffffffffbfc00380ull : 0xffffffff80000180ull) && c.cp0[14]==start,
                        "interrupt vector/EPC wrong");
                require((c.cp0[13]&0x8000007c)==(delay ? 0x80000000ull : 0) && (c.cp0[12]&2), "interrupt cause/status wrong");
                require(c.gpr[3]==0 && bus.reads==0 && !c.delay_slot, "interrupt fetched/executed interrupted instruction");
            } else {
                require(c.gpr[3]==123 && c.cp0[14]==0x12345678 && bus.reads==1 && (c.cp0[13]&0x7c)==0x7c,
                        "masked interrupt entered exception");
            }
            require((c.cp0[13]&0xff00)==(pending<<8), "pending requests corrupted");
        }
    // Only Cause.IP0/IP1 may be changed by MTC0; exception context, hardware and timer remain.
    require(m.write(start,4,0x40816800), "Cause write code failed");
    for (uint32_t value : {0u,0x100u,0x200u,0x300u,0xffffffffu}) {
        cpu.reset(start); bus.levels=0x7c;
        cpu.state.cp0[13]=0xb000fc30; cpu.state.gpr[1]=value; cpu.step();
        require(cpu.state.cp0[13]==(0xb000fc30ull|(value&0x300)), "Cause read-only fields writable");
    }
    // Timer latches only on Count equality, survives further ticks, and Compare writes clear it.
    require(m.write(start,4,0) && m.write(start+4,4,0), "timer code store failed");
    bus.levels=0;
    for (uint32_t count : {0u,1u,0xfffffffeu,0xffffffffu})
        for (uint32_t compare : {0u,1u,2u,0xffffffffu}) for (unsigned phase : {0u,1u}) {
            cpu.reset(start); auto& c=cpu.state; c.cp0[9]=count; c.cp0[11]=compare; c.instructions=phase;
            cpu.step(); const uint32_t next=count+(phase ? 1u : 0u);
            require(c.cp0[9]==next && bool(c.cp0[13]&0x8000)==bool(phase && next==compare), "timer boundary/equality wrong");
        }
    cpu.reset(start); cpu.state.cp0[9]=0xfffffffe; cpu.state.cp0[11]=0xffffffff;
    cpu.step(); cpu.step(); require(cpu.state.cp0[13]&0x8000, "timer not latched");
    cpu.state.pc=start; cpu.state.next_pc=start+4; cpu.step(); require(cpu.state.cp0[13]&0x8000, "timer latch lost after match");
    require(m.write(start,4,0x40815800), "Compare write code failed");
    cpu.state.pc=start; cpu.state.next_pc=start+4; cpu.state.gpr[1]=123;
    cpu.step(); require(cpu.state.cp0[11]==123 && !(cpu.state.cp0[13]&0x8000), "Compare did not acknowledge timer");
    // ERL selects ErrorEPC and clears ERL while retaining EXL; EXL selects EPC.
    require(m.write(start,4,0x42000018), "ERET code store failed");
    for (unsigned control : {0u,2u,4u,6u}) {
        cpu.reset(start); auto& c=cpu.state; c.cp0[12]=control; c.cp0[14]=start+0x40; c.cp0[30]=start+0x80;
        cpu.step(); require(c.pc==start+((control&4) ? 0x80 : 0x40) && c.cp0[12]==((control&4) ? control&~4u : control&~2u),
                            "ERET error/exception level wrong");
    }
    for (unsigned rd : {14u,30u}) for (uint32_t value : {0u,0x7fffffffu,0x80000000u,0xffffffffu}) {
        require(m.write(start,4,0x40810000|(rd<<11)), "EPC write code failed");
        cpu.reset(start); cpu.state.gpr[1]=value; cpu.step();
        require(cpu.state.cp0[rd]==sign32(value), "MTC0 EPC/ErrorEPC extension wrong");
    }
    // Timer deadline and pending latch survive full state restoration.
    m.reset(); require(m.write(start,4,0), "timer state code failed"); m.cpu.reset(start);
    m.cpu.state.cp0[9]=0xfffffffe; m.cpu.state.cp0[11]=0xffffffff; m.cpu.state.cp0[12]=0x8001;
    m.cpu.state.instructions=1;
    std::vector<uint8_t> deadline, timer_after, timer_replay;
    require(m.save(deadline), "timer deadline save failed");
    for (unsigned n=0;n<3;++n) m.cpu.step(); m.save(timer_after);
    require((m.cpu.state.cp0[13]&0x8000) && m.cpu.state.cp0[14]==start, "timer interrupt not delivered");
    require(m.restore(deadline), "timer deadline restore failed");
    for (unsigned n=0;n<3;++n) m.cpu.step(); m.save(timer_replay);
    require(timer_after==timer_replay, "timer deadline continuation diverged");
    // MI changes and SI acknowledgements drive a live level, even during EXL.
    m.reset(); require(m.write(start,4,0), "SI interrupt code failed"); m.cpu.reset(start);
    constexpr uint64_t si=0xffffffffa4800000ull, mi=0xffffffffa4300000ull;
    require(m.write(si,4,0x10000) && m.write(si+4,4,0), "SI pending setup failed");
    m.cpu.step(); require(!(m.cpu.state.cp0[13]&0x400) && m.si_interrupt, "MI-masked SI reached IP2");
    require(m.write(mi+12,4,8), "MI enable failed"); m.cpu.state.cp0[12]=0x401;
    m.cpu.step(); require(m.cpu.state.pc==0xffffffff80000180ull && (m.cpu.state.cp0[13]&0x400), "SI did not reach CPU");
    std::vector<uint8_t> pending, after, replay;
    require(m.save(pending), "pending interrupt save failed");
    require(m.write(si+24,4,0) && m.write(m.cpu.state.pc,4,0), "SI handler setup failed");
    m.cpu.step(); require(!(m.cpu.state.cp0[13]&0x400), "SI acknowledge did not clear live IP2"); m.save(after);
    require(m.restore(pending), "pending interrupt restore failed");
    require(m.write(si+24,4,0) && m.write(m.cpu.state.pc,4,0), "SI replay setup failed"); m.cpu.step(); m.save(replay);
    require(after==replay, "pending SI interrupt replay diverged");
    // A persistent level re-enters after ERET; acknowledgement permits normal execution.
    m.reset(); require(m.write(start,4,i(9,0,3,123)) && m.write(0xffffffff80000180ull,4,0x42000018), "interrupt return code failed");
    m.cpu.reset(start); require(m.write(si,4,0x10000) && m.write(si+4,4,0) && m.write(mi+12,4,8), "persistent SI setup failed");
    m.cpu.state.cp0[12]=0x401;
    m.cpu.step(); m.cpu.step(); require(m.cpu.state.pc==start && m.cpu.state.cp0[12]==0x401, "interrupt ERET did not return");
    m.cpu.step(); require(m.cpu.state.pc==0xffffffff80000180ull && m.cpu.state.gpr[3]==0, "persistent SI did not re-enter");
    require(m.write(si+24,4,0), "persistent SI acknowledge failed");
    m.cpu.step(); m.cpu.step(); require(m.cpu.state.gpr[3]==123 && !(m.cpu.state.cp0[13]&0x400), "acknowledged SI did not resume guest");
}
static void serial(n64::Machine& m) {
    constexpr uint64_t si = 0xffffffffa4800000ull, mi = 0xffffffffa4300000ull;
    constexpr uint64_t pif = 0xffffffffbfc007c0ull;
    m.reset();
    auto read = [&](uint64_t a, unsigned n = 4) { uint64_t v = 0; require(m.read(a,n,v), "serial read failed"); return v; };
    auto write = [&](uint64_t a, uint64_t v, unsigned n = 4) { require(m.write(a,n,v), "serial write failed"); };
    require(read(si+24)==0 && read(mi+8)==0 && read(mi+12)==0, "serial reset registers wrong");
    for (unsigned bit = 0; bit < 6; ++bit) {
        write(mi+12, 2u << (bit*2)); require(read(mi+12)==(1u << bit), "MI set mask wrong");
        write(mi+12, 1u << (bit*2)); require(read(mi+12)==0, "MI clear mask wrong");
    }
    // Identify and reset commands, absent channels, padding/skips, and invalid lengths.
    for (unsigned command : {0u,255u}) {
        m.pif.fill(0xaa); m.pif[0]=1; m.pif[1]=3; m.pif[2]=uint8_t(command); m.pif[6]=0xfe;
        write(pif+63,1,1); write(si+4,0);
        require(read(pif,8)==0x010300050002feaaull + (uint64_t(command) << 40), "controller identity wrong");
        require(read(pif+63,1)==0, "PIF control not cleared");
    }
    m.pif.fill(0xaa); m.pif[0]=0xff; m.pif[1]=m.pif[2]=m.pif[3]=m.pif[4]=0; m.pif[5]=1; m.pif[6]=4; m.pif[7]=1; m.pif[12]=0xfe;
    write(pif+63,1,1); write(si+4,0); require(m.pif[6]==0x84 && m.pif[8]==0xaa, "absent channel overwritten/responded");
    for (unsigned tx = 1; tx <= 63; ++tx) for (unsigned rx = 0; rx <= 63; ++rx) {
        m.pif.fill(0xaa); m.pif[0]=uint8_t(tx); m.pif[1]=uint8_t(rx); m.pif[2]=1;
        if (2+tx+rx < 63) m.pif[2+tx+rx]=0xfe;
        const auto original = m.pif;
        write(pif+63,1,1); write(si+4,0);
        if (tx != 1 || rx != 4) {
            require(m.pif[1]==(rx|0x40), "malformed controller length not rejected");
            for (unsigned k=2; k<63; ++k) require(m.pif[k]==original[k], "malformed packet changed payload");
        }
        require(m.pif[63]==0, "malformed packet overflowed control");
    }
    // DMA snapshots are bounded and failures leave memory/PIF/interrupt state unchanged.
    m.reset(); write(si,0x10000); m.ram[0x10000]=1; m.ram[0x10001]=4; m.ram[0x10002]=1;
    m.ram[0x10007]=0xfe; m.ram[0x1003f]=1;
    m.input_buttons = 1; write(si+16,0);
    require(read(si+24)==0x1000 && read(mi+8)==2 && m.pif[3]==0, "DMA format sampled input or failed completion");
    write(mi+12,8); m.yielded=true;
    std::vector<uint8_t> saved, after, replay;
    require(m.save(saved), "serial save failed");
    write(si+24,0); write(si+4,0); require(m.ram[0x10003]==0x80, "PIF read DMA wrong");
    m.save(after); require(m.restore(saved), "serial restore failed");
    require(m.si_interrupt && m.mi_mask==2 && m.yielded && m.input_buttons==1, "serial restored scalar wrong");
    write(si+24,0); write(si+4,0); m.save(replay); require(after==replay, "serial state continuation diverged");
    write(si+24,0);
    for (uint32_t address : {0x10001u,0x7fffc8u,0x800000u,0xffffffu}) {
        write(si,address); const auto before=m.pif; const auto guard=m.ram[0x10003];
        require(!m.write(si+16,4,0) && !m.write(si+4,4,0), "invalid serial DMA accepted");
        require(m.pif==before && m.ram[0x10003]==guard && !m.si_interrupt, "failed DMA mutated state");
    }
    uint64_t dummy=0;
    require(!m.write(si+16,2,0) && !m.read(si+24,8,dummy), "invalid MMIO width accepted");
    // Version and added scalar validation must reject without changing the active machine.
    m.save(saved);
    for (unsigned offset : {7u,600u,608u,623u,631u,639u}) {
        auto bad=saved; bad[offset]=0xff;
        require(!m.restore(bad), "invalid serial state accepted"); m.save(replay);
        require(saved==replay, "rejected serial state mutated machine");
    }
}
static void states(n64::Machine& m, const std::vector<uint8_t>& rom, const char* library) {
    program(m, {i(4, 0, 0, 3), i(9, 0, 3, 7)});
    m.cpu.step();
    m.cpu.state.hi = 123; m.cpu.state.lo = 456;
    m.frames = 99;
    require(m.write(0xffffffffa4000000ull, 8, 0x123456789abcdef0), "SP memory write failed");
    std::vector<uint8_t> pending, after, replay;
    require(m.save(pending), "pending state save failed");
    m.cpu.step(); m.save(after);
    require(m.restore(pending), "pending state load failed");
    m.cpu.step(); m.save(replay);
    require(after == replay && m.cpu.state.gpr[3] == 7, "pending delay/state roundtrip diverged");
    for (auto mutation : {0, 1, 2, 3}) {
        auto invalid = after;
        if (mutation == 0) invalid.pop_back();
        if (mutation == 1) invalid[0] ^= 1;
        if (mutation == 2) invalid[8] ^= 1; // normalized full ROM identity
        if (mutation == 3) invalid[32 + 7] = 2; // done is boolean
        require(!m.restore(invalid), "invalid state accepted");
        m.save(replay); require(replay == after, "rejected state mutated machine");
    }
    // Test the actual loadable ABI v2 plugin, not only its component classes.
    emulator_test::LoadedCore core(library, rom);
    core.plugin->run_frame({});
    std::vector<uint8_t> first, baseline, repeated;
    require(core.plugin->save_state(first), "plugin state save failed");
    for (unsigned n = 0; n < 3; ++n) core.plugin->run_frame({});
    core.plugin->save_state(baseline);
    require(core.plugin->load_state(first), "plugin state restore failed");
    for (unsigned n = 0; n < 3; ++n) core.plugin->run_frame({});
    core.plugin->save_state(repeated);
    require(repeated == baseline, "actual plugin continuation state diverged");
    auto bad_rom = rom; bad_rom[0x34] = 0;
    require(!core.plugin->load_rom(bad_rom.data(), bad_rom.size()), "unmarked retail-like image accepted");
    core.plugin->save_state(repeated); require(repeated == baseline, "failed cartridge load replaced active state");
    for (unsigned swap : {2u, 4u}) {
        auto bytes = rom;
        for (size_t pos = 0; pos < bytes.size(); pos += swap)
            for (unsigned j = 0; j < swap / 2; ++j) std::swap(bytes[pos + j], bytes[pos + swap - 1 - j]);
        n64::Machine other;
        require(other.load(bytes.data(), bytes.size()) && other.rom == rom, "v64/n64 byte order normalization failed");
    }
    core.plugin->unload_rom();
    require(!core.plugin->is_rom_loaded() && !core.plugin->save_state(repeated), "unload failed");
}
static void serial_plugin(const char* library, const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), {});
    emulator_test::LoadedCore core(library, rom);
    const auto* layout = core.plugin->get_controller_layout();
    require(layout && layout->button_count==14 && layout->controller_count==4, "N64 plugin controls wrong");
    using emu::VirtualButton;
    core.plugin->run_frame({emu::button_to_mask(VirtualButton::A)});
    std::vector<uint8_t> saved, baseline, replay;
    require(core.plugin->save_state(saved), "controller plugin save failed");
    n64::Machine mirror;
    require(mirror.load(rom.data(),rom.size()) && mirror.restore(saved), "controller plugin snapshot invalid");
    require(mirror.ram[0x20003]==0x80 && mirror.ram[0x20004]==0 && mirror.yielded && mirror.frames==1,
            "controller plugin input/yield did not reach guest");
    for (uint32_t input : {emu::button_to_mask(VirtualButton::Z),emu::button_to_mask(VirtualButton::CUp),0u})
        core.plugin->run_frame({input});
    core.plugin->save_state(baseline);
    require(core.plugin->load_state(saved), "controller plugin restore failed");
    for (uint32_t input : {emu::button_to_mask(VirtualButton::Z),emu::button_to_mask(VirtualButton::CUp),0u})
        core.plugin->run_frame({input});
    core.plugin->save_state(replay);
    require(baseline==replay, "controller plugin input continuation diverged");
}
int main(int argc, char** argv) {
    try {
        require(argc == 4, "usage: n64_machine_test plugin integer-ROM controller-ROM");
        std::ifstream file(argv[2], std::ios::binary);
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), {});
        n64::Machine machine;
        require(machine.load(rom.data(), rom.size()), "original N64 ROM load failed");
        control(machine); exceptions(machine); memory(machine); merge_memory(machine); masked_store_transaction(machine); multiply_divide(machine); interrupts(machine); serial(machine); states(machine, rom, argv[1]);
        serial_plugin(argv[1],argv[3]);
        std::cout << "N64 control/exception/memory/state/plugin checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
