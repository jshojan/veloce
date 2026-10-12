#include "rdp_rgba16_simd.hpp"
#include <cstdlib>
#include <iostream>
int main(){
    // Every packed color in every corner; channel expansion is a fixed
    // hardware bit replication table, independent of the SIMD shifts.
    constexpr int expansion[]={0,8,16,24,33,41,49,57,66,74,82,90,99,107,115,123,
        132,140,148,156,165,173,181,189,198,206,214,222,231,239,247,255};
    for(unsigned v=0;v<65536;++v){
        const std::array<uint16_t,4> words={uint16_t(v),uint16_t(v^65535),
            uint16_t(v*257+0x1357),uint16_t(v*7919+0x5a5a)};
        n64::RdpRgba16Simd::Quad out{};
        const bool decoded=n64::RdpRgba16Simd::decode(words,out);
        if(decoded!=n64::RdpRgba16Simd::available)return 1;
        if(!decoded)continue;
        for(unsigned corner=0;corner<4;++corner){
            const unsigned w=words[corner];
            const std::array<int32_t,4> expected={expansion[(w/2048)%32],
                expansion[(w/64)%32],expansion[(w/2)%32],w%2?255:0};
            if(out[corner]!=expected){std::cerr<<"RGBA16 decode mismatch\n";return 1;}
        }
    }
    std::cout<<"262144 packed colors checked; SIMD="<<n64::RdpRgba16Simd::available<<'\n';
}
