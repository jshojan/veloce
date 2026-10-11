#include "vr4300.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool ok){if(!ok)throw std::runtime_error("COP2 unusable observation mismatch");}
struct Bus final:n64::Memory{
 uint32_t instruction=0;unsigned reads=0,writes=0;
 bool read(uint64_t address,unsigned bytes,uint64_t& value)override{++reads;if(bytes!=4 || uint32_t(address)!=0xa0000400)return false;value=instruction;return true;}
 bool write(uint64_t,unsigned,uint64_t)override{++writes;return false;}
 bool write_masked(uint64_t,unsigned,uint64_t,uint8_t)override{++writes;return false;}
};
int main(){try{
 Bus bus;n64::VR4300 cpu(bus);cpu.reset(n64::sign32(0xa0000400));uint64_t count=0;
 const auto base=cpu.state;
 auto check=[&](uint32_t ins,bool delay=false,bool nested=false,bool bev=false){
  auto& s=cpu.state;s=base;s.cp0[12]=128|(nested?2:0)|(bev?1u<<22:0);s.cp0[13]=0x30000300;s.cp0[14]=0x12345678;s.cp0[8]=0x24681357;s.delay_slot=delay;
  for(unsigned r=1;r<32;++r)s.gpr[r]=(r%4==0?~uint64_t(0):r%4==1?0x80000001:r%4==2?0x100000000ull:0xffffffffa0008000ull);
  const auto gpr=s.gpr;s.hi=0x123456789abcdef0ull;s.lo=~s.hi;bus.instruction=ins;bus.reads=bus.writes=0;cpu.step();
  require(cpu.exception_taken && cpu.instruction_valid && cpu.instruction_word==ins && bus.reads==1 && !bus.writes);
  require(s.gpr==gpr && s.hi==0x123456789abcdef0ull && s.lo==0xedcba9876543210full && s.cp0[8]==0x24681357);
  require((s.cp0[13]&0x3000037c)==0x2000032c && s.cp0[12]==(128|(nested?2:0)|(bev?1u<<22:0)|2));
  require(s.cp0[14]==(nested?0x12345678:base.pc-(delay?4:0)) && bool(s.cp0[13]&0x80000000)==(!nested && delay));
  require(s.pc==n64::sign32(bev?0xbfc00380:0x80000180) && s.next_pc==s.pc+4 && !s.delay_slot);++count;
 };
 // Every payload encoding of COP2 and its four load/store major opcodes.
 for(unsigned op:{18u,50u,54u,58u,62u})for(uint32_t payload=0;payload<(1u<<26);++payload)check(op<<26|payload);
 for(unsigned op:{18u,50u,54u,58u,62u})for(bool delay:{false,true})for(bool nested:{false,true})for(bool bev:{false,true})for(unsigned rs=0;rs<32;++rs)for(unsigned rt=0;rt<32;++rt)check(op<<26|rs<<21|rt<<16|65535,delay,nested,bev);
 std::cout<<count<<" COP2 encoding/metadata/operand/no-data-access observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
