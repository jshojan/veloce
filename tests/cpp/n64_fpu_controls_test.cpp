#include "machine.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static constexpr uint64_t pc=0xffffffffa0400000ull;
static void instruction(n64::Machine& m,uint32_t word) {require(m.write(pc,4,word),"instruction write");}
static void prepare(n64::Machine& m,uint32_t status=0x20000000) {
    m.cpu.state={};m.cpu.state.pc=pc;m.cpu.state.next_pc=pc+4;
    m.cpu.state.cp0[12]=status;m.cpu.state.cp0[16]=2;
}
int main(int argc,char** argv) {
 try {
    require(argc==2,"original ROM required");std::ifstream f(argv[1],std::ios::binary);
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
    n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& s=m.cpu.state;
    // Full defined control domain: independent field composition and cause/enable test.
    instruction(m,0x44c1f800);
    for(unsigned cause=0;cause<64;++cause)for(unsigned enable=0;enable<32;++enable)
    for(unsigned flags=0;flags<32;++flags)for(unsigned rm=0;rm<4;++rm)for(unsigned cf=0;cf<4;++cf) {
        prepare(m);const uint32_t word=cause*4096+enable*128+flags*4+rm+(cf%2)*0x800000+(cf/2)*0x1000000;
        s.gpr[1]=0xabcdef00fe7c0000ull|word;m.cpu.step();
        const bool trap=(cause>=32)||bool((cause%32)&enable);
        require(s.fcr31==word,"full control domain/high GPR/reserved masking");
        require((s.cp0[13]&124)==(trap?60u:0u) && bool(s.cp0[12]&2)==trap,"enabled cause/FPE delivery");
        require(s.pc==(trap?0xffffffff80000180ull:pc+4),"FPE vector/normal PC");
        if(trap)require(s.cp0[14]==pc && !s.delay_slot,"FPE EPC and delay flag");
    }
    for(unsigned reg=0;reg<32;++reg)for(unsigned control:{0u,31u}) {
        prepare(m);s.fcr31=0x1800003;instruction(m,0x44400000|(reg<<16)|(control<<11));m.cpu.step();
        require(s.gpr[reg]==(reg?(control?0x1800003u:0xb00u):0u),"CFC1 destinations/FCR0 profile");
        prepare(m);s.fcr31=0x1000000;s.gpr[reg]=reg?0xffffffffffffffffull:0;
        instruction(m,0x44c00000|(reg<<16)|(control<<11));m.cpu.step();
        require(s.fcr31==(control?(reg?0x183ffffu:0u):0x1000000u),"CTC1 source zero/read-only FCR0");
    }
    // All signed offsets, true/false and ordinary/likely delay-slot semantics.
    require(m.write(pc+4,4,0x24210001),"delay increment");
    for(unsigned kind=0;kind<4;++kind)for(unsigned condition=0;condition<2;++condition)
    for(unsigned offset=0;offset<65536;++offset) {
        prepare(m);s.fcr31=condition*0x800000;instruction(m,0x45000000|(kind<<16)|offset);m.cpu.step();
        const bool take=condition==kind%2;const int64_t signed_offset=offset<32768?offset:int64_t(offset)-65536;
        require(s.pc==(kind>=2 && !take?pc+8:pc+4),"branch likely annul PC");
        require(s.next_pc==(take?pc+4+uint64_t(signed_offset*4):kind>=2?pc+12:pc+8),"all signed branch offsets");
        require(s.delay_slot==!(kind>=2 && !take),"branch delay marker");
        if(s.pc==pc+4) {m.cpu.step();require(s.gpr[1]==1,"taken/ordinary delay execution");}
        else require(s.gpr[1]==0,"likely annul side effect");
    }
    // Disabled COP1 wins before unsupported decoding; no transfer or branch side effect.
    for(unsigned rs=0;rs<32;++rs)for(unsigned control=0;control<32;++control) {
        prepare(m,0);s.fcr31=3;s.gpr[1]=77;s.cp0[13]=0x30000000;
        instruction(m,0x44010000|(rs<<21)|(control<<11));m.cpu.step();
        require((s.cp0[13]&0x3000007c)==0x1000002c && s.cp0[14]==pc && s.fcr31==3 && s.gpr[1]==77,"CU1 unusable precedence/CE1");
    }
    for(bool unusable:{false,true})for(bool bev:{false,true})for(bool nested:{false,true}) {
        prepare(m,(unusable?0u:0x20000000u)|(bev?0x400000u:0u)|(nested?2u:0u));
        s.delay_slot=true;s.cp0[14]=123;s.cp0[13]=0;s.gpr[1]=0x20000;
        instruction(m,0x44c1f800);m.cpu.step();
        require((s.cp0[13]&124)==(unusable?44u:60u),"delay-slot exception type");
        require(s.cp0[14]==(nested?123:pc-4) && bool(s.cp0[13]&0x80000000)==!nested,"delay/nested EPC BD");
        require(s.pc==(bev?0xffffffffbfc00380ull:0xffffffff80000180ull),"BEV exception vector");
        require(s.fcr31==(unusable?0u:0x20000u),"commit before exception/CU1 atomicity");
    }
    prepare(m);s.fcr31=0x183ffff;s.gpr[1]=0x1234;
    require(m.write(n64::sign32(0xa4500000),4,0x700008) &&
            m.write(n64::sign32(0xa4500008),4,1) &&
            m.write(n64::sign32(0xa4500014),4,15) &&
            m.write(n64::sign32(0xa4500010),4,1023) &&
            m.write(n64::sign32(0xa4500004),4,128),"legacy audio queue seed");
    for(unsigned n=0;n<4097;++n)m.clock();
    std::vector<uint8_t> saved,replay;require(m.save(saved),"control state save");s.fcr31=0;
    require(m.restore(saved) && s.fcr31==0x183ffff && m.save(replay) && replay==saved,"control state exact restore");
    for(unsigned bit=0;bit<64;++bit) {
        if(bit<32 && (bit<=17 || bit==23 || bit==24))continue;
        auto bad=saved;bad[bad.size()-1025-bit/8]|=uint8_t(1u<<(bit%8));
        require(!m.restore(bad) && m.save(replay) && replay==saved,"reserved scalar state rejection atomicity");
    }
    auto old=saved;old.resize(old.size()-1032);old[6]='0';old[7]='9';
    require(m.restore(old) && s.fcr31==0 && s.gpr[1]==0x1234,"S009 neutral control conversion");
    auto converted=saved;std::fill(converted.end()-1032,converted.end()-1024,0);
    require(m.save(replay) && replay==converted,"S009 preserves pending AI/host phases and all existing fields");
    // Resume a pending control exception identically after restore.
    require(m.restore(saved),"pending FCR31 restore");instruction(m,0x44c1f800);s.gpr[1]=0x21080;
    std::vector<uint8_t> before,after;require(m.save(before),"pending exception save");
    m.cpu.step();require(m.save(after) && (s.cp0[13]&124)==60,"pending enabled FPE");
    require(m.restore(before),"pending exception restore");m.cpu.step();
    require(m.save(replay) && replay==after,"pending exception exact continuation");
    m.reset();require(s.fcr31==0,"reset control state");
    std::cout<<"1048576 control combinations, 524288 branch offsets and 1024 CU1 cases passed; transfer destinations, delay/BEV/nested exceptions, atomic states and S009 conversion passed\n";
    return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
