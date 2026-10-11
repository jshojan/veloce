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
// Independent fields from NEC Figures 5-18 and 6-11.
static uint64_t expected(unsigned reg,uint64_t source){
 if(reg==26)return source%256;
 if(reg==29)return 0;
 return ((source>>8)&((1ull<<20)-1))*256+((source>>6)&3)*64;
}
int main(){try{
 Bus bus;n64::VR4300 cpu(bus);cpu.reset(0x4000);cpu.tlb.write(0,0x4000,0,(4u<<6)|23,(5u<<6)|23);auto initial=cpu.state;
 for(unsigned r=1;r<32;++r)initial.gpr[r]=0x1122334455660000ull+r;initial.llbit=true;initial.cp0[8]=0x1357;initial.hi=0x5555555555555555ull;initial.lo=0xaaaaaaaaaaaaaaaaull;uint64_t checks=0;
 auto check=[&](unsigned form,unsigned rt,uint64_t source,unsigned reg,unsigned old,bool delay){
  cpu.state=initial;auto& s=cpu.state;s.cp0[12]=old;s.cp0[26]=0x42;s.cp0[28]=0x012345c0;s.cp0[29]=0;s.gpr[rt]=rt?source:0;s.delay_slot=delay;const auto gpr=s.gpr;
  bus.instruction=16u<<26|form<<21|rt<<16|reg<<11;bus.reads=bus.writes=0;cpu.step();
  const unsigned mode=(old&6)?0:(old>>3)&3;const bool unusable=mode && !(old&0x10000000);const bool narrow=mode && form==5 && !(old&(mode==1?64:32));
  if(unusable || narrow){require(s.cp0[26]==0x42 && s.cp0[28]==0x012345c0 && s.cp0[29]==0,"rejected diagnostic write changed target");require(cpu.exception_taken && ((s.cp0[13]>>2)&31)==(unusable?11u:10u) && s.cp0[12]==(old|2),"Diagnostic write privilege priority/atomicity");require(s.cp0[14]==initial.pc-(delay?4:0) && bool(s.cp0[13]&0x80000000)==delay,"Diagnostic rejected write metadata");}
  else require(!cpu.exception_taken && s.cp0[reg]==expected(reg,gpr[rt]) && s.cp0[12]==old && s.pc==initial.pc+4,"Diagnostic writable/reserved fields");
  for(unsigned other:{26u,28u,29u})if(other!=reg)require(s.cp0[other]==(other==26?0x42ull:other==28?0x012345c0ull:0ull),"diagnostic write changed another diagnostic register");
  require(s.gpr==gpr && s.hi==initial.hi && s.lo==initial.lo && s.llbit && s.cp0[8]==initial.cp0[8] && bus.reads==1 && !bus.writes,"Diagnostic write changed operands/BadVAddr/LLbit or accessed data");++checks;
 };
 for(unsigned reg:{26u,28u,29u})for(unsigned form:{4u,5u})for(unsigned shift:{0u,16u,32u,48u})for(uint64_t pattern:{0ull,~0ull,0x5555555555555555ull,0xaaaaaaaaaaaaaaaaull})for(unsigned value=0;value<65536;++value)for(unsigned kernel:{0u,128u})check(form,5,(pattern&~(0xffffull<<shift))|uint64_t(value)<<shift,reg,kernel,false);
 for(unsigned reg:{26u,28u,29u})for(unsigned form:{4u,5u})for(unsigned rt=0;rt<32;++rt)for(unsigned bit=0;bit<64;++bit)for(bool invert:{false,true})for(unsigned privilege=0;privilege<3;++privilege)for(unsigned flags=0;flags<8;++flags)for(unsigned level:{0u,2u,4u,6u})for(bool cu0:{false,true})for(bool delay:{false,true})check(form,rt,invert?~(1ull<<bit):1ull<<bit,reg,privilege*8+(flags<<5)+level+(cu0?0x10000000:0),delay);
 // Actual CACHE consumers: every 20-bit physical tag, both software writes,
 // and every defined index-store state (exclude architecturally undefined states).
 for(unsigned form:{4u,5u})for(unsigned selector:{0u,1u})for(unsigned state:{0u,2u,3u}) {
  if(!selector && state==3)continue;
  for(unsigned page=0;page<(1u<<20);++page) {
   cpu.state=initial;cpu.caches.reset();auto& s=cpu.state;s.cp0[12]=128;
   s.gpr[1]=0xffffffffa0001020ull;s.gpr[5]=0xfedcba98f000003full|uint64_t(page)*256|uint64_t(state)*64;
   bus.instruction=16u<<26|form<<21|5u<<16|28u<<11;cpu.step();
   require(!cpu.exception_taken && s.cp0[28]==uint64_t(page)*256+state*64,"TagLo software write before CACHE");
   bus.instruction=47u<<26|1u<<21|(8u+selector)<<16;cpu.step();
   const auto& line=selector?cpu.caches.data[(s.gpr[1]>>4)&511]:cpu.caches.data[0];
   if(selector)require(line.page==page && line.flags==(state==3?3:state==2?1:0),"data index-store tag/state");
   else {const auto& instruction=cpu.caches.instruction[(s.gpr[1]>>5)&511];require(instruction.page==page && instruction.flags==(state==2?1:0),"instruction index-store tag/state");}
   require(!cpu.exception_taken,"Index_Store_Tag failed");
   s.cp0[29]=0;bus.instruction=47u<<26|1u<<21|(4u+selector)<<16;cpu.step();
   const unsigned returned=state?(selector?3:2):0;
   require(!cpu.exception_taken && s.cp0[28]==uint64_t(page)*256+returned*64 && s.cp0[29]==0,"Index_Load_Tag readback");
   ++checks;
  }
 }
 std::cout<<checks<<" Diagnostic field/source/privilege and actual CACHE consumer observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
