#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
static uint64_t address(unsigned n) {return n64::sign32(0xa4400000+n*4);}
static void put(n64::Machine& m,unsigned n,uint32_t v) {require(m.write(address(n),4,v),"VI write");}
static uint32_t get(n64::Machine& m,unsigned n) {uint64_t v=0;require(m.read(address(n),4,v),"VI read");return uint32_t(v);}
static std::array<uint8_t,4> bytes(uint32_t p) {std::array<uint8_t,4> b;std::memcpy(b.data(),&p,4);return b;}
int main(int argc,char** argv) {
 try {
    require(argc==3,"plugin and original ROM required");std::ifstream f(argv[2],std::ios::binary);
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& v=m.video_interface;
    constexpr uint32_t masks[]={0x1fbff,0xffffff,0xfff,0x3ff,0,0x3fffffff,0x3ff,0x1f0fff,0x0fff0fff,0x03ff03ff,0x03ff03ff,0x03ff03ff,0x0fff0fff,0x0fff0fff,0x7f};
    for(unsigned reg=0;reg<15;++reg)if(reg!=4)for(unsigned bit=0;bit<32;++bit) {
        put(m,reg,1u<<bit);require(get(m,reg)==((1u<<bit)&masks[reg]),"all writable/readable/reserved register bits");
    }
    put(m,0,0);
    for(unsigned value=0;value<16777216;++value) {put(m,1,0xff000000|value);require(get(m,1)==value,"all 24-bit origins");}
    for(unsigned value=0;value<4096;++value) {put(m,2,0xfffff000|value);require(get(m,2)==value,"all width fields");
        put(m,12,0xf000f000|value|(value<<16));put(m,13,0xf000f000|value|(value<<16));require(get(m,12)==(value|(value<<16)) && get(m,13)==get(m,12),"scale/offset state masks");}
    // Full timing/count/interrupt target combinations at one explicit VI clock per line.
    uint64_t irq_checks=0;
    for(unsigned total=0;total<1024;++total)for(unsigned target=0;target<1024;++target) {
        v.reset();put(m,7,0);put(m,8,0);put(m,6,total);put(m,3,target);put(m,0,2);
        unsigned want=0;uint64_t fields=0;
        const unsigned clocks=(total+1)*2+4;
        for(unsigned tick=0;tick<clocks;++tick) {
            v.tick();want+=2;
            if(want>total) {const unsigned field=total%2?want%2:1-want%2;want=field<=total?field:0;++fields;}
            bool irq;
            if(total%2 || target%2)irq=want/2==target/2;
            else if(target==0)irq=want==0 || want==total;
            else irq=want==target-want%2;
            require(get(m,4)==want && v.fields==fields && v.interrupt==irq,"all terminal-count/target progressive/interlaced IRQ cases");
            put(m,4,0xffffffff);require(get(m,4)==want && !v.interrupt,"CURRENT any-write ack preserves counter");
        }
        ++irq_checks;
    }
    // Every horizontal terminal count, including an unchanged leap scanline.
    for(unsigned horizontal=0;horizontal<4096;++horizontal) {
        v.reset();put(m,7,horizontal);put(m,8,(horizontal<<16)|horizontal);put(m,6,525);put(m,0,3);
        for(unsigned tick=0;tick<horizontal;++tick)v.tick();require(get(m,4)==0,"line advanced early");v.tick();require(get(m,4)==2,"missing scanline transition");
        for(unsigned tick=0;tick<=horizontal;++tick)v.tick();require(get(m,4)==4,"leap-equals-normal scanline period");
    }
    v.reset();put(m,7,3|(21u<<16));put(m,8,(7u<<16)|11u);put(m,6,5);put(m,0,2);
    for(unsigned pattern=0;pattern<5;++pattern) {
        for(unsigned tick=0;tick<4;++tick)v.tick();require(get(m,4)==2,"normal line before leap");
        const unsigned leap=(21>>pattern)&1,period=leap?12:8;
        for(unsigned tick=0;tick<period;++tick)v.tick();require(get(m,4)==4,"five-field leap selection");
        for(unsigned tick=0;tick<4;++tick)v.tick();require(get(m,4)==0 && v.pattern==(pattern+1)%5,"field leap rotation");
    }
    v.reset();put(m,7,0);put(m,8,0);put(m,6,5);put(m,3,3);put(m,0,0x42);v.tick();require(v.interrupt && get(m,4)==2,"SERRATE does not select IRQ parity policy");
    put(m,0,0);require(get(m,4)==0,"off CURRENT reset");for(unsigned n=0;n<100;++n)v.tick();require(get(m,4)==0,"off scanline activity");put(m,4,0);require(!v.interrupt,"off interrupt ack");
    v.reset();put(m,7,0);put(m,8,0);put(m,6,5);put(m,3,2);put(m,0,2);v.tick();
    require(!m.interrupt_pending() && (get(m,4)==2),"VI MI mask default");require(m.write(n64::sign32(0xa430000c),4,128),"MI VI enable");
    uint64_t mi=0;require(m.read(n64::sign32(0xa4300008),4,mi) && mi==8 && m.interrupt_pending()==4,"VI MI source/IP2");
    m.cpu.state.cp0[12]=0x401;m.cpu.step();require((m.cpu.state.cp0[13]&0x7c)==0 && (m.cpu.state.cp0[12]&2),"VI CPU interrupt delivery");put(m,4,0);require(!m.interrupt_pending(),"VI IRQ acknowledgement routing");
    // Every RGBA16 code, including all coverage-bit choices, through actual RAM decode.
    v.reset();put(m,1,0x700000);put(m,2,256);put(m,6,525);put(m,9,256);put(m,10,512);put(m,12,1024);put(m,13,1024);put(m,0,0x3302);
    for(unsigned n=0;n<65536;++n) {m.ram[0x700000+n*2]=uint8_t(n>>8);m.ram[0x700001+n*2]=uint8_t(n);}
    std::vector<uint32_t> pixels;int w=0,h=0;v.render(m.ram,pixels,w,h);require(w==256 && h==256,"native grid dimensions");
    for(unsigned n=0;n<65536;++n) {auto b=bytes(pixels[n]);const unsigned r=n/2048%32,g=n/64%32,blue=n/2%32;
        require(b==std::array<uint8_t,4>{uint8_t(r*8+r/4),uint8_t(g*8+g/4),uint8_t(blue*8+blue/4),255},"all RGBA16 words");}
    // Fractional scales/offsets, pitch, ignored upper Y-offset state and bounds.
    put(m,2,256);put(m,9,8);put(m,10,8);put(m,12,(1536u<<16)|512);put(m,13,(0xe00u<<16)|1536);
    // Bits 27:26 are stored but do not contribute to the vertical offset (0x200).
    v.render(m.ram,pixels,w,h);require(w==8 && h==4,"scaled dimensions");
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<8;++x) {
        const unsigned source=(y*1536+512)/1024*256+(x*512+1536)/1024;
        const unsigned r=source/2048%32,g=source/64%32,b=source/2%32;
        require(bytes(pixels[y*8+x])==std::array<uint8_t,4>{uint8_t(r*8+r/4),uint8_t(g*8+g/4),uint8_t(b*8+b/4),255},"fixed-point source coordinates/pitch");
    }
    put(m,1,0xffffff);v.render(m.ram,pixels,w,h);require(std::all_of(pixels.begin(),pixels.end(),[](uint32_t p){return bytes(p)==std::array<uint8_t,4>{0,0,0,255};}),"out-of-RAM pixels bounded black");
    put(m,0,0);v.render(m.ram,pixels,w,h);require(w==320 && h==240 && bytes(pixels[0])==std::array<uint8_t,4>{0,0,0,255},"blank ABI convention");
    // Dirty CPU cache is invisible to VI until writeback.
    v.reset();put(m,1,0x700000);put(m,2,1);put(m,6,525);put(m,9,1);put(m,10,2);put(m,12,1024);put(m,13,1024);put(m,0,0x3303);
    m.ram[0x700000]=17;m.ram[0x700001]=34;m.ram[0x700002]=51;m.ram[0x700003]=0;
    require(m.cpu.caches.write(m,n64::sign32(0x80700000),4,0xaabbcc00,15,3),"dirty VI source");v.render(m.ram,pixels,w,h);
    require(bytes(pixels[0])==std::array<uint8_t,4>{17,34,51,255},"VI snooped CPU data cache");uint32_t tag=0;require(m.cpu.caches.operation(m,25,n64::sign32(0x80700000),tag),"VI source writeback");v.render(m.ram,pixels,w,h);
    require(bytes(pixels[0])==std::array<uint8_t,4>{170,187,204,255},"VI did not see cache writeback");
    v.reset();put(m,7,3093);put(m,8,(3093u<<16)|3093);put(m,6,524);put(m,3,2);put(m,0,2);
    for(unsigned n=0;n<4097;++n)m.clock();std::vector<uint8_t> saved,after,replay;require(m.save(saved),"VI state save");
    for(unsigned n=0;n<100000;++n)m.clock();m.save(after);require(m.restore(saved),"VI restore");for(unsigned n=0;n<100000;++n)m.clock();m.save(replay);require(replay==after,"VI exact continuation");
    for(unsigned field=0;field<20;++field) {
        if(field==17)continue;auto bad=after;const size_t at=bad.size()-5976+field*8;
        const uint64_t invalid=field<15?uint64_t(~masks[field])&0xffffffffu:field==15?1024:field==16?4096:field==18?5:2;
        for(unsigned byte=0;byte<8;++byte)bad[at+byte]=uint8_t(invalid>>(56-byte*8));
        require(!m.restore(bad) && m.save(replay) && replay==after,"VI invalid scalar state atomicity");
    }
    auto old=after;old.resize(old.size()-5976);old[6]='1';old[7]='1';require(m.restore(old) && v.snapshot()==n64::VideoInterface{}.snapshot(),"S011 neutral VI conversion");
    for(unsigned reg=0;reg<16;++reg)for(unsigned width:{1u,2u,8u}) {uint64_t value=77;require(!m.read(address(reg),width,value) && value==77 && !m.write(address(reg),width,0),"VI word width contract");}
    uint64_t value=77;require(!m.read(address(0)+1,4,value) && value==77 && !m.write(address(0)+1,4,0),"VI alignment");require(!m.read(address(15),4,value) && !m.write(address(15),4,0),"pending staged line-buffer data rejected");
    emulator_test::LoadedCore core(argv[1],rom);unsigned captured=0;std::vector<uint32_t> baseline;
    for(unsigned quantum=0;quantum<2000;++quantum) {
        core.plugin->run_frame({});require(core.plugin->save_state(saved),"production save");
        if(saved[639]) {const auto fb=core.plugin->get_framebuffer();require(fb.width==256 && fb.height==256 && fb.pixels,"production video dimensions");baseline.assign(fb.pixels,fb.pixels+65536);
            require(core.plugin->save_state(after) && after==saved && core.plugin->load_state(saved),"get_framebuffer mutated state/restore");const auto replay_fb=core.plugin->get_framebuffer();require(std::equal(baseline.begin(),baseline.end(),replay_fb.pixels),"production video state replay");++captured;}
        if(saved[39])break;
    }
    require(captured==2,"production original color domains missing");core.plugin->reset();auto fb=core.plugin->get_framebuffer();require(fb.width==320 && fb.height==240 && bytes(fb.pixels[0])==std::array<uint8_t,4>{0,0,0,255},"production reset blank video");
    std::cout<<"16777216 origins, "<<irq_checks<<" vertical-total/target sequences, 4096 horizontal periods and 65536 RGBA16 words passed; leap pattern, IRQ/MI/CPU, scaled source/cache/bounds and production two-frame replay passed\n";return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
