#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
static uint64_t address(unsigned reg) {return n64::sign32(0xa4300000u+reg*4);}
static uint32_t get(n64::Machine& m,unsigned reg) {uint64_t v=0;require(m.read(address(reg),4,v),"MI read");return uint32_t(v);}
static void put(n64::Machine& m,unsigned reg,uint32_t v) {require(m.write(address(reg),4,v),"MI write");}
static unsigned mode_reference(unsigned old,unsigned command) {
    unsigned result=command%128;
    for(unsigned field=0;field<3;++field) {
        const unsigned base=field==2?12:7+field*2;
        unsigned flag=(old>>(7+field))&1;
        switch((command>>base)&3) {case 1:flag=0;break;case 2:case 3:flag=1;break;}
        result|=flag<<(7+field);
    }
    return result;
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"plugin and original ROM required");
        std::ifstream f(argv[2],std::ios::binary);require(f.good(),"ROM missing");
        std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
        n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");
        require(get(m,1)==0x02020102 && get(m,0)==0,"MI reset/revision");
        for(unsigned old=0;old<1024;++old) for(unsigned command=0;command<16384;++command) {
            m.mi_mode=old;m.dp_interrupt=true;
            put(m,0,0xffffc000u|command);
            require(get(m,0)==mode_reference(old,command),"mode command domain mismatch");
            require(m.dp_interrupt==!(command&0x800),"DP ack command mismatch");
        }
        for(unsigned old=0;old<64;++old) for(unsigned command=0;command<4096;++command) {
            m.mi_mask=old;put(m,3,0xfffff000u|command);unsigned expected=0;
            for(unsigned bit=0;bit<6;++bit) {
                const unsigned action=(command>>(bit*2))&3;
                const unsigned value=action>=2?1:action==1?0:(old>>bit)&1;
                expected|=value<<bit;
            }
            require(get(m,3)==expected,"all mask command transitions");
        }
        for(unsigned pending=0;pending<8;++pending) for(unsigned mask=0;mask<64;++mask) {
            m.si_interrupt=pending&1;m.pi_interrupt=pending&2;m.dp_interrupt=pending&4;m.mi_mask=mask;
            const unsigned request=(pending&1?2:0)|(pending&2?16:0)|(pending&4?32:0);
            require(get(m,2)==request && m.interrupt_pending()==((request&mask)?4:0),"pending/masked IP2");
            put(m,0,0x800);require(!m.dp_interrupt && m.si_interrupt==bool(pending&1) && m.pi_interrupt==bool(pending&2),"DP ack cleared other requests");
        }
        m.mi_mode=0;
        m.si_interrupt=m.pi_interrupt=false;m.dp_interrupt=true;m.mi_mask=32;
        m.cpu.reset(n64::sign32(0x80001000));m.cpu.state.cp0[12]=0x401;
        require(m.write(n64::sign32(0x80001000),4,0x24031234) && m.write(n64::sign32(0x80000180),4,0x42000018),"IRQ program placement");
        m.cpu.step();require(m.cpu.state.pc==n64::sign32(0x80000180) && m.cpu.state.cp0[14]==n64::sign32(0x80001000) && m.cpu.state.gpr[3]==0,"DP IRQ delivery");
        put(m,0,0x800);m.cpu.step();m.cpu.step();require(m.cpu.state.gpr[3]==0x1234 && !(m.cpu.state.cp0[13]&0x400),"DP ack/ERET");
        m.mi_mode=0x3ab;m.dp_interrupt=true;m.ri[4]=0x12345;
        std::vector<uint8_t> state,after,replay;require(m.save(state),"MI save");
        put(m,0,0x1280);put(m,3,0x555);m.save(after);
        require(m.restore(state),"MI restore");put(m,0,0x1280);put(m,3,0x555);m.save(replay);require(after==replay,"MI continuation");
        for(unsigned reg=0;reg<4;++reg) {
            for(unsigned width:{1u,2u,8u}) {uint64_t v=0x1234;require(!m.read(address(reg),width,v) && v==0x1234 && !m.write(address(reg),width,0),"MI invalid width");}
            uint64_t v=0x1234;require(!m.read(address(reg)+1,4,v) && v==0x1234 && !m.write(address(reg)+1,4,0),"MI unaligned");
        }
        const auto revision=get(m,1),pending=get(m,2);put(m,1,0);put(m,2,0xffffffff);
        require(get(m,1)==revision && get(m,2)==pending && m.save(replay) && replay==after,"read-only writes or invalid accesses changed state");
        for(unsigned index=0;index<2;++index) {
            auto bad=after;const size_t pos=bad.size()-1129936+index*8;
            bad[pos+6]=index==0?4:0;bad[pos+7]=index==0?0:2;
            require(!m.restore(bad) && m.save(replay) && replay==after,"invalid MI state atomicity");
        }
        auto old=state;old.resize(old.size()-1129936);old[6]='0';old[7]='4';
        require(m.restore(old) && !m.mi_mode && !m.dp_interrupt && m.ri[4]==0x12345,"S004 MI conversion");
        emulator_test::LoadedCore core(argv[1],rom);
        for(unsigned frame=0;frame<5000;++frame) {core.plugin->run_frame({});require(core.plugin->save_state(state) && m.restore(state),"plugin MI snapshot");if(m.done)break;}
        require(m.done && m.mi_mode==0 && m.mi_mask==63 && m.si_interrupt && m.pi_interrupt,"loaded MI corpus completion");
        m.reset();require(!m.mi_mode && !m.dp_interrupt,"MI reset");
        auto native=rom;std::memcpy(native.data()+0x34,"V64IP001",8);
        require(m.load(native.data(),native.size()) && m.save(state),"native state");m.frame_scheduling=emu::FrameScheduling::InstructionQuantum;m.save(state);old=state;old.resize(old.size()-1129936);old[6]='0';old[7]='4';m.mi_mode=1023;m.dp_interrupt=true;
        require(m.restore(old) && m.save(replay) && replay==state,"native S004 conversion");
        std::cout<<"16777216 mode transitions, 262144 mask transitions and 512 pending/mask combinations matched; DP ack/IRQ/ERET, atomic access/state rejection, replay, old states and loaded plugin passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
