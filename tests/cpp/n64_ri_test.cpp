#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
static uint64_t address(unsigned reg) {return n64::sign32(0xa4700000u+reg*4);}
static uint32_t get(n64::Machine& m,unsigned reg) {uint64_t value=0;require(m.read(address(reg),4,value),"RI read rejected");return uint32_t(value);}
static void put(n64::Machine& m,unsigned reg,uint32_t value) {require(m.write(address(reg),4,value),"RI write rejected");}
int main(int argc,char** argv) {
    try {
        require(argc==3,"plugin and original ROM required");
        std::ifstream f(argv[2],std::ios::binary);require(f.good(),"ROM missing");
        std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
        n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");
        for(unsigned i=0;i<524288;++i) {
            put(m,4,0xfff80000u|i);require(get(m,4)==i,"all 19-bit refresh fields");
        }
        for(unsigned mode=0;mode<16;++mode) for(unsigned config=0;config<128;++config) for(unsigned select=0;select<64;++select) {
            const unsigned wire=(select%8)+16*(select/8);
            put(m,0,0xfffffff0u|mode);put(m,1,0xffffff80u|config);put(m,3,0xffffff88u|wire);
            require(get(m,0)==mode && get(m,1)==config && get(m,3)==wire,"mode/config/select full field product");
            put(m,2,mode*config);require(m.ri_current_loaded && m.ri[2]==config,"any write latches current config");
            put(m,1,config^127);require(m.ri[2]==config && get(m,1)==(config^127),"current control changed without load");
        }
        for(unsigned latency=0;latency<16;++latency) {put(m,5,0xfffffff0u|latency);require(get(m,5)==latency,"latency field");}
        for(unsigned value=0;value<65536;++value) {put(m,7,0xffff0000u|value);require(get(m,6)==0,"settled error clear");}
        std::vector<uint8_t> state,after,replay;require(m.save(state),"save RI");
        for(unsigned reg:{0u,1u,2u,3u,4u,5u,7u})put(m,reg,0x12345);
        require(m.save(after) && m.restore(state),"restore RI");
        for(unsigned reg:{0u,1u,2u,3u,4u,5u,7u})put(m,reg,0x12345);
        require(m.save(replay) && replay==after,"RI exact continuation");
        for(unsigned reg=0;reg<8;++reg) {
            for(unsigned width:{1u,2u,8u}) {uint64_t value=0xabcdef;require(!m.read(address(reg),width,value) && value==0xabcdef && !m.write(address(reg),width,0),"RI invalid width");}
            uint64_t value=0xabcdef;require(!m.read(address(reg)+1,4,value) && value==0xabcdef && !m.write(address(reg)+1,4,0),"RI unaligned MMIO");
        }
        uint64_t sentinel=0xabcdef;
        require(!m.read(address(2),4,sentinel) && !m.read(address(7),4,sentinel) && sentinel==0xabcdef && !m.write(address(6),4,0),"RI access direction");
        require(m.save(replay) && replay==after,"invalid MMIO mutated state");
        for(unsigned scalar=0;scalar<7;++scalar) {
            auto bad=after;
            constexpr unsigned invalid[]={16,128,128,8,0x80000,16,2};
            const size_t at=bad.size()-76864+scalar*8;
            for(unsigned byte=0;byte<8;++byte)bad[at+byte]=uint8_t(uint64_t(invalid[scalar])>>(56-byte*8));
            require(!m.restore(bad) && m.save(replay) && replay==after,"invalid RI scalar mutation");
        }
        auto bad=after;std::fill(bad.end()-76816,bad.end()-76808,0);bad[bad.size()-76864+2*8+7]=1;
        require(!m.restore(bad) && m.save(replay) && replay==after,"unloaded current invariant");
        auto old=after;old.resize(old.size()-76864);old[6]='0';old[7]='3';require(m.restore(old) && !m.ri_current_loaded && m.ri==std::array<uint32_t,6>{},"S003 neutral RI conversion");
        require(m.save(state),"converted state");
        emulator_test::LoadedCore core(argv[1],rom);
        for(unsigned frame=0;frame<5000;++frame) {core.plugin->run_frame({});require(core.plugin->save_state(state) && m.restore(state),"plugin full RI state");if(m.done)break;}
        require(m.done && m.ri_current_loaded && m.ri[4]==524287 && m.ri[2]==127,"loaded plugin corpus did not finish");
        m.reset();require(m.ri==std::array<uint32_t,6>{} && !m.ri_current_loaded,"RI reset");
        // Original opt-in native entry: prove previous native states convert too.
        auto native=rom;std::memcpy(native.data()+0x34,"V64IP001",8);
        require(m.load(native.data(),native.size()),"native profile load");
        require(m.save(state),"native save");old=state;old.resize(old.size()-76864);old[6]='0';old[7]='3';
        put(m,1,127);put(m,2,1);
        require(m.restore(old) && m.save(replay) && replay==state,"native S003 conversion");
        std::cout<<"524288 refresh values, 131072 mode/config/select combinations, 16 latencies and 65536 error clears matched; latching, atomic rejection, states and loaded plugin passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
