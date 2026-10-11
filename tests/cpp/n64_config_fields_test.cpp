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
// Independent named-field construction from NEC Figure 5-16, not core helpers.
static uint64_t expected(unsigned ec,uint64_t source){return uint64_t(ec)<<28|6u<<16|0x646u<<4|((source>>24)&15)<<24|((source>>15)&1)<<15|((source>>3)&1)<<3|(source&7);}
int main(){try{
 Bus bus;n64::VR4300 cpu(bus);cpu.reset(0x4000);cpu.tlb.write(0,0x4000,0,(4u<<6)|23,(5u<<6)|23);
 auto initial=cpu.state;for(unsigned r=1;r<32;++r)initial.gpr[r]=0x1122334455660000ull+r;initial.llbit=true;uint64_t checks=0;
 auto check=[&](unsigned form,unsigned rt,uint64_t source,unsigned status,unsigned ec,bool delay){
  cpu.state=initial;auto& s=cpu.state;s.cp0[12]=status;s.cp0[16]=expected(ec,0x8002);s.gpr[rt]=rt?source:0;s.delay_slot=delay;const auto gpr=s.gpr;const auto old=s.cp0[16];
  bus.instruction=16u<<26|form<<21|rt<<16|16u<<11;bus.reads=bus.writes=0;cpu.step();
  const unsigned mode=(status&6)?0:(status>>3)&3;const bool unusable=mode && !(status&0x10000000);const bool narrow=mode && form==5 && !(status&(mode==1?64:32));
  if(unusable || narrow){require(cpu.exception_taken && ((s.cp0[13]>>2)&31)==(unusable?11u:10u) && s.cp0[16]==old,"Config privilege priority/atomicity");require(s.cp0[14]==initial.pc-(delay?4:0) && bool(s.cp0[13]&0x80000000)==delay,"Config rejected-write metadata");}
  else require(!cpu.exception_taken && s.cp0[16]==expected(ec,gpr[rt]) && s.pc==initial.pc+4,"Config fixed/writable fields");
  require(s.gpr==gpr && s.llbit && bus.reads==1 && !bus.writes,"Config write changed operands/LLbit or accessed data");++checks;
 };
 // Every sixteen-bit field position at four surrounding patterns, both write widths and kernel modes.
 for(unsigned form:{4u,5u})for(unsigned shift:{0u,16u,32u,48u})for(uint64_t pattern:{0ull,~0ull,0x5555555555555555ull,0xaaaaaaaaaaaaaaaaull})for(unsigned value=0;value<65536;++value)for(unsigned status:{0u,128u})check(form,5,(pattern&~(0xffffull<<shift))|uint64_t(value)<<shift,status,0,false);
 // All writable-bit combinations and all clock-field values; EP/BE here only test register readback.
 for(unsigned ec=0;ec<8;++ec)for(unsigned fields=0;fields<512;++fields)for(unsigned form:{4u,5u})for(bool delay:{false,true})check(form,5,((fields>>5)&15)<<24|((fields>>4)&1)<<15|(fields&15),128,ec,delay);
 for(unsigned form:{4u,5u})for(unsigned rt=0;rt<32;++rt)for(unsigned bit=0;bit<64;++bit)for(bool invert:{false,true})for(unsigned privilege=0;privilege<3;++privilege)for(unsigned flags=0;flags<8;++flags)for(unsigned level:{0u,2u,4u,6u})for(bool cu0:{false,true})for(bool delay:{false,true})check(form,rt,invert?~(1ull<<bit):1ull<<bit,privilege*8+(flags<<5)+level+(cu0?0x10000000:0),bit%8,delay);
 cpu.reset(0x4000);require(cpu.state.cp0[16]==expected(0,0x8002),"explicit homebrew Config reset profile");++checks;
 std::cout<<checks<<" Config field/source/privilege observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
