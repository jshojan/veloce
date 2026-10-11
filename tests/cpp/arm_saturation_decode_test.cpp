#include "arm.hpp"
#include <iostream>
#include <stdexcept>
struct Bus final:nds::Memory {
    uint32_t instruction=0;unsigned reads=0,writes=0;
    bool read(uint32_t address,unsigned bytes,uint32_t& value) override {
        ++reads;if(address!=0 || bytes!=4)return false;value=instruction;return true;
    }
    bool write(uint32_t,unsigned,uint32_t) override {++writes;return false;}
};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try {
    for(unsigned op=0;op<4;++op)for(unsigned profile=0;profile<3;++profile)for(unsigned invalid=0;invalid<4;++invalid){
        Bus bus;nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0);cpu.state.r[0]=0x80000000;cpu.state.r[1]=0x7fffffff;cpu.state.r[4]=0xdeadbeef;
        unsigned rn=invalid==1?15:1,rd=invalid==2?15:4,rm=invalid==3?15:0;
        bus.instruction=0xe1000050|(op<<21)|(rn<<16)|(rd<<12)|rm;
        const auto before=cpu.state;cpu.step();
        if(profile==0 || invalid){require(cpu.state.unsupported,"unsupported operand/version accepted");require(cpu.state.r==before.r&&cpu.state.cpsr==before.cpsr,"diagnostic stop mutated registers");}
        else require(!cpu.state.unsupported&&cpu.state.r[15]==4,"supported ARM9/ARM11 instruction rejected");
        require(bus.reads==1&&bus.writes==0,"unexpected memory transaction");
    }
    Bus bus;bus.instruction=0x01014050;nds::Arm seven(bus,false);seven.reset(0);seven.step();
    require(!seven.state.unsupported&&seven.state.r[15]==4,"failed condition executed unavailable instruction");
    std::cout<<"ARM saturation version/PC guard and failed-condition checks PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
