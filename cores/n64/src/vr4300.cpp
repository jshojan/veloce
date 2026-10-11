#include "vr4300.hpp"
#include "fpu_conversions.hpp"
#include "fpu_arithmetic.hpp"
#include "fpu_comparisons.hpp"

namespace n64 {
uint64_t sign32(uint32_t v) { return uint64_t(v) | ((v & 0x80000000u) ? 0xffffffff00000000ull : 0); }
uint64_t sign16(uint16_t v) { return uint64_t(v) | ((v & 0x8000u) ? 0xffffffffffff0000ull : 0); }
uint64_t arithmetic_right(uint64_t v, unsigned n) {
    n &= 63;
    if (!n) return v;
    return (v >> n) | ((v >> 63) ? (~uint64_t(0) << (64 - n)) : 0);
}
static bool signed_less(uint64_t a, uint64_t b) { return (a ^ (1ull << 63)) < (b ^ (1ull << 63)); }
static void multiply_doubleword(uint64_t a, uint64_t b, uint64_t& lo, uint64_t& hi) {
    // Four 32-bit products, with carries collected before composing each half.
    // This needs neither a host signed multiply nor a compiler-specific 128-bit type.
    const uint64_t a0 = uint32_t(a), a1 = a >> 32, b0 = uint32_t(b), b1 = b >> 32;
    const uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    const uint64_t middle = (p00 >> 32) + uint32_t(p01) + uint32_t(p10);
    lo = (middle << 32) | uint32_t(p00);
    hi = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
}
void VR4300::reset(uint64_t entry) {
    state = {};caches.reset();tlb.reset();exception_taken=instruction_valid=false;instruction_word=0;access_address=0;access_bytes=0;access_store=false;
    state.pc = entry;
    state.next_pc = entry + 4;
    // Explicit homebrew entry in kernel mode with interrupts off. Not a PIF/IPL reset.
    state.cp0[12] = 0;
    state.cp0[16] = 0x0006e462; // Explicit big-endian uncached homebrew profile; EC=0.
    state.cp0[15] = 0x00000b22;
    state.cp0[1] = 31;
}
void VR4300::exception(unsigned code, uint64_t pc, bool delay, uint64_t bad,bool refill,bool extended) {
    exception_taken=true;
    auto& s = state;
    if (!(s.cp0[12] & 2)) {
        s.cp0[14] = delay ? pc - 4 : pc;
        s.cp0[13] = (s.cp0[13] & ~0x80000000ull) | (delay ? 0x80000000ull : 0);
    }
    s.cp0[13] = (s.cp0[13] & ~0x7cull) | (code << 2);
    if(code==15)s.cp0[13]&=~0x30000000ull; // FPE hardware observations clear stale coprocessor error.
    if (code>=1 && code<=5) s.cp0[8] = bad;
    const bool nested=s.cp0[12]&2;
    if(code>=1 && code<=3) {
        s.cp0[10]=(bad&Tlb::hi_mask&~uint64_t(255))|(s.cp0[10]&255);
        s.cp0[4]=(s.cp0[4]&~uint64_t(0x7fffff))|((bad>>9)&0x7ffff0);
        s.cp0[20]=(s.cp0[20]&0xfffffffe00000000ull)|((bad>>31)&0x180000000ull)|((bad>>9)&0x7ffffff0);
    }
    s.cp0[12] |= 2;
    const uint64_t base=(s.cp0[12]&(1u<<22))?0xffffffffbfc00200ull:0xffffffff80000000ull;
    s.pc=base+((refill && !nested)?(extended?128:0):384);
    s.next_pc = s.pc + 4;
    s.delay_slot = false;
}
bool VR4300::translate(uint64_t address,bool store,uint64_t pc,bool delay,Tlb::Translation& t) {
    t=tlb.translate(address,uint32_t(state.cp0[12]),uint8_t(state.cp0[10]),uint32_t(state.cp0[16]),store);
    if(t.fault==Tlb::Fault::none)return true;
    unsigned code=store?3:2;
    if(t.fault==Tlb::Fault::address)code=store?5:4;
    else if(t.fault==Tlb::Fault::modified)code=1;
    else if(t.fault==Tlb::Fault::undefined)code=10; // Duplicate matches are architecturally undefined.
    exception(code,pc,delay,address,t.fault==Tlb::Fault::miss,t.extended);return false;
}
bool VR4300::read(uint64_t address,unsigned bytes,uint64_t& value,bool fetch,uint64_t pc,bool delay,uint64_t bad) {
    access_address=bad;access_bytes=bytes;access_store=false;
    if(address&(bytes-1)) {exception(4,pc,delay,bad);return false;}
    Tlb::Translation t;if(!translate(bad,false,pc,delay,t))return false;
    if(!fetch && !watch(t.physical,false,pc,delay))return false;
    if(!fetch && t.cached && !caches.data_hit(address,t.physical))state.llbit=false;
    if(!caches.read_translated(memory,address,t.physical&~uint32_t(bytes-1),t.cached,bytes,value,fetch)) {
        exception(fetch?6:7,pc,delay);return false;
    }
    return true;
}
bool VR4300::write(uint64_t address,unsigned bytes,uint64_t value,uint8_t mask,uint64_t pc,bool delay,uint64_t bad) {
    access_address=bad;access_bytes=bytes;access_store=true;
    if(address&(bytes-1)) {exception(5,pc,delay,bad);return false;}
    Tlb::Translation t;if(!translate(bad,true,pc,delay,t))return false;
    if(!watch(t.physical,true,pc,delay))return false;
    if(t.cached && !caches.data_hit(address,t.physical))state.llbit=false;
    if(!caches.write_translated(memory,address,t.physical&~uint32_t(bytes-1),t.cached,bytes,value,mask)) {
        exception(7,pc,delay);return false;
    }
    return true;
}
bool VR4300::watch(uint32_t physical,bool store,uint64_t pc,bool delay) {
    const auto& s=state;
    // Watch selects a physical eight-byte granule. EXL masks the comparison;
    // instruction fetches and CACHE operations do not call this data check.
    // Nonzero WatchHi describes an unsupported >32-bit physical address.
    if(!(s.cp0[12]&2) && (s.cp0[18]&(store?1:2)) && !(s.cp0[19]&15) &&
       (uint32_t(s.cp0[18])&~7u)==(physical&~7u)) {exception(23,pc,delay);return false;}
    return true;
}
void VR4300::step() {
    exception_taken=instruction_valid=false;instruction_word=0;access_address=0;access_bytes=0;access_store=false;
    auto& s = state;
    const uint64_t pc = s.pc;
    const bool delay = s.delay_slot;
    ++s.instructions;memory.clock();
    const unsigned wired=unsigned(s.cp0[6])&31,random=unsigned(s.cp0[1])&31;
    s.cp0[1]=(s.cp0[1]&32)|(random<=wired?31:random-1);
    // Count is an instruction-clock approximation; pipeline/bus timing is pending.
    if (!(s.instructions & 1)) {
        s.cp0[9] = uint32_t(s.cp0[9] + 1);
        if (s.cp0[9] == s.cp0[11]) s.cp0[13] |= 0x8000;
    }
    // Device levels are sampled at instruction boundaries. IP7 is the latched timer;
    // IP0/IP1 are software requests. Pipeline interrupt latency is not modeled.
    s.cp0[13] = (s.cp0[13] & ~0x7c00ull) | (uint64_t(memory.interrupt_pending() & 0x7c) << 8);
    if ((s.cp0[12] & 7) == 1 && (s.cp0[12] & s.cp0[13] & 0xff00)) {
        exception(0, pc, delay); return;
    }
    uint64_t fetched = 0;
    if(!read(pc,4,fetched,true,pc,delay,pc))return;
    const uint32_t ins = uint32_t(fetched);instruction_word=ins;instruction_valid=true;
    const unsigned op = ins >> 26, rs = (ins >> 21) & 31, rt = (ins >> 16) & 31;
    const unsigned rd = (ins >> 11) & 31, sa = (ins >> 6) & 31, fn = ins & 63;
    const uint64_t a = s.gpr[rs], b = s.gpr[rt], imm = sign16(uint16_t(ins));
    s.pc = s.next_pc;
    s.next_pc += 4;
    s.delay_slot = false;
    auto put = [&](unsigned reg, uint64_t value) { if (reg) s.gpr[reg] = value; };
    auto fault = [&](unsigned code, uint64_t bad = 0) { exception(code, pc, delay, bad); };
    auto linked_memory = [&](bool wide,bool conditional) {
        const unsigned mode=(s.cp0[12]&6)?0:unsigned(s.cp0[12]>>3)&3;
        if(wide && mode && !(s.cp0[12]&(mode==1?64:32))) {fault(10);return;}
        const unsigned bytes=wide?8:4;
        // In 32-bit mode the manual ignores the high half when forming VA.
        const bool extended=s.cp0[12]&(mode==0?128:mode==1?64:32);
        const auto address=extended?a+imm:sign32(uint32_t(a+imm));
        access_address=address;access_bytes=bytes;access_store=conditional;
        if(address&(bytes-1)) {fault(conditional?5:4,address);return;}
        Tlb::Translation t;if(!translate(address,conditional,pc,delay,t))return;
        if(!watch(t.physical,conditional,pc,delay))return;
        if(conditional) {
            // Conservative functional policy for a miss between LL and SC.
            // Translation/alignment exceptions still precede LLbit failure.
            if(t.cached && !caches.data_hit(address,t.physical))s.llbit=false;
            if(s.llbit && !caches.write_translated(memory,address,t.physical,t.cached,bytes,b,uint8_t((1u<<bytes)-1))) {fault(7);return;}
            put(rt,s.llbit?1:0);
        } else {
            uint64_t value=0;
            if(t.cached && !caches.data_hit(address,t.physical))s.llbit=false;
            if(!caches.read_translated(memory,address,t.physical,t.cached,bytes,value,false)) {fault(7);return;}
            put(rt,wide?value:sign32(uint32_t(value)));
            s.cp0[17]=(t.physical>>4)&0x0fffffff;s.llbit=true;
        }
    };
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
    auto cop1_usable = [&]() {
        if(s.cp0[12]&0x20000000)return true;
        s.cp0[13]=(s.cp0[13]&~0x30000000ull)|0x10000000;fault(11);return false;
    };
    auto fr = [&]() {return bool(s.cp0[12]&0x4000000);};
    auto fp_word = [&](unsigned reg) {
        return uint32_t(s.fpr[fr()?reg:reg&~1u]>>(!fr() && (reg&1)?32:0));
    };
    auto fp_put_word = [&](unsigned reg,uint32_t value) {
        const unsigned index=fr()?reg:reg&~1u,shift=!fr() && (reg&1)?32:0;
        // FR=1 upper bits are architecturally undefined; preservation is our convention.
        s.fpr[index]=(s.fpr[index]&~(uint64_t(0xffffffff)<<shift))|(uint64_t(value)<<shift);
    };
    auto fp_double_valid = [&](unsigned reg) {
        if(fr() || !(reg&1))return true;
        fault(10);return false; // Odd FR=0 doubleword forms are undefined; deterministic RI.
    };
    auto fp_memory = [&](unsigned bytes,bool store_value) {
        if(!cop1_usable() || (bytes==8 && !fp_double_valid(rt)))return;
        const auto address=a+imm;
        if(store_value)write(address,bytes,bytes==8?s.fpr[rt]:fp_word(rt),uint8_t((1u<<bytes)-1),pc,delay,address);
        else {uint64_t value=0;if(!read(address,bytes,value,false,pc,delay,address))return;
            if(bytes==8)s.fpr[rt]=value;else fp_put_word(rt,uint32_t(value));}
    };
    // These wide integer memory forms explicitly ignore VA63:32 in Kernel32.
    // Keep the full address in extended modes and for ordinary word/FP accesses.
    const unsigned mode=(s.cp0[12]&6)?0:unsigned(s.cp0[12]>>3)&3;
    const bool wide_address=op==26 || op==27 || op==39 || op==44 || op==45 || op==55 || op==63;
    const uint64_t integer_address=wide_address && !mode && !(s.cp0[12]&128)
        ? sign32(uint32_t(a+imm)) : a+imm;
    auto load = [&](unsigned bytes, bool signed_value) {
        const uint64_t address = integer_address;
        uint64_t value = 0;
        if(!read(address,bytes,value,false,pc,delay,address))return;
        if (signed_value) {
            if (bytes == 1) value |= (value & 128) ? 0xffffffffffffff00ull : 0;
            else if (bytes == 2) value = sign16(uint16_t(value));
            else if (bytes == 4) value = sign32(uint32_t(value));
        }
        put(rt, value);
    };
    auto store = [&](unsigned bytes) {
        const uint64_t address = integer_address;
        write(address,bytes,b,uint8_t((1u<<bytes)-1),pc,delay,address);
    };
    auto merge_load = [&](unsigned bytes, bool left) {
        const uint64_t address = integer_address, aligned = address & ~uint64_t(bytes - 1);
        const unsigned lane = address & (bytes - 1);
        uint64_t value = 0;
        if(!read(aligned,bytes,value,false,pc,delay,address))return;
        const uint64_t width_mask = bytes == 8 ? ~uint64_t(0) : 0xffffffffull;
        uint64_t result;
        if (left) {
            const unsigned shift = lane * 8;
            const uint64_t preserve = (uint64_t(1) << shift) - 1;
            result = (value << shift) | (b & preserve);
        } else {
            const unsigned shift = (bytes - 1 - lane) * 8;
            const uint64_t replace = width_mask >> shift;
            result = (value >> shift) | (b & (width_mask ^ replace));
        }
        put(rt, bytes == 4 ? sign32(uint32_t(result)) : result);
    };
    auto merge_store = [&](unsigned bytes, bool left) {
        const uint64_t address = integer_address, aligned = address & ~uint64_t(bytes - 1);
        const unsigned lane = address & (bytes - 1);
        const unsigned all_lanes = (1u << bytes) - 1;
        const uint8_t mask = left ? uint8_t((all_lanes << lane) & all_lanes)
                                  : uint8_t((1u << (lane + 1)) - 1);
        const uint64_t value = left ? b >> (lane * 8) : b << ((bytes - 1 - lane) * 8);
        write(aligned,bytes,value,mask,pc,delay,address);
    };
    // Doubleword integer instructions and LWU require 64-bit User/Supervisor mode;
    // Kernel32 remains legal. Decode this before arithmetic or data access.
    const bool wide_integer=op==0 ?
        (fn==20 || fn==22 || fn==23 || (fn>=28 && fn<=31) ||
         (fn>=44 && fn<=47) || fn==56 || fn==58 || fn==59 || fn==60 || fn==62 || fn==63) :
        (op==24 || op==25 || op==26 || op==27 || op==39 || op==44 || op==45 || op==52 || op==55 || op==60 || op==63);
    if(wide_integer && mode && !(s.cp0[12]&(mode==1?64:32))) {fault(10);return;}
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
        case 0x0f: break; // SYNC: settled cache operations have no outstanding bus queue.
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
        case 0x1c: case 0x1d:
            multiply_doubleword(a, b, s.lo, s.hi);
            if (fn == 0x1c) {
                // Signed 128-bit product from unsigned limbs. The corrections
                // affect only the high half; unsigned subtraction wraps safely.
                if (a >> 63) s.hi -= b;
                if (b >> 63) s.hi -= a;
            }
            break;
        case 0x1e: {
            const bool negative_a = a >> 63, negative_b = b >> 63;
            const uint64_t x = negative_a ? 0ull - a : a;
            const uint64_t y = negative_b ? 0ull - b : b;
            if (y) {
                const uint64_t quotient = x / y, remainder = x % y;
                s.lo = negative_a != negative_b ? 0ull - quotient : quotient;
                s.hi = negative_a ? 0ull - remainder : remainder;
            } else {
                // Deterministic convention for architecturally undefined /0.
                s.lo = negative_a ? 1 : ~uint64_t(0); s.hi = a;
            }
            break;
        }
        case 0x1f:
            s.lo = b ? a / b : ~uint64_t(0);
            s.hi = b ? a % b : a;
            break;
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
        case 0x30: if(!signed_less(a,b))fault(13); break;
        case 0x31: if(a>=b)fault(13); break;
        case 0x32: if(signed_less(a,b))fault(13); break;
        case 0x33: if(a<b)fault(13); break;
        case 0x34: if(a==b)fault(13); break;
        case 0x36: if(a!=b)fault(13); break;
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
            } else switch(rt){
                // Even unsigned trap immediates are sign-extended to the GPR
                // width before comparison. They never write the encoded rt.
                case 8: if(!signed_less(a,imm))fault(13); break;
                case 9: if(a>=imm)fault(13); break;
                case 10: if(signed_less(a,imm))fault(13); break;
                case 11: if(a<imm)fault(13); break;
                case 12: if(a==imm)fault(13); break;
                case 14: if(a!=imm)fault(13); break;
                default: fault(10); break;
            }
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
        case 16: {
            const auto status=s.cp0[12];
            if(!(status&6) && (status&24) && !(status&0x10000000)) {s.cp0[13]&=~0x30000000ull;fault(11);break;}
            const unsigned mode=(status&6)?0:unsigned(status>>3)&3;
            if((rs==1 || rs==5) && mode!=0 && !(status&(mode==1?64:32))) {fault(10);break;}
            if((rs==0 || rs==1) && !(ins&0x7ff))put(rt,rs==0?sign32(uint32_t(s.cp0[rd])):s.cp0[rd]);
            else if((rs==4 || rs==5) && !(ins&0x7ff)) {
                const uint64_t value=rs==4?sign32(uint32_t(b)):b;
                if(rd==8 || rd==15 || rd==27) { /* BadVAddr, PRId and CacheErr are read-only. */ }
                else if(rd==0)s.cp0[0]=value&0x8000003f;
                else if(rd==1)s.cp0[1]=(s.cp0[1]&31)|(value&32);
                else if(rd==2 || rd==3)s.cp0[rd]=value&Tlb::lo_mask;
                else if(rd==4)s.cp0[4]=(s.cp0[4]&0x7ffff0)|(value&~uint64_t(0x7fffff));
                else if(rd==5)s.cp0[5]=value&Tlb::mask_mask;
                else if(rd==6) {s.cp0[6]=value&63;s.cp0[1]=31;}
                else if(rd==10)s.cp0[10]=value&Tlb::hi_mask;
                else if(rd==20)s.cp0[20]=(s.cp0[20]&0x1fffffff0ull)|(value&0xfffffffe00000000ull);
                else if(rd==12) {
                    // NEC Figures 6-5/6-6: TS is read-only; RFU bits 23/19 read zero.
                    s.cp0[12]=(s.cp0[12]&0x00200000ull)|(value&0xff57ffffull);
                }
                else if(rd==13)s.cp0[13]=(s.cp0[13]&~0x300ull)|(value&0x300);
                else if(rd==11) {s.cp0[11]=uint32_t(value);s.cp0[13]&=~0x8000ull;}
                else if(rd==16) {
                    // NEC Figure 5-16: preserve strapped EC; fixed fields read 06/646.
                    // EP/BE/CU/K0 are writable. Physical EP/BE initialization timing is pending.
                    s.cp0[16]=(s.cp0[16]&0x70000000ull)|0x00066460ull|(value&0x0f00800full);
                }
                else if(rd==17)s.cp0[17]=value&0x0fffffff; // Diagnostic PA31:4, no LLbit side effect.
                else if(rd==18)s.cp0[18]=value&0xfffffffb; // PA31:3, R and W; bit 2 is reserved.
                else if(rd==19)s.cp0[19]=value&15; // Readable compatibility PA35:32 field.
                else s.cp0[rd]=(rd==14 || rd==30)?value:uint32_t(value);
            } else if(ins==0x42000001) {
                const auto& e=tlb.entries[unsigned(s.cp0[0])&31];
                s.cp0[10]=e.hi;s.cp0[5]=e.mask;s.cp0[2]=e.even;s.cp0[3]=e.odd;
            } else if(ins==0x42000002 || ins==0x42000006) {
                if(!tlb.write(unsigned(s.cp0[ins==0x42000002?0:1]),s.cp0[10],uint32_t(s.cp0[5]),uint32_t(s.cp0[2]),uint32_t(s.cp0[3])))fault(10);
            } else if(ins==0x42000008) {
                const int index=tlb.probe(s.cp0[10]);
                s.cp0[0]=index<0?0x80000000u:uint32_t(index);
            } else if(ins==0x42000018) {
                // ERET has no delay slot; error level takes priority over EXL.
                if(s.cp0[12]&4) {s.cp0[12]&=~4ull;s.pc=s.cp0[30];}
                else {s.cp0[12]&=~2ull;s.pc=s.cp0[14];}
                s.next_pc=s.pc+4;s.delay_slot=false;
                s.llbit=false;
            } else fault(10);
            break;
        }
        case 17:
            if(!cop1_usable())break;
            if((rs==0 || rs==1 || rs==4 || rs==5) && !(ins&0x7ff)) {
                const unsigned mode=(s.cp0[12]&6)?0:unsigned(s.cp0[12]>>3)&3;
                if((rs==1 || rs==5) && mode && !(s.cp0[12]&(mode==1?64:32))) {fault(10);break;}
                if((rs==1 || rs==5) && !fp_double_valid(rd))break;
                if(rs==0)put(rt,sign32(fp_word(rd)));
                else if(rs==1)put(rt,s.fpr[rd]);
                else if(rs==4)fp_put_word(rd,uint32_t(b));
                else s.fpr[rd]=b;
            } else if (rs == 2 && !(ins & 0x7ff) && (rd == 0 || rd == 31)) {
                // FCR0 is read-only. Revision zero is the selected VR4300 profile.
                put(rt, sign32(rd == 0 ? 0x00000b00 : s.fcr31));
            } else if (rs == 6 && !(ins & 0x7ff) && (rd == 0 || rd == 31)) {
                if (rd == 31) {
                    s.fcr31 = uint32_t(b) & 0x0183ffff;
                    // CTC1 commits the control word before raising an enabled cause.
                    if ((s.fcr31 & 0x20000) || ((s.fcr31 >> 5) & s.fcr31 & 0xf80)) fault(15);
                }
            } else if (rs == 8 && rt < 4) {
                branch(bool(s.fcr31 & 0x800000) == bool(rt & 1), rt & 2);
            } else if(rs>=16 && fn<=7) {
                if(!fr() && ((rd|sa|(fn<4?rt:0))&1)) {fault(10);break;}
                const uint64_t left=rs==16?fp_word(rd):s.fpr[rd],right=rs==16?fp_word(rt):s.fpr[rt];
                const auto result=fn>=4 && rt?FpuArithmetic::Result{0,32}:FpuArithmetic::calculate(rs,fn,left,right,s.fcr31&3,s.fcr31&0x1000000,(s.fcr31>>7)&31);
                if(fn!=6 || result.causes) {
                    s.fcr31=(s.fcr31&~0x3f000u)|(result.causes<<12);
                    if((result.causes&32) || (result.causes&((s.fcr31>>7)&31))) {fault(15);break;}
                    s.fcr31|=(result.causes&31)<<2;
                }
                if(rs==16)fp_put_word(sa,uint32_t(result.bits));else s.fpr[sa]=result.bits;
            } else if(rs>=16 && fn>=48) {
                if(!fr() && ((rd|rt)&1)) {fault(10);break;} // Undefined odd FR=0 source convention.
                const auto result=sa?FpuComparisons::Result{false,32}:FpuComparisons::calculate(rs,fn&15,rs==16?fp_word(rd):s.fpr[rd],rs==16?fp_word(rt):s.fpr[rt]);
                s.fcr31=(s.fcr31&~0x3f000u)|(result.causes<<12);
                if((result.causes&32) || (result.causes&((s.fcr31>>7)&31))) {fault(15);break;}
                s.fcr31=(s.fcr31&~0x800000u)|(unsigned(result.condition)<<23)|(result.causes<<2);
            } else if(rs>=16 && ((fn>=8 && fn<=15) || fn==32 || fn==33 || fn==36 || fn==37)) {
                const unsigned target=fn==32?16:fn==33?17:fn==36 || (fn>=12 && fn<=15)?20:21;
                const unsigned mode=(s.cp0[12]&6)?0:unsigned(s.cp0[12]>>3)&3;
                if(target==21 && mode && !(s.cp0[12]&(mode==1?64:32))) {fault(10);break;}
                if(!fr() && ((rd|sa)&1)) {fault(10);break;} // Undefined odd FPR operation convention.
                const unsigned rm=fn>=8 && fn<=15?fn&3:s.fcr31&3;
                const uint64_t input=rs==16 || rs==20?fp_word(rd):s.fpr[rd];
                const auto result=rt?FpuConversions::Result{0,32}:FpuConversions::convert(rs,target,input,rm,s.fcr31&0x1000000,(s.fcr31>>7)&31);
                s.fcr31=(s.fcr31&~0x3f000u)|(result.causes<<12);
                if((result.causes&32) || (result.causes&((s.fcr31>>7)&31)))fault(15);
                else {s.fcr31|=(result.causes&31)<<2;
                    if(target==16 || target==20)fp_put_word(sa,uint32_t(result.bits));else s.fpr[sa]=result.bits;}
            } else fault(10); // Remaining COP1 operations are explicitly unsupported.
            break;
        case 18: case 50: case 54: case 58: case 62:
            // CU2 is checked before reserved suboperations or data translation.
            // Enabled expansion-coprocessor behavior is still unsupported.
            if(!(s.cp0[12]&0x40000000)) {s.cp0[13]=(s.cp0[13]&~0x30000000ull)|0x20000000;fault(11);}
            else fault(10);
            break;
        case 24: add(rt, a, imm, true, false, true); break;
        case 25: add(rt, a, imm, true, false, false); break;
        case 26: merge_load(8, true); break;
        case 27: merge_load(8, false); break;
        case 32: load(1, true); break;
        case 33: load(2, true); break;
        case 34: merge_load(4, true); break;
        case 35: load(4, true); break;
        case 36: load(1, false); break;
        case 37: load(2, false); break;
        case 38: merge_load(4, false); break;
        case 39: load(4, false); break;
        case 40: store(1); break;
        case 41: store(2); break;
        case 42: merge_store(4, true); break;
        case 43: store(4); break;
        case 44: merge_store(8, true); break;
        case 45: merge_store(8, false); break;
        case 46: merge_store(4, false); break;
        case 47: {
            const auto status=s.cp0[12];
            if(!(status&6) && (status&24) && !(status&0x10000000)) {s.cp0[13]&=~0x30000000ull;fault(11);break;}
            if(!PrimaryCaches::defined(rt)) {fault(10);break;}
            uint32_t tag=uint32_t(s.cp0[28]);
            Tlb::Translation t;access_address=a+imm;access_bytes=0;access_store=false;
            if(rt>=13 && !translate(a+imm,false,pc,delay,t))break;
            const auto old_flags=(rt&1)?caches.data[((a+imm)>>4)&511].flags:caches.instruction[((a+imm)>>5)&511].flags;
            if(!caches.operation_translated(memory,rt,a+imm,t.physical,tag)) {fault(7);break;}
            const auto new_flags=(rt&1)?caches.data[((a+imm)>>4)&511].flags:caches.instruction[((a+imm)>>5)&511].flags;
            if((old_flags&1) && !(new_flags&1))s.llbit=false;
            if(rt==4 || rt==5) {s.cp0[28]=tag;s.cp0[29]=0;}
            break;
        }
        case 49: fp_memory(4,false); break;
        case 48: linked_memory(false,false); break;
        case 52: linked_memory(true,false); break;
        case 56: linked_memory(false,true); break;
        case 60: linked_memory(true,true); break;
        case 53: fp_memory(8,false); break;
        case 57: fp_memory(4,true); break;
        case 61: fp_memory(8,true); break;
        case 55: load(8, false); break;
        case 63: store(8); break;
        default: fault(10); break;
        }
    }
    s.gpr[0] = 0;
}
}
