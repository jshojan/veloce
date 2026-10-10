#pragma once
#include <cstdint>
namespace n64 {
// Functional integer ALU: signed saturation differs from accumulator LOW16.
// Flags use VCO low/high, VCC low/high, VCE; physical interlocks are pending.
struct RspAlu {
    struct Result {uint16_t word,low;unsigned flags;};
    static bool supported(unsigned fn){return fn==16 || fn==17 || fn==19 || fn==20 || fn==21 || (fn>=40 && fn<=45);}
    static Result evaluate(unsigned fn,uint16_t left,uint16_t right,unsigned old){
        const int a=left<32768?int(left):int(left)-65536;
        const int b=right<32768?int(right):int(right)-65536;
        int value=0;unsigned flags=old;bool saturate=false;
        switch(fn){
        case 16:value=a+b+int(old&1);flags&=~3u;saturate=true;break;
        case 17:value=a-b-int(old&1);flags&=~3u;saturate=true;break;
        case 19:value=a<0?-b:a>0?b:0;saturate=a<0;break;
        case 20:value=int(left)+int(right);flags=(old&~3u)|unsigned(value>65535);break;
        case 21:value=int(left)-int(right);flags=(old&~3u)|unsigned(value<0)|(unsigned(value!=0)<<1);break;
        case 40:value=left&right;break;
        case 41:value=~(left&right);break;
        case 42:value=left|right;break;
        case 43:value=~(left|right);break;
        case 44:value=left^right;break;
        case 45:value=~(left^right);break;
        }
        const uint16_t low=uint16_t(value);
        const uint16_t word=saturate?(value<-32768?uint16_t(0x8000):value>32767?uint16_t(0x7fff):low):low;
        return {word,low,flags};
    }
};
}
