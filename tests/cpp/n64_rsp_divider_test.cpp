#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
using Bank=std::array<std::array<uint8_t,16>,32>;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static unsigned half(const Bank& b,unsigned r,unsigned i){return b[r][2*i]*256u+b[r][2*i+1];}
static void put(Bank& b,unsigned r,unsigned i,unsigned x){b[r][2*i]=uint8_t(x/256);b[r][2*i+1]=uint8_t(x);}
static uint64_t root_floor(uint64_t n){uint64_t result=0,bit=1ull<<62;while(bit>n)bit/=4;while(bit){if(n>=result+bit){n-=result+bit;result=result/2+bit;}else result/=2;bit/=4;}return result;}
static uint16_t entry(bool root,unsigned i){if(!root)return i?uint16_t(((17179869184ull/(512+i))+1)/256):65535;const uint64_t a=i<256?256+i:2*i;return uint16_t(root_floor(17592186044415ull/a)/2);}
static uint32_t reference(bool root,uint32_t raw){if(raw==0)return 2147483647;if(raw==4294934528u)return 4294901760u;const bool negative=raw>=2147483648u;const uint64_t mag=negative?(raw>4294934528u?4294967296ull-raw:4294967295ull-raw):raw;unsigned exponent=0;for(auto x=mag;x>1;x/=2)++exponent;const uint64_t normalized=(mag-(1ull<<exponent))*(1ull<<(32-exponent));const unsigned index=unsigned(normalized/(root?16777216:8388608))+(root && exponent%2==1?256:0);const uint64_t significand=1073741824ull+uint64_t(entry(root,index))*16384;const unsigned scale=root?exponent/2:exponent;const uint32_t positive=uint32_t(significand/(1ull<<scale));return negative?4294967295u-positive:positive;}
static constexpr unsigned lane_map[16][8]={{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,0,2,2,4,4,6,6},{1,1,3,3,5,5,7,7},{0,0,0,0,4,4,4,4},{1,1,1,1,5,5,5,5},{2,2,2,2,6,6,6,6},{3,3,3,3,7,7,7,7},{0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3},{4,4,4,4,4,4,4,4},{5,5,5,5,5,5,5,5},{6,6,6,6,6,6,6,6},{7,7,7,7,7,7,7,7}};
static void flags(std::array<uint16_t,3>& f,unsigned bits,unsigned i){f[0]|=uint16_t(((bits%2)+256*(bits/2%2))<<i);f[1]|=uint16_t(((bits/4%2)+256*(bits/8%2))<<i);f[2]|=uint16_t((bits/16%2)<<i);}
static uint32_t ins(unsigned fn,unsigned e,unsigned vs,unsigned vt,unsigned vd){return 0x4a000000|(e<<21)|(vt<<16)|(vs<<11)|(vd<<6)|fn;}
static void install(n64::Machine& m,uint32_t word){for(unsigned b=0;b<4;++b)m.sp[4096+b]=uint8_t(word>>(24-b*8));m.sp_interface.pc=0;m.sp_interface.status=0;m.rsp.next_pc=4;m.rsp.delay_slot=false;m.rsp.blocked=false;}
static void step(n64::Machine& m,uint32_t word){install(m,word);m.rsp.step(m.sp_interface,m.dp_interface,m.sp,m.ram);}
int main(int argc,char** argv){try{
 uint64_t values=0;
 for(unsigned i=0;i<512;++i)for(bool root:{false,true})check(n64::RspDivider::table(root,i)==entry(root,i),"integer table");
 for(unsigned u=0;u<65536;++u)for(bool root:{false,true}){const uint32_t raw=u>=32768?0xffff0000u|u:u;check(n64::RspDivider::calculate(root,raw)==reference(root,raw),"all 16-bit values");++values;}
 for(unsigned exp=0;exp<32;++exp)for(unsigned fraction=0;fraction<512;++fraction)for(unsigned tail: {0u,1u,7u,255u}){const uint64_t whole=(1ull<<exp)+((uint64_t(fraction)<<exp)/512)+tail;if(whole>0xffffffffu)continue;for(uint32_t raw:{uint32_t(whole),uint32_t(0u-uint32_t(whole))})for(bool root:{false,true}){check(n64::RspDivider::calculate(root,raw)==reference(root,raw),"full-width exponent/mantissa/sign edges");++values;}}
 for(unsigned high=0;high<65536;++high)for(unsigned low:{0u,1u,32767u,32768u,65535u,0xe834u,0x1234u,0x8001u})for(unsigned producer:{50u,54u})for(unsigned consumer:{49u,53u}){n64::RspDivider d;check(d.execute(producer,uint16_t(high))==0 && d.pending,"high sets flag including zero");const uint32_t raw=(high<<16)|low;check(d.execute(consumer,uint16_t(low))==uint16_t(reference(consumer==53,raw)) && d.output==reference(consumer==53,raw) && !d.pending,"cross-family shared latch");const auto again=d.execute(consumer,uint16_t(low));check(again==uint16_t(reference(consumer==53,low>=32768?0xffff0000u|low:low)),"pending consumed once");}
 // Published numeric hardware-test expectations (E834 high/low chains).
 for(unsigned high:{50u,54u}){n64::RspDivider d;d.execute(high,0xe834);check(d.execute(49,0xe834)==0xfffa,"published wide RCP low");check(d.execute(49,0xe834)==0x9e1b,"published narrow RCP low");d.execute(high,0xe834);check(d.execute(53,0xe834)==0x5bc2,"published wide RSQ low");check(d.execute(53,0xe834)==0xc2ff,"published narrow RSQ low");}
 n64::RspVector vec;std::array<uint32_t,32> scalar{};std::array<uint8_t,8192> memory{};uint64_t vectors=0;
 for(unsigned vt=0;vt<32;++vt)for(unsigned field=0;field<32;++field)for(unsigned alias=0;alias<2;++alias)for(unsigned e=0;e<16;++e)for(unsigned fn=48;fn<=54;++fn)for(unsigned profile=0;profile<4;++profile){
  vec.reset();const unsigned vd=alias?vt:(vt+1)%32,de=field%8;
  for(unsigned r=0;r<32;++r)for(unsigned i=0;i<8;++i)put(vec.registers,r,i,(r*8191+i*6553+profile*32767)%65536);
  for(unsigned i=0;i<8;++i){vec.accumulator[i]=(0xdeadbeef0000ull+i*0x100000000ull)&vec.acc_mask;flags(vec.flags,profile*8+i,i);}
  vec.divider.input=uint16_t(profile*16383);vec.divider.output=profile*0x55555555u;vec.divider.pending=profile%2;
  auto bank=vec.registers;auto acc=vec.accumulator;const auto old_flags=vec.flags;auto d=vec.divider;
  const unsigned raw=half(bank,vt,e%8);unsigned word;
  if(fn==51)word=half(bank,vt,lane_map[e][de]);
  else if(fn==50 || fn==54){word=d.output/65536;d.input=uint16_t(raw);d.pending=true;}
  else{const uint32_t input=(fn==49 || fn==53) && d.pending?uint32_t(d.input)*65536+raw:raw>=32768?0xffff0000u|raw:raw;d.output=reference(fn>=52,input);d.pending=false;word=d.output%65536;}
  for(unsigned i=0;i<8;++i){acc[i]=acc[i]/65536*65536+half(bank,vt,lane_map[e][i]);}
  put(bank,vd,de,word);
  check(vec.execute(ins(fn,e,field,vt,vd),scalar,memory) && vec.registers==bank && vec.accumulator==acc && vec.flags==old_flags && vec.divider.snapshot()==d.snapshot(),"single-lane/selection/alias/flags/upper guards");++vectors;
 }
 for(unsigned fn:{55u,63u}){const auto bank=vec.registers;const auto raw=vec.divider.snapshot();const auto before=vec.accumulator;const auto f=vec.flags;check(vec.execute(ins(fn,15,31,31,31),scalar,memory) && vec.registers==bank && vec.accumulator==before && vec.flags==f && vec.divider.snapshot()==raw,"no-op guards");}
 check(argc==2,"original ROM required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};n64::Machine m;check(m.load(rom.data(),rom.size()),"ROM load");m.rsp.vectors=vec;m.rsp.vectors.divider.input=0;m.rsp.vectors.divider.pending=true;install(m,ins(49,0,0,1,2));std::vector<uint8_t> saved,after,replay;check(m.save(saved),"S020 save");check(m.rsp.step(m.sp_interface,m.dp_interface,m.sp,m.ram) && m.save(after) && m.restore(saved),"pending restore");check(m.rsp.step(m.sp_interface,m.dp_interface,m.sp,m.ram) && m.save(replay) && after==replay,"pending divider exact replay");
 for(unsigned bit=1;bit<16;++bit){auto bad=saved;bad[bad.size()-1053129-bit/8]|=uint8_t(1u<<(bit%8));check(m.restore(saved) && !m.restore(bad) && m.save(replay) && replay==saved,"reserved divider bits atomic rejection");}
 auto old=saved;old.resize(old.size()-1053136);old[6]='1';old[7]='9';check(m.restore(old) && m.rsp.vectors.divider.snapshot()==0 && m.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8),"S019 neutral divider conversion");
 step(m,0x4a000012);check(m.rsp.blocked && m.rsp.blocked_word==0x4a000012 && m.sp_interface.status==1 && !m.sp_interface.interrupt,"rare operation still blocks");
 std::cout<<values<<" divider value cases; 2,097,152 high/low chains; "<<vectors<<" vector register/selector/alias cases; tables, published observations, S020 replay/legacy/rejection PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
