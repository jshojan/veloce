#pragma once
#include <cstdint>
namespace n64 {
// Functional lane comparisons. Flag bits: VCO low/high, VCC low/high, VCE.
// Upper accumulator slices and physical pipeline timing belong to the caller.
struct RspCompare {
    struct Result {uint16_t word;unsigned flags;};
    static Result evaluate(unsigned fn,uint16_t left,uint16_t right,unsigned old){
        const int a=left<32768?int(left):int(left)-65536;
        const int b=right<32768?int(right):int(right)-65536;
        bool c0=old&1,c1=old&2,q0=old&4,q1=old&8,ce=old&16;
        uint16_t word=left;
        switch(fn){
        case 32:q0=a<b || (left==right && c0 && c1);q1=false;word=q0?left:right;c0=c1=false;break;
        case 33:q0=left==right && !c1;q1=false;word=right;c0=c1=false;break;
        case 34:q0=left!=right || c1;q1=false;word=left;c0=c1=false;break;
        case 35:q0=a>b || (left==right && !(c0 && c1));q1=false;word=q0?left:right;c0=c1=false;break;
        case 36:
            if(c0){const unsigned sum=unsigned(left)+right;const bool zero=uint16_t(sum)==0,carry=sum>65535;
                if(!c1)q0=(zero && !carry) || (ce && (zero || !carry));
                word=q0?uint16_t(-int(right)):left;
            }else{if(!c1)q1=left>=right;word=q1?right:left;}
            c0=c1=ce=false;break;
        case 37:
            c0=((left^right)&32768)!=0;
            if(c0){const int sum=a+b;q1=b<0;q0=sum<=0;ce=sum==-1;c1=sum!=0 && sum!=-1;word=q0?uint16_t(-b):left;}
            else{const int diff=a-b;q0=b<0;q1=diff>=0;ce=false;c1=diff!=0;word=q1?right:left;}
            break;
        case 38:
            if((left^right)&32768){q1=b<0;q0=a+b<0;word=q0?uint16_t(~right):left;}
            else{q0=b<0;q1=a>=b;word=q1?right:left;}
            c0=c1=ce=false;break;
        case 39:word=q0?left:right;c0=c1=false;break;
        }
        return {word,unsigned(c0)|(unsigned(c1)<<1)|(unsigned(q0)<<2)|(unsigned(q1)<<3)|(unsigned(ce)<<4)};
    }
};
}
