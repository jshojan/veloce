#include "vr4300.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:n64::Memory{
    std::array<uint8_t,65536> bytes{};bool fail=false;unsigned stores=0;
    bool read(uint64_t address,unsigned width,uint64_t& value)override{const auto p=uint32_t(address)&0x1fffffff;if(p+width>bytes.size() || (fail && p>=0x8000))return false;value=0;for(unsigned n=0;n<width;++n)value=value*256+bytes[p+n];return true;}
    bool write(uint64_t address,unsigned width,uint64_t value)override{const auto p=uint32_t(address)&0x1fffffff;if(p+width>bytes.size() || (fail && p>=0x8000))return false;++stores;for(unsigned n=0;n<width;++n)bytes[p+n]=uint8_t(value>>(8*(width-n-1)));return true;}
    bool write_masked(uint64_t,unsigned,uint64_t,uint8_t)override{return false;}
    void instruction(uint32_t ins){write(0x400,4,ins);}
};
static uint32_t encode(unsigned op,unsigned base,unsigned target,unsigned imm){return op<<26|base<<21|target<<16|imm;}
int main(){try{
    Bus bus;n64::VR4300 cpu(bus);uint64_t checks=0;
    const uint64_t patterns[]={0,1,0x7fffffff,0x80000000,0xffffffff,0x100000000,0x8000000000000000ull,~uint64_t(0),0x0123456789abcdefull,0xfedcba9876543210ull};
    auto start=[&](){cpu.reset(n64::sign32(0xa0000400));cpu.state.cp0[16]=3;cpu.state.cp0[12]=128;bus.fail=false;};
    auto execute=[&](uint32_t ins){cpu.state.pc=n64::sign32(0xa0000400);if(cpu.state.cp0[12]&24){cpu.state.pc=0x400;cpu.tlb.write(0,0,0,23,23);}cpu.state.next_pc=cpu.state.pc+4;bus.instruction(ins);cpu.step();};
    // Every signed offset, both widths, all selected data patterns, actual cache-backed storage.
    for(unsigned wide=0;wide<2;++wide)for(unsigned imm=0;imm<65536;++imm)for(auto value:patterns){
        start();bus.write(0x8000,8,value);const int offset=imm<32768?int(imm):int(imm)-65536;
        cpu.state.gpr[4]=n64::sign32(uint32_t(0x80008000-offset));execute(encode(wide?52:48,4,5,imm));
        const uint64_t word=value>>32;const uint64_t want=wide?value:word|(word&0x80000000?0xffffffff00000000ull:0);
        require(!cpu.exception_taken && cpu.state.gpr[5]==want,"linked load/sign extension");require(cpu.state.cp0[17]==0x800,"physical LLAddr");
        cpu.state.gpr[6]=~value;execute(encode(wide?60:56,4,6,imm));require(!cpu.exception_taken && cpu.state.gpr[6]==1,"linked conditional success");
        uint64_t actual=0;require(cpu.caches.read(bus,n64::sign32(0x80008000),wide?8:4,actual,false,3) && actual==(wide?~value:uint32_t(~value)),"conditional store data");++checks;
    }
    // All base/target assignments (distinct; source is captured before rt overwrite).
    for(unsigned wide=0;wide<2;++wide)for(unsigned base=1;base<32;++base)for(unsigned target=0;target<32;++target)if(base!=target){
        start();bus.write(0x8000,8,0x89abcdef01234567ull);cpu.state.gpr[base]=n64::sign32(0x80008000);execute(encode(wide?52:48,base,target,0));
        require(!cpu.exception_taken && cpu.state.gpr[target]==(target?(wide?0x89abcdef01234567ull:0xffffffff89abcdefull):0),"register linked load");
        cpu.state.gpr[target]=target?0x1122334455667788ull:0;execute(encode(wide?60:56,base,target,0));require(!cpu.exception_taken && cpu.state.gpr[target]==(target?1:0) && cpu.state.gpr[base]==n64::sign32(0x80008000),"register conditional result");++checks;
    }
    for(unsigned wide=0;wide<2;++wide){
        start();bus.write(0x8000,8,0x1122334455667788ull);cpu.state.gpr[4]=n64::sign32(0x80008000);cpu.state.gpr[6]=99;
        execute(encode(wide?60:56,4,6,0));require(!cpu.exception_taken && !cpu.state.gpr[6],"reset conditional failure");uint64_t raw=0;bus.read(0x8000,8,raw);require(raw==0x1122334455667788ull,"failed SC wrote memory");
        execute(encode(wide?52:48,4,5,0));cpu.state.cp0[14]=n64::sign32(0xa0000400);execute(0x42000018);cpu.state.gpr[6]=99;execute(encode(wide?60:56,4,6,0));require(!cpu.exception_taken && !cpu.state.gpr[6],"ERET conditional failure");
        for(unsigned low=1;low<(wide?8u:4u);++low){start();cpu.state.gpr[4]=n64::sign32(0x80008000+low);cpu.state.gpr[6]=99;execute(encode(wide?60:56,4,6,0));require(cpu.exception_taken && ((cpu.state.cp0[13]>>2)&31)==5 && cpu.state.gpr[6]==99,"SC address fault precedence");++checks;}
        for(unsigned mode:{8u,16u}){start();cpu.state.cp0[12]=mode;cpu.state.gpr[4]=0x8000;execute(encode(wide?52:48,4,5,0));require(cpu.exception_taken && ((cpu.state.cp0[13]>>2)&31)==(wide?10u:2u),"doubleword privilege");++checks;}
        ++checks;
    }
    // Defined 32-bit kernel doubleword forms ignore the high VA half.
    for(unsigned op:{48u,52u}){start();bus.write(0x8000,8,0x8000000012345678ull);cpu.state.cp0[12]=0;cpu.state.gpr[4]=0x1234567880008000ull;execute(encode(op,4,5,0));require(!cpu.exception_taken && cpu.state.gpr[5]==(op==52?0x8000000012345678ull:0xffffffff80000000ull),"32-bit kernel linked VA");cpu.state.gpr[6]=0x123456789abcdef0ull;execute(encode(op+8,4,6,0));require(!cpu.exception_taken && cpu.state.gpr[6]==1,"32-bit kernel conditional VA");++checks;}
    // A failed condition cannot suppress TLB miss, invalid or modified exceptions.
    for(unsigned wide=0;wide<2;++wide)for(unsigned fault:{1u,2u,3u})for(bool delay:{false,true}){start();cpu.state.gpr[4]=0x20000;cpu.state.gpr[6]=99;cpu.state.delay_slot=delay;if(fault!=2)cpu.tlb.write(1,0x20000,0,fault==1?27:25,fault==1?27:25);execute(encode(wide?60:56,4,6,0));require(cpu.exception_taken && ((cpu.state.cp0[13]>>2)&31)==(fault==2?3: fault) && cpu.state.cp0[8]==0x20000 && cpu.state.gpr[6]==99 && bool(cpu.state.cp0[13]&0x80000000)==delay,"SC TLB exception precedence");++checks;}
    // The specified SC pseudocode tests LLbit without clearing it on success.
    for(unsigned wide=0;wide<2;++wide){start();bus.write(0x8000,8,1);cpu.state.gpr[4]=n64::sign32(0x80008000);execute(encode(wide?52:48,4,5,0));for(unsigned n=0;n<2;++n){cpu.state.gpr[6]=9+n;execute(encode(wide?60:56,4,6,0));require(!cpu.exception_taken && cpu.state.gpr[6]==1,"SC changed LLbit");}++checks;}
    // These are conservative functional cache-interference conventions, not hardware timing goldens.
    for(unsigned op:{17u,21u}){start();bus.write(0x8000,8,1);cpu.state.gpr[4]=n64::sign32(0x80008000);execute(encode(48,4,5,0));execute(encode(47,4,op,0));cpu.state.gpr[6]=99;execute(encode(56,4,6,0));require(!cpu.exception_taken && !cpu.state.gpr[6],"cache invalidation model");++checks;}
    start();bus.write(0x8000,8,1);cpu.state.gpr[4]=n64::sign32(0x80008000);execute(encode(48,4,5,0));cpu.state.gpr[9]=n64::sign32(0x80008010);execute(encode(35,9,10,0));cpu.state.gpr[6]=99;execute(encode(56,4,6,0));require(!cpu.exception_taken && !cpu.state.gpr[6],"intervening cache miss model");++checks;
    for(unsigned wide=0;wide<2;++wide){
        // The base and target can alias: address and store data precede result write.
        start();bus.write(0x8000,8,0x1122334455667788ull);cpu.state.gpr[4]=n64::sign32(0x80008000);execute(encode(wide?52:48,4,4,0));require(!cpu.exception_taken && cpu.state.gpr[4]==(wide?0x1122334455667788ull:0x11223344),"LL base/target alias");cpu.state.gpr[4]=n64::sign32(0x80008000);execute(encode(wide?60:56,4,4,0));uint64_t actual=0;require(!cpu.exception_taken && cpu.state.gpr[4]==1 && cpu.caches.read(bus,n64::sign32(0x80008000),wide?8:4,actual,false,3) && actual==(wide?n64::sign32(0x80008000):0x80008000),"SC base/target alias");++checks;
        start();bus.write(0x8000,8,0x123456789abcdef0ull);cpu.tlb.write(1,0,0,(8u<<6)|31,(8u<<6)|31);execute(encode(wide?52:48,0,5,0));require(!cpu.exception_taken && cpu.state.cp0[17]==0x800,"zero base translated PA");cpu.state.gpr[6]=7;execute(encode(wide?60:56,0,6,0));require(!cpu.exception_taken && cpu.state.gpr[6]==1,"zero base conditional");++checks;
        start();bus.fail=true;cpu.state.gpr[4]=n64::sign32(0x80008000);cpu.state.gpr[5]=99;cpu.state.cp0[17]=77;execute(encode(wide?52:48,4,5,0));require(cpu.exception_taken && ((cpu.state.cp0[13]>>2)&31)==7 && cpu.state.gpr[5]==99 && cpu.state.cp0[17]==77 && !cpu.state.llbit,"LL bus fault preservation");++checks;
    }
    start();bus.write(0x8000,8,1);cpu.state.gpr[4]=n64::sign32(0x80008000);execute(encode(48,4,5,0));cpu.state.gpr[9]=~uint64_t(0);execute((16u<<26)|(4u<<21)|(9u<<16)|(17u<<11));require(cpu.state.cp0[17]==0xfffffff && cpu.state.llbit,"LLAddr diagnostic mask");cpu.state.gpr[6]=99;execute(encode(56,4,6,0));require(!cpu.exception_taken && cpu.state.gpr[6]==1,"diagnostic LLAddr changed reservation");++checks;
    std::cout<<checks<<" linked load/conditional store observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
