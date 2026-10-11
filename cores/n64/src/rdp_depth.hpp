#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include "hidden_rdram.hpp"
namespace n64 {
// Stateless native depth encoding/decision component. Production triangle
// dispatch does not yet use it; primitive depth and blender pipeline wiring
// remain separate work. Physical accesses/timing are owned by the caller.
struct RdpDepth {
    static uint16_t compress(unsigned z) {
        z&=0x3ffff;
        if(z<0x20000)return uint16_t((z>>4)&0x1ffc);
        if(z<0x30000)return uint16_t(0x2000|((z>>3)&0x1ffc));
        if(z<0x38000)return uint16_t(0x4000|((z>>2)&0x1ffc));
        if(z<0x3c000)return uint16_t(0x6000|((z>>1)&0x1ffc));
        if(z<0x3e000)return uint16_t(0x8000|(z&0x1ffc));
        if(z<0x3f000)return uint16_t(0xa000|((z<<1)&0x1ffc));
        if(z<0x3f800)return uint16_t(0xc000|((z<<2)&0x1ffc));
        return uint16_t(0xe000|((z<<2)&0x1ffc));
    }
    static unsigned decompress(uint16_t word) {
        constexpr std::array<unsigned,8> shifts={6,5,4,3,2,1,0,0};
        constexpr std::array<unsigned,8> bases={0,0x20000,0x30000,0x38000,0x3c000,0x3e000,0x3f000,0x3f800};
        const unsigned exponent=word>>13,mantissa=(word>>2)&2047;
        return ((mantissa<<shifts[exponent])+bases[exponent])&0x3ffff;
    }
    // Native encoding ORs all set-bit indices, rather than selecting the MSB.
    static unsigned delta_encoding(uint16_t value) {
        unsigned code=0;for(unsigned n=0;n<16;++n)if(value&(1u<<n))code|=n;return code;
    }
    static unsigned highest(uint16_t value) {
        if(!value)return 0;
        unsigned result=32768;while(!(result&value))result>>=1;return result;
    }
    struct Result {
        bool pass=false,blend=false,overflow=false;
        unsigned coverage=0,shift_a=0,shift_b=0,stored_delta=15;
        bool operator==(const Result& r)const{return pass==r.pass && blend==r.blend && overflow==r.overflow && coverage==r.coverage && shift_a==r.shift_a && shift_b==r.shift_b && stored_delta==r.stored_delta;}
    };
    static Result compare(bool enabled,unsigned kind,bool aa,bool force,unsigned z,uint16_t delta,
                          uint16_t word,unsigned hidden,unsigned coverage,unsigned memory_coverage) {
        Result out;out.coverage=coverage;out.overflow=(memory_coverage+coverage)&8;
        const unsigned encoded=delta_encoding(delta);z&=0x3ffff;
        if(!enabled){out.pass=true;out.blend=force || (!out.overflow && aa);out.shift_b=encoded<11?4:15-encoded;return out;}
        const unsigned previous=decompress(word);out.stored_delta=((word&3)<<2)|(hidden&3);
        out.shift_a=unsigned(std::clamp(int(encoded)-int(out.stored_delta),0,4));
        out.shift_b=unsigned(std::clamp(int(out.stored_delta)-int(encoded),0,4));
        unsigned memory_delta=1u<<out.stored_delta;bool coplanar=false;
        const unsigned precision=word>>13;
        if(precision<3){
            if(memory_delta==32768){coplanar=true;memory_delta=65535;}
            else memory_delta=std::max(memory_delta*2,16u>>precision);
        }
        const unsigned dz=highest(uint16_t(delta|memory_delta)),tolerance=dz*8;
        const bool farther=coplanar || z+tolerance>=previous;
        const bool nearer=coplanar || int64_t(z)-tolerance<=previous;
        const bool infront=z<previous,maximum=previous==0x3ffff;
        out.blend=force || (!out.overflow && aa && farther);
        switch(kind&3){
        case 0:out.pass=maximum || (out.overflow?infront:nearer);break;
        case 1:
            if(infront && farther && out.overflow){const unsigned shift=delta_encoding(uint16_t(dz));
                const unsigned coefficient=((previous>>shift)-(z>>shift))&15;
                out.coverage=((coefficient*coverage)>>3)&15;out.pass=true;
            }else out.pass=maximum || (out.overflow?infront:nearer);
            break;
        case 2:out.pass=infront || maximum;break;
        case 3:out.pass=farther && nearer && !maximum;break;
        }
        return out;
    }
    static bool store(size_t address,unsigned z,unsigned encoded,std::vector<uint8_t>& ram,HiddenRdram& hidden) {
        if(encoded>15 || (address&1) || address>ram.size() || ram.size()-address<2 || hidden.size()!=ram.size())return false;
        const unsigned word=compress(z)|(encoded>>2);
        ram[address]=uint8_t(word>>8);ram[address+1]=uint8_t(word);
        hidden.rdp_halfword(address,encoded&3,false);return true;
    }
};
}
