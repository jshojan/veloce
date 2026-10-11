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
int main(){try{
 Bus bus;n64::VR4300 cpu(bus);cpu.reset(0x4000);cpu.tlb.write(0,0x4000,0,(4u<<6)|23,(5u<<6)|23);
 auto base=cpu.state;base.cp0[8]=0xfedcba9800060001ull;base.cp0[15]=0xb22;base.cp0[27]=0;
 for(unsigned r=1;r<32;++r)base.gpr[r]=0x1122334455660000ull+r;
 base.llbit=true;base.hi=0x5555555555555555ull;base.lo=0xaaaaaaaaaaaaaaaaull;uint64_t checks=0;
 auto check=[&](unsigned rd,unsigned rs,unsigned rt,uint64_t value,unsigned status,bool delay){
  cpu.state=base;auto& s=cpu.state;s.cp0[12]=status;s.gpr[rt]=rt?value:0;s.delay_slot=delay;const auto gpr=s.gpr;
  bus.instruction=16u<<26|rs<<21|rt<<16|rd<<11;bus.reads=bus.writes=0;cpu.step();
  const unsigned mode=(status&6)?0:(status>>3)&3;const bool unusable=mode && !(status&0x10000000);const bool narrow=mode && rs==5 && !(status&(mode==1?64:32));
  if(unusable || narrow){require(cpu.exception_taken && ((s.cp0[13]>>2)&31)==(unusable?11u:10u),"CP0 write privilege priority");require(s.cp0[14]==base.pc-(delay?4:0) && bool(s.cp0[13]&0x80000000)==delay,"CP0 rejected write metadata");}
  else require(!cpu.exception_taken && s.pc==base.pc+4 && s.next_pc==base.pc+8,"read-only write faulted or changed control flow");
  require(s.cp0[rd]==base.cp0[rd] && s.gpr==gpr && s.hi==base.hi && s.lo==base.lo && s.llbit,"read-only CP0 write modified operands/register");
  require(bus.reads==1 && !bus.writes,"CP0 write accessed data bus");++checks;
 };
 // Every sixteen-bit source domain at all four positions, four surrounding patterns.
 for(unsigned rd:{8u,15u,27u})for(unsigned rs:{4u,5u})for(unsigned shift:{0u,16u,32u,48u})for(uint64_t pattern:{0ull,~0ull,0x5555555555555555ull,0xaaaaaaaaaaaaaaaaull})for(unsigned value=0;value<65536;++value)for(unsigned status:{0u,128u}){
  const auto input=(pattern&~(0xffffull<<shift))|(uint64_t(value)<<shift);check(rd,rs,5,input,status,false);
 }
 // Every source allocation, one-hot/one-clear bits, privilege/extension/EXL/ERL/CU0 and delay context.
 for(unsigned rd:{8u,15u,27u})for(unsigned rs:{4u,5u})for(unsigned rt=0;rt<32;++rt)for(unsigned bit=0;bit<64;++bit)for(bool invert:{false,true})for(unsigned privilege=0;privilege<3;++privilege)for(unsigned flags=0;flags<8;++flags)for(unsigned level:{0u,2u,4u,6u})for(bool cu0:{false,true})for(bool delay:{false,true}){
  const auto input=invert?~(1ull<<bit):(1ull<<bit);check(rd,rs,rt,input,privilege*8+(flags<<5)+level+(cu0?0x10000000:0),delay);
 }
 // BadVAddr is still written by actual address exceptions after an ignored MTC0.
 cpu.state=base;cpu.state.cp0[12]=128;cpu.state.gpr[4]=0x6001;bus.instruction=35u<<26|4u<<21|5u<<16;cpu.step();require(cpu.exception_taken && cpu.state.cp0[8]==0x6001 && ((cpu.state.cp0[13]>>2)&31)==4,"hardware BadVAddr update suppressed");++checks;
 std::cout<<checks<<" CP0 read-only source/mode/metadata observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
