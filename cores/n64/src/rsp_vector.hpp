#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace n64 {
// Functional vector storage/transfers. Arithmetic and physical load interlocks are pending.
class RspVector {
public:
    std::array<std::array<uint8_t,16>,32> registers{};
    std::array<uint16_t,3> flags{}; // VCO, VCC, VCE (8 bits).
    using State=std::array<uint64_t,67>;
    void reset(){*this=RspVector{};}
    State snapshot() const{State s{};for(unsigned r=0;r<32;++r)for(unsigned h=0;h<2;++h)for(unsigned b=0;b<8;++b)s[r*2+h]=(s[r*2+h]<<8)|registers[r][h*8+b];for(unsigned i=0;i<3;++i)s[64+i]=flags[i];return s;}
    bool restore(const State& s){if(s[64]>65535 || s[65]>65535 || s[66]>255)return false;for(unsigned r=0;r<32;++r)for(unsigned b=0;b<16;++b)registers[r][b]=uint8_t(s[r*2+b/8]>>(56-8*(b%8)));for(unsigned i=0;i<3;++i)flags[i]=uint16_t(s[64+i]);return true;}
    bool execute(uint32_t ins,std::array<uint32_t,32>& scalar,std::array<uint8_t,8192>& memory){
        const unsigned op=ins>>26,base=(ins>>21)&31,vt=(ins>>16)&31,kind=(ins>>11)&31,e=(ins>>7)&15;
        auto sign16=[](unsigned v){return uint32_t(v)|((v&32768)?0xffff0000u:0u);};
        if(op==18){
            if(base==0 || base==4){if(ins&127)return false;auto& v=registers[kind];if(base==0){if(vt)scalar[vt]=sign16(unsigned(v[e])*256+v[(e+1)%16]);}else{v[e]=uint8_t(scalar[vt]>>8);if(e<15)v[e+1]=uint8_t(scalar[vt]);}return true;}
            if(base==2 || base==6){if(ins&2047)return false;const unsigned index=std::min(kind&3,2u);if(base==2){if(vt)scalar[vt]=sign16(flags[index]);}else{flags[index]=uint16_t(scalar[vt]&(index==2?255:65535));}return true;}
            return false;
        }
        if((op!=50 && op!=58) || kind>=12)return false;
        constexpr unsigned scale[]={1,2,4,8,16,16,8,8,16,16,16,16};
        const int offset=int(ins&127)-((ins&64)?128:0);
        const uint32_t a=scalar[base]+uint32_t(offset*int(scale[kind]));
        auto read=[&](uint32_t at){return memory[at&4095];};
        auto write=[&](uint32_t at,uint8_t value){memory[at&4095]=value;};
        auto& v=registers[vt];
        if(kind<=4){const unsigned count=kind==4?16-(a&15):1u<<kind;
            for(unsigned n=0;n<count;++n){if(op==50){if(e+n<16)v[e+n]=read(a+n);}else write(a+n,v[(e+n)&15]);}return true;}
        if(kind==5){const unsigned count=a&15;for(unsigned n=0;n<count;++n){const unsigned index=e+16-count+n;if(op==50){if(index<16)v[index]=read((a&~15u)+n);}else write((a&~15u)+n,v[index&15]);}return true;}
        if(op==50){
            if(kind==10)return true; // Hardware-observed LWV no-op.
            if(kind==11){const unsigned group=vt&24;for(unsigned n=0;n<8;++n){auto& dest=registers[group+((e/2+n)&7)];for(unsigned b=0;b<2;++b)dest[2*n+b]=read((a&~7u)+((unsigned(a&8)+e+2*n+b)&15));}return true;}
            if(kind<=8){for(unsigned n=0;n<8;++n){const unsigned index=(unsigned(a&7)+16-e+n*(kind==8?2:1))&15;const unsigned value=unsigned(read((a&~7u)+index))<<(kind==6?8:7);v[2*n]=uint8_t(value>>8);v[2*n+1]=uint8_t(value);}return true;}
            std::array<uint8_t,16> packed{};
            // LFV e=1 first packed low bit follows the hardware-test author's profile;
            // its non-linear-input physical behavior remains ungraded.
            constexpr unsigned order[]={0,4,8,12,8,12,0,4};
            for(unsigned n=0;n<8;++n){const unsigned index=(unsigned(a&7)+order[n]+(n?16-e:e))&15;const unsigned value=unsigned(read((a&~7u)+index))*128;packed[2*n]=uint8_t(value>>8);packed[2*n+1]=uint8_t(value);}
            for(unsigned n=e;n<std::min(e+8,16u);++n)v[n]=packed[n];
            return true;
        }
        if(kind==6 || kind==7){for(unsigned n=0;n<8;++n){const unsigned index=e+n;const unsigned lane=index&7;const unsigned value=unsigned(v[2*lane])*256+v[2*lane+1];const unsigned shift=((index&8)!=0)^(kind==7)?7:8;write(a+n,uint8_t(value>>shift));}return true;}
        if(kind==8){for(unsigned n=0;n<8;++n){const unsigned at=(e+2*n)&15,value=unsigned(v[at])*256+v[(at+1)&15];write((a&~7u)+((unsigned(a&7)+2*n)&15),uint8_t(value>>7));}return true;}
        if(kind==9){constexpr int first[]={0,6,-1,-1,1,7,-1,-1,4,-1,-1,3,5,-1,-1,0};const int lane=first[e];for(unsigned n=0;n<4;++n){unsigned value=0;if(lane>=0){const unsigned index=(unsigned(lane)&4)|((unsigned(lane)+n)&3);value=(unsigned(v[2*index])*256+v[2*index+1])/128;}write((a&~7u)+((unsigned(a&7)+4*n)&15),uint8_t(value));}return true;}
        if(kind==10){for(unsigned n=0;n<16;++n)write((a&~7u)+((unsigned(a&7)+n)&15),v[(e+n)&15]);return true;}
        const unsigned group=vt&24,element=e&14;
        for(unsigned n=0;n<8;++n)for(unsigned b=0;b<2;++b){const unsigned index=(16+2*n-element+b)&15;write((a&~7u)+((unsigned(a&7)+16+2*n-element+b)&15),registers[group+n][index]);}
        return true;
    }
};
}
