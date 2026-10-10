#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// Shared reciprocal/square-root input/output latches. Integer table entries
// satisfy quantized rational/root inequalities; no host floating point is used.
struct RspDivider {
    uint16_t input=0;
    uint32_t output=0;
    bool pending=false;
    uint64_t snapshot() const{return (uint64_t(input)<<48)|(uint64_t(output)<<16)|unsigned(pending);}
    bool restore(uint64_t raw){if(raw&0xfffe)return false;input=uint16_t(raw>>48);output=uint32_t(raw>>16);pending=raw&1;return true;}
    static uint16_t table(bool root,unsigned index){
        static const auto tables=[](){std::array<std::array<uint16_t,512>,2> t{};
            for(unsigned i=0;i<512;++i){t[0][i]=i?uint16_t((( (1ull<<34)/(512+i)+1)>>8)):0xffff;
                const uint64_t denominator=i<256?256+i:512+2*(i-256);
                unsigned lo=0,hi=262144;
                while(lo<hi){const unsigned mid=(lo+hi+1)/2;if(denominator*mid*mid<(1ull<<44))lo=mid;else hi=mid-1;}
                t[1][i]=uint16_t(lo/2);
            }return t;
        }();return tables[root][index];
    }
    static uint32_t calculate(bool root,uint32_t raw){
        if(!raw)return 0x7fffffff;
        if(raw==0xffff8000)return 0xffff0000;
        const uint32_t adjusted=raw>0xffff8000?raw-1:raw;
        const bool negative=adjusted&0x80000000;
        const uint32_t magnitude=negative?~adjusted:adjusted;
        unsigned leading=0;for(uint32_t mask=0x80000000;!(magnitude&mask);mask>>=1)++leading;
        const unsigned shift=leading+1;
        const uint32_t normalized=uint32_t(uint64_t(magnitude)<<shift);
        const unsigned index=root?(normalized>>24)|((shift&1)<<8):normalized>>23;
        const uint32_t result=(0x40000000u|(uint32_t(table(root,index))<<14))>>(root?(32-shift)/2:32-shift);
        return negative?~result:result;
    }
    uint16_t execute(unsigned fn,uint16_t value){
        if(fn==50 || fn==54){input=value;pending=true;return uint16_t(output>>16);}
        const uint32_t raw=(fn==49 || fn==53) && pending?(uint32_t(input)<<16)|value:(value&0x8000?0xffff0000u:0u)|value;
        output=calculate(fn>=52,raw);pending=false;return uint16_t(output);
    }
};
}
