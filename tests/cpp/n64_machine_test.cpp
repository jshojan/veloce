#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
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
    require(m.rom == rom && (m.cpu.state.cp0[13] & 0x7c) == 20 && m.cpu.state.cp0[8] == 0xffffffffb0000003ull,
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
int main(int argc, char** argv) {
    try {
        require(argc == 3, "usage: n64_machine_test plugin ROM");
        std::ifstream file(argv[2], std::ios::binary);
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), {});
        n64::Machine machine;
        require(machine.load(rom.data(), rom.size()), "original N64 ROM load failed");
        control(machine); exceptions(machine); memory(machine); merge_memory(machine); masked_store_transaction(machine); multiply_divide(machine); states(machine, rom, argv[1]);
        std::cout << "N64 control/exception/memory/state/plugin checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
