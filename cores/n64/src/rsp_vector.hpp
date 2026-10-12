#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include "rsp_compare.hpp"
#include "rsp_alu.hpp"
#include "rsp_divider.hpp"
#include "rsp_simd.hpp"
namespace n64 {
// Functional vector storage, transfers, multiply/accumulate, comparison and integer ALU execution.
// Physical load/pipeline interlocks and remaining arithmetic are pending.
class RspVector {
public:
    std::array<std::array<uint8_t,16>,32> registers{};
    std::array<uint64_t,8> accumulator{}; // Raw 48-bit lanes.
    using AccState=std::array<uint64_t,8>;
    static constexpr uint64_t acc_mask=0xffffffffffffull;
    RspDivider divider;
    bool simd_enabled=true; // Host-only; excluded from native save and rollback state.
    AccState acc_snapshot() const{return accumulator;}
    bool acc_restore(const AccState& a){for(auto v:a)if(v&~acc_mask)return false;accumulator=a;return true;}
    std::array<uint16_t,3> flags{}; // VCO, VCC, VCE (8 bits).
    using State=std::array<uint64_t,67>;
    void reset(){*this=RspVector{};}
    State snapshot() const{State s{};for(unsigned r=0;r<32;++r)for(unsigned h=0;h<2;++h)for(unsigned b=0;b<8;++b)s[r*2+h]=(s[r*2+h]<<8)|registers[r][h*8+b];for(unsigned i=0;i<3;++i)s[64+i]=flags[i];return s;}
    bool restore(const State& s){if(s[64]>65535 || s[65]>65535 || s[66]>255)return false;for(unsigned r=0;r<32;++r)for(unsigned b=0;b<16;++b)registers[r][b]=uint8_t(s[r*2+b/8]>>(56-8*(b%8)));for(unsigned i=0;i<3;++i)flags[i]=uint16_t(s[64+i]);return true;}
    bool execute(uint32_t ins,std::array<uint32_t,32>& scalar,std::array<uint8_t,8192>& memory){
        const unsigned op=ins>>26,base=(ins>>21)&31,vt=(ins>>16)&31,kind=(ins>>11)&31,e=(ins>>7)&15;
        auto sign16=[](unsigned v){return uint32_t(v)|((v&32768)?0xffff0000u:0u);};
        if(op==18){
            if(base>=16){const unsigned fn=ins&63;if(fn>=48 && fn<=54)return divide(ins);if(fn==55 || fn==63)return true;return RspAlu::supported(fn) || (fn>=32 && fn<=39)?lane_math(ins):multiply(ins);}
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
private:
    static int64_t signed16(unsigned v){return v<32768?int64_t(v):int64_t(v)-65536;}
    static int64_t signed48(uint64_t v){return v<(1ull<<47)?int64_t(v):int64_t(v)-int64_t(1ull<<48);}
    static int64_t floor_shift(int64_t v,unsigned bits){const int64_t divisor=int64_t(1ull<<bits);return v>=0?v/divisor:-((-v+divisor-1)/divisor);}
    static uint16_t clamp_signed(int64_t v){return v<-32768?0x8000:v>32767?0x7fff:uint16_t(v);}
    unsigned lane(unsigned reg,unsigned at) const{return unsigned(registers[reg][2*at])*256+registers[reg][2*at+1];}
    void set_lane(unsigned reg,unsigned at,uint16_t value){registers[reg][2*at]=uint8_t(value>>8);registers[reg][2*at+1]=uint8_t(value);}
    bool divide(uint32_t ins){
        const unsigned fn=ins&63,vt=(ins>>16)&31,de=(ins>>11)&7,vd=(ins>>6)&31,e=(ins>>21)&15;
        std::array<uint16_t,8> selected{};
        const uint16_t raw=uint16_t(lane(vt,e&7));
        for(unsigned i=0;i<8;++i){const unsigned source=e<2?i:e<4?(i&6)|(e&1):e<8?(i&4)|(e&3):e&7;selected[i]=uint16_t(lane(vt,source));}
        for(unsigned i=0;i<8;++i)accumulator[i]=(accumulator[i]&0xffffffff0000ull)|selected[i];
        set_lane(vd,de,fn==51?selected[de]:divider.execute(fn,raw));return true;
    }
    bool lane_math(uint32_t ins){
        const unsigned fn=ins&63,vt=(ins>>16)&31,vs=(ins>>11)&31,vd=(ins>>6)&31,e=(ins>>21)&15;
        RspSimd::Words left{},right{};
        for(unsigned i=0;i<8;++i){const unsigned selected=e<2?i:e<4?(i&6)|(e&1):e<8?(i&4)|(e&3):e&7;left[i]=lane(vs,i);right[i]=lane(vt,selected);}
        RspSimd::Words accelerated;
        if(simd_enabled && RspSimd::alu(fn,left,right,accumulator,flags,accelerated)){
            for(unsigned i=0;i<8;++i)set_lane(vd,i,accelerated[i]);
            return true;
        }
        std::array<uint16_t,3> next{};
        for(unsigned i=0;i<8;++i){
            const unsigned packed=((flags[0]>>i)&1)|(((flags[0]>>(i+8))&1)<<1)|(((flags[1]>>i)&1)<<2)|(((flags[1]>>(i+8))&1)<<3)|(((flags[2]>>i)&1)<<4);
            RspAlu::Result result{};
            if(RspAlu::supported(fn))result=RspAlu::evaluate(fn,uint16_t(left[i]),uint16_t(right[i]),packed);
            else{const auto comparison=RspCompare::evaluate(fn,uint16_t(left[i]),uint16_t(right[i]),packed);result={comparison.word,comparison.word,comparison.flags};}
            set_lane(vd,i,result.word);accumulator[i]=(accumulator[i]&0xffffffff0000ull)|result.low;
            next[0]|=uint16_t(((result.flags&1)|((result.flags&2)<<7))<<i);
            next[1]|=uint16_t((((result.flags>>2)&1)|((result.flags&8)<<5))<<i);
            next[2]|=uint16_t(((result.flags>>4)&1)<<i);
        }
        flags=next;return true;
    }
    bool multiply(uint32_t ins){
        const unsigned fn=ins&63,vt=(ins>>16)&31,vs=(ins>>11)&31,vd=(ins>>6)&31,e=(ins>>21)&15;
        if(fn==29){for(unsigned i=0;i<8;++i)set_lane(vd,i,e>=8 && e<=10?uint16_t(accumulator[i]>>(16*(10-e))):0);return true;}
        if(fn>=16)return false;
        RspSimd::Words left{},right{};
        for(unsigned i=0;i<8;++i){const unsigned selected=e<2?i:e<4?(i&6)|(e&1):e<8?(i&4)|(e&3):e&7;left[i]=lane(vs,i);right[i]=lane(vt,selected);}
        RspSimd::Words accelerated;
        if(simd_enabled && RspSimd::multiply(fn,left,right,accumulator,accelerated)){
            for(unsigned i=0;i<8;++i)set_lane(vd,i,accelerated[i]);
            return true;
        }
        for(unsigned i=0;i<8;++i){const int64_t a=signed16(left[i]),b=signed16(right[i]),old=signed48(accumulator[i]);int64_t value=0;
            switch(fn){
            case 0:case 1:value=a*b*2+32768;break;
            case 2:case 10:value=old;if((fn==2 && old>=0) || (fn==10 && old<0))value+=b*((vs&1)?65536:1);break;
            case 3:value=a*b;if(value<0)value+=31;value*=65536;break;
            case 4:value=int64_t(uint64_t(left[i])*right[i]/65536);break;
            // M uses signed VS × unsigned VT; N uses unsigned VS × signed VT.
            case 5:value=a*int64_t(right[i]);break;
            case 6:value=int64_t(left[i])*b;break;
            case 7:value=a*b*65536;break;
            case 8:case 9:value=old+a*b*2;break;
            case 11:value=old;if(!(accumulator[i]&0x200000)){if(old<0)value+=0x200000;else if(old>=0x400000)value-=0x200000;}break;
            case 12:value=old+int64_t(uint64_t(left[i])*right[i]/65536);break;
            case 13:value=old+a*int64_t(right[i]);break;
            case 14:value=old+int64_t(left[i])*b;break;
            case 15:value=old+a*b*65536;break;
            }
            accumulator[i]=uint64_t(value)&acc_mask;const int64_t wrapped=signed48(accumulator[i]);uint16_t result=0;
            if(fn==3 || fn==11)result=clamp_signed(floor_shift(wrapped,17))&0xfff0;
            else if(fn==1 || fn==9)result=wrapped<0?0:wrapped>=0x80000000ll?0xffff:uint16_t(accumulator[i]>>16);
            else if(fn==4 || fn==6)result=uint16_t(accumulator[i]);
            else if(fn==12 || fn==14)result=wrapped<-0x80000000ll?0:wrapped>=0x80000000ll?0xffff:uint16_t(accumulator[i]);
            else result=clamp_signed(floor_shift(wrapped,16));
            set_lane(vd,i,result);
        }
        return true;
    }
};
}
