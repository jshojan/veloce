#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstring>
using emulator_test::require;
static std::vector<uint8_t> snapshot(nds::Machine& m) { std::vector<uint8_t> result; require(m.save(result),"machine save"); return result; }
static uint32_t read(nds::Machine& m,bool nine,uint32_t address,unsigned width=4) { uint32_t value=0; require(m.read(nine,address,width,value),"bus read"); return value; }
static void put(std::vector<uint8_t>& image,unsigned at,uint32_t value) { for(unsigned n=0;n<4;++n) image[at+n]=uint8_t(value>>(n*8)); }
static void modes(nds::Machine& m) {
    constexpr uint32_t control=0x04004008;
    for(unsigned mode=0;mode<3;++mode) {
        require(m.write(true,control,4,0x80008000),"enable full memory");
        for(unsigned bank=0;bank<4;++bank) require(m.write(bank%2,0x02010000+bank*0x400000,4,0x13579bdf+bank),"seed banks");
        require(m.write(true,control,4,0x80000000|mode<<14),"set RAM mode");
        for(bool nine:{false,true}) {
            require(read(m,nine,control)==(0x80000000u|mode<<14),"RAM mode propagation");
            for(unsigned bank=0;bank<4;++bank) require(read(m,nine,0x02010000+bank*0x400000)==0x13579bdf+(mode==2?bank:0),"bank/mirror mode");
            for(uint32_t alias:{0x0c000000u,0x0d000000u}) for(unsigned bank=0;bank<4;++bank)
                require(read(m,nine,alias+0x10000+bank*0x400000)==(mode==2?0x13579bdf+bank:0),"extended mirror/zero mode");
            if(mode<2) {
                require(m.write(nine,0x0c010000,4,0xdeadbeef) && m.write(nine,0x0d010000,4,0xdeadbeef),"ignored mirror stores");
                require(read(m,nine,0x02010000)==0x13579bdf,"zero region store mutated RAM");
            }
        }
    }
    // ARM7 memory-size bits are read-only, independent access lock is writable.
    require(m.write(false,control,4,0x80000000) && read(m,false,control)==0x80008000,"ARM7 read-only mode");
    for(unsigned width:{1u,2u,4u}) for(unsigned lane=0;lane+width<=4;lane+=width) {
        const uint32_t expected=width==4?0x80008000:(0x80008000u>>(lane*8))&((1u<<(width*8))-1);
        require(read(m,true,control+lane,width)==expected,"SCFG byte lanes");
    }
    require(m.write(true,control,2,0x4000) && read(m,false,control)==0x80004000,"partial SCFG write");
    require(m.write(true,control+1,1,0x80) && read(m,true,control)==0x80008000,"byte SCFG write");
    auto before=snapshot(m);
    for(uint32_t bad:{1u,0x80008001u,0xffffffffu}) require(!m.write(true,control,4,bad) && snapshot(m)==before,"unsupported SCFG bits mutated state");
    require(m.write(true,control,4,0x8000) && read(m,true,control)==0,"SCFG lock visibility fixture");
    require(m.write(true,control,4,0x80000000) && read(m,false,control)==0x80008000 && read(m,true,0x02410000)==0x13579be0,"locked mode changed");
    auto locked=snapshot(m); m.reset(); require(read(m,true,control)==0x80008000,"reset access lock");
    require(m.restore(locked) && snapshot(m)==locked && read(m,true,control)==0,"restore access lock");
    m.reset();
    require(m.write(true,control,4,0x8000c000),"32MB limit selector");
    require(m.write(false,0x0c010000,4,0x12345678) && read(m,true,0x02010000)==0x12345678,"32MB mode first chip mirror");
    uint32_t unchanged=0xdeadbeef;
    require(!m.read(true,0x0d010000,4,unchanged) && unchanged==0xdeadbeef && !m.write(false,0x0d010000,4,0),"unmodeled open bus diagnostic");
    m.reset();
}
static uint64_t all_bytes(nds::Machine& m) {
    uint64_t observations=0;
    // One deterministic byte per address, spanning every byte of all 16 MiB.
    for(uint32_t offset=0;offset<0x1000000;++offset) {
        const uint8_t value=uint8_t(offset^(offset>>8)^(offset>>16));
        const bool owner=(offset/0x400000)%2;
        require(m.write(owner,0x0c000000+offset,1,value),"full RAM byte write"); ++observations;
        for(uint32_t alias:{0x02000000u,0x0c000000u,0x0d000000u}) {
            require(read(m,!owner,alias+offset,1)==value,"full RAM cross-CPU byte mirror"); ++observations;
        }
    }
    for(bool nine:{false,true}) for(unsigned width:{1u,2u,4u}) for(uint32_t offset:{0u,1u,0x3ffffeu,0x3fffffu,0x400000u,0x7fffffu,0xbfffffu,0xfffffcu}) {
        if(uint64_t(offset)+width>0x1000000) continue;
        uint32_t expected=0;
        for(unsigned n=0;n<width;++n) { const auto at=offset+n; expected|=uint32_t(uint8_t(at^(at>>8)^(at>>16)))<<(n*8); }
        for(uint32_t alias:{0x02000000u,0x0c000000u,0x0d000000u}) { require(read(m,nine,alias+offset,width)==expected,"bank boundary endian bytes"); ++observations; }
    }
    // An invalid transfer across the top of a mirror must leave all bytes intact.
    const auto before=snapshot(m); uint32_t value=0x12345678;
    require(!m.write(true,0x0dffffff,4,0) && !m.read(false,0x0dffffff,4,value) && value==0x12345678 && snapshot(m)==before,"boundary access atomicity");
    return observations;
}
static void state_and_loader(nds::Machine& m,const std::vector<uint8_t>& image) {
    const auto before=snapshot(m);
    for(unsigned field:{0x12u,0x180u,0x20u,0x24u,0x28u,0x2cu}) {
        auto bad=image; bad[field]^=1; require(!m.load(bad.data(),bad.size()) && snapshot(m)==before,"invalid image mutated loaded core");
    }
    auto ds=image; ds[0x12]=0; std::memcpy(ds.data()+0x180,"VDSHB001",8);
    nds::Machine ordinary; require(ordinary.load(ds.data(),ds.size()),"DS fixture load");
    require(!m.load(ds.data(),ds.size()) && !ordinary.load(image.data(),image.size()),"cross-profile images accepted");
    require(!ordinary.restore(before) && !m.restore(snapshot(ordinary)),"cross-profile states accepted");
    for(unsigned byte:{0u,7u,32u,40u,48u,176u,16875872u}) {
        auto bad=before; bad[byte]^=0x80; require(!m.restore(bad) && snapshot(m)==before,"invalid scalar state mutated core");
    }
    auto bad=before; bad.back()^=1; require(!m.restore(bad) && snapshot(m)==before,"inconsistent SCFG state accepted");
    bad=before; bad.pop_back(); require(!m.restore(bad) && snapshot(m)==before,"truncated state accepted");
    // Execute both CPUs from original segments, including an ARM9 segment in high RAM.
    auto high=std::vector<uint8_t>(528); high[0x12]=2; std::memcpy(high.data()+0x180,"VDSI0001",8);
    for(auto spec:{std::array<uint32_t,5>{0x20,512,0x02c00000,0x02c00000,8},{0x30,520,0x03800000,0x03800000,8}})
        for(unsigned n=0;n<4;++n) put(high,spec[0]+n*4,spec[n+1]);
    put(high,512,0xe3a0005a); put(high,516,0xeafffffe); put(high,520,0xe3a000a5); put(high,524,0xeafffffe);
    require(m.load(high.data(),high.size()),"high RAM segment rejected"); m.frame();
    require(m.cpu9.state.r[0]==0x5a && m.cpu7.state.r[0]==0xa5 && !m.cpu9.state.unsupported && !m.cpu7.state.unsupported,"dual-CPU high RAM execution");
    auto prefix=snapshot(m); m.frame(); auto final=snapshot(m); require(m.restore(prefix),"high execution restore"); m.frame(); require(snapshot(m)==final,"dual-CPU exact replay");
}
int main(int argc,char** argv) {
    try {
        require(argc==4,"usage: dsi_machine_test dsi-plugin original.vdsi ds-plugin"); std::ifstream stream(argv[2],std::ios::binary);
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(stream)),{}); nds::Machine m(true); require(m.load(rom.data(),rom.size()),"DSi original image load");
        modes(m); const auto observations=all_bytes(m); state_and_loader(m,rom);
        emulator_test::LoadedCore core(argv[1],rom); auto& plugin=*core.plugin;
        auto ds=rom; ds[0x12]=0; std::memcpy(ds.data()+0x180,"VDSHB001",8);
        emulator_test::LoadedCore legacy(argv[3],ds);
        require(std::string(legacy.plugin->get_info().name)=="DS" && std::string(legacy.plugin->get_info().file_extensions[0])==".nds" &&
            std::string(plugin.get_info().file_extensions[0])==".vdsi","coexisting DS/DSi plugin metadata collision");
        require(legacy.netplay().get_max_state_size()==4292968 && !legacy.netplay().load_state_fast(emulator_test::snapshot(core.netplay()).data(),16875896),"coexisting profile state isolation");
        const auto info=plugin.get_info(); require(std::string(info.name)=="DSi" && std::string(info.file_extensions[0])==".vdsi","DSi plugin metadata");
        const auto* layout=plugin.get_controller_layout(); require(layout && std::string(layout->platform_name)=="DSi" && layout->button_count==12,"DSi controller layout");
        auto& netplay=core.netplay(); require(netplay.get_max_players()==1 && netplay.get_max_state_size()==16875896,"DSi state/netplay size");
        auto before=emulator_test::snapshot(netplay); netplay.run_frame_netplay(0xfff,0); auto final=emulator_test::snapshot(netplay);
        require(netplay.load_state_fast(before.data(),before.size()),"fast state restore"); netplay.run_frame_netplay(0xfff,0xffff); require(emulator_test::snapshot(netplay)==final,"DSi host keypad/viewer replay");
        require(!netplay.load_state_fast(before.data(),before.size()-8),"DS legacy size accepted in DSi plugin");
        plugin.reset(); require(plugin.get_frame_count()==0 && plugin.get_cycle_count()==0,"DSi plugin reset"); plugin.unload_rom(); require(!plugin.is_rom_loaded() && !plugin.save_state(final),"DSi plugin unload");
        std::cout<<observations<<" DSi byte/width bus observations matched; RAM modes, controls, image/state isolation, high-RAM execution, replay and plugin/controller/netplay contracts passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
