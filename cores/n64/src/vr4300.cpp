#include "vr4300.hpp"

namespace n64 {
uint64_t sign32(uint32_t v) { return uint64_t(v) | ((v & 0x80000000u) ? 0xffffffff00000000ull : 0); }
uint64_t sign16(uint16_t v) { return uint64_t(v) | ((v & 0x8000u) ? 0xffffffffffff0000ull : 0); }
uint64_t arithmetic_right(uint64_t v, unsigned n) {
    n &= 63;
    if (!n) return v;
    return (v >> n) | ((v >> 63) ? (~uint64_t(0) << (64 - n)) : 0);
}
static bool signed_less(uint64_t a, uint64_t b) { return (a ^ (1ull << 63)) < (b ^ (1ull << 63)); }
void VR4300::reset(uint64_t entry) {
    state = {};
    state.pc = entry;
    state.next_pc = entry + 4;
    // Explicit homebrew entry in kernel mode with interrupts off. Not a PIF/IPL reset.
    state.cp0[12] = 0;
    state.cp0[15] = 0x00000b22;
}
void VR4300::exception(unsigned code, uint64_t pc, bool delay, uint64_t bad) {
    auto& s = state;
    if (!(s.cp0[12] & 2)) {
        s.cp0[14] = delay ? pc - 4 : pc;
        s.cp0[13] = (s.cp0[13] & ~0x80000000ull) | (delay ? 0x80000000ull : 0);
    }
    s.cp0[13] = (s.cp0[13] & ~0x7cull) | (code << 2);
    if (code == 4 || code == 5) s.cp0[8] = bad;
    s.cp0[12] |= 2;
    s.pc = (s.cp0[12] & (1u << 22)) ? 0xffffffffbfc00380ull : 0xffffffff80000180ull;
    s.next_pc = s.pc + 4;
    s.delay_slot = false;
}
void VR4300::step() {
    auto& s = state;
    const uint64_t pc = s.pc;
    const bool delay = s.delay_slot;
    ++s.instructions;
    // Count is an instruction-clock approximation; pipeline/bus timing is pending.
    if (!(s.instructions & 1)) s.cp0[9] = uint32_t(s.cp0[9] + 1);
    uint64_t fetched = 0;
    if ((pc & 3) || !memory.read(pc, 4, fetched)) { exception(4, pc, delay, pc); return; }
    const uint32_t ins = uint32_t(fetched);
    const unsigned op = ins >> 26, rs = (ins >> 21) & 31, rt = (ins >> 16) & 31;
    const unsigned rd = (ins >> 11) & 31, sa = (ins >> 6) & 31, fn = ins & 63;
    const uint64_t a = s.gpr[rs], b = s.gpr[rt], imm = sign16(uint16_t(ins));
    s.pc = s.next_pc;
    s.next_pc += 4;
    s.delay_slot = false;
    auto put = [&](unsigned reg, uint64_t value) { if (reg) s.gpr[reg] = value; };
    auto fault = [&](unsigned code, uint64_t bad = 0) { exception(code, pc, delay, bad); };
    auto branch = [&](bool take, bool likely = false) {
        if (likely && !take) { s.pc = s.next_pc; s.next_pc += 4; }
        else { s.delay_slot = true; if (take) s.next_pc = pc + 4 + imm * 4; }
    };
    auto jump = [&](uint64_t target) { s.delay_slot = true; s.next_pc = target; };
    auto add = [&](unsigned reg, uint64_t x, uint64_t y, bool wide, bool subtract, bool trap) {
        const uint64_t sign = wide ? 1ull << 63 : 1ull << 31;
        const uint64_t result = subtract ? x - y : x + y;
        const bool overflow = subtract ? (((x ^ y) & (x ^ result) & sign) != 0)
                                       : (((~(x ^ y)) & (x ^ result) & sign) != 0);
        if (trap && overflow) fault(12);
        else put(reg, wide ? result : sign32(uint32_t(result)));
    };
    auto load = [&](unsigned bytes, bool signed_value) {
        const uint64_t address = a + imm;
        uint64_t value = 0;
        if ((address & (bytes - 1)) || !memory.read(address, bytes, value)) { fault(4, address); return; }
        if (signed_value) {
            if (bytes == 1) value |= (value & 128) ? 0xffffffffffffff00ull : 0;
            else if (bytes == 2) value = sign16(uint16_t(value));
            else if (bytes == 4) value = sign32(uint32_t(value));
        }
        put(rt, value);
    };
    auto store = [&](unsigned bytes) {
        const uint64_t address = a + imm;
        if ((address & (bytes - 1)) || !memory.write(address, bytes, b)) fault(5, address);
    };
    if (op == 0) {
        switch (fn) {
        case 0x00: put(rd, sign32(uint32_t(b) << sa)); break;
        case 0x02: put(rd, sign32(uint32_t(b) >> sa)); break;
        case 0x03: put(rd, sign32(uint32_t(arithmetic_right(sign32(uint32_t(b)), sa)))); break;
        case 0x04: put(rd, sign32(uint32_t(b) << (a & 31))); break;
        case 0x06: put(rd, sign32(uint32_t(b) >> (a & 31))); break;
        case 0x07: put(rd, sign32(uint32_t(arithmetic_right(sign32(uint32_t(b)), a & 31)))); break;
        case 0x08: jump(a); break;
        case 0x09: put(rd, pc + 8); jump(a); break;
        case 0x0c: fault(8); break;
        case 0x0d: fault(9); break;
        case 0x0f: break; // SYNC: uncached interpreter has no pending transactions.
        case 0x10: put(rd, s.hi); break;
        case 0x11: s.hi = a; break;
        case 0x12: put(rd, s.lo); break;
        case 0x13: s.lo = a; break;
        case 0x14: put(rd, b << (a & 63)); break;
        case 0x16: put(rd, b >> (a & 63)); break;
        case 0x17: put(rd, arithmetic_right(b, a & 63)); break;
        case 0x18: {
            // Signed 32-bit multiplication in a safe 64-bit host domain.
            const int64_t x = (a & 0x80000000u) ? int64_t(uint32_t(a)) - 0x100000000ll : int64_t(uint32_t(a));
            const int64_t y = (b & 0x80000000u) ? int64_t(uint32_t(b)) - 0x100000000ll : int64_t(uint32_t(b));
            const uint64_t product = uint64_t(x * y);
            s.lo = sign32(uint32_t(product)); s.hi = sign32(uint32_t(product >> 32)); break;
        }
        case 0x19: {
            const uint64_t product = uint64_t(uint32_t(a)) * uint32_t(b);
            s.lo = sign32(uint32_t(product)); s.hi = sign32(uint32_t(product >> 32)); break;
        }
        case 0x1a: {
            const int64_t x = (a & 0x80000000u) ? int64_t(uint32_t(a)) - 0x100000000ll : int64_t(uint32_t(a));
            const int64_t y = (b & 0x80000000u) ? int64_t(uint32_t(b)) - 0x100000000ll : int64_t(uint32_t(b));
            s.lo = sign32(uint32_t(y ? x / y : x < 0 ? 1 : -1));
            s.hi = sign32(uint32_t(y ? x % y : x)); break;
        }
        case 0x1b:
            s.lo = sign32(uint32_t(b) ? uint32_t(a) / uint32_t(b) : 0xffffffffu);
            s.hi = sign32(uint32_t(b) ? uint32_t(a) % uint32_t(b) : uint32_t(a)); break;
        case 0x20: add(rd, a, b, false, false, true); break;
        case 0x21: add(rd, a, b, false, false, false); break;
        case 0x22: add(rd, a, b, false, true, true); break;
        case 0x23: add(rd, a, b, false, true, false); break;
        case 0x24: put(rd, a & b); break;
        case 0x25: put(rd, a | b); break;
        case 0x26: put(rd, a ^ b); break;
        case 0x27: put(rd, ~(a | b)); break;
        case 0x2a: put(rd, signed_less(a, b)); break;
        case 0x2b: put(rd, a < b); break;
        case 0x2c: add(rd, a, b, true, false, true); break;
        case 0x2d: add(rd, a, b, true, false, false); break;
        case 0x2e: add(rd, a, b, true, true, true); break;
        case 0x2f: add(rd, a, b, true, true, false); break;
        case 0x38: put(rd, b << sa); break;
        case 0x3a: put(rd, b >> sa); break;
        case 0x3b: put(rd, arithmetic_right(b, sa)); break;
        case 0x3c: put(rd, b << (sa + 32)); break;
        case 0x3e: put(rd, b >> (sa + 32)); break;
        case 0x3f: put(rd, arithmetic_right(b, sa + 32)); break;
        default: fault(10); break;
        }
    } else {
        switch (op) {
        case 1:
            if (rt == 0 || rt == 1 || rt == 2 || rt == 3 || rt == 16 || rt == 17 || rt == 18 || rt == 19) {
                const bool negative = a >> 63;
                if (rt & 16) put(31, pc + 8);
                branch((rt & 1) ? !negative : negative, rt & 2);
            } else fault(10);
            break;
        case 2: jump(((pc + 4) & ~0x0fffffffull) | (uint64_t(ins & 0x03ffffff) << 2)); break;
        case 3: put(31, pc + 8); jump(((pc + 4) & ~0x0fffffffull) | (uint64_t(ins & 0x03ffffff) << 2)); break;
        case 4: case 20: branch(a == b, op == 20); break;
        case 5: case 21: branch(a != b, op == 21); break;
        case 6: case 22: branch(a == 0 || (a >> 63), op == 22); break;
        case 7: case 23: branch(a != 0 && !(a >> 63), op == 23); break;
        case 8: add(rt, a, imm, false, false, true); break;
        case 9: add(rt, a, imm, false, false, false); break;
        case 10: put(rt, signed_less(a, imm)); break;
        case 11: put(rt, a < imm); break;
        case 12: put(rt, a & uint16_t(ins)); break;
        case 13: put(rt, a | uint16_t(ins)); break;
        case 14: put(rt, a ^ uint16_t(ins)); break;
        case 15: put(rt, sign32(uint32_t(ins & 0xffff) << 16)); break;
        case 16:
            if (rs == 0) put(rt, sign32(uint32_t(s.cp0[rd])));
            else if (rs == 4) s.cp0[rd] = uint32_t(b);
            else if (ins == 0x42000018) {
                // ERET has no delay slot. Error-level reset handling is pending.
                s.cp0[12] &= ~2ull; s.pc = s.cp0[14]; s.next_pc = s.pc + 4; s.delay_slot = false;
            } else fault(10);
            break;
        case 24: add(rt, a, imm, true, false, true); break;
        case 25: add(rt, a, imm, true, false, false); break;
        case 32: load(1, true); break;
        case 33: load(2, true); break;
        case 35: load(4, true); break;
        case 36: load(1, false); break;
        case 37: load(2, false); break;
        case 39: load(4, false); break;
        case 40: store(1); break;
        case 41: store(2); break;
        case 43: store(4); break;
        case 55: load(8, false); break;
        case 63: store(8); break;
        default: fault(10); break;
        }
    }
    s.gpr[0] = 0;
}
}
