#pragma once
#include "hidden_rdram.hpp"
#include <array>
namespace n64 {
// RDP memory color uses zero-filled low RGB bits for RGBA16, unlike the VI's
// display expansion. Coverage's upper bit is in RAM; two lower bits are hidden.
struct RdpFramebuffer {
    static void rejected(size_t at,unsigned bytes,const std::array<unsigned,3>& rgb,unsigned coverage,HiddenRdram* hidden) {
        if(!hidden)return;
        if(bytes==2)hidden->rejected_halfword(at,coverage&3);
        else hidden->rejected_word(at,(rgb[1]&1)*3,0);
    }
    static uint32_t read16(const uint8_t* ram,unsigned hidden) {
        const unsigned word=(unsigned(ram[0])<<8)|ram[1];
        return ((word>>8)&248)<<24 | ((word>>3)&248)<<16 | ((word<<2)&248)<<8 |
               ((((word&1)<<2)|hidden)<<5);
    }
    static void write(size_t at,unsigned bytes,const std::array<unsigned,3>& rgb,unsigned coverage,
                      std::vector<uint8_t>& ram,HiddenRdram* hidden) {
        if(bytes==2){const unsigned word=((rgb[0]>>3)<<11)|((rgb[1]>>3)<<6)|((rgb[2]>>3)<<1)|(coverage>>2);
            ram[at]=uint8_t(word>>8);ram[at+1]=uint8_t(word);hidden->rdp_halfword(at,coverage&3,true);
        }else{for(unsigned lane=0;lane<3;++lane)ram[at+lane]=uint8_t(rgb[lane]);ram[at+3]=uint8_t(coverage<<5);
            if(hidden)hidden->rdp_word(at,(rgb[1]&1)*3,0);
        }
    }
};
}
