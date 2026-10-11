#include "arm.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
struct Bus final:nds::Memory {
    uint32_t instruction=0;std::array<uint8_t,4> ram{0xdd,0xcc,0xbb,0xaa};
    unsigned stage=0;bool fail_read=false,fail_write=false;
    bool read(uint32_t address,unsigned bytes,uint32_t& value) override {
        if(address==0 && bytes==4){if(stage++)throw std::runtime_error("repeated fetch");value=instruction;return true;}
        if(stage++!=1)throw std::runtime_error("read order");if(fail_read)return false;
        if(address<0x100 || uint64_t(address)+bytes>0x104)return false;
        value=0;for(unsigned i=0;i<bytes;++i)value|=uint32_t(ram[address-0x100+i])<<(8*i);return true;
    }
    bool write(uint32_t address,unsigned bytes,uint32_t value) override {
        if(stage++!=2)throw std::runtime_error("write before read");if(fail_write)return false;
        if(address<0x100 || uint64_t(address)+bytes>0x104)return false;
        for(unsigned i=0;i<bytes;++i)ram[address-0x100+i]=uint8_t(value>>(8*i));return true;
    }
};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try {
    unsigned count=0;
    for(unsigned profile=0;profile<3;++profile)for(unsigned rn=0;rn<15;++rn)for(unsigned rd=0;rd<15;++rd)for(unsigned rm=0;rm<15;++rm){
        if(rn==rd || rn==rm)continue;
        for(unsigned lane=0;lane<5;++lane)for(unsigned flags=0;flags<(profile?32u:16u);++flags){
            const bool byte=lane!=0;const unsigned offset=byte?lane-1:0;
            Bus bus;nds::Arm cpu(bus,profile==1,profile==2);cpu.reset(0);
            for(unsigned r=0;r<15;++r)cpu.state.r[r]=0xdead0000+r;
            cpu.state.r[rn]=0x100+offset;cpu.state.r[rm]=0x01020304;cpu.state.cpsr=0xdf|(flags<<(profile?27:28));
            const auto before=cpu.state;bus.instruction=0xe1000090|(uint32_t(byte)<<22)|(rn<<16)|(rd<<12)|rm;
            cpu.step();auto want=before.r;want[rd]=byte?std::array<uint32_t,4>{0xdd,0xcc,0xbb,0xaa}[offset]:0xaabbccdd;want[15]=4;
            auto memory=std::array<uint8_t,4>{0xdd,0xcc,0xbb,0xaa};if(byte)memory[offset]=4;else memory={4,3,2,1};
            require(!cpu.state.unsupported&&cpu.state.r==want&&cpu.state.cpsr==before.cpsr&&bus.ram==memory&&bus.stage==3,"swap register/memory semantics");++count;
        }
    }
    for(unsigned invalid=0;invalid<5;++invalid){Bus bus;nds::Arm cpu(bus,true);cpu.reset(0);
        const unsigned rn=invalid==0?15:3,rd=invalid==1?15:invalid==3?3:4,rm=invalid==2?15:invalid==4?3:0;
        bus.instruction=0xe1000090|(rn<<16)|(rd<<12)|rm;const auto before=cpu.state;cpu.step();
        require(cpu.state.unsupported&&cpu.state.r==before.r&&cpu.state.cpsr==before.cpsr&&bus.stage==1,"unpredictable swap operand performed transaction");}
    for(bool write_failure:{false,true}){Bus bus;bus.fail_read=!write_failure;bus.fail_write=write_failure;nds::Arm cpu(bus,true);cpu.reset(0);cpu.state.r[3]=0x100;cpu.state.r[0]=0x01020304;
        bus.instruction=0xe1034090;const auto before=cpu.state;cpu.step();require(cpu.state.unsupported&&cpu.state.r==before.r&&bus.ram==std::array<uint8_t,4>{0xdd,0xcc,0xbb,0xaa}&&bus.stage==(write_failure?3u:2u),"failed exchange committed destination/write");}
    for(unsigned offset=1;offset<4;++offset){Bus bus;nds::Arm cpu(bus,true);cpu.reset(0);cpu.state.r[3]=0x100+offset;cpu.state.r[0]=0x01020304;bus.instruction=0xe1034090;cpu.step();
        const uint32_t rotated[]={0,0xddaabbcc,0xccddaabb,0xbbccddaa};require(cpu.state.r[4]==rotated[offset]&&bus.ram==std::array<uint8_t,4>{4,3,2,1},"legacy alignment model");}
    Bus bus;bus.instruction=0x010f4090;nds::Arm cpu(bus,false);cpu.reset(0);cpu.step();require(!cpu.state.unsupported&&cpu.state.r[15]==4&&bus.stage==1,"failed condition accessed data");
    std::cout<<count<<" ARM swap field/flag/byte-lane checks, failure ordering and legacy alignment PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
