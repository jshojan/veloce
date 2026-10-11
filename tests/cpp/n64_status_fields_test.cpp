#include "vr4300.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:n64::Memory {
 uint32_t instruction=0;unsigned reads=0,writes=0;
 bool read(uint64_t,unsigned,uint64_t& value)override{++reads;value=instruction;return true;}
 bool write(uint64_t,unsigned,uint64_t)override{++writes;return true;}
 bool write_masked(uint64_t,unsigned,uint64_t,uint8_t)override{++writes;return true;}
};
// Named field positions from NEC Figures 6-5/6-6, independent of the core mask.
static uint64_t expected(uint64_t old,uint64_t source){const uint64_t fields=((1ull<<19)-1)|(1ull<<20)|(1ull<<22)|(255ull<<24);return (old&(1ull<<21))|(source&fields);}
int main(){try{
 Bus bus;n64::VR4300 cpu(bus);cpu.reset(0x4000);cpu.tlb.write(0,0x4000,0,(4u<<6)|23,(5u<<6)|23);auto initial=cpu.state;
 for(unsigned r=1;r<32;++r)initial.gpr[r]=0x1122334455660000ull+r;initial.llbit=true;initial.cp0[8]=0x1357;initial.hi=0x5555555555555555ull;initial.lo=0xaaaaaaaaaaaaaaaaull;uint64_t checks=0;
 auto check=[&](unsigned form,unsigned rt,uint64_t source,unsigned old,bool delay){
  cpu.state=initial;auto& s=cpu.state;s.cp0[12]=old;s.gpr[rt]=rt?source:0;s.delay_slot=delay;const auto gpr=s.gpr;
  bus.instruction=16u<<26|form<<21|rt<<16|12u<<11;bus.reads=bus.writes=0;cpu.step();
  const unsigned mode=(old&6)?0:(old>>3)&3;const bool unusable=mode && !(old&0x10000000);const bool narrow=mode && form==5 && !(old&(mode==1?64:32));
  if(unusable || narrow){require(cpu.exception_taken && ((s.cp0[13]>>2)&31)==(unusable?11u:10u) && s.cp0[12]==(old|2),"Status write privilege priority/atomicity");require(s.cp0[14]==initial.pc-(delay?4:0) && bool(s.cp0[13]&0x80000000)==delay,"Status rejected write metadata");}
  else require(!cpu.exception_taken && s.cp0[12]==expected(old,gpr[rt]) && s.pc==initial.pc+4,"Status writable/reserved/TS fields");
  require(s.gpr==gpr && s.hi==initial.hi && s.lo==initial.lo && s.llbit && s.cp0[8]==initial.cp0[8] && bus.reads==1 && !bus.writes,"Status write changed operands/BadVAddr/LLbit or accessed data");++checks;
 };
 check(4,5,1ull<<21,128,false); // An ordinary software write cannot manufacture shutdown.
 for(unsigned form:{4u,5u})for(unsigned shift:{0u,16u,32u,48u})for(uint64_t pattern:{0ull,~0ull,0x5555555555555555ull,0xaaaaaaaaaaaaaaaaull})for(unsigned value=0;value<65536;++value)for(unsigned kernel:{0u,128u})for(bool ts:{false,true})check(form,5,(pattern&~(0xffffull<<shift))|uint64_t(value)<<shift,kernel|(ts?1u<<21:0),false);
 // All source allocations, bit values and existing privilege/width/CU0/EXL/ERL/TS contexts.
 for(unsigned form:{4u,5u})for(unsigned rt=0;rt<32;++rt)for(unsigned bit=0;bit<64;++bit)for(bool invert:{false,true})for(unsigned privilege=0;privilege<3;++privilege)for(unsigned flags=0;flags<8;++flags)for(unsigned level:{0u,2u,4u,6u})for(bool cu0:{false,true})for(bool delay:{false,true})for(bool ts:{false,true})check(form,rt,invert?~(1ull<<bit):1ull<<bit,privilege*8+(flags<<5)+level+(cu0?0x10000000:0)+(ts?1u<<21:0),delay);
 cpu.reset(0x4000);require(!(cpu.state.cp0[12]&(1u<<21)),"explicit reset did not clear TS");++checks;
 std::cout<<checks<<" Status field/source/privilege/TS observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
