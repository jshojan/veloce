#include "dp_interface.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
static void check(bool ok){if(!ok){std::cerr<<"two-cycle RDP mismatch\n";std::exit(1);}}

int main(){
    n64::RdpCombiner combine;
    // Cycle zero multiplies TEXEL0 by SHADE. Cycle one returns COMBINED.
    combine.combine=(1ull<<52)|(15ull<<28)|(4ull<<47)|(7ull<<15)|
        (7ull<<44)|(7ull<<12)|(7ull<<41)|(4ull<<9)|
        (15ull<<37)|(15ull<<24)|(31ull<<32)|
        (7ull<<21)|(7ull<<3)|(7ull<<18);
    check(combine.independent_two_cycle(true) && !combine.independent_two_cycle(false));
    unsigned values=0;
    for(unsigned texel=0;texel<256;++texel)for(unsigned shade=0;shade<256;++shade){
        n64::RdpCombiner::Sources inputs;
        inputs.texel0={texel,texel,texel,255};
        inputs.shade={shade,shade,shade,shade};
        const auto first=combine.evaluate(0,inputs);
        inputs.combined=first.combined;
        const auto second=combine.evaluate(1,inputs);
        const unsigned expected=(texel*shade+128)/256;
        check(first.clamped[0]==expected && second.clamped[0]==expected &&
              second.clamped[3]==shade);
        ++values;
    }
    {auto bad=combine;bad.combine|=2ull<<6;check(!bad.independent_two_cycle(true));}
    {auto bad=combine;bad.combine=(bad.combine&~(7ull<<15));check(!bad.independent_two_cycle(true));}
    {auto bad=combine;bad.combine=(bad.combine&~(31ull<<32))|(13ull<<32);check(!bad.independent_two_cycle(true));}
    {auto bad=combine;bad.combine=(bad.combine&~(7ull<<18));check(!bad.independent_two_cycle(true));}
    {auto bad=combine;bad.combine=(bad.combine&~(7ull<<6))|(1ull<<6);check(!bad.independent_two_cycle(true));}

    // BL0 fogs the combiner pixel with stepped shade alpha. With blending
    // disabled, BL1's source zero must still receive that fogged pixel.
    constexpr uint64_t mux=(3ull<<30)|(2ull<<26)|(1ull<<20)|(1ull<<16);
    unsigned fog_cases=0;
    for(unsigned shade=0;shade<256;++shade)for(unsigned pixel=0;pixel<256;++pixel){
        n64::RdpBlender blender;blender.fog=0x604020ff;
        n64::RdpBlender::Color color={pixel,pixel,pixel,255};
        const auto result=blender.pixel_two_cycle(mux,color,shade,0x123456e0,8,1,1);
        const unsigned a=shade/8,b=(255-shade)/8;
        const unsigned expected=(0x60*a+pixel*(b+1))/32;
        check(result.write && result.rgb[0]==expected);
        ++fog_cases;
    }
    check(n64::RdpBlender::fog_two_cycle_supported(mux));
    check(!n64::RdpBlender::fog_two_cycle_supported(mux^(1ull<<30)));
    // An original synthetic native packet exercises actual DP dispatch and
    // framebuffer writes: textured, shaded, depth-gradient triangle in the
    // supported two-cycle fog profile. Depth compare is disabled here.
    n64::RdpPacket packet;packet.count=packet.needed=22;
    packet.words[0]=0xcf80001000080000ull;
    packet.words[1]=0x00020000ffff0000ull;
    packet.words[2]=0;packet.words[3]=65536;
    packet.words[4]=0x0080008000800080ull;
    n64::DpInterface dp;dp.combiner=combine;dp.blender.fog=0xa0a0a0ff;
    dp.texture.tiles[0].descriptor=(3ull<<51)|(1ull<<41);
    dp.texture.tiles[0].bounds=(4ull<<12)|4;
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){
        const unsigned at=((y*4+x)^((y&1)?2:0))*2;
        dp.tmem.bytes[at]=64;dp.tmem.bytes[at+1]=64;
        dp.tmem.bytes[2048+at]=64;dp.tmem.bytes[2049+at]=128;
    }
    dp.color_image=(3ull<<51)|(3ull<<32)|64;
    dp.other_mode=(1ull<<52)|(15ull<<36)|(3ull<<42)|mux;
    dp.scissor=(16ull<<12)|16;
    std::vector<uint8_t> ram(512,0x5a);
    for(unsigned n=0;n<22;++n)for(unsigned lane=0;lane<8;++lane)
        ram[0x100+n*8+lane]=uint8_t(packet.words[n]>>(56-lane*8));
    dp.write(0,0x100);dp.write(1,0x1b0);
    std::array<uint8_t,8192> sp{};bool interrupt=false;
    for(unsigned n=0;n<22;++n)dp.tick(ram,sp,interrupt);
    check(!dp.blocked && dp.current==0x1b0 && !interrupt);
    for(unsigned position:{4u,8u,9u,12u}){
        const unsigned at=64+position*4;
        check(ram[at]==96 && ram[at+1]==96 && ram[at+2]==96);
    }
    std::cout<<values<<" combiner feedback values and "<<fog_cases
             <<" fogged pixels; synthetic DP framebuffer PASS\n";
}
