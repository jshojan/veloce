#pragma once
#include <cstdint>
namespace n64 {
// Integer-bit conversion: no dependency on host floating-point state or NaN conventions.
class FpuConversions {
public:
    enum Format : unsigned { single=16,dual=17,word=20,longword=21 };
    struct Result {uint64_t bits=0;unsigned causes=0;}; // I,U,O,Z,V,E from low to high.
    static Result convert(unsigned source,unsigned target,uint64_t bits,unsigned rm,bool flush,unsigned enables) {
        if((target==single || target==dual)?(source!=single && source!=dual && source!=word && source!=longword) || source==target:
            (target!=word && target!=longword) || (source!=single && source!=dual))return assist();
        bool negative=false;uint64_t magnitude=0;int scale=0;
        if(source==word || source==longword) {
            if(source==word) {bits=uint32_t(bits);negative=bits>>31;magnitude=uint32_t(negative?0-bits:bits);}
            else {negative=bits>>63;magnitude=negative?0-bits:bits;
                if(negative?magnitude>(uint64_t(1)<<55):magnitude>=(uint64_t(1)<<55))return assist();}
        } else {
            const unsigned fraction=source==single?23:52,bias=source==single?127:1023,maxexp=source==single?255:2047;
            negative=(bits>>(fraction+(source==single?8:11)))&1;
            const uint64_t tail=bits&((uint64_t(1)<<fraction)-1);const unsigned exponent=unsigned(bits>>fraction)&maxexp;
            if(exponent==maxexp) {
                if(target==word || target==longword)return assist();
                const unsigned destfrac=target==single?23:52,destexp=target==single?255:2047;
                if(!tail)return {(uint64_t(negative)<<(destfrac+(target==single?8:11)))|(uint64_t(destexp)<<destfrac),0};
                // Legacy MIPS: top payload bit 1 signals invalid; top bit 0 needs software.
                if(!(tail&(uint64_t(1)<<(fraction-1))))return assist();
                return {target==single?0x7fbfffffull:0x7ff7ffffffffffffull,16};
            }
            if(!exponent && tail)return assist();
            if(exponent) {magnitude=(uint64_t(1)<<fraction)|tail;scale=int(exponent)-int(bias)-int(fraction);}
        }
        if(target==word || target==longword) {
            Rounded rounded{};
            if(scale>=0) {if(scale>=64 || magnitude>(~uint64_t(0)>>scale))return assist();rounded.value=magnitude<<scale;}
            else rounded=round(magnitude,unsigned(-scale),rm,negative);
            const uint64_t limit=uint64_t(1)<<(target==word?31:53);
            if(target==word?(negative?rounded.value>limit:rounded.value>=limit):rounded.value>=limit)return assist();
            const auto result=negative?0-rounded.value:rounded.value;
            return {target==word?uint32_t(result):result,rounded.lost?1u:0u};
        }
        const unsigned fraction=target==single?23:52,bias=target==single?127:1023,maxexp=target==single?255:2047;
        const uint64_t sign=uint64_t(negative)<<(fraction+(target==single?8:11));
        if(!magnitude)return {sign,0};
        unsigned top=0;for(uint64_t copy=magnitude;copy>>1;copy>>=1)++top;
        int exponent=int(top)+scale;
        const int minimum=1-int(bias),maximum=int(bias);
        // Quantize at the minimum exponent first: rounding can rescue a tiny result.
        if(exponent<minimum) {
            const int shift=minimum-int(fraction)-scale;
            const auto tiny=shift>=0?round(magnitude,unsigned(shift),rm,negative):Rounded{magnitude<<unsigned(-shift),false};
            if(tiny.value>=(uint64_t(1)<<fraction))return {sign|(uint64_t(1)<<fraction),tiny.lost?1u:0u};
            if(!flush || (enables&3))return assist();
            const bool away=(rm==2 && !negative)||(rm==3 && negative);
            return {sign|(away?(uint64_t(1)<<fraction):0),3};
        }
        const int shift=int(top)-int(fraction);
        auto rounded=shift>=0?round(magnitude,unsigned(shift),rm,negative):Rounded{magnitude<<unsigned(-shift),false};
        if(rounded.value>=(uint64_t(1)<<(fraction+1))) {rounded.value>>=1;++exponent;}
        if(exponent>maximum) {
            const bool infinity=rm==0 || (rm==2 && !negative)||(rm==3 && negative);
            return {sign|(infinity?(uint64_t(maxexp)<<fraction):((uint64_t(maxexp)<<fraction)-1)),5};
        }
        return {sign|(uint64_t(exponent+int(bias))<<fraction)|(rounded.value&((uint64_t(1)<<fraction)-1)),rounded.lost?1u:0u};
    }
private:
    struct Rounded {uint64_t value=0;bool lost=false;};
    static Result assist() {return {0,32};}
    static Rounded round(uint64_t value,unsigned shift,unsigned rm,bool negative) {
        if(!shift)return {value,false};
        const uint64_t quotient=shift>=64?0:value>>shift;
        const uint64_t remainder=shift>=64?value:value&((uint64_t(1)<<shift)-1);
        if(!remainder)return {quotient,false};
        bool increment=(rm==2 && !negative)||(rm==3 && negative);
        if(rm==0 && shift<=64) {const uint64_t half=uint64_t(1)<<(shift-1);increment=remainder>half || (remainder==half && (quotient&1));}
        return {quotient+unsigned(increment),true};
    }
};
}
