#include "aarch64.hpp"
namespace nx {
static int64_t signed_bits(uint64_t value,unsigned bits){
    const uint64_t sign=uint64_t(1)<<(bits-1);
    return int64_t(value& (sign-1))-int64_t(value&sign);
}
static uint64_t unsigned_product_high(uint64_t a,uint64_t b){
    // Four 32-bit limbs; every intermediate fits an unsigned 64-bit word.
    const uint64_t a0=uint32_t(a),a1=a>>32,b0=uint32_t(b),b1=b>>32;
    const uint64_t low=a0*b0,middle=a1*b0+(low>>32);
    const uint64_t cross=a0*b1+uint32_t(middle);
    return a1*b1+(middle>>32)+(cross>>32);
}
static uint64_t integer_divide(uint64_t a,uint64_t b,unsigned width,bool is_signed){
    const uint64_t mask=width==64?~uint64_t(0):0xffffffffu,sign=uint64_t(1)<<(width-1);
    a&=mask;b&=mask;
    if(!b)return 0;
    if(!is_signed)return a/b;
    const bool a_negative=a&sign,b_negative=b&sign;
    const uint64_t left=a_negative?(-a)&mask:a,right=b_negative?(-b)&mask:b;
    const auto quotient=left/right;
    // Magnitudes avoid host signed MIN/-1 overflow. Architectural results
    // truncate toward zero, retain the low width bits and leave NZCV alone.
    return (a_negative!=b_negative?-quotient:quotient)&mask;
}
bool Aarch64::condition(unsigned c) const{
    const bool n=state.nzcv&8,z=state.nzcv&4,carry=state.nzcv&2,v=state.nzcv&1;
    bool result=false;
    switch(c>>1){case 0:result=z;break;case 1:result=carry;break;case 2:result=n;break;case 3:result=v;break;case 4:result=carry&&!z;break;case 5:result=n==v;break;case 6:result=n==v&&!z;break;case 7:result=true;break;}
    return (c&1)&&c!=15?!result:result;
}
uint64_t Aarch64::add(uint64_t a,uint64_t b,unsigned carry,unsigned width,bool flags){
    const uint64_t mask=width==64?~uint64_t(0):0xffffffffu,sign=uint64_t(1)<<(width-1);
    a&=mask;b&=mask;
    const auto partial=(a+b)&mask,result=(partial+carry)&mask;
    if(flags)state.nzcv=unsigned(bool(result&sign))*8+unsigned(result==0)*4+
        unsigned(partial<a || result<partial)*2+unsigned(bool((~(a^b)&(a^result))&sign));
    return result;
}
bool Aarch64::step(){
    if(state.fault)return false;
    const uint64_t pc=state.pc;++state.steps;
    if(pc&3)return fail(1,pc,0);
    uint64_t fetched=0;if(!bus.read(pc,4,fetched,true))return fail(2,pc,0);
    const uint32_t w=uint32_t(fetched);const unsigned rd=w&31,rn=(w>>5)&31,rm=(w>>16)&31,width=(w>>31)?64:32;
    const uint64_t mask=width==64?~uint64_t(0):0xffffffffu;
    uint64_t next=pc+4;
    if(w==0xd503201f){} // NOP. Other hints/barriers remain unsupported.
    else if((w&0xffffffe0)==0xd53b4200)put(rd,uint64_t(state.nzcv)<<28); // MRS NZCV.
    else if((w&0xffffffe0)==0xd51b4200)state.nzcv=unsigned(reg(rd)>>28)&15; // MSR NZCV.
    else if((w&0x1f800000)==0x12800000){
        const unsigned op=(w>>29)&3,shift=((w>>21)&3)*16;
        if(op==1 || (width==32 && shift>=32))return fail(3,pc,w);
        const uint64_t immediate=uint64_t((w>>5)&65535)<<shift;
        put(rd,(op==0?~immediate:op==2?immediate:(reg(rd)&~(uint64_t(65535)<<shift))|immediate)&mask);
    }
    else if((w&0x1f800000)==0x11000000){
        const bool subtract=w&(1u<<30),flags=w&(1u<<29);
        uint64_t b=uint64_t((w>>10)&4095)<<((w&(1u<<22))?12:0);
        if(subtract)b=~b;
        put(rd,add(reg(rn,true),b,subtract,width,flags),!flags);
    }
    else if((w&0x1f200000)==0x0b000000 || (w&0x1fe0fc00)==0x1a000000){
        const bool subtract=w&(1u<<30),flags=w&(1u<<29),with_carry=(w&0x1fe0fc00)==0x1a000000;
        uint64_t b=reg(rm)&mask;
        if(!with_carry){const unsigned shift=(w>>22)&3,amount=(w>>10)&63;
            if(shift==3 || amount>=width)return fail(3,pc,w);
            if(shift==0)b=(b<<amount)&mask;
            else if(shift==1)b>>=amount;
            else if(amount){const auto sign=uint64_t(1)<<(width-1);b=(b>>amount)|((b&sign)?mask^(mask>>amount):0);}
        }
        const unsigned carry=with_carry?((state.nzcv>>1)&1):unsigned(subtract);
        put(rd,add(reg(rn),subtract?~b:b,carry,width,flags));
    }
    else if((w&0x1f000000)==0x0a000000){
        const unsigned op=(w>>29)&3,shift=(w>>22)&3,amount=(w>>10)&63;
        if(amount>=width)return fail(3,pc,w);
        uint64_t b=reg(rm)&mask;
        if(shift==0)b=(b<<amount)&mask;
        else if(shift==1)b>>=amount;
        else if(shift==2 && amount)b=(b>>amount)|((b&(uint64_t(1)<<(width-1)))?mask^(mask>>amount):0);
        else if(shift==3 && amount)b=((b>>amount)|(b<<(width-amount)))&mask;
        if(w&(1u<<21))b=(~b)&mask;
        const auto a=reg(rn)&mask,result=op==0 || op==3?a&b:op==1?a|b:a^b;
        put(rd,result);
        if(op==3)state.nzcv=unsigned(bool(result&(uint64_t(1)<<(width-1))))*8+unsigned(!result)*4;
    }
    else if((w&0x3fe00800)==0x1a800000){
        // Conditional select: both sources use XZR, never SP; NZCV is preserved.
        uint64_t result=reg(rn);
        if(!condition((w>>12)&15)){
            result=reg(rm);
            if(w&(1u<<30))result=~result;
            if(w&1024)++result;
        }
        put(rd,result&mask);
    }
    else if((w&0x7fe0fc00)==0x1ac00800 || (w&0x7fe0fc00)==0x1ac00c00){
        put(rd,integer_divide(reg(rn),reg(rm),width,bool(w&1024)));
    }
    else if((w&0x7f000000)==0x1b000000){
        const unsigned type=(w>>21)&7,ra=(w>>10)&31;const bool subtract=w&32768;
        const uint64_t a=reg(rn),b=reg(rm),accumulator=reg(ra);
        uint64_t result=0;
        if(type==0){const auto product=(a&mask)*(b&mask);result=(subtract?accumulator-product:accumulator+product)&mask;}
        else if(width==64 && (type==1 || type==5)){
            uint64_t left=uint32_t(a),right=uint32_t(b);
            if(type==1){if(left&0x80000000u)left|=0xffffffff00000000ULL;if(right&0x80000000u)right|=0xffffffff00000000ULL;}
            const auto product=left*right;result=subtract?accumulator-product:accumulator+product;
        }
        else if(width==64 && (type==2 || type==6) && !subtract && ra==31){
            result=unsigned_product_high(a,b);
            if(type==2){if(a>>63)result-=b;if(b>>63)result-=a;}
        }
        else return fail(3,pc,w);
        put(rd,result);
    }
    else if((w&0x7c000000)==0x14000000){if(w>>31)state.x[30]=next;next=pc+uint64_t(signed_bits(w&0x3ffffff,26)*4);}
    else if((w&0xff000010)==0x54000000){if(condition(w&15))next=pc+uint64_t(signed_bits((w>>5)&0x7ffff,19)*4);}
    else if((w&0x7e000000)==0x34000000){if(bool(reg(rd)&mask)==bool(w&(1u<<24)))next=pc+uint64_t(signed_bits((w>>5)&0x7ffff,19)*4);}
    else if((w&0xfffffc1f)==0xd61f0000 || (w&0xfffffc1f)==0xd63f0000 || (w&0xfffffc1f)==0xd65f0000){
        next=reg(rn);if((w&0xfffffc1f)==0xd63f0000)state.x[30]=pc+4;
    }
    else if((w&0x1f000000)==0x10000000){
        const int64_t offset=signed_bits(((w>>5)&0x7ffff)*4+((w>>29)&3),21);
        put(rd,(w>>31)?(pc&~uint64_t(4095))+uint64_t(offset*4096):pc+uint64_t(offset));
    }
    else if((w&0x3b000000)==0x39000000){
        const unsigned size=1u<<(w>>30),op=(w>>22)&3;
        if(op>1 || (w&(1u<<26)))return fail(3,pc,w);
        const uint64_t address=reg(rn,true)+uint64_t((w>>10)&4095)*size;
        if(op){uint64_t value;if(!bus.read(address,size,value))return fail(4,address,w);put(rd,value);}
        else if(!bus.write(address,size,reg(rd)))return fail(5,address,w);
    }
    else return fail(3,pc,w);
    state.pc=next;return true;
}
}
