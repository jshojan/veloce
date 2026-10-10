#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
static uint64_t address(unsigned reg) {return n64::sign32(0xa4040000+reg*4);}
static void put(n64::Machine& m,unsigned reg,uint32_t v) {require(m.write(address(reg),4,v),"SP write");}
static uint32_t get(n64::Machine& m,unsigned reg) {uint64_t v=0;require(m.read(address(reg),4,v),"SP read");return uint32_t(v);}
int main(int argc,char** argv) {
 try {
    require(argc==3,"plugin and original SP ROM required");
    std::ifstream file(argv[2],std::ios::binary);require(file.good(),"ROM missing");
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
    n64::Machine m;require(m.load(rom.data(),rom.size()),"load SP ROM");
    auto& sp=m.sp_interface;
    constexpr unsigned command_bits[]={0,5,7,9,11,13,15,17,19,21,23,3};
    constexpr unsigned status_bits[]={0,5,6,7,8,9,10,11,12,13,14};
    for(unsigned seed=0;seed<4096;++seed)for(unsigned field=0;field<12;++field)for(unsigned command=0;command<4;++command) {
        unsigned initial=0;for(unsigned b=0;b<11;++b)if(seed&(1<<b))initial|=1<<status_bits[b];
        sp.status=initial|2;sp.interrupt=seed>>11;
        put(m,4,0xfe000004u|(command<<command_bits[field]));
        unsigned wanted=initial,irq=seed>>11;
        if(command==1 || command==2) {
            if(field==11)irq=command-1;
            else if(command==1)wanted&=~(1u<<status_bits[field]);
            else wanted|=1u<<status_bits[field];
        }
        require(get(m,4)==wanted && sp.interrupt==irq,"status command transition/broke clear");
    }
    for(unsigned raw=0;raw<65536;++raw) {
        put(m,7,0xffff0000|raw);require(get(m,7)==0 && get(m,7)==1 && get(m,7)==1,"all semaphore write values");
        require(m.write(n64::sign32(0xa4080000),4,0xffff0000|raw),"PC write");uint64_t v=0;
        require(m.read(n64::sign32(0xa4080000),4,v) && v==(raw&4092),"PC field mask");
    }
    // Both directions, every 12-bit raw length, both banks and four wrap boundaries.
    for(unsigned direction=0;direction<2;++direction)for(unsigned bank:{0u,4096u})for(unsigned start:{0u,8u,4080u,4088u})for(unsigned raw=0;raw<4096;++raw) {
        const unsigned bytes=(raw/8+1)*8;
        for(unsigned n=0;n<8192;++n) {m.sp[n]=uint8_t(n*37+91+(n/4096)*17);m.ram[0x700000+n]=uint8_t(n*11+23);}
        put(m,0,0xffffe007u|bank|start);put(m,1,0xff700007);put(m,direction?3:2,raw);
        require(get(m,0)==bank+((start+bytes)%4096),"DMA bank-wrapped SP pointer");
        for(unsigned n=0;n<8192;++n) {
            if(direction) {
                const uint8_t wanted=n<bytes?uint8_t(((start+n)%4096+bank)*37+91+(bank/4096)*17):uint8_t(n*11+23);
                require(m.ram[0x700000+n]==wanted,"DMA RAM byte/guard");
                require(m.sp[n]==uint8_t(n*37+91+(n/4096)*17),"DMA write source mutated");
            } else {
                const unsigned offset=(n%4096+4096-start)%4096;
                const uint8_t wanted=(n/4096==bank/4096 && offset<bytes)?uint8_t(offset*11+23):uint8_t(n*37+91+(n/4096)*17);
                require(m.sp[n]==wanted,"DMA SP byte/other bank guard");
                require(m.ram[0x700000+n]==uint8_t(n*11+23),"DMA read source mutated");
            }
        }
        require(get(m,4)%32==1 && get(m,5)==0 && get(m,6)==0,"settled DMA flags");
    }
    // Full count/64-bit stride product with 8-byte rows, through both directions.
    sp.reset();
    for(unsigned direction=0;direction<2;++direction)for(unsigned count=0;count<256;++count)for(unsigned stride=0;stride<512;++stride) {
        const unsigned skip=stride*8,rows=count+1;
        std::fill(m.sp.begin(),m.sp.end(),0x55);
        for(unsigned row=0;row<rows;++row)for(unsigned n=0;n<8;++n) {
            m.ram[0x600000+row*(8+skip)+n]=direction?0xa5:uint8_t(row*7+n*31);
            if(direction)m.sp[(4088+row*8+n)%4096]=uint8_t(row*7+n*31);
        }
        put(m,0,4088);put(m,1,0x600000);put(m,direction?3:2,(skip<<20)|(count<<12));
        for(unsigned row=0;row<rows;++row)for(unsigned n=0;n<8;++n)
            require((direction?m.ram[0x600000+row*(8+skip)+n]:m.sp[(4088+row*8+n)%4096])==uint8_t(row*7+n*31),"DMA count/stride rows");
    }
    // CPU caches stay stale across real SP DMA, then reveal memory after invalidation.
    sp.reset();m.cpu.caches.reset();uint64_t v=0;
    m.ram[0x700000]=9;require(m.cpu.caches.read(m,n64::sign32(0x80700000),1,v,false,3) && v==9,"cached DMA seed");
    m.sp[0]=77;put(m,0,0);put(m,1,0x700000);put(m,3,0);
    require(m.cpu.caches.read(m,n64::sign32(0x80700000),1,v,false,3) && v==9,"DMA snooped CPU D cache");
    uint32_t tag=0;require(m.cpu.caches.operation(m,17,n64::sign32(0x80700000),tag),"cache invalidate");
    require(m.cpu.caches.read(m,n64::sign32(0x80700000),1,v,false,3) && v==77,"DMA memory invisible after invalidation");
    // Set/clear source and mask produce a real CPU interrupt, then ERET resumes it.
    m.reset();put(m,4,16);require(m.interrupt_pending()==0,"SP unmasked IRQ");
    require(m.write(n64::sign32(0xa430000c),4,2),"MI enable SP");require(m.interrupt_pending()==4,"SP masked IRQ missing");
    m.cpu.state.cp0[12]=0x401;m.cpu.step();require((m.cpu.state.cp0[12]&2) && (m.cpu.state.cp0[13]&0x7c)==0 && (m.cpu.state.cp0[13]&0x400),"CPU SP interrupt delivery");
    put(m,4,8);require(!m.interrupt_pending() && get(m,4)==1,"SP IRQ acknowledgment changed HALT");
    require(m.write(n64::sign32(0xa0000180),4,0x42000018),"ERET instruction");m.cpu.step();require(!(m.cpu.state.cp0[12]&2),"SP IRQ ERET");
    // Snapshot controls incl. acquired semaphore and staged addresses, exact continuation.
    put(m,0,0x1007);put(m,1,0x700003);put(m,4,0x1555552);get(m,7);
    std::vector<uint8_t> state,after,replay;require(m.save(state),"SP state save");
    auto continuation=[&]() {put(m,2,0x0080200f);put(m,4,0xaaaaad);get(m,7);};
    continuation();require(m.save(after) && m.restore(state),"SP state restore");continuation();require(m.save(replay) && replay==after,"SP exact state replay");
    for(unsigned field=0;field<9;++field) {
        constexpr uint32_t invalid[]={1,1,1,1,1,4,1,2,2};
        auto bad=after;const size_t at=bad.size()-34032+field*8;bad[at+7]|=invalid[field];
        require(!m.restore(bad) && m.save(replay) && replay==after,"SP invalid state rejection atomicity");
    }
    auto old=after;old.resize(old.size()-34032);old[6]='0';old[7]='7';require(m.restore(old) && sp.status==1 && !sp.interrupt && !sp.semaphore,"S007 neutral SP conversion");
    require(m.save(state),"access baseline");
    for(unsigned reg=0;reg<8;++reg)for(unsigned width:{1u,2u,8u}) {v=0xabcdef;require(!m.read(address(reg),width,v) && v==0xabcdef && !m.write(address(reg),width,0),"SP widths");}
    for(unsigned reg=0;reg<8;++reg) {v=0xabcdef;require(!m.read(address(reg)+1,4,v) && v==0xabcdef && !m.write(address(reg)+1,4,0),"SP alignment");}
    for(unsigned reg:{5u,6u})require(!m.write(address(reg),4,0),"SP read-only direction");
    put(m,0,0);put(m,1,0x7ffff8);require(m.save(state),"rejected DMA baseline");
    const auto memory=m.sp;const uint8_t last=m.ram.back();
    require(!m.write(address(2),4,8) && !m.write(address(3),4,0x00100000) && m.save(replay) && replay==state && m.sp==memory && m.ram.back()==last,"DMA bound/stride rejection atomicity");
    emulator_test::LoadedCore core(argv[1],rom);
    for(unsigned frame=0;frame<5000;++frame) {core.plugin->run_frame({});require(core.plugin->save_state(state) && m.restore(state),"loaded plugin SP state");if(m.done)break;}
    require(m.done,"loaded plugin SP corpus incomplete");m.reset();require(sp.snapshot()==n64::SpInterface{}.snapshot(),"SP reset");
    std::cout<<"196608 status transitions, 65536 PC/semaphore values, 65536 full-byte DMA cases and 262144 count/stride transfers passed; guards, interrupt/ERET, cache staleness, state replay/rejection and loaded plugin passed\n";return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
