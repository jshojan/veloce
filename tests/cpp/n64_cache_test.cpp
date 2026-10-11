#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
static uint64_t cached(uint32_t p) {return n64::sign32(0x80000000u|p);}
static uint64_t uncached(uint32_t p) {return n64::sign32(0xa0000000u|p);}
static uint64_t read(n64::Machine& m,uint32_t p,unsigned bytes=4) {uint64_t v=0;require(m.read(uncached(p),bytes,v),"bus read");return v;}
static void write(n64::Machine& m,uint32_t p,unsigned bytes,uint64_t v) {require(m.write(uncached(p),bytes,v),"bus write");}
static void operation(n64::Machine& m,unsigned op,uint32_t p,uint32_t& tag) {require(m.cpu.caches.operation(m,op,cached(p),tag),"cache operation");}
int main(int argc,char** argv) {
    try {
        require(argc==3,"plugin and original ROM required");std::ifstream f(argv[2],std::ios::binary);require(f.good(),"fixture");
        std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;require(m.load(rom.data(),rom.size()),"load");auto& c=m.cpu.caches;uint32_t tag=0;
        for(uint32_t page=0;page<1048576;++page) {
            tag=(page<<8)|128;operation(m,8,0x1000,tag);tag=0;operation(m,4,0x1000,tag);require(tag==((page<<8)|128),"all I physical tags");
            tag=(page<<8)|128;operation(m,9,0x2000,tag);tag=0;operation(m,5,0x2000,tag);require(tag==((page<<8)|192) && c.data[0].flags==1,"all D clean tags/read-valid encoding");
            tag=(page<<8)|192;operation(m,9,0x2000,tag);require(c.data[0].flags==3,"dirty tag state");
        }
        c.reset();
        for(unsigned index=0;index<512;++index) for(unsigned offset=0;offset<32;++offset) {
            tag=((index+7)<<8)|128;operation(m,8,index*32+offset,tag);tag=0;operation(m,4,index*32,tag);require(tag==(((index+7)<<8)|128),"I index/lane");
            tag=((index+5)<<8)|192;operation(m,9,index*16+(offset%16),tag);tag=0;operation(m,5,index*16,tag);require(tag==(((index+5)<<8)|192),"D index/lane");
        }
        c.reset();
        for(unsigned index=0;index<512;++index) for(unsigned word=0;word<8;++word) for(unsigned value=0;value<256;++value) {
            const uint32_t p=0x400000+index*32+word*4;
            operation(m,16,p,tag);write(m,p,4,0x24020000u|value);
            uint64_t observed=0;require(c.read(m,cached(p),4,observed,true,3) && observed==(0x24020000u|value),"I fill word");
            write(m,p,4,0x24020000u|(255-value));
            require(c.read(m,cached(p),4,observed,true,3) && observed==(0x24020000u|value),"instruction DMA/uncached mutation incorrectly coherent");
            operation(m,16,p,tag);
            require(c.read(m,cached(p),4,observed,true,3) && observed==(0x24020000u|(255-value)),"I hit invalidate/refetch");
            write(m,p,4,0x24020011);operation(m,20,p,tag);
            require(c.read(m,cached(p),4,observed,true,3) && observed==0x24020011,"explicit I fill");
        }
        c.reset();
        // Data widths/masked stores, dirty collision eviction and unaffected bytes.
        for(unsigned width:{1u,2u,4u,8u}) for(unsigned lane=0;lane<16;lane+=width) {
            const uint32_t p=0x700000+lane;for(unsigned n=0;n<16;++n)write(m,0x700000+n,1,n+17);
            c.reset();uint64_t old=0;require(c.read(m,cached(p),width,old,false,3),"D fill");
            const uint8_t mask=uint8_t((1u<<width)-1);
            require(c.write(m,cached(p),width,0xfedcba9876543210ull,mask,3),"D store");
            for(unsigned n=0;n<16;++n)require(read(m,0x700000+n,1)==n+17,"dirty store leaked to bus");
            uint64_t collision=0;require(c.read(m,cached(p+8192),width,collision,false,3),"dirty collision fill");
            for(unsigned n=0;n<16;++n)require(read(m,0x700000+n,1)==(n>=lane && n<lane+width?uint8_t(0xfedcba9876543210ull>>(8*(width-1-(n-lane)))):n+17),"dirty eviction payload/guard");
        }
        for(unsigned mask=1;mask<256;++mask) {
            c.reset();write(m,0x700000,8,0x0102030405060708ull);
            require(c.write(m,cached(0x700000),8,0xffeeddccbbaa9988ull,uint8_t(mask),3),"masked cache store");operation(m,21,0x700000,tag);
            for(unsigned n=0;n<8;++n)require(read(m,0x700000+n,1)==(mask&(1u<<n)?uint8_t(0xffeeddccbbaa9988ull>>(56-8*n)):n+1),"cached merge mask bytes");
        }
        c.reset();write(m,0x700000,8,0x12345678);uint64_t observed=0;
        require(c.read(m,cached(0x700000),8,observed,false,3),"DMA stale seed");
        m.pi[0]=0x700000;m.pi[1]=0x10000000;require(m.write(uncached(0x0460000c),4,1),"real PI DMA");
        require(c.read(m,cached(0x700000),8,observed,false,3) && observed==0x12345678,"PI DMA incorrectly snooped D-cache");
        operation(m,17,0x700000,tag);require(c.read(m,cached(0x700000),2,observed,false,3) && observed==0x8037,"DMA invalidate visibility");
        // Miss operations preserve the resident line; index writeback uses its tag.
        c.reset();write(m,0x700000,8,0x1111);
        require(c.write(m,cached(0x700000),8,0x2222,255,3),"index dirty seed");
        operation(m,17,0x702000,tag);operation(m,25,0x702000,tag);
        require(c.data[0].flags==3 && read(m,0x700000,8)==0x1111,"hit miss changed resident cache");
        operation(m,1,0x702000,tag);
        require(read(m,0x700000,8)==0x2222 && c.data[0].flags==0,"index writeback used operand address instead of tag");
        require(c.write(m,cached(0x700000),8,0x3333,255,3),"create dirty seed");operation(m,13,0x702000,tag);
        require(read(m,0x700000,8)==0x3333 && c.data[0].flags==3 && c.data[0].page==0x702,"create dirty exclusive did not evict/re-tag");
        require(c.write(m,cached(0x702000),8,0x4444,255,3) && c.write(m,cached(0x702008),8,0x5555,255,3),"complete created line");
        operation(m,21,0x702000,tag);require(read(m,0x702000,8)==0x4444 && read(m,0x702008,8)==0x5555 && c.data[0].flags==0,"hit writeback invalidate");
        c.reset();write(m,0x400000,4,0x24020003);require(c.read(m,cached(0x400000),4,observed,true,3),"I writeback seed");write(m,0x400000,4,0x24020007);operation(m,24,0x400000,tag);
        require(read(m,0x400000)==0x24020003 && c.instruction[0].flags==1,"I hit writeback failed to write stale instruction bytes");
        // CPU executes stale and refreshed instruction words, rather than only reading data.
        c.reset();m.cpu.state.cp0[16]=3;write(m,0x400000,4,0x24020003);
        auto step=[&](uint64_t pc) {m.cpu.state.pc=pc;m.cpu.state.next_pc=pc+4;m.cpu.state.delay_slot=false;m.cpu.state.cp0[12]=0;m.cpu.step();};
        step(cached(0x400000));require(m.cpu.state.gpr[2]==3,"cached instruction execution");write(m,0x400000,4,0x24020007);step(cached(0x400000));require(m.cpu.state.gpr[2]==3,"stale instruction execution");operation(m,16,0x400000,tag);step(cached(0x400000));require(m.cpu.state.gpr[2]==7,"refreshed instruction execution");
        // Defined opcodes and privilege/address/undefined-operation exception context.
        for(unsigned op=0;op<32;++op) {
            c.reset();m.cpu.state.gpr[1]=cached(0x700000);m.cpu.state.cp0[28]=0;
            write(m,0x04000000,4,(47u<<26)|(1u<<21)|(op<<16));step(uncached(0x04000000));
            require(bool(m.cpu.state.cp0[12]&2)==!n64::PrimaryCaches::defined(op),"CACHE subopcode routing");
        }
        c.reset();write(m,0x400000,4,(47u<<26)|(1u<<21)|(8u<<16));m.cpu.state.gpr[1]=cached(0x700000);
        require(m.cpu.tlb.write(0,0x400000,0,(0x400u<<6)|22,(0x401u<<6)|22),"user instruction mapping");
        m.cpu.state.pc=0x400000;m.cpu.state.next_pc=m.cpu.state.pc+4;m.cpu.state.cp0[12]=16;m.cpu.step();require((m.cpu.state.cp0[13]&0x7c)==44 && !(m.cpu.state.cp0[13]&0x30000000),"CACHE coprocessor unusable after legal user fetch");
        write(m,0x04000000,4,(47u<<26)|(1u<<21)|(16u<<16));
        m.cpu.state.gpr[1]=0x700000;step(uncached(0x04000000));require((m.cpu.state.cp0[13]&0x7c)==8 && m.cpu.state.cp0[8]==0x700000,"CACHE hit operation TLB refill");
        m.cpu.tlb.reset();
        c.reset();m.cpu.state.cp0[16]=3;write(m,0x700000,8,0xabcdef);require(c.write(m,cached(0x700000),8,0x12345678,255,3),"pending dirty state");
        require(c.read(m,cached(0x400000),4,observed,true,3),"pending I state");
        std::vector<uint8_t> state,after,replay;require(m.save(state),"cache save");operation(m,21,0x700000,tag);operation(m,0,0x400000,tag);m.save(after);
        require(m.restore(state),"cache restore");operation(m,21,0x700000,tag);operation(m,0,0x400000,tag);m.save(replay);require(after==replay,"dirty/stale state exact replay");
        for(size_t at:{after.size()-80960,after.size()-80960+511*48,after.size()-56384,after.size()-40032}) {
            auto bad=after;bad[at+5]=16;require(!m.restore(bad) && m.save(replay) && replay==after,"cache tag field rejection atomicity");
            bad=after;bad[at+15]=2;require(!m.restore(bad) && m.save(replay) && replay==after,"cache flag rejection atomicity");
        }
        auto old=state;old.resize(old.size()-80960);old[6]='0';old[7]='6';require(m.restore(old) && m.cpu.caches.data[0].flags==0,"legacy invalid caches");
        // Run the real shared plugin to completion; its host oracle grades the file separately.
        emulator_test::LoadedCore core(argv[1],rom);core.plugin->run_frame({});require(core.plugin->save_state(state) && m.restore(state) && m.cpu.state.cp0[16]==3,"loaded cached plugin snapshot");
        std::cout<<"1048576 physical tags and 1048576 instruction staleness/refill cases matched; indices, dirty eviction, widths/merge masks, real DMA staleness, CPU CACHE/privilege, replay, rejected states and loaded plugin passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
