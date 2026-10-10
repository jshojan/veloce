#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>
using emulator_test::require;
static uint32_t read(ctr::Machine& m,unsigned id,uint32_t a,unsigned width=4) { uint32_t value=0; require(m.read(id,a,width,value),"3DS bus read"); return value; }
static std::vector<uint8_t> state(ctr::Machine& m) { std::vector<uint8_t> result; require(m.save(result),"3DS state save"); return result; }
static uint32_t reverse_reference(unsigned kind,uint32_t value) {
    const uint32_t bytes[]={value%256,(value/256)%256,(value/65536)%256,value/16777216};
    if(kind==0) return bytes[3]+bytes[2]*256+bytes[1]*65536+bytes[0]*16777216;
    if(kind==1) return bytes[1]+bytes[0]*256+bytes[3]*65536+bytes[2]*16777216;
    const uint32_t half=bytes[1]+bytes[0]*256; return half+(half>=32768?0xffff0000u:0);
}
static bool predicate(unsigned cond,unsigned flags) {
    const bool n=flags&16,z=flags&8,c=flags&4,v=flags&2;
    const bool answers[]={z,!z,c,!c,n,!n,v,!v,c&&!z,!c||z,n==v,n!=v,!z&&n==v,z||n!=v,true};
    return answers[cond];
}
static uint64_t reversals(ctr::Machine& m) {
    constexpr uint32_t patterns[]={0,1,127,128,255,0x7fff,0x8000,0x7fff8000,0x8000ffff,0x80000000,0xffffffff,0xfffffffe,0x5555aaaa,0x89abcdef,0x12345678,0xa55aff00};
    constexpr uint32_t words[]={0x06bf0f30,0x06bf0fb0,0x06ff0fb0};
    uint64_t count=0;
    const auto check=[&](unsigned id,bool thumb,unsigned kind,unsigned rd,unsigned rm,uint32_t value,unsigned flags,unsigned cond,unsigned ge) {
        auto& cpu=m.cpu(id);cpu.reset(0x20004000);
        for(unsigned r=0;r<15;++r) cpu.state.r[r]=0x13579bdf+r;
        cpu.state.r[rm]=value;cpu.state.cpsr=0xdf|(thumb?0x20:0)|(flags<<27)|(ge<<16);
        auto expected=cpu.state.r; const uint32_t psw=cpu.state.cpsr;
        const uint32_t ins=thumb?0xba00|(kind==0?0:kind==1?64:192)|rm<<3|rd:cond<<28|words[kind]|rd<<12|rm;
        require(m.write(id,0x20004000,thumb?2:4,ins),"reversal instruction write");
        if(thumb || predicate(cond,flags)) expected[rd]=reverse_reference(kind,value);
        expected[15]=0x20004000+(thumb?2:4);cpu.step();
        require(!cpu.state.unsupported && cpu.state.r==expected && cpu.state.cpsr==psw && cpu.state.steps==1,"ARM11 reversal register/flags/PC");++count;
    };
    for(unsigned id=0;id<2;++id) {
        for(bool thumb:{false,true}) for(unsigned kind=0;kind<3;++kind) for(unsigned flags=0;flags<32;++flags) for(unsigned n=0;n<65536;++n)
            check(id,thumb,kind,4,0,n+(65535u-n)*65536,flags,14,0);
        for(unsigned kind=0;kind<3;++kind) for(unsigned rd=0;rd<15;++rd) for(unsigned rm=0;rm<15;++rm) for(unsigned flags=0;flags<32;++flags)
            for(unsigned cond=0;cond<15;++cond) for(auto value:patterns) check(id,false,kind,rd,rm,value,flags,cond,(rd+rm+flags)%16);
        for(unsigned kind=0;kind<3;++kind) for(unsigned rd=0;rd<8;++rd) for(unsigned rm=0;rm<8;++rm) for(unsigned flags=0;flags<32;++flags)
            for(auto value:patterns) check(id,true,kind,rd,rm,value,flags,14,(rd+rm+flags)%16);
        for(bool thumb:{false,true}) for(unsigned kind=0;kind<3;++kind) for(unsigned bit=0;bit<32;++bit) for(bool complement:{false,true})
            for(unsigned flags=0;flags<32;++flags) for(unsigned ge=0;ge<16;++ge)
                check(id,thumb,kind,4,0,complement?~(1u<<bit):1u<<bit,flags,14,ge);
    }
    // Unsupported/unpredictable forms are separate diagnostic fixtures.
    for(unsigned id=0;id<3;++id) for(unsigned kind=0;kind<3;++kind) for(unsigned rd=0;rd<16;++rd) for(unsigned rm=0;rm<16;++rm) {
        if(id<2 && rd!=15 && rm!=15) continue;
        auto& cpu=m.cpu(id);cpu.reset(0x20004000);cpu.state.r[0]=0x89abcdef;const auto before=cpu.state.r;const auto psw=cpu.state.cpsr;
        require(m.write(id,0x20004000,4,0xe0000000|words[kind]|rd<<12|rm),"diagnostic instruction write");cpu.step();
        require(cpu.state.unsupported && cpu.state.r==before && cpu.state.cpsr==psw,"ARMv6 unsupported/PC form mutation");
    }
    for(unsigned id=0;id<3;++id) for(unsigned kind=0;kind<4;++kind) for(unsigned fields=0;fields<64;++fields) {
        if(id<2 && kind!=2) continue;
        auto& cpu=m.cpu(id);cpu.reset(0x20004000);cpu.state.cpsr|=0x20;const auto before=cpu.state.r;const auto psw=cpu.state.cpsr;
        require(m.write(id,0x20004000,2,0xba00|kind<<6|fields),"Thumb diagnostic write");cpu.step();
        require(cpu.state.unsupported && cpu.state.r==before && cpu.state.cpsr==psw,"Thumbv6 unsupported/reserved form mutation");
    }
    // Ensure existing DS/DSi ARM9 and ARM7 paths do not acquire ARMv6 instructions.
    struct Ram final:nds::Memory {
        uint32_t ins=0; bool read(uint32_t,unsigned width,uint32_t& value) override { value=ins&(width==2?65535:0xffffffffu);return true; }
        bool write(uint32_t,unsigned,uint32_t) override { return true; }
    } memory;
    for(bool nine:{false,true}) for(bool thumb:{false,true}) for(unsigned kind=0;kind<3;++kind) {
        nds::Arm cpu(memory,nine);cpu.reset(0);cpu.state.cpsr|=thumb?0x20:0;
        memory.ins=thumb?0xba04|(kind==0?0:kind==1?64:192):0xe0000000|words[kind]|4<<12;
        const auto before=cpu.state.r;const auto psw=cpu.state.cpsr;cpu.step();
        require(cpu.state.unsupported && cpu.state.r==before && cpu.state.cpsr==psw,"DS architecture feature leak");
    }
    return count;
}
static uint64_t ram_and_keys(ctr::Machine& m) {
    uint64_t count=0;
    for(auto spec:{std::array<uint32_t,3>{0x20000000,0x8000000,3},{0x1ff80000,0x80000,3},{0x08000000,0x100000,1}}) {
        for(uint32_t offset=0;offset<spec[1];++offset) {
            const unsigned owner=spec[2]==1?2:offset%3;
            const uint8_t value=uint8_t(offset^(offset>>8)^(offset>>16)^(offset>>24));
            require(m.write(owner,spec[0]+offset,1,value),"all-byte RAM write");++count;
            for(unsigned id=spec[2]==1?2:0;id<3;++id) { require(read(m,id,spec[0]+offset,1)==value,"all-byte RAM shared/private read");++count; }
        }
        for(unsigned id=0;id<3;++id) for(unsigned width:{1u,2u,4u}) for(uint32_t offset:{0u,1u,3u,spec[1]-4}) {
            uint32_t value=0;for(unsigned n=0;n<width;++n) { const auto at=offset+n;value|=uint32_t(uint8_t(at^(at>>8)^(at>>16)^(at>>24)))<<(n*8); }
            if(spec[2]==1 && id<2) { uint32_t ignored=0x12345678;require(!m.read(id,spec[0]+offset,width,ignored) && ignored==0x12345678 && !m.write(id,spec[0]+offset,width,0),"ARM9 private ownership"); }
            else { require(read(m,id,spec[0]+offset,width)==value,"boundary width endian");++count; }
        }
    }
    for(uint32_t base:{0x08000000u,0x1ff80000u,0x20000000u}) {
        const uint32_t end=base==0x08000000?0x08100000:base==0x1ff80000?0x20000000:0x28000000;
        // AXI and FCRAM are contiguous mapped regions; only true unmapped ends reject.
        if(base==0x1ff80000) continue;
        const auto old=read(m,2,end-4);uint32_t value=0xabcdef12;
        require(!m.write(2,end-1,4,0) && !m.read(2,end-1,4,value) && value==0xabcdef12 && read(m,2,end-4)==old,"RAM boundary atomicity");
    }
    for(unsigned input=0;input<4096;++input) {
        m.set_input(input); unsigned pressed=0;
        const unsigned virtual_bits[]={0,1,7,6,11,10,8,9,5,4,2,3};
        for(unsigned n=0;n<12;++n) if(input&(1u<<virtual_bits[n])) pressed+=1u<<n;
        for(unsigned id=0;id<3;++id) {
            require(read(m,id,0x10146000,2)==4095-pressed && read(m,id,0x10146000,1)==((4095-pressed)%256) &&
                read(m,id,0x10146001,1)==((4095-pressed)/256),"HID digital input lanes");
            require(m.write(id,0x10146000,2,0) && read(m,id,0x10146000,2)==4095-pressed,"read-only HID input");
        }
    }
    uint32_t ignored=0;
    require(!m.read(0,0x10146000,4,ignored) && !m.write(0,0x10146000,4,0),"HID IRQ circuit falsely supplied");
    return count;
}
static void put(std::vector<uint8_t>& out,unsigned at,uint32_t value) { for(unsigned n=0;n<4;++n) out[at+n]=uint8_t(value>>(n*8)); }
static std::vector<uint8_t> guest() {
    // Two ARM11 contexts and ARM9 write independent captures in shared FCRAM.
    std::vector<uint8_t> rom(64);std::copy_n("V3DSP001",8,rom.begin());
    for(unsigned id=0;id<3;++id) {
        const uint32_t words[]={0xe3a01000|id,0xe3a02020,0xe1a02c02,0xe3a03000|(0x10+id*4),0xe5831000,0xeafffffe};
        const uint32_t address=id==2?0x08000000:0x20010000+id*0x10000;
        // r3 = FCRAM base + the low capture offset.
        auto code=std::array<uint32_t,6>{};std::copy(std::begin(words),std::end(words),code.begin());code[3]=0xe3823000|(0x10+id*4);
        for(unsigned n=0;n<4;++n) put(rom,16+id*16+n*4,n==0?rom.size():n==1||n==2?address:sizeof(words));
        for(auto word:code) { const auto offset=rom.size();rom.resize(offset+4);put(rom,unsigned(offset),word); }
    }
    return rom;
}
static void loaders_states(ctr::Machine& m,const std::vector<uint8_t>& image) {
    require(m.load(image.data(),image.size()),"restore original image before loader gates");const auto before=state(m);
    for(unsigned at:{0u,8u,16u,20u,24u,28u,32u,36u,40u,44u,48u,52u,56u,60u}) {
        auto bad=image;bad[at]^=1;require(!m.load(bad.data(),bad.size()) && state(m)==before,"invalid original image mutation");
    }
    auto bad=before;bad[0]='D';require(!m.restore(bad) && state(m)==before,"wrong state profile");
    for(unsigned at:{8u,16u,32u,40u,48u,176u,192u}) { bad=before;bad[at]^=0x80;require(!m.restore(bad) && state(m)==before,"invalid scalar state mutation"); }
    bad=before;bad.pop_back();require(!m.restore(bad) && state(m)==before,"truncated state mutation");
    auto rom=guest();require(m.load(rom.data(),rom.size()),"mixed guest load");m.frame();
    for(unsigned id=0;id<3;++id) require(read(m,id,0x20000010+id*4)==id && !m.cpu(id).state.unsupported,"mixed guest three-CPU captures");
    auto prefix=state(m);m.frame();auto final=state(m);require(m.restore(prefix),"machine replay restore");m.frame();require(state(m)==final,"three-CPU full RAM state replay");
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"usage: ctr_machine_test plugin original.v3ds");std::ifstream file(argv[2],std::ios::binary);
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)),{});ctr::Machine m;require(m.load(rom.data(),rom.size()),"3DS original image rejected");
        const auto registers=reversals(m),bus=ram_and_keys(m);loaders_states(m,rom);
        auto probe=guest();emulator_test::LoadedCore loaded(argv[1],probe);auto& plugin=*loaded.plugin;const auto info=plugin.get_info();
        require(std::string(info.name)=="3DS" && std::string(info.file_extensions[0])==".v3ds","3DS plugin identity");
        const auto* layout=plugin.get_controller_layout();require(layout && std::string(layout->platform_name)=="3DS" && layout->button_count==12,"3DS controller layout");
        plugin.run_frame({0xfff});for(unsigned id=0;id<3;++id) require(plugin.read_memory(0x10+id*4)==id,"plugin three-context capture");
        std::vector<uint8_t> prefix,final,replay;require(plugin.save_state(prefix) && prefix.size()==135791096,"3DS plugin complete state size");
        plugin.run_frame({0xabc});require(plugin.save_state(final) && plugin.load_state(prefix),"plugin state restore");plugin.run_frame({0xabc});require(plugin.save_state(replay) && replay==final,"plugin full state/input replay");
        plugin.reset();require(plugin.get_cycle_count()==0 && plugin.get_frame_count()==0,"3DS reset counters");plugin.unload_rom();require(!plugin.is_rom_loaded() && !plugin.save_state(replay),"3DS unload identity");
        std::cout<<registers<<" ARM11 reversal register snapshots and "<<bus<<" valid RAM bus observations matched; 4096 HID inputs, feature isolation, ownership, atomic loaders/states, three-context guest, replay and plugin passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
