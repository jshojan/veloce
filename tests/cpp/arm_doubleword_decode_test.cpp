// SPDX-License-Identifier: MIT
#include "arm.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
struct Bus final:nds::Memory {
    uint32_t instruction=0,address=0,low=0x89abcdef,high=0x76543210;
    unsigned fail=0;std::vector<uint32_t> reads,writes;
    bool read(uint32_t at,unsigned width,uint32_t& value) override {
        reads.push_back(at);if(reads.size()==1 && at==0x80 && width==4){value=instruction;return true;}
        if(width!=4 || (at!=address && at!=address+4) || (fail==1 && at==address) || (fail==2 && at==address+4))return false;
        value=at==address ? low:high;return true;
    }
    bool write(uint32_t at,unsigned width,uint32_t value) override {
        writes.push_back(at);if(width!=4 || (at!=address && at!=address+4) || (fail==1 && at==address) || (fail==2 && at==address+4))return false;
        (at==address ? low:high)=value;return true;
    }
};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
uint32_t encode(bool store,bool immediate,bool pre,bool wb,bool up,unsigned rd,unsigned rn,unsigned rm,unsigned offset=120){
    return 0xe00000d0|(uint32_t(store)<<5)|(uint32_t(pre)<<24)|(uint32_t(up)<<23)|(uint32_t(immediate)<<22)|(uint32_t(wb)<<21)|(rn<<16)|(rd<<12)|(immediate ? ((offset&240)<<4)|(offset&15):rm);
}
int main(){try{
    uint64_t checked=0;
    for(unsigned profile=0;profile<3;++profile)for(unsigned store=0;store<2;++store)for(unsigned imm=0;imm<2;++imm)
    for(unsigned mode=0;mode<4;++mode)for(unsigned up=0;up<2;++up)for(unsigned rd=0;rd<16;++rd)for(unsigned rn=0;rn<16;++rn)for(unsigned rm=0;rm<16;++rm){
        const bool pre=mode&2,wb=mode&1,update=!pre||wb;
        Bus bus;bus.instruction=encode(store,imm,pre,wb,up,rd,rn,rm);nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0x80);
        for(unsigned r=0;r<15;++r)cpu.state.r[r]=8*(r+1);
        cpu.state.cpsr=0xdf|(((rd+rn*3+rm*5+mode+up+store)&31)<<27);
        const auto before=cpu.state;const uint32_t base=rn==15 ? 0x88:before.r[rn],offset=imm ? 120:before.r[rm];
        const uint32_t next=up ? base+offset:base-offset;bus.address=pre ? next:base;
        const bool valid=profile && !(rd&1) && rd<14 && (pre || !wb) &&
            (!update || (rn!=15 && rn!=rd && rn!=rd+1)) &&
            (imm || (rm!=15 && (store || (rm!=rd && rm!=rd+1)) && (profile==2 || !update || rn!=rm)));
        auto want=before.r;
        if(valid){if(!store){want[rd]=bus.low;want[rd+1]=bus.high;}if(update)want[rn]=next;want[15]=0x84;}
        cpu.step();
        if(!(cpu.state.unsupported==!valid && cpu.state.r==want && cpu.state.cpsr==before.cpsr)) {
            std::cerr<<"profile="<<profile<<" store="<<store<<" imm="<<imm<<" mode="<<mode<<" up="<<up<<" rd="<<rd<<" rn="<<rn<<" rm="<<rm<<" valid="<<valid<<" stopped="<<cpu.state.unsupported<<" instruction="<<std::hex<<bus.instruction<<" address="<<bus.address<<'\n';
            for(unsigned r=0;r<16;++r)if(cpu.state.r[r]!=want[r])std::cerr<<"r"<<r<<" got="<<cpu.state.r[r]<<" want="<<want[r]<<'\n';
            throw std::runtime_error("doubleword fields/aliases/profile mismatch");
        }
        require(bus.reads.size()==(valid && !store ? 3u:1u) && bus.writes.size()==(valid && store ? 2u:0u),"wrong doubleword bus count");
        if(valid && store)require(bus.low==before.r[rd] && bus.high==before.r[rd+1],"wrong store pair/order");
        if(!valid)require(bus.low==0x89abcdef && bus.high==0x76543210,"invalid doubleword mutated RAM");++checked;
    }
    // All immediate fields and full-width register offsets, both directions,
    // all NZCV/Q, ARM9/ARM11, all three valid addressing modes and operations.
    const uint32_t wide[]={0,1,0x100,0x7fffffff,0x80000000,0xffffffff,0xffffff00,0x55555555,0xaaaaaaaa};
    for(unsigned profile=1;profile<3;++profile)for(unsigned store=0;store<2;++store)for(unsigned imm=0;imm<2;++imm)
    for(unsigned mode=0;mode<3;++mode)for(unsigned up=0;up<2;++up)for(unsigned flags=0;flags<32;++flags)
    for(unsigned index=0;index<(imm ? 256u:9u);++index){
        const uint32_t offset=imm ? index:wide[index],target=0xfffffff8;
        Bus bus;bus.address=target;bus.instruction=encode(store,imm,mode!=2,mode==1,up,0,2,3,offset);
        nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0x80);cpu.state.r[0]=0x80000001;cpu.state.r[1]=0xffffffff;
        cpu.state.r[2]=mode==2 ? target:(up ? target-offset:target+offset);cpu.state.r[3]=offset;cpu.state.cpsr=0xdf|(flags<<27);
        auto want=cpu.state.r;const auto cpsr=cpu.state.cpsr;
        if(!store){want[0]=bus.low;want[1]=bus.high;}if(mode)want[2]=mode==1 ? target:(up ? target+offset:target-offset);want[15]=0x84;
        cpu.step();require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==cpsr,"doubleword offset/wrap/all-flags mismatch");++checked;
    }
    // Default U=0 alignment stops before data accesses on each supported CPU.
    for(unsigned profile=1;profile<3;++profile)for(unsigned store=0;store<2;++store)for(unsigned lane=1;lane<8;++lane){
        Bus bus;bus.instruction=encode(store,true,true,false,true,0,2,3,0);bus.address=0x1000+lane;
        nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0x80);cpu.state.r[2]=bus.address;const auto before=cpu.state;cpu.step();
        require(cpu.state.unsupported && cpu.state.r==before.r && bus.reads.size()==1 && bus.writes.empty(),"unaligned doubleword made transaction");++checked;
    }
    for(unsigned store=0;store<2;++store)for(unsigned failure=1;failure<=2;++failure){
        Bus bus;bus.instruction=encode(store,true,true,true,true,0,2,3,0);bus.address=0x1000;bus.fail=failure;
        nds::Arm cpu(bus,true);cpu.reset(0x80);cpu.state.r[0]=1;cpu.state.r[1]=2;cpu.state.r[2]=bus.address;const auto before=cpu.state;cpu.step();
        require(cpu.state.unsupported && cpu.state.r==before.r && cpu.state.cpsr==before.cpsr,"failed doubleword committed registers/writeback");
        require(bus.high==0x76543210 && bus.low==(store && failure==2 ? 1u:0x89abcdefu),"failed store did not preserve documented partial bus effects");++checked;
    }
    for(unsigned store=0;store<2;++store){Bus bus;bus.instruction=encode(store,false,false,true,true,15,15,15)&0x0fffffff;nds::Arm cpu(bus,false);cpu.reset(0x80);cpu.step();require(!cpu.state.unsupported && cpu.state.r[15]==0x84 && bus.reads.size()==1 && bus.writes.empty(),"failed condition executed invalid/unavailable doubleword");++checked;}
    // Register offset form's high nibble is SBZ.
    for(unsigned nibble=1;nibble<16;++nibble){Bus bus;bus.instruction=encode(false,false,true,false,true,0,2,3)|(nibble<<8);nds::Arm cpu(bus,true);cpu.reset(0x80);const auto before=cpu.state;cpu.step();require(cpu.state.unsupported && cpu.state.r==before.r && bus.reads.size()==1,"doubleword nonzero SBZ accepted");++checked;}
    // Tightening reserved MSR register bits must preserve every implemented
    // flag source/register and all rotated immediate flag encodings.
    for(unsigned profile=0;profile<3;++profile) {
        const uint32_t mask=profile ? 0xf8000000:0xf0000000;
        for(unsigned reg=0;reg<15;++reg)for(unsigned flags=0;flags<32;++flags){
            Bus bus;bus.instruction=0xe128f000|reg;nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0x80);
            cpu.state.cpsr=0x500000df;cpu.state.r[reg]=(flags<<27)|0x1234567;
            auto want=cpu.state.r;want[15]=0x84;const uint32_t psw=(cpu.state.cpsr&~mask)|(cpu.state.r[reg]&mask);
            cpu.step();require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==psw && bus.reads.size()==1 && bus.writes.empty(),"MSR register mask regression");++checked;
        }
        for(unsigned field=0;field<4096;++field){
            Bus bus;bus.instruction=0xe328f000|field;nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0x80);cpu.state.cpsr=0xf80000df;
            const unsigned rotation=(field>>8)*2;const uint32_t byte=field&255;
            const uint32_t value=rotation ? (byte>>rotation)|(byte<<(32-rotation)):byte;
            const uint32_t psw=(cpu.state.cpsr&~mask)|(value&mask);auto want=cpu.state.r;want[15]=0x84;
            cpu.step();require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==psw && bus.reads.size()==1 && bus.writes.empty(),"MSR immediate mask regression");++checked;
        }
    }
    std::cout<<"ARM doubleword "<<checked<<" field/profile/offset/flags/failure checks PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
