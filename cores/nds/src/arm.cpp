#include "arm.hpp"
namespace nds {
static uint32_t rotate(uint32_t value,unsigned count) {
    count&=31; return count ? (value>>count)|(value<<(32-count)):value;
}
bool Arm::condition(unsigned code,uint32_t psw) {
    const bool n=psw>>31,z=(psw>>30)&1,c=(psw>>29)&1,v=(psw>>28)&1;
    switch(code) {
    case 0:return z; case 1:return !z; case 2:return c; case 3:return !c;
    case 4:return n; case 5:return !n; case 6:return v; case 7:return !v;
    case 8:return c&&!z; case 9:return !c||z; case 10:return n==v;
    case 11:return n!=v; case 12:return !z&&n==v; case 13:return z||n!=v;
    case 14:return true; default:return false;
    }
}
uint32_t Arm::shift(uint32_t value,unsigned type,unsigned amount,bool by_register,bool& carry) {
    if(by_register) { amount&=255; if(!amount) return value; }
    else if(!amount) {
        if(type==0) return value;
        if(type==3) { const auto result=(value>>1)|(uint32_t(carry)<<31); carry=value&1; return result; }
        amount=32;
    }
    if(type==0) {
        carry=amount<=32 && ((value>>(32-amount))&1);
        return amount<32 ? value<<amount:0;
    }
    if(type==1) {
        carry=amount<=32 && ((value>>(amount-1))&1);
        return amount<32 ? value>>amount:0;
    }
    if(type==2) {
        const bool negative=value>>31;
        if(amount>=32) { carry=negative; return negative ? ~0u:0; }
        carry=(value>>(amount-1))&1;
        return (value>>amount)|(negative ? ~0u<<(32-amount):0);
    }
    const auto result=rotate(value,amount); carry=result>>31; return result;
}
void Arm::step() {
    auto& s=state; if(s.unsupported) return;
    ++s.steps;
    if(s.cpsr&0x20) { thumb_step(); return; }
    const uint32_t pc=s.r[15]; uint32_t ins=0;
    if((pc&3) || !bus.read(pc,4,ins)) { s.unsupported=true; return; }
    const unsigned cond=ins>>28;
    if(cond==15 && v5) { s.unsupported=true; return; }
    if(!condition(cond,s.cpsr)) { s.r[15]=pc+4; return; }
    const auto operand=[&](unsigned reg,bool late=false) { return reg==15 ? pc+(late ? 12:8):s.r[reg]; };
    if((ins&0x0ffffff0)==0x012fff10 || (ins&0x0ffffff0)==0x012fff30) {
        const bool link=ins&0x20;
        if(link && (!v5 || (ins&15)==15)) { s.unsupported=true; return; }
        const auto target=operand(ins&15);
        if(link && (target&3)==2) { s.unsupported=true; return; }
        if(link) s.r[14]=pc+4;
        s.cpsr=(s.cpsr&~0x20u)|((target&1)<<5);
        s.r[15]=target&((target&1) ? ~1u:~3u); return;
    }
    if((ins&0x0fff0fff)==0x010f0000) {
        const unsigned rd=(ins>>12)&15;
        if(rd==15) { s.unsupported=true; return; }
        s.r[rd]=s.cpsr; s.r[15]=pc+4; return;
    }
    if((ins&0x0ffff000)==0x0128f000 || (ins&0x0ffff000)==0x0328f000) {
        const uint32_t value=(ins&(1u<<25)) ? rotate(ins&255,((ins>>8)&15)*2):operand(ins&15);
        const uint32_t mask=v5 ? 0xf8000000u:0xf0000000u;
        s.cpsr=(s.cpsr&~mask)|(value&mask); s.r[15]=pc+4; return;
    }
    if((ins&0x0e000000)==0x0a000000) {
        const uint32_t displacement=(ins&0xffffff)<<2;
        const uint32_t extended=displacement|(displacement&0x02000000 ? 0xfc000000u:0);
        if(ins&(1u<<24)) s.r[14]=pc+4;
        s.r[15]=pc+8+extended; return;
    }
    if((ins&0x0c000000)==0x04000000) {
        const bool pre=ins&(1u<<24),up=ins&(1u<<23),byte=ins&(1u<<22),wb=ins&(1u<<21),load=ins&(1u<<20);
        const unsigned rn=(ins>>16)&15,rd=(ins>>12)&15;
        if((ins&(1u<<25)) && (ins&16)) { s.unsupported=true; return; }
        if(((!pre || wb) && rn==15) || (load && (!pre || wb) && rn==rd) || (byte && rd==15)) { s.unsupported=true; return; }
        bool carry=(s.cpsr>>29)&1;
        const auto offset=(ins&(1u<<25)) ? shift(operand(ins&15),(ins>>5)&3,(ins>>7)&31,false,carry):ins&4095;
        const auto base=operand(rn),adjusted=up ? base+offset:base-offset,address=pre ? adjusted:base;
        uint32_t value=0;
        if(load) {
            if(!bus.read(byte ? address:address&~3u,byte ? 1:4,value)) { s.unsupported=true; return; }
            if(!byte) value=rotate(value,(address&3)*8);
        } else if(!bus.write(byte ? address:address&~3u,byte ? 1:4,rd==15 ? pc+(v5 ? 8:12):s.r[rd])) {
            s.unsupported=true; return;
        }
        if(!pre || wb) s.r[rn]=adjusted;
        if(load) {
            if(rd==15 && v5) s.cpsr=(s.cpsr&~0x20u)|((value&1)<<5);
            s.r[rd]=rd==15 ? value&((v5 && (value&1)) ? ~1u:~3u):value;
        }
        if(!load || rd!=15) s.r[15]=pc+4;
        return;
    }
    const bool multiply=(ins&0x0fc000f0)==0x00000090;
    const bool long_multiply=(ins&0x0f8000f0)==0x00800090;
    if(multiply || long_multiply) {
        const bool accumulate=ins&(1u<<21),set=ins&(1u<<20);
        const unsigned high=(ins>>16)&15,low=(ins>>12)&15,rs=(ins>>8)&15,rm=ins&15;
        // Stop for architecturally unpredictable operands and nonzero MUL SBZ.
        if(high==15 || rs==15 || rm==15 || (long_multiply ? low==15 || high==low:accumulate ? low==15:low!=0)) {
            s.unsupported=true; return;
        }
        const auto signed_value=[](uint32_t value)->int64_t { return value&0x80000000 ? int64_t(value)-0x100000000LL:value; };
        // Read every source before writing either destination (including aliases).
        uint64_t result=long_multiply && (ins&(1u<<22)) ?
            uint64_t(signed_value(s.r[rm])*signed_value(s.r[rs])):uint64_t(s.r[rm])*s.r[rs];
        if(accumulate) result+=long_multiply ? (uint64_t(s.r[high])<<32)|s.r[low]:s.r[low];
        if(long_multiply) { s.r[low]=uint32_t(result); s.r[high]=uint32_t(result>>32); }
        else { result=uint32_t(result); s.r[high]=uint32_t(result); }
        // ARMv4 S forms have unpredictable C (and long forms V). Preserve them
        // deterministically; hardware scoring excludes only those undefined bits.
        if(set) s.cpsr=(s.cpsr&0x3fffffff)|((long_multiply ? result>>32:result)&0x80000000)|
            (result==0 ? 0x40000000u:0);
        s.r[15]=pc+4; return;
    }
    if((ins&0x0c000000)!=0 || (!(ins&(1u<<25)) && (ins&0x90)==0x90)) { s.unsupported=true; return; }
    const unsigned op=(ins>>21)&15,rn=(ins>>16)&15,rd=(ins>>12)&15;
    const bool set=ins&(1u<<20),test=op>=8 && op<=11;
    if((test && !set) || (rd==15 && set && !test)) { s.unsupported=true; return; }
    bool carry=(s.cpsr>>29)&1;
    uint32_t b=0;
    if(ins&(1u<<25)) {
        const unsigned count=((ins>>8)&15)*2; b=rotate(ins&255,count); if(count) carry=b>>31;
    } else {
        const bool reg=ins&16;
        if(reg && ((ins>>8)&15)==15) { s.unsupported=true; return; }
        b=shift(operand(ins&15,reg),(ins>>5)&3,reg ? operand((ins>>8)&15)&255:(ins>>7)&31,reg,carry);
    }
    const uint32_t a=operand(rn),old_carry=(s.cpsr>>29)&1;
    uint32_t result=0; bool overflow=(s.cpsr>>28)&1,arithmetic=false;
    uint32_t x=a,y=b,extra=0;
    switch(op) {
    case 0:case 8:result=a&b;break;
    case 1:case 9:result=a^b;break;
    case 2:case 10:y=~b;extra=1;arithmetic=true;break;
    case 3:x=b;y=~a;extra=1;arithmetic=true;break;
    case 4:case 11:arithmetic=true;break;
    case 5:extra=old_carry;arithmetic=true;break;
    case 6:y=~b;extra=old_carry;arithmetic=true;break;
    case 7:x=b;y=~a;extra=old_carry;arithmetic=true;break;
    case 12:result=a|b;break; case 13:result=b;break;
    case 14:result=a&~b;break; case 15:result=~b;break;
    }
    if(arithmetic) {
        const uint64_t wide=uint64_t(x)+y+extra; result=uint32_t(wide); carry=wide>>32;
        const auto signed_value=[](uint32_t value)->int64_t { return value&0x80000000 ? int64_t(value)-0x100000000LL:value; };
        const int64_t mathematical=signed_value(x)+signed_value(y)+extra;
        overflow=mathematical< -0x80000000LL || mathematical>0x7fffffffLL;
    }
    if(set || test) s.cpsr=(s.cpsr&0x0fffffff)|(result&0x80000000)|(result==0 ? 0x40000000u:0)|
        (carry ? 0x20000000u:0)|(overflow ? 0x10000000u:0);
    if(!test) s.r[rd]=rd==15 ? result&~3u:result;
    if(test || rd!=15) s.r[15]=pc+4;
}
void Arm::thumb_step() {
    auto& s=state; const uint32_t pc=s.r[15]; uint32_t ins=0;
    if((pc&1) || !bus.read(pc,2,ins)) { s.unsupported=true; return; }
    const auto operand=[&](unsigned reg) { return reg==15 ? pc+4:s.r[reg]; };
    bool carry=(s.cpsr>>29)&1,overflow=(s.cpsr>>28)&1;
    const auto flags=[&](uint32_t result) {
        s.cpsr=(s.cpsr&0x0fffffff)|(result&0x80000000)|(result==0 ? 0x40000000u:0)|
            (carry ? 0x20000000u:0)|(overflow ? 0x10000000u:0);
    };
    const auto arithmetic=[&](uint32_t a,uint32_t b,bool subtract,unsigned extra=0) {
        const int64_t value=subtract ? int64_t(a)-b-extra:int64_t(a)+b+extra;
        const uint32_t result=uint32_t(value);
        carry=subtract ? value>=0:value>0xffffffffLL;
        overflow=((subtract ? a^b:~(a^b))&(a^result)&0x80000000)!=0;
        return result;
    };
    if((ins&0xf800)==0x1800) {
        const bool immediate=ins&0x400,subtract=ins&0x200;
        const unsigned rd=ins&7,rn=(ins>>3)&7,third=(ins>>6)&7;
        const auto result=arithmetic(s.r[rn],immediate ? third:s.r[third],subtract);
        s.r[rd]=result; flags(result);
    } else if((ins&0xe000)==0) {
        const unsigned type=(ins>>11)&3,rd=ins&7,rm=(ins>>3)&7;
        const auto result=shift(s.r[rm],type,(ins>>6)&31,false,carry);
        s.r[rd]=result; flags(result);
    } else if((ins&0xe000)==0x2000) {
        const unsigned op=(ins>>11)&3,rd=(ins>>8)&7; const auto immediate=ins&255;
        const auto result=op==0 ? immediate:arithmetic(s.r[rd],immediate,op!=2);
        if(op!=1) s.r[rd]=result;
        flags(result);
    } else if((ins&0xfc00)==0x4000) {
        const unsigned op=(ins>>6)&15,rd=ins&7,rm=(ins>>3)&7;
        if(op==13 && rd==rm) { s.unsupported=true; return; }
        const uint32_t a=s.r[rd],b=s.r[rm]; uint32_t result=0;
        switch(op) {
        case 0:case 8:result=a&b;break; case 1:result=a^b;break;
        case 2:case 3:case 4:result=shift(a,op-2,b,true,carry);break;
        case 5:result=arithmetic(a,b,false,carry);break;
        case 6:result=arithmetic(a,b,true,!carry);break;
        case 7:result=shift(a,3,b,true,carry);break;
        case 9:result=arithmetic(0,b,true);break;
        case 10:result=arithmetic(a,b,true);break; case 11:result=arithmetic(a,b,false);break;
        case 12:result=a|b;break; case 13:result=uint32_t(uint64_t(a)*b);break;
        case 14:result=a&~b;break; case 15:result=~b;break;
        }
        if(op!=8 && op!=10 && op!=11) s.r[rd]=result;
        // ARMv4 Thumb MUL carry is unpredictable; the model retains it.
        flags(result);
    } else if((ins&0xfc00)==0x4400) {
        const unsigned op=(ins>>8)&3,rd=(ins&7)|((ins>>4)&8),rm=(ins>>3)&15;
        if(op==3) {
            const bool link=ins&0x80;
            if((ins&7) || (link && (!v5 || rm==15))) { s.unsupported=true; return; }
            const auto target=operand(rm);
            if((target&3)==2) { s.unsupported=true; return; }
            if(link) s.r[14]=(pc+2)|1;
            s.cpsr=(s.cpsr&~0x20u)|((target&1)<<5);
            s.r[15]=target&~1u; return;
        }
        if(!(ins&0xc0) || (op==1 && rd==15)) { s.unsupported=true; return; }
        const auto a=operand(rd),b=operand(rm);
        const auto result=op==1 ? arithmetic(a,b,true):op==0 ? a+b:b;
        if(op==1) flags(result);
        else if(rd==15) { s.r[15]=result&~1u; return; }
        else s.r[rd]=result;
    } else if((ins&0xf000)==0xa000) {
        s.r[(ins>>8)&7]=((ins&0x800) ? s.r[13]:(pc+4)&~3u)+((ins&255)<<2);
    } else if((ins&0xff00)==0xb000) {
        const uint32_t offset=(ins&127)<<2;
        s.r[13]=(ins&0x80) ? s.r[13]-offset:s.r[13]+offset;
    } else if((ins&0xf000)==0xd000) {
        const unsigned cond=(ins>>8)&15;
        if(cond>=14) { s.unsupported=true; return; }
        const uint32_t delta=(ins&255)<<1;
        s.r[15]=pc+(condition(cond,s.cpsr) ? 4+ (delta|(delta&256 ? 0xfffffe00u:0)):2); return;
    } else if((ins&0xf800)==0xe000) {
        const uint32_t delta=(ins&2047)<<1;
        s.r[15]=pc+4+(delta|(delta&2048 ? 0xfffff000u:0)); return;
    } else { s.unsupported=true; return; }
    s.r[15]=pc+2;
}
}
