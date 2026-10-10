#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
using emulator_test::require;
static uint32_t read(wiiu::Machine& m,unsigned id,uint32_t a,unsigned w=4) { uint32_t value=0;require(m.read(id,a,w,value),"Wii U bus read");return value; }
static std::vector<uint8_t> state(wiiu::Machine& m) { std::vector<uint8_t> out;require(m.save(out),"Wii U state save");return out; }
static void put(std::vector<uint8_t>& out,size_t at,uint32_t value) { for(unsigned n=0;n<4;++n)out[at+n]=uint8_t(value>>(24-n*8)); }
static uint64_t immediates(wiiu::Machine& m) {
    constexpr uint32_t patterns[]={0,1,0x7fff,0x8000,0x7fffffff,0x80000000,0xffffffff,0x89abcdef};
    constexpr uint32_t values[]={0,1,127,128,255,0x7fff,0x8000,65535};uint64_t count=0;
    const auto check=[&](unsigned id,unsigned op,unsigned rt,unsigned ra,uint32_t value,uint32_t encoded,unsigned flags) {
        auto& cpu=m.cpus[id];cpu={};cpu.pc=0x1000;
        for(unsigned reg=0;reg<32;++reg)cpu.r[reg]=0x13579bdf+reg;
        cpu.r[ra]=value;cpu.cr=0xa5a5a5a5^(ra*0x01010101);cpu.xer=(flags<<29)|127;cpu.lr=0xabcdef00;cpu.ctr=0x12345678;
        auto expected=cpu.r;const auto before=cpu;
        const int64_t signed_imm=int64_t(encoded)-(encoded>=32768?65536:0);
        const int64_t sum=(ra?int64_t(value):0)+(op==14?signed_imm:signed_imm*65536);
        expected[rt]=uint32_t(sum);
        require(m.write(id,0x1000,4,op<<26|rt<<21|ra<<16|encoded),"immediate instruction write");m.step(id);
        require(!cpu.unsupported && cpu.r==expected && cpu.cr==before.cr && cpu.xer==before.xer && cpu.lr==before.lr && cpu.ctr==before.ctr && cpu.pc==0x1004 && cpu.steps==1,"Espresso immediate registers/flags/PC");++count;
    };
    for(unsigned id=0;id<3;++id) {
        for(unsigned op:{14u,15u})for(uint32_t source:{0u,1u,0x7fffffffu,0x80000000u})for(unsigned ra:{0u,3u})for(unsigned flags=0;flags<8;++flags)
            for(unsigned encoded=0;encoded<65536;++encoded)check(id,op,11,ra,source,encoded,flags);
        for(unsigned op:{14u,15u})for(unsigned rt=0;rt<32;++rt)for(unsigned ra=0;ra<32;++ra)for(unsigned flags=0;flags<8;++flags)
            for(auto source:patterns)for(auto encoded:values)check(id,op,rt,ra,source,encoded,flags);
    }
    return count;
}
static uint64_t ram_and_dense_states(wiiu::Machine& m) {
    uint64_t count=0;
    for(auto spec:{std::array<uint32_t,2>{0,0x02000000},{0x10000000,0x80000000}}) {
        for(uint32_t offset=0;offset<spec[1];++offset) {
            const uint8_t value=uint8_t(offset^(offset>>8)^(offset>>16)^(offset>>24));const unsigned owner=offset%3;
            require(m.write(owner,spec[0]+offset,1,value),"all-byte RAM write");++count;
            for(unsigned id=0;id<3;++id) { require(read(m,id,spec[0]+offset,1)==value,"all-byte three-context RAM sharing");++count; }
        }
        for(unsigned id=0;id<3;++id)for(unsigned width:{1u,2u,4u})for(uint32_t offset:{0u,1u,4093u,4094u,4095u,spec[1]-4}) {
            uint32_t want=0;for(unsigned n=0;n<width;++n) { const auto at=offset+n;want=want*256+uint8_t(at^(at>>8)^(at>>16)^(at>>24)); }
            require(read(m,id,spec[0]+offset,width)==want,"RAM endian/page/boundary widths");++count;
        }
        const uint32_t end=spec[0]+spec[1];const auto old=read(m,0,end-4);uint32_t value=0x12345678;
        require(!m.write(0,end-1,4,0) && !m.read(0,end-1,4,value) && value==0x12345678 && read(m,0,end-4)==old,"unmapped boundary atomicity");
    }
    auto full=state(m);require(full.size()==2183168992ull,"all physical RAM pages retained");
    for(uint32_t address:{0u,0x1fffffffu,0x10000000u,0x50000000u,0x8fffffffu})require(m.write(1,address,1,read(m,1,address,1)^1),"dense replay mutation");
    require(m.restore(full) && state(m)==full,"complete dense RAM state replay");
    return count;
}
static std::vector<uint8_t> guest() {
    std::vector<uint8_t> rom(64);std::copy_n("VWUP0001",8,rom.begin());
    for(unsigned id=0;id<3;++id) {
        const uint32_t words[]={15u<<26|12<<21|0xffe0,24u<<26|12<<21|12<<16|12,14u<<26|1<<21|id,14u<<26|3<<21|(0x2000+id*4),
            36u<<26|1<<21|3<<16,32u<<26|4<<21|12<<16,36u<<26|4<<21|3<<16|0x40,18u<<26};
        const uint32_t addresses[]={0x01fff000,0x10000000,0x8ffff000};
        for(unsigned n=0;n<4;++n)put(rom,16+id*16+n*4,n==0?rom.size():n==1||n==2?addresses[id]:sizeof(words));
        for(auto word:words) { const auto at=rom.size();rom.resize(at+4);put(rom,at,word); }
    }
    return rom;
}
static void loaders_states_input(wiiu::Machine& m,const std::vector<uint8_t>& original) {
    require(m.load(original.data(),original.size()),"original reload");auto baseline=state(m);
    for(unsigned at:{0u,8u,19u,23u,27u,31u,35u,39u,43u,47u,51u,55u,59u,63u}) { auto bad=original;bad[at]^=1;require(!m.load(bad.data(),bad.size()) && state(m)==baseline,"invalid image atomicity"); }
    auto bad=original;std::copy_n(original.begin()+16,16,bad.begin()+32);require(!m.load(bad.data(),bad.size()) && state(m)==baseline,"overlapping image atomicity");
    bad=original;bad.resize(63);require(!m.load(bad.data(),bad.size()) && state(m)==baseline,"short image atomicity");
    for(unsigned at:{0u,8u,16u,32u,40u,48u,304u,320u,352u,984u}) { bad=baseline;bad[at]^=0x80;require(!m.restore(bad) && state(m)==baseline,"invalid state scalar atomicity"); }
    bad=baseline;bad.pop_back();require(!m.restore(bad) && state(m)==baseline,"truncated state atomicity");
    bad=baseline;put(bad,992,532480);require(!m.restore(bad) && state(m)==baseline,"unmapped page state index");
    bad=baseline;put(bad,992+4100,0);require(!m.restore(bad) && state(m)==baseline,"unordered page state index");
    bad=baseline;std::fill(bad.begin()+996,bad.begin()+996+4096,0);require(!m.restore(bad) && state(m)==baseline,"noncanonical zero page state");
    auto probe=guest();require(m.load(probe.data(),probe.size()),"physical-end guest load");
    for(unsigned input=0;input<4096;++input) {
        m.set_input(input);for(unsigned id=0;id<3;++id)require(read(m,id,0xffe0000c)==input,"digital input latch");
        require(m.write(0,0xffe0000c,4,0) && read(m,0,0xffe0000c)==input,"read-only input latch");
    }
    m.frame();for(unsigned id=0;id<3;++id)require(read(m,id,0x2000+id*4)==id && read(m,id,0x2040+id*4)==4095 && !m.cpus[id].unsupported,"three-context high-RAM guest/input captures");
    auto before=state(m);m.set_input(123);m.frame();auto after=state(m);require(m.restore(before),"sparse replay restore");m.set_input(123);m.frame();require(state(m)==after,"sparse CPU/RAM/input replay");
    require(m.write(0,0x20000000,4,0) && state(m)==after,"zero write creates observable sparse page");
    require(m.write(0,0x20000000,4,0x12345678),"new page write");require(state(m).size()==after.size()+4100,"new nonzero page missing");
    require(m.write(0,0x20000000,4,0) && state(m)==after,"cleared page not canonical");
    uint32_t ignored=0x12345678;
    for(uint32_t address:{0x02000000u,0x08000000u,0x0c000000u,0x0d000000u,0x90000000u,0xc0000000u,0xffffffffu})
        require(!m.read(0,address,4,ignored) && ignored==0x12345678 && !m.write(0,address,4,0),"unsupported hardware accepted");
    require(!m.read(3,0,1,ignored) && !m.write(3,0,1,0) && !m.read(0,0,3,ignored) && !m.write(0,0,3,0),"invalid bus fields accepted");
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"usage: wiiu_machine_test plugin original.vwiiu");std::ifstream file(argv[2],std::ios::binary);
        std::vector<uint8_t> original((std::istreambuf_iterator<char>(file)),{});wiiu::Machine m;require(m.load(original.data(),original.size()),"original Wii U image rejected");
        const auto registers=immediates(m),bus=ram_and_dense_states(m);loaders_states_input(m,original);
        auto probe=guest();emulator_test::LoadedCore loaded(argv[1],probe);auto& plugin=*loaded.plugin;const auto info=plugin.get_info();
        require(std::string(info.name)=="Wii U" && std::string(info.file_extensions[0])==".vwiiu","Wii U plugin identity");
        const auto* layout=plugin.get_controller_layout();require(layout && std::string(layout->platform_name)=="Wii U" && layout->button_count==12,"Wii U controller identity");
        plugin.run_frame({4095});for(unsigned id=0;id<3;++id)require(plugin.read_memory(0x2000+id*4)==0 && plugin.read_memory(0x2003+id*4)==id && plugin.read_memory(0x2042+id*4)==15 && plugin.read_memory(0x2043+id*4)==255,"plugin endian/input captures");
        std::vector<uint8_t> before,after,replay;require(plugin.save_state(before),"plugin state save");plugin.run_frame({123});require(plugin.save_state(after) && plugin.load_state(before),"plugin state restore");plugin.run_frame({123});require(plugin.save_state(replay) && replay==after,"plugin input replay");
        plugin.reset();require(!plugin.get_cycle_count() && !plugin.get_frame_count(),"reset counters");plugin.unload_rom();require(!plugin.is_rom_loaded() && !plugin.save_state(replay),"unload state");
        std::cout<<registers<<" immediate register snapshots and "<<bus<<" RAM bus observations matched; full 2 GiB MEM2/32 MiB MEM1, dense/sparse replay, atomic loaders/states, 4096 original input latches, three-context guest and plugin passed\n";
    }catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
