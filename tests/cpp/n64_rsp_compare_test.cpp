#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
using Bank=std::array<std::array<uint8_t,16>,32>;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static int sx(unsigned x){return int(x%32768)-int(x/32768)*32768;}
static unsigned half(const Bank& b,unsigned r,unsigned i){return b[r][2*i]*256u+b[r][2*i+1];}
static void put(Bank& b,unsigned r,unsigned i,unsigned x){b[r][2*i]=uint8_t(x/256);b[r][2*i+1]=uint8_t(x);}
// Independent whole-integer conditions: no production lane evaluator is used.
static std::pair<unsigned,unsigned> oracle(unsigned fn,unsigned u,unsigned v,unsigned bits){
 const int a=sx(u),b=sx(v);bool low=bits%2,high=bits/2%2,lt=bits/4%2,ge=bits/8%2,ext=bits/16%2;unsigned out=u;
 if(fn<36){
  switch(fn){case 32:lt=a<b || (a==b && low && high);out=lt?u:v;break;case 33:lt=a==b && !high;out=v;break;case 34:lt=a!=b || high;break;case 35:lt=a>b || (a==b && (!low || !high));out=lt?u:v;break;}
  low=high=ge=false;
 }else if(fn==36){
  if(low){if(!high){const unsigned sum=u+v;lt=ext?(sum<=65536):(sum==0);}out=lt?(65536-v)%65536:u;}
  else{if(!high)ge=u>=v;out=ge?v:u;}low=high=ext=false;
 }else if(fn==37 || fn==38){
  const bool opposite=(a<0)!=(b<0);
  if(opposite){ge=b<0;lt=fn==37?a+b<=0:a+b<0;out=lt?(fn==37?(65536-v)%65536:65535-v):u;}
  else{lt=b<0;ge=a>=b;out=ge?v:u;}
  low=fn==37 && opposite;high=fn==37 && (opposite?(a+b!=0 && a+b!=-1):a!=b);ext=fn==37 && opposite && a+b==-1;
 }else{out=lt?u:v;low=high=false;}
 return {out,unsigned(low)+2*unsigned(high)+4*unsigned(lt)+8*unsigned(ge)+16*unsigned(ext)};
}
static constexpr unsigned lane_map[16][8]={{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,0,2,2,4,4,6,6},{1,1,3,3,5,5,7,7},{0,0,0,0,4,4,4,4},{1,1,1,1,5,5,5,5},{2,2,2,2,6,6,6,6},{3,3,3,3,7,7,7,7},{0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3},{4,4,4,4,4,4,4,4},{5,5,5,5,5,5,5,5},{6,6,6,6,6,6,6,6},{7,7,7,7,7,7,7,7}};
static unsigned packed(const std::array<uint16_t,3>& f,unsigned i){return (f[0]/(1u<<i)%2)+2*(f[0]/(1u<<(i+8))%2)+4*(f[1]/(1u<<i)%2)+8*(f[1]/(1u<<(i+8))%2)+16*(f[2]/(1u<<i)%2);}
static void flags(std::array<uint16_t,3>& f,unsigned bits,unsigned i){f[0]|=uint16_t(((bits%2)+256*(bits/2%2))<<i);f[1]|=uint16_t(((bits/4%2)+256*(bits/8%2))<<i);f[2]|=uint16_t((bits/16%2)<<i);}
static uint32_t ins(unsigned fn,unsigned e,unsigned vs,unsigned vt,unsigned vd){return 0x4a000000|(e<<21)|(vt<<16)|(vs<<11)|(vd<<6)|fn;}
static void install(n64::Machine& m,uint32_t word){for(unsigned b=0;b<4;++b)m.sp[4096+b]=uint8_t(word>>(24-b*8));m.sp_interface.pc=0;m.sp_interface.status=0;m.rsp.next_pc=4;m.rsp.delay_slot=false;m.rsp.blocked=false;}
static void step(n64::Machine& m,uint32_t word){install(m,word);m.rsp.step(m.sp_interface,m.dp_interface,m.sp,m.ram);}
int main(int argc,char** argv){try{
 // Complete 16-bit VS sweep against eight boundary/relationship partners and
 // every incoming lane flag combination for every comparison opcode.
 uint64_t count=0;
 for(unsigned u=0;u<65536;++u){const unsigned partners[]={0,1,32767,32768,65535,u,(65536-u)%65536,65535-u};for(auto v:partners)for(unsigned bits=0;bits<32;++bits)for(unsigned fn=32;fn<40;++fn){const auto want=oracle(fn,u,v,bits);const auto got=n64::RspCompare::evaluate(fn,uint16_t(u),uint16_t(v),bits);check(got.word==want.first && got.flags==want.second,"lane corpus");++count;}}
 // Actual vector execution: all selectors, register positions, alias patterns,
 // nonuniform lane flags, accumulator upper guards and all other bank bytes.
 n64::RspVector vec;std::array<uint32_t,32> scalar{};std::array<uint8_t,8192> memory{};
 for(unsigned position=0;position<32;++position)for(unsigned alias=0;alias<5;++alias)for(unsigned profile=0;profile<4;++profile)for(unsigned e=0;e<16;++e)for(unsigned fn=32;fn<40;++fn){
  vec.reset();const unsigned vs=position,vt=alias>=3?vs:(position+1)%32,vd=alias==1 || alias==4?vs:alias==2?vt:(position+2)%32;
  for(unsigned r=0;r<32;++r)for(unsigned i=0;i<8;++i)put(vec.registers,r,i,(r*8191+i*6553+profile*32767)%65536);
  for(unsigned i=0;i<8;++i){vec.accumulator[i]=(0xdeadbeef0000ull+i*0x100000000ull)&vec.acc_mask;flags(vec.flags,profile*8+i,i);}
  auto expected=vec.registers;const auto before=vec.accumulator;std::array<uint16_t,3> next{};auto after=before;
  for(unsigned i=0;i<8;++i){const auto want=oracle(fn,half(vec.registers,vs,i),half(vec.registers,vt,lane_map[e][i]),packed(vec.flags,i));put(expected,vd,i,want.first);after[i]=before[i]/65536*65536+want.first;flags(next,want.second,i);}
  check(vec.execute(ins(fn,e,vs,vt,vd),scalar,memory),"vector accepted");check(vec.registers==expected && vec.accumulator==after && vec.flags==next,"vector flags/alias/guards");check(scalar==std::array<uint32_t,32>{} && memory==std::array<uint8_t,8192>{},"scalar and memory guards");
 }
 // VCH creates the flags consumed by VCL and VMRG, including negative zero
 // and the -1 complement boundary. Expected literal values are hand derived.
 for(unsigned fn: {36u,39u}){vec.reset();put(vec.registers,0,0,0x7fff);put(vec.registers,1,0,0x8000);check(vec.execute(ins(37,0,0,1,2),scalar,memory),"VCH chain");check(half(vec.registers,2,0)==0x8000 && packed(vec.flags,0)==29,"VCH -1 boundary");check(vec.execute(ins(fn,0,0,1,2),scalar,memory),"clip/merge chain");check(half(vec.registers,2,0)==(fn==36?0x8000:0x7fff),"chain result");}
 check(argc==2,"original ROM required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};n64::Machine m;check(m.load(rom.data(),rom.size()),"ROM load");
 for(unsigned fn=32;fn<40;++fn){m.rsp.vectors=vec;step(m,ins(fn,0,0,1,2));check(!m.rsp.blocked && m.sp_interface.pc==4 && !(m.sp_interface.status&3) && !m.sp_interface.interrupt,"actual RSP dispatcher");}
 // Existing S019 carries the complete comparison flags and accumulator. Replay
 // from a pending VCL instruction must reproduce the entire serialized state.
 m.rsp.vectors=vec;install(m,ins(36,0,0,1,2));std::vector<uint8_t> saved,final,replay;check(m.save(saved),"save");step(m,ins(36,0,0,1,2));check(m.save(final),"final save");check(m.restore(saved),"comparison restore");step(m,ins(36,0,0,1,2));check(m.save(replay) && replay==final,"comparison replay");
 install(m,13);m.sp_interface.status=64;check(m.rsp.step(m.sp_interface,m.dp_interface,m.sp,m.ram) && m.sp_interface.pc==4 && (m.sp_interface.status&3)==3 && m.sp_interface.interrupt,"actual BREAK IRQ");m.sp_interface.interrupt=false;
 step(m,0x4a000010);check(m.rsp.blocked && m.rsp.blocked_word==0x4a000010 && (m.sp_interface.status&1) && !(m.sp_interface.status&2),"unsupported arithmetic retained");
 std::cout<<"RSP comparison: "<<count<<" lane cases; 81,920 vector alias/selector cases; dispatcher, clipping chain and save replay PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
