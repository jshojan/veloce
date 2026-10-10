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
    const uint32_t pc=s.r[15]; uint32_t ins=0;
    if((pc&3) || !bus.read(pc,4,ins)) { s.unsupported=true; return; }
    const unsigned cond=ins>>28;
    if(cond==15 && v5) { s.unsupported=true; return; }
    if(!condition(cond,s.cpsr)) { s.r[15]=pc+4; return; }
    const auto operand=[&](unsigned reg,bool late=false) { return reg==15 ? pc+(late ? 12:8):s.r[reg]; };
    if((ins&0x0ffffff0)==0x012fff10) {
        const auto target=operand(ins&15);
        if(target&1) { s.unsupported=true; return; }
        s.r[15]=target&~3u; return;
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
            // ARMv5 load-to-PC interworking needs Thumb; stop diagnostically.
            if(rd==15 && v5 && (value&1)) { s.unsupported=true; return; }
        } else if(!bus.write(byte ? address:address&~3u,byte ? 1:4,rd==15 ? pc+(v5 ? 8:12):s.r[rd])) {
            s.unsupported=true; return;
        }
        if(!pre || wb) s.r[rn]=adjusted;
        if(load) s.r[rd]=rd==15 ? value&~3u:value;
        if(!load || rd!=15) s.r[15]=pc+4;
        return;
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
}
