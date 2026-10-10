#pragma once
#include <cstdint>
namespace n64 {
// VR4300 arithmetic on integer significands. Host FP modes and NaNs are irrelevant.
class FpuArithmetic {
public:
    struct Result {uint64_t bits=0;unsigned causes=0;}; // I,U,O,Z,V,E.
    static Result calculate(unsigned fmt,unsigned fn,uint64_t left,uint64_t right,unsigned rm,bool flush,unsigned enables) {
        if((fmt!=16 && fmt!=17) || fn>7)return {0,32};
        const unsigned f=fmt==16?23:52,eb=fmt==16?8:11,bias=fmt==16?127:1023;
        const uint64_t signbit=uint64_t(1)<<(f+eb),inf=((uint64_t(1)<<eb)-1)<<f;
        if(fmt==16) {left=uint32_t(left);right=uint32_t(right);}
        if(fn==6)return {left,0}; // MOV neither interprets the operand nor changes FCSR.
        auto a=decode(left,f,bias,inf),b=decode(right,f,bias,inf);
        if(a.assist || (fn<4 && b.assist))return {0,32};
        if(a.nan || (fn<4 && b.nan))return {fmt==16?0x7fbfffffull:0x7ff7ffffffffffffull,16};
        const auto invalid=[&]()->Result{return {fmt==16?0x7fbfffffull:0x7ff7ffffffffffffull,16};};
        if(fn==5)return {left&~signbit,0};
        if(fn==7)return {left^signbit,0};
        if(fn==4) {
            if(a.negative && (a.sig || a.infinity))return invalid();
            if(a.infinity || !a.sig)return {left,0};
            // Radicand = significand * 2^(f+6+parity). Restoring square root,
            // two input bits per step; remainder and root each fit uint64_t.
            const unsigned parity=unsigned(a.exponent)&1,shift=f+6+parity;
            uint64_t root=0,remainder=0;
            for(int pair=int(f+3);pair>=0;--pair) {
                unsigned digit=0;
                for(int j=1;j>=0;--j) {const int bit=pair*2+j-int(shift);digit=(digit<<1)|unsigned(bit>=0 && bit<64?((a.sig>>bit)&1):0);}
                remainder=(remainder<<2)|digit;const uint64_t trial=(root<<2)|1;
                root<<=1;if(remainder>=trial) {remainder-=trial;root|=1;}
            }
            if(remainder)root|=1;
            return pack(root,(a.exponent-int(parity))/2,false,f,eb,bias,rm,flush,enables);
        }
        if(fn<=1) {
            if(fn==1)b.negative=!b.negative;
            if(a.infinity || b.infinity) {
                if(a.infinity && b.infinity && a.negative!=b.negative)return invalid();
                return {inf|((a.infinity?a.negative:b.negative)?signbit:0),0};
            }
            if(!a.sig && !b.sig)return {(a.negative==b.negative?a.negative:rm==3)?signbit:0,0};
            if(!a.sig)return {(right&~signbit)|(b.negative?signbit:0),0};
            if(!b.sig)return {left,0};
            if(a.exponent<b.exponent || (a.exponent==b.exponent && a.sig<b.sig)) {const auto tmp=a;a=b;b=tmp;}
            uint64_t x=a.sig<<3,y=jam(b.sig<<3,unsigned(a.exponent-b.exponent));
            x=a.negative==b.negative?x+y:x-y;
            if(!x)return {rm==3?signbit:0,0};
            return pack(x,a.exponent,a.negative,f,eb,bias,rm,flush,enables);
        }
        const bool negative=a.negative!=b.negative;
        if(fn==2) {
            if((a.infinity && !b.sig && !b.infinity) || (b.infinity && !a.sig && !a.infinity))return invalid();
            if(a.infinity || b.infinity)return {inf|(negative?signbit:0),0};
            if(!a.sig || !b.sig)return {negative?signbit:0,0};
            const auto product=multiply(a.sig,b.sig);
            return pack(shrink(product,f-3),a.exponent+b.exponent,negative,f,eb,bias,rm,flush,enables);
        }
        if((a.infinity && b.infinity) || (!a.sig && !a.infinity && !b.sig && !b.infinity))return invalid();
        if(a.infinity)return {inf|(negative?signbit:0),0};
        if(b.infinity || !a.sig)return {negative?signbit:0,0};
        if(!b.sig)return {inf|(negative?signbit:0),8};
        // Long division of sig * 2^(f+4); the remainder is at most 54 bits.
        uint64_t quotient=0,remainder=0;
        for(int bit=int(2*f+4);bit>=0;--bit) {
            const int source=bit-int(f+4);remainder=(remainder<<1)|(source>=0?((a.sig>>source)&1):0);
            quotient<<=1;if(remainder>=b.sig) {remainder-=b.sig;quotient|=1;}
        }
        if(remainder)quotient|=1;
        return pack(quotient,a.exponent-b.exponent-1,negative,f,eb,bias,rm,flush,enables);
    }
private:
    struct Operand {uint64_t sig;int exponent;bool negative,infinity,nan,assist;};
    static Operand decode(uint64_t bits,unsigned f,unsigned bias,uint64_t inf) {
        const uint64_t tail=bits&((uint64_t(1)<<f)-1),exponent=(bits&inf)>>f;
        const bool special=(bits&inf)==inf,nan=special && tail;
        return {exponent && !special?(uint64_t(1)<<f)|tail:0,int(exponent)-int(bias),bool(bits>>(f+(f==23?8:11))),special && !tail,nan,
            (!exponent && tail) || (nan && !(tail&(uint64_t(1)<<(f-1))))};
    }
    static uint64_t jam(uint64_t value,unsigned shift) {
        if(!shift)return value;
        if(shift>=64)return value!=0;
        return (value>>shift)|unsigned((value&((uint64_t(1)<<shift)-1))!=0);
    }
    struct Wide {uint64_t high,low;};
    static Wide multiply(uint64_t a,uint64_t b) {
        const uint64_t al=uint32_t(a),ah=a>>32,bl=uint32_t(b),bh=b>>32;
        const uint64_t lo=al*bl,t=ah*bl+(lo>>32),mid=(t&0xffffffff)+al*bh;
        return {ah*bh+(t>>32)+(mid>>32),(mid<<32)|uint32_t(lo)};
    }
    static uint64_t shrink(Wide v,unsigned shift) {
        // Product is at most 106 bits; these shifts are 20 or 49.
        return (v.high<<(64-shift))|(v.low>>shift)|unsigned((v.low&((uint64_t(1)<<shift)-1))!=0);
    }
    static Result pack(uint64_t sig,int exponent,bool negative,unsigned f,unsigned eb,unsigned bias,unsigned rm,bool flush,unsigned enables) {
        const uint64_t hidden=uint64_t(1)<<(f+3),sign=uint64_t(negative)<<(f+eb),inf=((uint64_t(1)<<eb)-1)<<f;
        while(sig>=hidden*2) {sig=jam(sig,1);++exponent;}
        while(sig<hidden) {sig<<=1;--exponent;}
        const int minimum=1-int(bias);
        if(exponent<minimum) {sig=jam(sig,unsigned(minimum-exponent));exponent=minimum;}
        uint64_t rounded=sig>>3;const unsigned remainder=unsigned(sig)&7;
        const bool increment=rm==0?(remainder>4 || (remainder==4 && (rounded&1))):remainder && ((rm==2 && !negative)||(rm==3 && negative));
        rounded+=unsigned(increment);
        if(rounded>=(uint64_t(1)<<(f+1))) {rounded>>=1;++exponent;}
        if(exponent>int(bias)) {
            const bool infinity=rm==0 || (rm==2 && !negative)||(rm==3 && negative);
            return {sign|(infinity?inf:inf-1),5};
        }
        if(rounded<(uint64_t(1)<<f)) {
            if(!flush || (enables&3))return {0,32};
            const bool away=(rm==2 && !negative)||(rm==3 && negative);
            return {sign|(away?(uint64_t(1)<<f):0),3};
        }
        return {sign|(uint64_t(exponent+int(bias))<<f)|(rounded&((uint64_t(1)<<f)-1)),remainder?1u:0u};
    }
};
}
