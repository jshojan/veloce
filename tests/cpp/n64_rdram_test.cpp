#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
static uint32_t encode(unsigned id) {
    return ((id%64)<<26)|(((id/64)%2)<<23)|(((id/128)%256)<<8)|((id/32768)<<7);
}
static n64::RdramDevices::State configured() {
    n64::RdramDevices::State s{};
    for(unsigned n=0;n<4;++n) {s[n][0]=encode((n+1)*2);s[n][1]=8;s[n][2]=0x02000000;}
    return s;
}
static uint64_t addr(uint32_t p) {return n64::sign32(0xa0000000u|p);}
static void put(n64::Machine& m,uint32_t p,uint32_t value) {require(m.write(addr(p),4,value),"device write");}
static uint32_t get(n64::Machine& m,uint32_t p) {uint64_t v=0;require(m.read(addr(p),4,v),"device read");return uint32_t(v);}
int main(int argc,char** argv) {
    try {
        require(argc==3,"plugin and original ROM required");n64::RdramDevices d;
        const auto base=configured();
        for(unsigned chip=0;chip<4;++chip) for(unsigned id=0;id<65536;++id) {
            require(d.restore(base),"configure");d.write(((chip+1)*2<<10)+4,encode(id),true,0);
            require(d.snapshot()[chip][0]==encode(id),"device ID payload");
            unsigned expected=0;const unsigned wanted=(id%512)/2;
            for(unsigned n=0;n<4;++n) {
                const unsigned candidate=n==chip?id:(n+1)*2;
                if(candidate/2==wanted) {expected=encode(candidate);break;}
            }
            require(d.read((id%512)*1024+4,true)==expected,"all device ID permutations/select priority");
        }
        constexpr unsigned bits[]={3,4,5,11,12,19,20,21,27,28,29};
        for(unsigned value=0;value<2048;++value) for(unsigned old_delay=0;old_delay<8;++old_delay) for(unsigned repeat=0;repeat<=128;++repeat) {
            auto state=base;state[0][1]=old_delay<<3;require(d.restore(state),"delay setup");
            uint32_t word=0;for(unsigned bit=0;bit<11;++bit)word|=((value>>bit)&1)<<bits[bit];
            d.write(2048+8,word,true,repeat);
            const auto changed=old_delay==1?word:repeat>=16?((word>>16)|(word<<16))&0x38381838u:state[0][1];
            require(d.snapshot()[0][1]==changed && d.read(2048+8,true)==(changed|0x03030203),"delay/repeat domain");
        }
        require(d.restore(base),"selectors setup");
        for(unsigned select=0;select<512;++select)for(unsigned offset=0;offset<1024;offset+=4) {
            const unsigned reg=(offset/4)%16;unsigned expected=0;
            if(select/2>=1 && select/2<=4 && offset<512) {
                if(reg==0)expected=0xb4190010;
                else if(reg==1)expected=encode((select/2)*2);
                else if(reg==2)expected=0x0303020b;
                else if(reg==3)expected=0xc2c0c0c0;
                else if(reg==9)expected=0x500;
            }
            require(d.read(select*1024+offset,true)==expected && d.read(0x80000+select*1024+offset,true)==0 && d.read(select*1024+offset,false)==0,"register/select/row aliases and broadcast/inactive reads");
        }
        for(unsigned n=0;n<256;++n) {
            require(d.restore(base),"mode setup");const uint32_t word=n*0x01010101u;
            d.write(2048+12,word,true,0);require(d.snapshot()[0][2]==word && d.read(2048+12,true)==((word&0x02000000)?word^0xc0c0c0c0:0),"mode read encoding");
        }
        d.reset();d.write(8,8,true,0);require(d.snapshot()[0][1]==0x20,"initial write delay restriction");
        d.write(0x80008,0x00080000,true,16);d.write(4,encode(2),true,0);d.write(2048+12,0x02000000,true,0);
        require(d.read(2048,true)==0xb4190010 && d.read(0,true)==0,"first disabled discovery");
        d.write(4,encode(4),true,0);d.write(4096+12,0x02000000,true,0);require(d.read(4096,true)==0xb4190010,"second device discovery");
        d.write(0x80000+512,0x12345678,true,0);require(d.read(2048+512,true)==0x12345678 && d.read(4096+512,true)==0x12345678,"broadcast row storage");
        std::ifstream f(argv[2],std::ios::binary);require(f.good(),"fixture missing");std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
        n64::Machine m;require(m.load(rom.data(),rom.size()) && m.rdram_devices.restore(base),"machine load");
        put(m,0x0470000c,20);put(m,0x04700008,0);put(m,0x04300000,0x2000);
        require(get(m,0x03f00804)==encode(2),"MI upper register read");put(m,0x04300000,0x1000);require(get(m,0x03f00804)==0,"MI upper suppression");put(m,0x04300000,0x210f);
        put(m,0x03f80008,0x00080008);require(!(m.mi_mode&128) && (m.mi_mode&127)==15,"repeat consumed once");
        std::vector<uint8_t> state,after,replay;require(m.save(state),"save");put(m,0x03f00810,0xabcdef);m.save(after);
        require(m.restore(state),"restore");put(m,0x03f00810,0xabcdef);m.save(replay);require(after==replay,"device replay");
        for(unsigned chip=0;chip<4;++chip) {
            auto bad=after;bad[bad.size()-1129920+chip*72+8+7]|=1;
            require(!m.restore(bad) && m.save(replay) && replay==after,"invalid delay state atomicity");
            bad=after;bad[bad.size()-1129920+chip*72]=1;
            require(!m.restore(bad) && m.save(replay) && replay==after,"overflow device state atomicity");
        }
        for(unsigned width:{1u,2u,8u}) {uint64_t v=9;require(!m.read(addr(0x03f00800),width,v) && v==9 && !m.write(addr(0x03f00800),width,0),"device widths");}
        uint64_t v=9;require(!m.read(addr(0x03f00801),4,v) && v==9 && !m.write(addr(0x03f00801),4,0),"device alignment");require(m.save(replay) && replay==after,"invalid MMIO mutated state");
        auto old=state;old.resize(old.size()-1129920);old[6]='0';old[7]='5';require(m.restore(old) && m.rdram_devices.snapshot()==n64::RdramDevices{}.snapshot(),"old neutral devices");
        emulator_test::LoadedCore core(argv[1],rom);
        for(unsigned frame=0;frame<1000;++frame) {core.plugin->run_frame({});require(core.plugin->save_state(state) && m.restore(state),"loaded plugin device state");if(m.done)break;}
        require(m.done,"device original program incomplete");
        auto native=rom;std::memcpy(native.data()+0x34,"V64IP001",8);require(m.load(native.data(),native.size()) && m.save(state),"native fixture");m.frame_scheduling=emu::FrameScheduling::InstructionQuantum;m.save(state);old=state;old.resize(old.size()-1129920);old[6]='0';old[7]='5';m.rdram_devices.restore(base);require(m.restore(old) && m.save(replay) && replay==state,"native legacy conversion");
        std::cout<<"262144 ID writes, 2113536 delay/repeat cases, 131072 selector/register aliases and 256 mode patterns matched; discovery, broadcast, MI one-shot/upper, states, rejection and loaded plugin passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
