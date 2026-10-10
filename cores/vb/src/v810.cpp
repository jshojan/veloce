#include "v810.hpp"

namespace vb {
uint32_t extend(uint32_t value, unsigned bits) {
    const uint32_t sign = 1u << (bits-1), mask = (1u << bits)-1;
    value &= mask;
    return (value ^ sign)-sign;
}
void V810::reset() {
    state = {}; state.pc = 0xfffffff0;
    // Test profile initializes otherwise unspecified registers to zero.
    state.sr[4] = 0xfff0;
    state.sr[5] = 0x8000; // NP is set by reset. Other board reset paths are pending.
}
bool V810::condition(unsigned code, uint32_t psw) {
    const bool z=psw&1, s=psw&2, ov=psw&4, cy=psw&8;
    const bool values[] = {ov,cy,z,cy||z,s,true,s!=ov,(s!=ov)||z};
    return (code&8) ? !values[code&7] : values[code&7];
}
void V810::divide_zero(uint32_t pc) {
    auto& c=state;
    const uint32_t psw=c.sr[5];
    if (psw&0x8000) {
        // Fatal machine-fault writes are visible on the operand bus. A board
        // may reject these addresses; the CPU still halts until reset.
        bus.write(0,4,0xffffff80); bus.write(4,4,psw); bus.write(8,4,pc);
        c.pc=pc; c.halted=true;
    } else if (psw&0x4000) {
        c.sr[2]=pc; c.sr[3]=psw;
        c.sr[4]=(c.sr[4]&0xffff)|0xff800000;
        c.sr[5]=(psw|0x9000)&~0x2000u; c.pc=0xffffffd0;
    } else {
        c.sr[0]=pc; c.sr[1]=psw;
        c.sr[4]=(c.sr[4]&0xffff0000)|0xff80;
        c.sr[5]=(psw|0x5000)&~0x2000u; c.pc=0xffffff80;
    }
}
void V810::step() {
    auto& c=state;
    if (c.halted || c.unsupported) return;
    ++c.steps;
    uint32_t first=0;
    if (!bus.read(c.pc,2,first)) { c.unsupported=true; return; }
    const uint32_t pc=c.pc;
    c.pc+=2;
    const unsigned op=first>>10, r1=first&31, r2=(first>>5)&31;
    const uint32_t a=c.gpr[r1], b=c.gpr[r2];
    auto put=[&](unsigned reg,uint32_t value) { if (reg) c.gpr[reg]=value; };
    auto flags=[&](uint32_t result,bool carry,bool overflow,bool keep_carry=false) {
        c.sr[5]=(c.sr[5]&~15u)|(result==0 ? 1u:0u)|(result>>31 ? 2u:0u)|
                (overflow ? 4u:0u)|((keep_carry ? (c.sr[5]&8)!=0 : carry) ? 8u:0u);
    };
    auto add=[&](uint32_t x,uint32_t y,bool sub,bool compare=false) {
        const uint32_t result=sub ? x-y : x+y;
        const bool cy=sub ? x<y : uint64_t(x)+y>0xffffffff;
        const bool ov=sub ? ((x^y)&(x^result)&0x80000000)!=0 : ((~(x^y))&(x^result)&0x80000000)!=0;
        flags(result,cy,ov); if (!compare) put(r2,result);
    };
    auto shift=[&](uint32_t value,unsigned amount,unsigned kind) {
        amount&=31;
        uint32_t result=value; bool cy=false;
        if (amount) {
            if (kind==0) { cy=(value>>(32-amount))&1; result=value<<amount; }
            else { cy=(value>>(amount-1))&1; result=value>>amount;
                if (kind==2 && (value&0x80000000)) result|=~0u<<(32-amount); }
        }
        flags(result,cy,false); put(r2,result);
    };
    if ((first&0xe000)==0x8000) {
        if (condition((first>>9)&15,c.sr[5])) c.pc=pc+extend(first&0x1fe,9);
        return;
    }
    if (op>=0x28) {
        uint32_t tail=0;
        if (!bus.read(c.pc,2,tail)) { c.pc=pc; c.unsupported=true; return; }
        c.pc+=2;
        const uint32_t imm=extend(tail,16);
        const uint32_t address=a+imm;
        uint32_t value=0;
        switch(op) {
        case 0x28: put(r2,a+imm); break;
        case 0x29: add(a,imm,false); break;
        case 0x2a: case 0x2b:
            if(op==0x2b) put(31,pc+4);
            c.pc=pc+extend(((first&0x3ff)<<16)|(tail&0xfffe),26); break;
        case 0x2c: value=a|tail; flags(value,false,false,true); put(r2,value); break;
        case 0x2d: value=a&tail; flags(value,false,false,true); put(r2,value); break;
        case 0x2e: value=a^tail; flags(value,false,false,true); put(r2,value); break;
        case 0x2f: put(r2,a+(tail<<16)); break;
        case 0x30: case 0x31: case 0x33: {
            const unsigned bytes=op==0x30 ? 1 : op==0x31 ? 2 : 4;
            if (!bus.read(address&~uint32_t(bytes-1),bytes,value)) c.unsupported=true;
            else put(r2,bytes==4 ? value : extend(value,bytes*8));
            break;
        }
        case 0x34: case 0x35: case 0x37: {
            const unsigned bytes=op==0x34 ? 1 : op==0x35 ? 2 : 4;
            if (!bus.write(address&~uint32_t(bytes-1),bytes,b)) c.unsupported=true;
            break;
        }
        default: c.unsupported=true; break;
        }
    } else {
        switch(op) {
        case 0: put(r2,a); break;
        case 1: add(b,a,false); break;
        case 2: add(b,a,true); break;
        case 3: add(b,a,true,true); break;
        case 4: shift(b,a,0); break;
        case 5: shift(b,a,1); break;
        case 6: c.pc=a&~1u; break;
        case 7: shift(b,a,2); break;
        case 8: case 10: {
            const auto signed_word=[](uint32_t x) -> int64_t {
                return x&0x80000000 ? int64_t(x)-0x100000000LL : int64_t(x);
            };
            const int64_t product=op==8 ? signed_word(a)*signed_word(b) : 0;
            const uint64_t bits=op==8 ? uint64_t(product) : uint64_t(a)*b;
            const uint32_t low=uint32_t(bits);
            const bool overflow=op==8 ? product!=signed_word(low) : (bits>>32)!=0;
            put(30,uint32_t(bits>>32)); put(r2,low);
            flags(low,false,overflow,true); break;
        }
        case 9: case 11: {
            if (!a) { divide_zero(pc); break; }
            uint32_t quotient,remainder; bool overflow=false;
            if (op==9) {
                const int64_t dividend=b&0x80000000 ? int64_t(b)-0x100000000LL : b;
                const int64_t divisor=a&0x80000000 ? int64_t(a)-0x100000000LL : a;
                const int64_t q=dividend/divisor;
                quotient=uint32_t(q); remainder=uint32_t(dividend%divisor);
                overflow=q>0x7fffffffLL;
            } else { quotient=b/a; remainder=b%a; }
            put(30,remainder); put(r2,quotient);
            flags(quotient,false,overflow,true); break;
        }
        case 0x0c: case 0x0d: case 0x0e: case 0x0f: {
            const uint32_t value=op==0x0c ? b|a : op==0x0d ? b&a : op==0x0e ? b^a : ~a;
            flags(value,false,false,true); put(r2,value); break;
        }
        case 0x10: put(r2,extend(r1,5)); break;
        case 0x11: add(b,extend(r1,5),false); break;
        case 0x12: put(r2,condition(r1&15,c.sr[5])); break;
        case 0x13: add(b,extend(r1,5),true,true); break;
        case 0x14: shift(b,r1,0); break;
        case 0x15: shift(b,r1,1); break;
        case 0x17: shift(b,r1,2); break;
        case 0x19: {
            const unsigned saved=(c.sr[5]&0x8000) ? 2 : 0;
            c.pc=c.sr[saved]; c.sr[5]=c.sr[saved+1]; break;
        }
        case 0x1a: c.halted=true; break;
        case 0x1c:
            if(r1==0 || r1==2 || r1==25) c.sr[r1]=b&~1u;
            else if(r1==1 || r1==3 || r1==5) c.sr[r1]=b&0xff3ff;
            else if(r1==24) { if(b&~2u) c.unsupported=true; else c.sr[r1]=b; }
            else if(r1!=4 && r1!=6 && r1!=7) c.unsupported=true;
            break;
        case 0x1d:
            if(r1<=5 || r1==24 || r1==25) put(r2,c.sr[r1]); else c.unsupported=true;
            break;
        default: c.unsupported=true; break;
        }
    }
    c.gpr[0]=0;
}
}
