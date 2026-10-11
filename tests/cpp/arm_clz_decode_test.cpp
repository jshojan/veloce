#include "arm.hpp"
#include <iostream>
#include <stdexcept>
struct Bus final:nds::Memory {
    uint32_t instruction=0;unsigned reads=0,writes=0;
    bool read(uint32_t address,unsigned bytes,uint32_t& value) override {++reads;if(address || bytes!=4)return false;value=instruction;return true;}
    bool write(uint32_t,unsigned,uint32_t) override {++writes;return false;}
};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try {
    unsigned cases=0;
    for(bool eleven:{false,true})for(unsigned rd=0;rd<15;++rd)for(unsigned rm=0;rm<15;++rm)for(unsigned kind=0;kind<65;++kind)for(unsigned flags=0;flags<32;++flags){
        Bus bus;nds::Arm cpu(bus,!eleven,eleven);cpu.reset(0);
        for(unsigned r=0;r<15;++r)cpu.state.r[r]=0xdead0000+r;
        unsigned count=32;uint32_t value=0;
        if(kind){const unsigned bit=(kind-1)%32;value=kind<=32 ? uint32_t(1)<<bit:uint32_t((uint64_t(1)<<(bit+1))-1);count=31-bit;}
        cpu.state.r[rm]=value;cpu.state.cpsr=0xdf|(flags<<27);const auto before=cpu.state;
        bus.instruction=0xe16f0f10|(rd<<12)|rm;cpu.step();
        auto want=before.r;want[rd]=count;want[15]=4;
        require(!cpu.state.unsupported&&cpu.state.r==want&&cpu.state.cpsr==before.cpsr,"CLZ register/flag preservation");
        require(bus.reads==1&&bus.writes==0,"CLZ memory transaction");++cases;
    }
    for(unsigned invalid=0;invalid<3;++invalid){Bus bus;nds::Arm cpu(bus,invalid!=0);cpu.reset(0);
        const unsigned rd=invalid==1?15:4,rm=invalid==2?15:0;bus.instruction=0xe16f0f10|(rd<<12)|rm;
        const auto before=cpu.state;cpu.step();require(cpu.state.unsupported&&cpu.state.r==before.r&&cpu.state.cpsr==before.cpsr,"unsupported CLZ mutates registers");}
    Bus bus;bus.instruction=0x016f4f10;nds::Arm seven(bus,false);seven.reset(0);seven.step();
    require(!seven.state.unsupported&&seven.state.r[15]==4,"failed CLZ condition executed");
    std::cout<<cases<<" ARM9/ARM11 CLZ register-field/flag checks and diagnostic guards PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
