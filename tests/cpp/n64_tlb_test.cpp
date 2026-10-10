#include "machine.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
using Fault=n64::Tlb::Fault;
static void require(bool ok,const char* msg) {if(!ok)throw std::runtime_error(msg);}
static constexpr uint64_t pc=0xffffffffa0600000ull;
static void code(n64::Machine& m,uint32_t ins) {require(m.write(pc,4,ins),"instruction write");}
static void prepare(n64::Machine& m,uint32_t status=0) {
    m.cpu.state={};m.cpu.state.cp0[12]=status;m.cpu.state.cp0[16]=2;m.cpu.state.cp0[1]=31;
    m.cpu.state.pc=pc;m.cpu.state.next_pc=pc+4;
}
int main(int argc,char** argv) {
 try {
    require(argc==2,"original ROM required");std::ifstream f(argv[1],std::ios::binary);
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;
    require(m.load(rom.data(),rom.size()),"ROM load");auto& tlb=m.cpu.tlb;auto& s=m.cpu.state;
    uint64_t asid_checks=0,offset_checks=0;
    for(unsigned shift=0;shift<=12;shift+=2) {
        const unsigned page=4096u<<shift,mask=2*page-8192;
        for(unsigned stored=0;stored<256;++stored)for(unsigned current=0;current<256;++current)
        for(unsigned globals=0;globals<4;++globals)for(unsigned half=0;half<2;++half) {
            require(tlb.write(0,0x20000000+stored,mask,0x123456 | (globals&1),0x234566 | ((globals>>1)&1)),"defined page write");
            auto t=tlb.translate(0x20000035ull+uint64_t(half)*page,0,uint8_t(current),2,false);
            const bool match=stored==current || globals==3;
            require(t.fault==(match?Fault::none:Fault::miss),"all ASID/global pairs");
            if(match) {const uint32_t frame=half?0x234566:0x123456;const uint32_t expected=((frame/64)*4096/page)*page+53;
                require(t.physical==expected && t.cached==(((frame/8)%8)!=2),"page half/PFN alignment/cache attribute");}
            ++asid_checks;
        }
        // Every byte within 4 KiB, and every larger-page 4 KiB boundary plus last byte.
        require(tlb.write(0,0x20000000,mask,0x123457,0x234567),"offset entry");
        for(unsigned half=0;half<2;++half) {
            for(unsigned offset=0;offset<4096;++offset) {
                auto t=tlb.translate(0x20000000ull+uint64_t(half)*page+offset,0,0,2,false);
                const unsigned frame=half?0x234567:0x123457;
                require(t.fault==Fault::none && t.physical==((frame/64)*4096/page)*page+offset,"all low page lanes");++offset_checks;
            }
            for(unsigned offset=0;offset<page;offset+=4096)for(unsigned lane:{0u,4095u}) {
                auto t=tlb.translate(0x20000000ull+uint64_t(half)*page+offset+lane,0,0,2,false);
                const unsigned frame=half?0x234567:0x123457;
                require(t.fault==Fault::none && t.physical==((frame/64)*4096/page)*page+offset+lane,"large-page boundaries");++offset_checks;
            }
        }
    }
    uint64_t permission_checks=0;
    for(unsigned shift=0;shift<=12;shift+=2)for(unsigned even=0;even<64;++even)for(unsigned odd=0;odd<64;++odd)
    for(unsigned half=0;half<2;++half)for(bool store:{false,true})for(bool mismatch:{false,true}) {
        const unsigned page=4096u<<shift;tlb.write(0,0x20000007,page*2-8192,(0x600u<<6)|even,(0x601u<<6)|odd);
        auto t=tlb.translate(0x20000020ull+uint64_t(half)*page,0,mismatch?6:7,2,store);
        const unsigned attr=half?odd:even;const bool match=!mismatch || ((even%2)*(odd%2));
        const Fault want=!match?Fault::miss:!(attr/2%2)?Fault::invalid:store && !(attr/4%2)?Fault::modified:Fault::none;
        require(t.fault==want,"independent even/odd valid/dirty/global/CCA fields");
        if(want==Fault::none)require(t.cached==(attr/8%8!=2),"all mapped CCAs");++permission_checks;
    }
    for(unsigned raw=0;raw<4096;++raw) {
        const auto before=tlb.entries[0];const unsigned mask=raw*8192;
        bool valid=raw==0 || raw==3 || raw==15 || raw==63 || raw==255 || raw==1023 || raw==4095;
        require(tlb.write(0,0x20000000,mask,22,22)==valid,"all page-mask inputs");
        if(!valid)require(tlb.entries[0].hi==before.hi && tlb.entries[0].mask==before.mask,"undefined mask rejection atomicity");
    }
    // Full twenty-bit frame domain, uncached xkphys and every CCA.
    uint64_t frame_checks=0;
    for(unsigned frame=0;frame<1048576;++frame)for(unsigned half=0;half<2;++half) {
        tlb.write(0,0x20000000,0,(frame*64)|22,((1048575-frame)*64)|22);
        auto t=tlb.translate(0x20000039+half*4096,0,0,2,false);
        require(t.fault==Fault::none && t.physical==(half?1048575-frame:frame)*4096+57 && !t.cached,"all physical frame numbers");++frame_checks;
    }
    tlb.reset();
    for(unsigned region:{0u,1u,3u})for(unsigned mode=0;mode<3;++mode)for(unsigned shift:{0u,2u,12u}) {
        const uint64_t va=(uint64_t(region)<<62)|0x1234000020ull;
        const uint32_t status=(mode<<3)|32|64|128;const uint32_t page=4096u<<shift,mask=page*2-8192;
        tlb.write(0,va,mask,23,23);auto t=tlb.translate(va,status,0,2,false);
        require(t.fault==((region==3 && mode!=0)||(region==1 && mode==2)?Fault::address:Fault::none),"64-bit mapped region privileges");
    }
    for(unsigned cca=0;cca<8;++cca)for(unsigned mode=0;mode<3;++mode)for(unsigned enabled=0;enabled<2;++enabled) {
        const uint64_t va=0x8000000000123450ull|(uint64_t(cca)<<59);
        auto t=tlb.translate(va,(mode<<3)|(enabled?224:0),0,2,false);
        require(t.fault==((mode || !enabled)?Fault::address:Fault::none),"xkphys kernel/64-bit privilege");
        if(!mode && enabled)require(t.physical==0x123450 && t.cached==(cca!=2),"xkphys CCA and physical bits");
        require(tlb.translate(va|(1ull<<32),(mode<<3)|224,0,2,false).fault==Fault::address,"xkphys reserved address bits");
    }
    for(unsigned mode=0;mode<3;++mode)for(unsigned segment=0;segment<8;++segment)for(unsigned extended=0;extended<2;++extended) {
        const uint32_t low=segment*0x20000000u+32;const uint64_t va=n64::sign32(low);
        const uint32_t status=(mode<<3)|(extended?224:0);
        const bool denied=(mode==2 && segment>=4)||(mode==1 && (segment==4 || segment==5 || segment==7));
        tlb.write(0,va,0,23,23);auto t=tlb.translate(va,status,0,2,false);
        require(t.fault==(denied?Fault::address:Fault::none),"compatibility segment privilege matrix");
    }
    for(unsigned bit=40;bit<62;++bit) {
        require(tlb.translate((1ull<<bit)|32,128,0,2,false).fault==Fault::address,"extended user region hole");
        require(tlb.translate((1ull<<bit)|32,0,0,2,false).fault==Fault::address,"32-bit noncanonical address");
    }
    require(tlb.translate(32,4,0,3,false).physical==32 && !tlb.translate(32,4,0,3,false).cached,"ERL physical user segment");
    tlb.reset();
    // Every entry/index bit-5 alias, masked CP0 writes and TLBR global normalization.
    for(unsigned index=0;index<64;++index)for(unsigned globals=0;globals<4;++globals) {
        prepare(m);s.gpr[1]=0xffffffff80000000ull|index;code(m,0x40810000);m.cpu.step();
        s.cp0[10]=0x20000000;s.cp0[5]=0x6000;s.cp0[2]=0x123456|(globals&1);s.cp0[3]=0x234566|((globals>>1)&1);
        code(m,0x42000002);s.pc=pc;s.next_pc=pc+4;m.cpu.step();
        code(m,0x42000001);s.pc=pc;s.next_pc=pc+4;s.cp0[10]=0;m.cpu.step();
        require(s.cp0[10]==0x20000000 && s.cp0[5]==0x6000 && s.cp0[2]==(0x123456u|unsigned(globals==3)) && s.cp0[3]==(0x234566u|unsigned(globals==3)),"TLBWI/TLBR indexed and global AND");
        tlb.reset();
    }
    for(unsigned wired=0;wired<64;++wired) {
        prepare(m);s.gpr[1]=wired;code(m,0x40813000);m.cpu.step();require(s.cp0[6]==wired && s.cp0[1]==31,"Wired resets Random");
        s.cp0[10]=0x20000000;s.cp0[2]=22;s.cp0[3]=22;
        for(unsigned n=0;n<128;++n) {
            code(m,0x42000006);s.pc=pc;s.next_pc=pc+4;const unsigned previous=s.cp0[1]&31;m.cpu.step();
            const unsigned expected=previous<=wired%32?31:previous-1;
            require((s.cp0[1]&31)==expected && (s.cp0[1]&31)>=wired%32 && tlb.entries[expected].hi==0x20000000,"TLBWR and functional Random range");
        }
        tlb.reset();
    }
    for(unsigned mode=0;mode<3;++mode)for(unsigned control=0;control<8;++control)for(unsigned cu=0;cu<2;++cu)
    for(uint32_t ins:{0x42000001u,0x42000002u,0x42000006u,0x42000008u}) {
        prepare(m,(mode<<3)|control|(cu?0x10000000:0));tlb.reset();
        tlb.write(0,0x400000,0,(0x600u<<6)|23,(0x601u<<6)|23);
        code(m,ins);require(m.write(n64::sign32(0xa0400000),4,ins),"ERL direct instruction seed");
        s.pc=0x400000;s.next_pc=s.pc+4;s.cp0[0]=31;s.cp0[10]=0x20000000;s.cp0[2]=22;s.cp0[3]=22;
        m.cpu.step();const bool usable=mode==0 || (control&6) || cu;
        require(m.cpu.exception_taken==!usable,"all CP0 usable modes and control instructions");
        if(!usable)require((s.cp0[13]&0x3000007c)==44 && tlb.entries[31].hi==0xc00000ffff03e000ull,"CP0 unusable CE0/atomicity");
    }
    // DMTC0 preserves implemented high VPN/region fields; DMFC0 returns them.
    prepare(m,128);tlb.reset();s.gpr[1]=0xc0000056789ab0efull;code(m,0x40a15000);m.cpu.step();
    require(s.cp0[10]==0xc0000056789aa0efull,"DMTC0 EntryHi fill/reserved masking");
    code(m,0x40225000);s.pc=pc;s.next_pc=pc+4;m.cpu.step();require(s.gpr[2]==0xc0000056789aa0efull,"DMFC0 EntryHi full width");
    for(unsigned mode=0;mode<3;++mode)for(unsigned control=0;control<8;++control)for(unsigned cu=0;cu<2;++cu)
    for(unsigned wide=0;wide<2;++wide)for(uint32_t ins:{0x40225000u,0x40a15000u}) {
        prepare(m,(mode<<3)|control|(cu?0x10000000:0)|(wide?224:0));tlb.reset();
        tlb.write(0,0x400000,0,(0x600u<<6)|23,(0x601u<<6)|23);
        code(m,ins);require(m.write(n64::sign32(0xa0400000),4,ins),"DMCP0 fetch seed");
        s.pc=0x400000;s.next_pc=s.pc+4;s.cp0[10]=0x20000000;s.gpr[1]=0xc0000056789ab0efull;s.gpr[2]=77;
        m.cpu.step();const bool kernel=mode==0 || (control&6);const bool usable=kernel || cu;
        const unsigned want=!usable?11:!kernel && !wide?10:0;
        require(m.cpu.exception_taken==bool(want) && (s.cp0[13]&124)==want*4,"DMFC0/DMTC0 32-bit user/supervisor restriction");
        if(want)require(s.cp0[10]==0x20000000 && s.gpr[2]==77,"DMCP0 restricted access atomicity");
    }
    // CPU load/store/fetch faults: no RAM or destination changes on denied access.
    for(bool store:{false,true})for(unsigned kind=0;kind<4;++kind)for(bool delay:{false,true})for(bool bev:{false,true})for(bool nested:{false,true})for(bool extended:{false,true}) {
        prepare(m,(bev?0x400000:0)|(nested?2:0)|(extended?128:0));tlb.reset();
        if(kind)tlb.write(0,0x20000000,0,kind==1?16:kind==2?18:22,22);
        s.gpr[1]=0x20000020;s.gpr[2]=0xdeadbeef;s.cp0[14]=123;s.cp0[4]=0xabcdef0000000000ull;s.cp0[20]=0x1234567800000000ull;
        s.delay_slot=delay;code(m,store?0xac220000:0x8c220000);require(m.write(n64::sign32(0xa0000020),4,0x11000000),"permission seed");
        const bool fault=kind<2 || (store && kind==2);const unsigned want=kind==2?1:store?3:2;
        m.cpu.step();require(m.cpu.exception_taken==fault,"CPU translated permission outcome");
        if(fault) {
            const uint64_t base=bev?0xffffffffbfc00200ull:0xffffffff80000000ull;
            require((s.cp0[13]&124)==want*4 && s.cp0[8]==0x20000020 && s.pc==base+(!kind && !nested?(extended?128:0):384),"miss/invalid/Mod and refill vector");
            require(s.cp0[14]==(nested?123:delay?pc-4:pc) && bool(s.cp0[13]&0x80000000)==(delay && !nested),"TLB EPC/BD/nested preservation");
            require(s.cp0[10]==0x20000000 && s.cp0[4]==0xabcdef0000100000ull && s.cp0[20]==0x1234567800100000ull,"TLB fault Context/XContext/EntryHi");
            require(m.ram[32]==17 && s.gpr[2]==0xdeadbeef,"fault mutated RAM/destination");
        } else require(store?m.ram[32]==0xde:s.gpr[2]==0x11000000,"successful translated access");
    }
    // Mapped instruction fetch, CCA, alias cache index and raw fetch fault.
    prepare(m);tlb.reset();tlb.write(0,0x20000000,0,(0x600u<<6)|22,(0x601u<<6)|22);
    code(m,0x24020077);s.pc=0x20000000;s.next_pc=s.pc+4;m.cpu.step();
    require(s.gpr[2]==0x77 && m.cpu.instruction_word==0x24020077 && !m.cpu.exception_taken,"mapped production CPU instruction fetch");
    prepare(m);tlb.reset();s.pc=0x20000000;s.next_pc=s.pc+4;m.cpu.step();
    require((s.cp0[13]&124)==8 && !m.cpu.instruction_valid && s.cp0[14]==0x20000000,"instruction TLB refill atomicity");
    prepare(m);tlb.reset();m.cpu.caches.reset();tlb.write(0,0x20002000,0,(0x600u<<6)|30,(0x601u<<6)|30);
    s.pc=0x20002000;s.next_pc=s.pc+4;m.cpu.step();require(m.cpu.caches.instruction[256].flags==1 && m.cpu.caches.instruction[0].flags==0,"virtually indexed physical instruction tag");
    prepare(m);s.gpr[1]=0x20002000;s.gpr[2]=0x12345678;code(m,0xac220000);m.cpu.step();
    require(m.cpu.caches.data[0].flags==3 && m.ram[0x600000]==0xac,"translated cached store remained dirty");
    // DMA/uncached writes do not snoop mapped I/D aliases.
    require(m.write(pc,4,0x24020033),"uncached mutation");s.pc=0x20002000;s.next_pc=s.pc+4;m.cpu.step();require(s.gpr[2]==0x77,"mapped I alias was coherent");
    uint32_t tag=0;require(m.cpu.caches.operation_translated(m,16,0x20002000,0x600000,tag),"mapped invalidate");
    s.pc=0x20002000;s.next_pc=s.pc+4;m.cpu.step();require(s.gpr[2]==0x33,"mapped I invalidate/refetch");
    // Alignment beats a missing mapping; bus failures preserve BadVAddr.
    prepare(m);tlb.reset();s.gpr[1]=0x20000021;code(m,0x8c220000);m.cpu.step();
    require((s.cp0[13]&124)==16 && s.cp0[8]==0x20000021 && s.cp0[10]==0,"alignment before translation");
    prepare(m);tlb.reset();tlb.write(0,0x20000000,0,(0x20000u<<6)|22,22);
    s.gpr[1]=0x20000020;s.cp0[8]=123;s.gpr[2]=77;code(m,0x8c220000);m.cpu.step();
    require((s.cp0[13]&124)==28 && s.cp0[8]==123 && s.gpr[2]==77 && m.cpu.access_address==0x20000020,"mapped physical bus rejection/BadVAddr preservation");
    prepare(m);tlb.reset();tlb.write(0,0x20000000,0,(0x20000u<<6)|22,22);
    s.pc=0x20000000;s.next_pc=s.pc+4;s.cp0[8]=123;m.cpu.step();
    require((s.cp0[13]&124)==24 && s.cp0[8]==123 && !m.cpu.instruction_valid,"mapped fetch bus rejection");
    // Restore the meaningful mapped/dirty-cache state for serialization checks.
    prepare(m);tlb.reset();tlb.write(0,0x20002000,0,(0x600u<<6)|30,(0x601u<<6)|30);
    // Full state preserves mappings and dirty cache, and rejects malformed entries atomically.
    std::vector<uint8_t> saved,replay;require(m.save(saved),"TLB state save");tlb.reset();require(m.restore(saved) && m.save(replay) && replay==saved,"TLB state restore");
    for(unsigned index=0;index<32;++index)for(unsigned field=0;field<4;++field) {
        auto bad=saved;const size_t at=bad.size()-1184+index*32+field*8;
        const uint64_t value=field==0?1ull<<40:field==1?8192:field==2?1ull<<26:1;
        for(unsigned byte=0;byte<8;++byte)bad[at+byte]=uint8_t(value>>(56-byte*8));
        require(!m.restore(bad) && m.save(replay) && replay==saved,"invalid TLB entry state atomicity");
    }
    auto old=saved;old.resize(old.size()-1184);old[7]='0';require(m.restore(old) && tlb.probe(0x20002000)==-1,"S010 neutral TLB conversion");
    prepare(m);tlb.reset();s.gpr[1]=0x20000020;s.cp0[8]=123;code(m,0x8c220000);
    require(m.save(saved),"pending TLB fault save");m.cpu.step();std::vector<uint8_t> after;m.save(after);
    require(m.restore(saved),"pending TLB restore");m.cpu.step();m.save(replay);require(replay==after,"TLB fault exact continuation");
    std::cout<<asid_checks<<" ASID/global/half cases, "<<frame_checks<<" physical frames, "<<offset_checks<<" page offsets, "<<permission_checks<<" independent permission/CCA cases and 4096 mask encodings passed; segments/CCA, indexed/random controls, CPU faults/fetch/cache aliases and state continuation passed\n";
    return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
