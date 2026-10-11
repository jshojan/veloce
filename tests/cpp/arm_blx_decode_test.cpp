#include "arm.hpp"
#include <iostream>
#include <stdexcept>
struct Bus final:nds::Memory {
    uint32_t at=0,instruction=0;unsigned reads=0;
    bool read(uint32_t address,unsigned bytes,uint32_t& value) override {++reads;if(address!=at || bytes!=4)return false;value=instruction;return true;}
    bool write(uint32_t,unsigned,uint32_t) override {throw std::runtime_error("branch wrote memory");}
};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
uint32_t target(uint32_t pc,uint32_t field,unsigned h){const int64_t offset=(field<0x800000?int64_t(field):int64_t(field)-0x1000000)*4+2*h;return uint32_t(int64_t(pc)+8+offset);}
int main(){try {
    Bus bus;bus.at=0x02002000;nds::Arm cpu(bus,true);cpu.reset(bus.at);uint64_t cases=0;
    for(uint32_t field=0;field<0x1000000;++field)for(unsigned h=0;h<2;++h){
        cpu.state.r[15]=bus.at;cpu.state.r[14]=0x89abcdef;cpu.state.cpsr=0xdf|((field&31)<<27);bus.instruction=0xfa000000|(h<<24)|field;
        cpu.step();require(!cpu.state.unsupported&&cpu.state.r[15]==target(bus.at,field,h)&&cpu.state.r[14]==bus.at+4&&cpu.state.cpsr==(0xff|((field&31)<<27)),"full displacement/H domain");++cases;
    }
    for(unsigned profile=0;profile<3;++profile)for(uint32_t pc:{0u,4u,0x02002000u,0xfffffff8u,0xfffffffcu})for(uint32_t field:{0u,1u,0x7fffffu,0x800000u,0xfffffeu,0xffffffu})for(unsigned h=0;h<2;++h)for(unsigned flags=0;flags<32;++flags){
        Bus b;b.at=pc;b.instruction=0xfa000000|(h<<24)|field;nds::Arm c(b,profile==1,profile==2);c.reset(pc);
        for(unsigned r=0;r<15;++r)c.state.r[r]=0xdead0000+r;c.state.cpsr=0xdf|(flags<<27);const auto before=c.state;
        c.step();auto want=before.r;if(profile){want[14]=pc+4;want[15]=target(pc,field,h);}else want[15]=pc+4;
        require(!c.state.unsupported&&c.state.r==want&&c.state.cpsr==(profile?(before.cpsr|0x20):before.cpsr)&&b.reads==1,"architecture/wrap/flags/register preservation");
    }
    bus.instruction=0xf3a00001;cpu.reset(bus.at);const auto before=cpu.state;cpu.step();require(cpu.state.unsupported&&cpu.state.r==before.r&&cpu.state.cpsr==before.cpsr,"unavailable unconditional encoding retired");
    std::cout<<cases<<" BLX displacement/H encodings plus ARM9/ARM11 wrap/flag/register and ARM7 NV checks PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
