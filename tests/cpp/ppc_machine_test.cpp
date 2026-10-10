#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstring>
using emulator_test::require;
static uint32_t arithmetic(unsigned xo,unsigned rt,unsigned ra,unsigned rb,unsigned oe,unsigned rc) {
    return 31u<<26|rt<<21|ra<<16|rb<<11|oe<<10|xo<<1|rc;
}
static uint32_t spr(unsigned reg,unsigned number,bool write) {
    return 31u<<26|reg<<21|(number&31)<<16|(number&992)<<6|(write?467u:339u)<<1;
}
static int64_t signed_value(uint32_t value) { return value<0x80000000u ? value:int64_t(value)-0x100000000LL; }
static void put(std::vector<uint8_t>& bytes,size_t position,uint32_t value) {
    for(unsigned i=0;i<4;++i) bytes[position+i]=uint8_t(value>>(24-i*8));
}
static std::vector<uint8_t> image(bool wii) {
    std::vector<uint8_t> bytes(40); std::memcpy(bytes.data(),wii?"VWIP0001":"VGCP0001",8);
    put(bytes,8,0x80000000); put(bytes,12,0x80000000); put(bytes,16,8);
    put(bytes,32,14u<<26|3<<21|3<<16|1); put(bytes,36,0x4bfffffc); return bytes;
}
static void execute(ppc::Machine& machine,uint32_t instruction) {
    machine.cpu.pc=0x80002000; machine.cpu.steps=0; machine.cpu.unsupported=false; machine.done=false;
    require(machine.write(machine.cpu.pc,4,instruction),"write instruction"); machine.step();
}
static uint64_t arithmetic_checks(ppc::Machine& m) {
    constexpr uint32_t values[]={0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef};
    uint64_t count=0;
    auto check=[&](unsigned xo,unsigned rt,unsigned ra,unsigned rb,uint32_t a,uint32_t b,unsigned flags,unsigned oe,unsigned rc,uint32_t cr) {
        m.cpu={}; for(unsigned n=0;n<32;++n) m.cpu.r[n]=0x12345678u+n;
        m.cpu.r[ra]=a; m.cpu.r[rb]=b; // Same-source encodings read the last supplied value.
        const auto before=m.cpu.r; a=before[ra]; b=before[rb];
        const bool sub=xo==40 || xo==8 || xo==136;
        const unsigned ca=flags&1,borrow=xo==136?1-ca:0,extra=xo==138?ca:0;
        const int64_t wide=sub?int64_t(b)-a-borrow:int64_t(a)+b+extra;
        const int64_t math=sub?signed_value(b)-signed_value(a)-borrow:signed_value(a)+signed_value(b)+extra;
        const uint32_t result=uint32_t(wide);
        const unsigned carry=xo==266 || xo==40?ca:sub?wide>=0:wide>=0x100000000LL;
        const bool ov=oe?(math< -0x80000000LL || math>0x7fffffffLL):((flags>>1)&1);
        const bool so=((flags>>2)&1) || (oe && ov);
        const uint32_t xer=uint32_t(so)<<31|uint32_t(ov)<<30|carry<<29|127;
        const uint32_t wantcr=rc?(cr&0x0fffffffu)|((result==0?2u:result&0x80000000?8u:4u)|unsigned(so))<<28:cr;
        m.cpu.xer=flags<<29|127; m.cpu.cr=cr; m.cpu.lr=0x87654321; m.cpu.ctr=0x11223344;
        execute(m,arithmetic(xo,rt,ra,rb,oe,rc));
        require(!m.cpu.unsupported && m.cpu.pc==0x80002004 && m.cpu.steps==1,"arithmetic decode/PC");
        for(unsigned n=0;n<32;++n) require(m.cpu.r[n]==(n==rt?result:before[n]),"arithmetic register/alias");
        require(m.cpu.cr==wantcr && m.cpu.xer==xer && m.cpu.lr==0x87654321 && m.cpu.ctr==0x11223344,"arithmetic special registers"); ++count;
    };
    for(unsigned xo:{266u,10u,138u,40u,8u,136u}) {
        for(auto a:values) for(auto b:values) for(unsigned flags=0;flags<8;++flags)
            for(unsigned oe=0;oe<2;++oe) for(unsigned rc=0;rc<2;++rc)
                for(unsigned rt:{3u,4u,5u}) for(uint32_t cr:{0u,0xffffffffu,0xa5a5a5a5u,0x12345678u}) check(xo,rt,3,4,a,b,flags,oe,rc,cr);
        for(unsigned rt=0;rt<32;++rt) for(unsigned ra=0;ra<32;++ra) for(unsigned rb=0;rb<32;++rb)
            for(unsigned oe=0;oe<2;++oe) for(unsigned rc=0;rc<2;++rc) check(xo,rt,ra,rb,0x80000000,0xffffffff,5,oe,rc,0xa5a5a5a5);
    }
    return count;
}
struct Capture final:emu::ITestSink {
    std::string text; unsigned finishes=0; int status=0;
    void write(const char* bytes,size_t size) override { text.append(bytes,size); }
    void finish(int value) override { ++finishes; status=value; }
    void note(const char*,const char*) override { require(false,"unexpected transport metadata"); }
    void blob(const char*,const uint8_t*,size_t) override { require(false,"unexpected transport blob"); }
    void request_reset() override { require(false,"unexpected transport reset"); }
};
static void transport(ppc::Machine& m) {
    Capture capture; m.sink=&capture; m.done=false;
    require(m.write(0xfff00000,1,'A') && m.write(0xfff00008,4,0x89abcdef) && m.write(0xfff00000,1,'\n'),"test transport writes");
    require(capture.text=="A89abcdef\n" && capture.finishes==0,"transport did not preserve observations");
    uint32_t value=0;
    require(!m.write(0xfff00000,4,0) && !m.write(0xfff00008,1,0) && !m.write(0xfff00004,1,0) && !m.read(0xfff00008,4,value),"transport widths/read");
    require(m.write(0xfff00004,4,0) && m.done && capture.finishes==1 && capture.status==-1,"transport claimed a verdict");
    m.cpu.steps=0; m.step(); require(m.cpu.steps==0,"completed execution advanced"); m.sink=nullptr; m.done=false;
}
static void memory_and_control(ppc::Machine& m) {
    uint32_t value=0;
    for(unsigned width:{1u,2u,4u}) for(uint32_t address:{0x100u,0x80000100u,0xc0000100u}) {
        require(m.write(address,width,0x89abcdef),"alias write"); require(m.read(0x100,width,value) && value==(0x89abcdefu& (width==4?0xffffffffu:width==2?65535u:255u)),"big endian width");
    }
    require(m.write(0x100,4,0x89abcdef) && m.read(0x100,1,value) && value==0x89 && m.read(0x102,2,value) && value==0xcdef,"byte order");
    const auto end=m.ram.size()-1; m.ram[end]=0x55;
    require(!m.write(uint32_t(end),4,0) && m.ram[end]==0x55,"partial boundary mutation");
    for(uint32_t address:{0x01800000u,0x20000000u,0xa0000000u,0xe0000000u}) require(!m.read(address,4,value),"invalid alias");
    require(!m.read(0,3,value) && !m.write(0,3,0),"invalid width");
    m.cpu={}; m.cpu.r[0]=0x1000; execute(m,14u<<26|3<<21|0xffff); require(m.cpu.r[3]==0xffffffff,"ADDI RA0/signed");
    execute(m,15u<<26|3<<21|0x8000); require(m.cpu.r[3]==0x80000000,"ADDIS signed");
    execute(m,24u<<26|3<<21|0xffff); require(m.cpu.r[0]==0x8000ffff,"ORI destination zero register");
    m.cpu.r[3]=0x1234; execute(m,25u<<26|3<<21|4<<16|0x89ab); require(m.cpu.r[4]==0x89ab1234,"ORIS");
    m.cpu.r[3]=0x80000100; m.cpu.r[4]=0x89abcdef;
    for(auto spec:{std::array<unsigned,3>{36,32,4},{44,40,2},{38,34,1}}) {
        execute(m,spec[0]<<26|4<<21|3<<16); require(!m.cpu.unsupported,"store decode");
        execute(m,spec[1]<<26|5<<21|3<<16); require(!m.cpu.unsupported && m.cpu.r[5]==(spec[2]==4?0x89abcdefu:spec[2]==2?0xcdefu:0xefu),"load decode");
        m.cpu.r[3]=0x80000101; execute(m,spec[1]<<26|5<<21|3<<16); require(m.cpu.unsupported==(spec[2]>1),"alignment diagnostic"); m.cpu.r[3]=0x80000100;
    }
    require(m.write(0x100,4,0x12345678),"seed literal address"); m.cpu.r[0]=0xfff00000; execute(m,32u<<26|5<<21|0x100); require(!m.cpu.unsupported && m.cpu.r[5]==0x12345678,"load RA0 literal");
    for(unsigned number:{1u,8u,9u}) {
        m.cpu.r[3]=0xffffffff; execute(m,spr(3,number,true)); execute(m,spr(4,number,false)); require(m.cpu.r[4]==(number==1?0xe000007fu:0xffffffffu),"SPR transfer");
        execute(m,spr(4,number,false)|1); require(m.cpu.unsupported,"SPR Rc diagnostic");
    }
    for(unsigned number=0;number<1024;++number) if(number!=1 && number!=8 && number!=9) {
        execute(m,spr(4,number,false)); require(m.cpu.unsupported,"unknown SPR read diagnostic");
        execute(m,spr(4,number,true)); require(m.cpu.unsupported,"unknown SPR write diagnostic");
    }
    m.cpu.r[3]=0x80000101; m.cpu.r[4]=0xabcdef12; const auto untouched=m.ram[0x101];
    execute(m,36u<<26|4<<21|3<<16); require(m.cpu.unsupported && m.ram[0x101]==untouched,"unaligned store mutation");
    m.cpu.r[3]=0x81800000; m.cpu.r[4]=0xabcdef12;
    execute(m,32u<<26|4<<21|3<<16); require(m.cpu.unsupported && m.cpu.r[4]==0xabcdef12,"invalid load mutation");
    for(unsigned mask=0;mask<256;++mask) {
        m.cpu.cr=0xa5a5a5a5; m.cpu.r[3]=0x12345678; uint32_t want=0xa5a5a5a5;
        for(unsigned field=0;field<8;++field) if(mask&(128>>field)) { auto bits=0xf0000000u>>(field*4); want=(want&~bits)|(0x12345678&bits); }
        execute(m,31u<<26|3<<21|mask<<12|144<<1); require(!m.cpu.unsupported && m.cpu.cr==want,"MTCRF field mask");
        execute(m,31u<<26|4<<21|19<<1); require(m.cpu.r[4]==want,"MFCR");
    }
    for(uint32_t ins:{31u<<26|19u<<1|1u,31u<<26|19u<<1|0x800u,31u<<26|144u<<1|1u,31u<<26|144u<<1|0x100000u,0u}) { execute(m,ins); require(m.cpu.unsupported,"reserved opcode diagnostic"); }
    execute(m,0x4bfffffd); require(m.cpu.pc==0x80001ffc && m.cpu.lr==0x80002004,"backward BL");
    execute(m,0x48000013); require(m.cpu.pc==0x10 && m.cpu.lr==0x80002004,"absolute BL");
    m.cpu.lr=0x80000103; execute(m,0x4e800021); require(m.cpu.pc==0x80000100 && m.cpu.lr==0x80002004,"BLRL uses old LR");
    m.cpu.lr=0x80000103; execute(m,0x4e800020); require(m.cpu.pc==0x80000100 && m.cpu.lr==0x80000103,"BLR");
}
static void loader_states(ppc::Machine& m,bool wii) {
    auto rom=image(wii); require(m.load(rom.data(),rom.size()),"valid marked image"); m.frame();
    std::vector<uint8_t> before,after,replay; require(m.save(before),"save"); m.frame(); require(m.save(after),"second save");
    require(m.restore(before),"restore"); m.frame(); require(m.save(replay) && replay==after,"exact machine replay");
    require(m.restore(before),"restore before invalid inputs");
    const auto unchanged=[&]() { require(m.save(replay) && replay==before,"invalid input mutated machine"); };
    for(unsigned position:{0u,8u,11u,12u,15u,16u,19u,20u,31u}) { auto bad=rom; bad[position]^=1; require(!m.load(bad.data(),bad.size()),"bad image accepted"); unchanged(); }
    require(!m.load(nullptr,rom.size()) && !m.load(rom.data(),rom.size()-1),"invalid image size/null"); unchanged();
    for(size_t position:{size_t(0),size_t(15),size_t(16),size_t(31),size_t(40),size_t(48),size_t(307),size_t(311),size_t(320),size_t(328),size_t(352)}) {
        auto bad=before; bad[position]^= (position==307 || position==311)?1:2; require(!m.restore(bad),"invalid state accepted"); unchanged();
    }
    auto bad=before; bad.pop_back(); require(!m.restore(bad),"truncated state"); unchanged();
    ppc::Machine other(!wii); auto otherrom=image(!wii); require(other.load(otherrom.data(),otherrom.size()) && !other.restore(before),"cross profile state");
    auto wrongrom=rom; put(wrongrom,32,14u<<26|3<<21|3<<16|2);
    require(m.load(wrongrom.data(),wrongrom.size()) && !m.restore(before),"state from other ROM accepted");
    require(m.load(rom.data(),rom.size()),"reload original image");
    m.reset(); require(m.cpu.pc==0x80000000 && m.cpu.r[3]==0 && m.frames==0 && !m.done && !m.cpu.unsupported,"reset");
    m.rom.clear(); m.reset(); require(!m.save(replay),"unloaded save");
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"two plugin paths required"); uint64_t count=0;
        for(bool wii:{false,true}) {
            ppc::Machine m(wii); count+=arithmetic_checks(m); memory_and_control(m); transport(m); loader_states(m,wii);
            const auto rom=image(wii); emulator_test::LoadedCore core(argv[wii?2:1],rom); auto& plugin=*core.plugin;
            const auto info=plugin.get_info(); require(std::string(info.name)==(wii?"Wii":"GameCube") && std::string(info.file_extensions[0])==(wii?".vwi":".vgc"),"plugin identity");
            std::vector<uint8_t> before,after,replay; require(plugin.save_state(before),"plugin save");
            plugin.run_frame({}); require(plugin.save_state(after),"plugin frame save"); require(plugin.load_state(before),"plugin restore"); plugin.run_frame({}); require(plugin.save_state(replay) && replay==after,"plugin replay");
            require(plugin.get_frame_count()==1 && plugin.get_cycle_count()==100000 && std::string(plugin.test_channels())=="ppcdebug","plugin counters/channel");
            require(plugin.get_framebuffer().width==640 && plugin.get_audio().sample_count==0,"blank graphics/audio metadata");
            plugin.unload_rom(); plugin.run_frame({}); require(!plugin.is_rom_loaded() && plugin.get_rom_crc32()==0 && plugin.get_frame_count()==0 && !plugin.save_state(replay),"plugin unload");
        }
        std::cout<<count<<" PowerPC arithmetic/register snapshots matched; loader, memory, branches, states and both plugins passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
