// SPDX-License-Identifier: MIT
#include "arm.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
struct Bus final:nds::Memory {
    uint32_t instruction=0;unsigned reads=0,writes=0;
    bool read(uint32_t address,unsigned width,uint32_t& value) override {
        ++reads;if(address || width!=4)return false;value=instruction;return true;
    }
    bool write(uint32_t,unsigned,uint32_t) override {++writes;return false;}
};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int64_t integer(uint32_t n,unsigned bits=32){return n<(uint64_t(1)<<(bits-1)) ? int64_t(n):int64_t(n)-(int64_t(1)<<bits);}
struct Form {unsigned family,x,y;};
uint32_t encode(Form f,unsigned d,unsigned a,unsigned m,unsigned s,unsigned cond=14){
    const uint32_t base[]={0x01600080,0x01000080,0x01400080,0x012000a0,0x01200080};
    return (cond<<28)|base[f.family]|(d<<16)|(a<<12)|(s<<8)|m|(f.y<<6)|((f.family<3 ? f.x:0)<<5);
}
// Oracle works on a pre-operation register copy, with Euclidean division for
// word products and modulo arithmetic for the long sum. It reads no decoder.
nds::ArmState reference(nds::ArmState input,Form f,unsigned d,unsigned a,unsigned m,unsigned s){
    const int64_t left=f.family>=3 ? integer(input.r[m]):integer((input.r[m]>>(16*f.x))&65535,16);
    const int64_t right=integer((input.r[s]>>(16*f.y))&65535,16);
    int64_t answer=left*right;
    if(f.family>=3){const int64_t remainder=answer%65536;answer/=65536;if(remainder<0)--answer;}
    if(f.family==2){
        uint64_t sum=(uint64_t(input.r[d])<<32)|input.r[a];sum+=uint64_t(answer);
        input.r[a]=uint32_t(sum);input.r[d]=uint32_t(sum>>32);
    }else{
        if(f.family==1 || f.family==4){
            answer+=integer(input.r[a]);if(answer< -2147483648LL || answer>2147483647LL)input.cpsr|=0x08000000;
        }
        input.r[d]=uint32_t(answer);
    }
    input.r[15]=4;++input.steps;return input;
}
int main(){try{
    uint64_t checked=0;
    for(unsigned family=0;family<5;++family)for(unsigned x=0;x<(family<3 ? 2u:1u);++x)for(unsigned y=0;y<2;++y){
        Form form{family,x,y};
        const bool accumulator=family==1 || family==2 || family==4;
        for(unsigned d=0;d<16;++d)for(unsigned a=0;a<16;++a)for(unsigned m=0;m<16;++m)for(unsigned rs=0;rs<16;++rs){
            for(unsigned profile=0;profile<3;++profile){
                Bus bus;bus.instruction=encode(form,d,a,m,rs);nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0);
                for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x80007fff^(0x9e3779b9u*r);
                cpu.state.cpsr=0xdf|(((d*7+a*5+m*3+rs+family)&31)<<27);
                const auto before=cpu.state;
                const bool valid=profile && d<15 && m<15 && rs<15 &&
                    (accumulator ? a<15:a==0) && (family!=2 || d!=a);
                cpu.step();
                if(valid){const auto want=reference(before,form,d,a,m,rs);require(!cpu.state.unsupported && cpu.state.r==want.r && cpu.state.cpsr==want.cpsr,"DSP register/alias/version oracle mismatch");}
                else require(cpu.state.unsupported && cpu.state.r==before.r && cpu.state.cpsr==before.cpsr,"invalid DSP encoding changed registers or was accepted");
                require(bus.reads==1 && bus.writes==0,"DSP instruction made data transaction");++checked;
            }
        }
        // Every NZCV/Q assignment with full-width boundaries and both versions.
        constexpr std::array<uint32_t,8> values={0,1,0xffffffff,0x7fffffff,0x80000000,0x80007fff,0x7fff8000,0x5555aaaa};
        constexpr std::array<uint64_t,6> seeds={0,0x7fffffff,0x80000000,0xffffffffffffffffULL,0x7fffffffffffffffULL,0x8000000000000000ULL};
        for(auto m:values)for(auto rs:values)for(auto seed:seeds)for(unsigned flags=0;flags<32;++flags)for(unsigned profile=1;profile<3;++profile){
            Bus bus;const unsigned d=family==2 ? 5:4,a=accumulator ? (family==2 ? 4:2):0;
            bus.instruction=encode(form,d,a,0,1);nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0);
            cpu.state.r[0]=m;cpu.state.r[1]=rs;cpu.state.r[2]=uint32_t(seed);cpu.state.r[4]=uint32_t(seed);cpu.state.r[5]=uint32_t(seed>>32);cpu.state.cpsr=0xdf|(flags<<27);
            const auto want=reference(cpu.state,form,d,a,0,1);cpu.step();
            require(!cpu.state.unsupported && cpu.state.r==want.r && cpu.state.cpsr==want.cpsr,"DSP flag/rounding/long-wrap boundary mismatch");++checked;
        }
        Bus skipped;skipped.instruction=encode(form,15,15,15,15,0);nds::Arm seven(skipped,false);seven.reset(0);seven.step();
        require(!seven.state.unsupported && seven.state.r[15]==4,"failed condition executed unavailable/invalid DSP form");
    }
    std::cout<<"ARM DSP multiplication "<<checked<<" register-field/profile/boundary checks PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
