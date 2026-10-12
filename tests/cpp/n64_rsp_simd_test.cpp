#include "rsp_vector.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace n64;
static void put(RspVector& v,unsigned reg,unsigned lane,uint16_t word){v.registers[reg][lane*2]=uint8_t(word>>8);v.registers[reg][lane*2+1]=uint8_t(word);}
int main(){try{
    uint32_t seed=0x9e3779b9;
    for(unsigned sample=0;sample<65536;++sample){
        std::array<uint8_t,16> bytes{};
        for(auto& byte:bytes){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;byte=uint8_t(seed);}
        const auto words=RspSimd::load_words(bytes);
        for(unsigned lane=0;lane<8;++lane)
            if(words[lane]!=(unsigned(bytes[2*lane])<<8|bytes[2*lane+1]))
                throw std::runtime_error("vector register byte-order load changed");
    }
    constexpr unsigned functions[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,19,20,21,29,32,33,34,35,36,37,38,39,40,41,42,43,44,45};
    constexpr uint64_t edges[]={0,1,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0x80000001,
        0xffffffff,0x100000000,0x7fffffffffff,0x800000000000,0xffffffff0000,0xffffffffffff,
        0xffff80000000,0xffff7fffffff,0xffff80000001,0x1ffff,0xffffffff7fff};
    RspVector accelerated,reference;reference.simd_enabled=false;
    std::array<uint32_t,32> sa{},sb{};std::array<uint8_t,8192> ma{},mb{};
    uint64_t cases=0;
    for(unsigned fn:functions)for(unsigned value=0;value<(RspSimd::available?65536u:64u);++value){
        accelerated.registers={};reference.registers={};
        const unsigned alias=(value>>4)&3,e=value&15,vs=1,vt=alias==3?1:2,vd=alias==0?3:alias==2?2:1;
        for(unsigned i=0;i<8;++i){
            const uint16_t left=uint16_t(value*(i*2+1)+i*7919),right=uint16_t(value*257+0x1357+i*4099);
            put(accelerated,vs,i,left);put(reference,vs,i,left);put(accelerated,vt,i,right);put(reference,vt,i,right);
            const uint64_t acc=(edges[((value>>4)+i)%std::size(edges)]+(value&31))&RspVector::acc_mask;
            accelerated.accumulator[i]=reference.accumulator[i]=acc;
        }
        accelerated.flags=reference.flags={uint16_t(value),uint16_t(value*7919+0x5a5a),uint16_t((value^(value>>8))&255)};
        const uint32_t ins=0x4a000000u|(e<<21)|(vt<<16)|(vs<<11)|(vd<<6)|fn;
        if(!accelerated.execute(ins,sa,ma) || !reference.execute(ins,sb,mb) ||
           accelerated.registers!=reference.registers || accelerated.accumulator!=reference.accumulator ||
           accelerated.flags!=reference.flags || accelerated.divider.snapshot()!=reference.divider.snapshot() || sa!=sb || ma!=mb){
            std::cerr<<"fn="<<fn<<" value="<<value<<" e="<<e<<" alias="<<alias<<'\n';
            throw std::runtime_error("SIMD/scalar vector continuation changed");
        }
        ++cases;
    }
    if(RspSimd::available){
        RspSimd::Words a{},b{},out{};std::array<uint64_t,8> acc{};std::array<uint16_t,3> flags{};
        for(unsigned fn:{0u,1u,4u,5u,6u,7u,8u,9u,12u,13u,14u,15u})
            if(!RspSimd::multiply(fn,a,b,acc,out))throw std::runtime_error("multiply SIMD family missing");
        for(unsigned fn:{16u,17u,19u,20u,21u,39u,40u,41u,42u,43u,44u,45u})
            if(!RspSimd::alu(fn,a,b,acc,flags,out))throw std::runtime_error("ALU SIMD family missing");
    }
    std::cout<<cases<<" vector/accumulator/flag/scalar/DMEM continuations; SSE2="<<RspSimd::available<<" PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
