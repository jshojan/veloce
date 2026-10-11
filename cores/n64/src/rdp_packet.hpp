#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// Complete native command framing. One fetched 64-bit word per nominal issue
// quantum; no claim about physical FIFO depth, prefetch or rasterizer timing.
struct RdpPacket {
    static constexpr unsigned capacity=22;
    uint32_t start=0,count=0,needed=0;
    std::array<uint64_t,capacity> words{};
    static unsigned length(unsigned op){
        op&=63;
        if(op>=8 && op<=15)return 4+((op&4)?8:0)+((op&2)?8:0)+((op&1)?2:0);
        return op==36 || op==37?2:1;
    }
    bool ready() const{return count && count==needed;}
    void clear(){*this=RdpPacket{};}
    void push(uint32_t address,uint64_t word){
        if(!count){start=address;needed=length(unsigned(word>>56));}
        words[count++]=word;
    }
    using State=std::array<uint64_t,25>;
    State snapshot() const{State s{};s[0]=start;s[1]=count;s[2]=needed;for(unsigned i=0;i<capacity;++i)s[i+3]=words[i];return s;}
    bool restore(const State& s){
        if((s[0]&~0xfffff8ull) || s[1]>capacity || s[2]>capacity || s[1]>s[2])return false;
        if(!s[1]){for(auto word:s)if(word)return false;}
        else if(s[2]!=length(unsigned(s[3]>>56)))return false;
        for(unsigned i=unsigned(s[1]);i<capacity;++i)if(s[i+3])return false;
        start=uint32_t(s[0]);count=uint32_t(s[1]);needed=uint32_t(s[2]);for(unsigned i=0;i<capacity;++i)words[i]=s[i+3];return true;
    }
};
}
