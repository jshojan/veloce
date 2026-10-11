// SPDX-License-Identifier: MIT
#include "arm.hpp"
#include <iostream>
#include <stdexcept>
struct Bus final:nds::Memory {
    uint32_t instruction=0,begin=0,words[16]{};unsigned reads=0,writes=0,fail=17;
    bool read(uint32_t address,unsigned width,uint32_t& value)override {
        if(reads++==0){if(address!=0x80 || width!=4)return false;value=instruction;return true;}
        unsigned i=(address-begin)/4;if(width!=4 || ((address-begin)&3) || i>=16 || i==fail)return false;value=words[i];return true;
    }
    bool write(uint32_t address,unsigned width,uint32_t value)override {
        ++writes;unsigned i=(address-begin)/4;if(width!=4 || ((address-begin)&3) || i>=16 || i==fail)return false;words[i]=value;return true;
    }
};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
uint32_t instruction(unsigned mask,unsigned rn,unsigned mode,bool wb,bool load,bool user=false){return 0xe8000000|((mode&1)<<24)|(((mode>>1)&1)<<23)|(uint32_t(user)<<22)|(uint32_t(wb)<<21)|(uint32_t(load)<<20)|(rn<<16)|mask;}
int main(){try{
    uint64_t checked=0;
    for(unsigned profile=0;profile<3;++profile)for(unsigned load=0;load<2;++load)for(unsigned mode=0;mode<4;++mode)for(unsigned wb=0;wb<2;++wb)
    for(unsigned rn=0;rn<16;++rn)for(unsigned mask=0;mask<65536;++mask){
        Bus bus;bus.instruction=instruction(mask,rn,mode,wb,load);nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0x80);
        for(unsigned reg=0;reg<15;++reg)cpu.state.r[reg]=0x01020304u*(reg+1);if(rn<15)cpu.state.r[rn]=0x1000+(mask&3);
        cpu.state.cpsr=0xdf|(((mask+rn+mode+wb+load)&(profile ? 31:15))<<(profile ? 27:28));const auto before=cpu.state;
        unsigned list[16],count=0;for(unsigned reg=0;reg<16;++reg)if(mask&(1<<reg))list[count++]=reg;
        const uint32_t base=before.r[rn],span=count*4;
        const uint32_t start=(mode&2) ? base+((mode&1) ? 4:0):base-span+((mode&1) ? 0:4);
        bus.begin=start&~3u;for(unsigned i=0;i<16;++i)bus.words[i]=0xa5a50000|i;
        if(mask&0x8000)bus.words[count-1]=0x12345679; // Thumb on v5+, ARM on v4.
        const bool valid=rn<15 && mask && (!wb || !(mask&(1<<rn)) || (!load && !(mask&((1<<rn)-1))));
        auto want=before.r;uint32_t expected_psw=before.cpsr;
        if(valid){
            if(load)for(unsigned i=0;i<count;++i)want[list[i]]=bus.words[i];
            if(wb)want[rn]=(mode&2) ? base+span:base-span;
            if(load && (mask&0x8000)){want[15]=0x12345678;if(profile)expected_psw|=0x20;}
            else want[15]=0x84;
        }
        cpu.step();require(cpu.state.unsupported==!valid && cpu.state.r==want && cpu.state.cpsr==expected_psw,"ARM multiple list/base/order/flags/profile mismatch");
        require(bus.reads==(valid && load ? count+1:1) && bus.writes==(valid && !load ? count:0),"ARM multiple transaction count mismatch");
        if(valid && !load)for(unsigned i=0;i<count;++i)require(bus.words[i]==(list[i]==15 ? 0x80+(profile ? 8:12):before.r[list[i]]),"ARM multiple stored wrong value/order/base");
        ++checked;
    }
    // Pin all flag assignments, address wrap lanes, PC target alignments and
    // user/exception-return diagnostic rejection independently of the sweep.
    const uint32_t bases[]={0,1,2,3,0x1000,0xfffffffc,0xffffffff};
    const unsigned lists[]={1,3,8,0x8000,0x8001,0xffff};
    const uint32_t targets[]={0x12345678,0x12345679,0x1234567a,0x1234567b};
    for(unsigned profile=0;profile<3;++profile)for(unsigned flags=0;flags<(profile ? 32:16);++flags)for(unsigned mode=0;mode<4;++mode)
    for(uint32_t base:bases)for(unsigned mask:lists)for(uint32_t target:targets){
        Bus bus;bus.instruction=instruction(mask,3,mode,false,true);nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0x80);cpu.state.r[3]=base;cpu.state.cpsr=0xdf|(flags<<(profile ? 27:28));
        const auto before=cpu.state;unsigned count=0;for(unsigned reg=0;reg<16;++reg)count+=(mask>>reg)&1;
        const uint32_t start=(mode&2) ? base+((mode&1) ? 4:0):base-4*count+((mode&1) ? 0:4);bus.begin=start&~3u;
        for(unsigned i=0;i<count;++i)bus.words[i]=0xabc00000+i;if(mask&0x8000)bus.words[count-1]=target;
        const bool valid=!profile || !(mask&0x8000) || (target&3)!=2;auto want=before.r;uint32_t psw=before.cpsr;
        if(valid){unsigned index=0;for(unsigned reg=0;reg<16;++reg)if(mask&(1<<reg))want[reg]=bus.words[index++];
            if(mask&0x8000){want[15]=target&((profile && (target&1)) ? ~1u:~3u);if(profile)psw|=(target&1)<<5;}else want[15]=0x84;}
        cpu.step();require(cpu.state.unsupported==!valid && cpu.state.r==want && cpu.state.cpsr==psw,"ARM multiple all-flags/wrap/PC target mismatch");++checked;
    }
    for(unsigned profile=0;profile<3;++profile)for(unsigned load=0;load<2;++load)for(unsigned mode=0;mode<4;++mode)for(unsigned wb=0;wb<2;++wb){
        Bus bus;bus.instruction=instruction(0xffff,3,mode,wb,load,true);nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0x80);const auto before=cpu.state;cpu.step();
        require(cpu.state.unsupported && cpu.state.r==before.r && cpu.state.cpsr==before.cpsr && bus.reads==1 && bus.writes==0,"missing banked/SPSR form was fabricated");++checked;
        Bus skipped;skipped.instruction=instruction(0,15,mode,wb,load,true)&0x0fffffff;nds::Arm skip(skipped,profile==1,profile==2);skip.reset(0x80);skip.step();
        require(!skip.state.unsupported && skip.state.r[15]==0x84 && skipped.reads==1 && skipped.writes==0,"failed condition executed invalid ARM block transfer");++checked;
    }
    for(unsigned load=0;load<2;++load)for(unsigned fail=0;fail<16;++fail){
        Bus bus;bus.instruction=instruction(0xfff7,3,2,true,load);bus.begin=0x1000;bus.fail=fail;nds::Arm cpu(bus,true);cpu.reset(0x80);cpu.state.r[3]=0x1000;
        for(unsigned r=0;r<15;++r)if(r!=3)cpu.state.r[r]=0xdead0000+r;for(unsigned i=0;i<16;++i)bus.words[i]=0x12340000+i;
        const auto before=cpu.state;cpu.step();const bool failed=fail<15;
        if(failed || load)require(cpu.state.unsupported && cpu.state.r==before.r && cpu.state.cpsr==before.cpsr,"failed ARM multiple committed register/writeback prefix");
        if(!failed && !load)require(!cpu.state.unsupported && cpu.state.r[3]==0x103c && cpu.state.r[15]==0x84,"successful ARM multiple did not complete writeback");
        if(!load)for(unsigned i=0;i<15;++i){const unsigned r=i<3 ? i:i+1;const uint32_t expected=i<fail ? (r==15 ? 0x88:before.r[r]):0x12340000+i;require(bus.words[i]==expected,"failed ARM multiple store prefix lost/misordered");}++checked;
    }
    std::cout<<"ARM multiple "<<checked<<" list/base/profile/flags/wrap/failure checks PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
