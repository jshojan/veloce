#include "rdp_tmem.hpp"
#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <algorithm>
using Memory=std::array<uint8_t,4096>;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
// Independent inverse-bank address algebra, using division/remainders and a
// byte-level scatter. Does not call production helpers or use its bank walk.
static uint32_t oracle(unsigned op,uint64_t command,const n64::RdpTexture& texture,const std::vector<uint8_t>& ram,Memory& want){
 const bool block=op==51,palette=op==48;const auto& tile=texture.tiles[(command/16777216)%8];
 const unsigned a=(command/(1ull<<44))%4096,b=(command/(1ull<<32))%4096,c=(command/4096)%4096,d=command%4096;
 const unsigned start_x=block?a:a/4,start_y=block?b%1024:b/4,end_x=!block && b==((d/4)*4+3)?0:block?c:c/4,end_y=block?start_y:d/4;
 const unsigned count=(end_x+4097-start_x)%4096;const bool narrow_palette=palette && texture.size()==2;
 const unsigned step=narrow_palette?1:8/(1u<<(texture.size()-1));uint32_t groups=0;
 auto read=[&](uint64_t at){at%=16777216;return at<ram.size()?ram[size_t(at)]:uint8_t(0);};
 for(unsigned y=start_y;y<=end_y;++y)for(unsigned n=0;n*step<count;++n){
  const int x=block && start_x>=2048?int(start_x)-4096:int(start_x);
  const uint64_t at=(uint64_t(texture.address())+uint32_t(int(y*texture.width())+x)*(1u<<(texture.size()-1))+(narrow_palette?2:8)*n)%16777216;
  const unsigned raw_s=(a*8+n*((block?128:512)/(1u<<texture.size())))%65536;
  const int signed_s=int(raw_s)-(raw_s>=32768?65536:0)-int(a*8);
  const int divisor=block || palette?8:32;
  const unsigned s=unsigned(signed_s>=0?signed_s/divisor:-((-signed_s+divisor-1)/divisor));
  unsigned t=block?n*d/2048:y-start_y;
  unsigned stride=(tile.line()*t)%512,half_s=tile.format()==1 || tile.size()==1?s/2:tile.size()>=2?s:s/4;
  unsigned q=((tile.address()+stride)*4+half_s%2048)%2048;q-=q/2%2*2;
  auto location=[&](unsigned bank){const unsigned original=bank^(t%2*2);unsigned address=q/4*4+original+(original<q%4?4:0);return (address^(t%2*2))%1024;};
  auto pair=[&](unsigned address,uint8_t hi,uint8_t lo){want[(address%2048)*2]=hi;want[(address%2048)*2+1]=lo;};
  auto data=[&](unsigned i){return read(at+(palette && at%2==0?i%2:i));};
  if(tile.format()==1 || (tile.format()==0 && tile.size()==3)){
   unsigned first=(half_s/2%2+t%2)%2*2;
   for(unsigned i=0;i<2;++i){unsigned dest=location(first+i);if(tile.format()==1){pair(dest,data(i*4),data(i*4+2));pair(dest+1024,data(i*4+1),data(i*4+3));}else{pair(dest,data(i*4),data(i*4+1));pair(dest+1024,data(i*4+2),data(i*4+3));}}
  }else for(unsigned bank=0;bank<4;++bank){unsigned i=(bank+t%2*2)%4;pair(location(bank)+(q>=1024?1024:0),data(2*i),data(2*i+1));}
  ++groups;
 }
 return groups;
}
static uint64_t command(unsigned op,unsigned tile,unsigned sl,unsigned tl,unsigned sh,unsigned last){return (uint64_t(op)<<56)|(uint64_t(sl)<<44)|(uint64_t(tl)<<32)|(uint64_t(tile)<<24)|(uint64_t(sh)<<12)|last;}
int main(int argc,char** argv){try{
 std::vector<uint8_t> ram(65536);for(unsigned i=0;i<ram.size();++i)ram[i]=uint8_t(i*37+i/256);
 uint64_t cases=0;
 auto run=[&](unsigned op,unsigned source,unsigned format,unsigned size,unsigned base,unsigned pitch,unsigned align,unsigned count,unsigned dxt,unsigned sl=0,unsigned tl=0){
  n64::RdpTexture texture;texture.image=(uint64_t(source)<<51)|(31ull<<32)|(256+align);const unsigned tile=unsigned(cases)%8;texture.tiles[tile].descriptor=(uint64_t(format)<<53)|(uint64_t(size)<<51)|(uint64_t(pitch)<<41)|(uint64_t(base)<<32);
  const uint64_t cmd=command(op,tile,sl,tl,op==51?(sl+count-1)%4096:(sl+4*(count-1))%4096,op==51?dxt:tl);
  n64::RdpTmem memory;memory.bytes.fill(0xa5);Memory want=memory.bytes;const auto state=texture.snapshot();const uint32_t expected=oracle(op,cmd,texture,ram,want);uint32_t observed;
  check(memory.load(op,cmd,texture,ram,observed)==0 && observed==expected && memory.bytes==want,"TMEM byte scatter mismatch");
  auto after=state;after[2*tile+1]=cmd&n64::RdpTexture::bounds_mask;check(texture.snapshot()==after,"load bounds update isolation");++cases;
 };
 for(unsigned base=0;base<512;++base)for(unsigned source=1;source<=3;++source)for(unsigned size=0;size<4;++size)for(unsigned format=0;format<8;++format)for(unsigned align=0;align<8;++align)run(51,source,format,size,base,3,align,17,872);
 const unsigned bases[]={0,255,511},pitches[]={0,1,511};
 for(unsigned dxt=0;dxt<4096;++dxt)for(unsigned profile=0;profile<4;++profile)for(unsigned base:bases)for(unsigned pitch:pitches)run(51,profile==3?2:profile+1,profile==3?1:0,profile==3?2:profile+1,base,pitch,dxt%8,21,dxt);
 for(unsigned count=1;count<4096;++count)for(unsigned profile=0;profile<4;++profile)run(51,profile==3?2:profile+1,profile==3?1:0,profile==3?2:profile+1,511,1,count%8,count,103);
 for(unsigned raw=0;raw<65536;++raw){for(unsigned n=0;n<8;++n)ram[256+n]=uint8_t((raw^(n*4097))>>(n%2?0:8));run(51,2,0,2,raw%512,0,0,1,0);}
 for(unsigned op:{48u,52u})for(unsigned source=1;source<4;++source)for(unsigned size=0;size<4;++size)for(unsigned format=0;format<8;++format)for(unsigned base:bases)for(unsigned align=0;align<8;++align)for(unsigned fraction=0;fraction<4;++fraction)run(op,source,format,size,base,5,align,7,0,fraction,4+fraction);
 for(unsigned raw=0;raw<4096;++raw)for(unsigned source=1;source<4;++source)for(unsigned op:{51u,52u})run(op,source,0,source,raw%512,raw%512,raw%8,4,raw,raw,(raw*37)%4096);
 // Multirow tiles: fractional origins, all row parities and extreme strides.
 for(unsigned source=1;source<4;++source)for(unsigned fraction=0;fraction<4;++fraction)for(unsigned rows=1;rows<=8;++rows)for(unsigned pitch:pitches){
  n64::RdpTexture texture;texture.image=(uint64_t(source)<<51)|(31ull<<32)|256;texture.tiles[7].descriptor=(uint64_t(source)<<51)|(uint64_t(pitch)<<41)|(511ull<<32);n64::RdpTmem memory;memory.bytes.fill(0xa5);Memory want=memory.bytes;const auto cmd=command(52,7,fraction,4+fraction,4*6+fraction,4*rows+fraction);const auto expected=oracle(52,cmd,texture,ram,want);uint32_t got;check(!memory.load(52,cmd,texture,ram,got) && got==expected && memory.bytes==want,"multirow tile bytes");++cases;
 }
 // Literal normal, odd-row, split-bank and palette observations, independent of oracle.
 for(unsigned i=0;i<16;++i)ram[256+i]=uint8_t(i+1);
 n64::RdpTexture texture;texture.image=(2ull<<51)|256;texture.tiles[0].descriptor=2ull<<51;n64::RdpTmem memory;uint32_t groups;
 check(!memory.load(51,command(51,0,0,0,7,2048),texture,ram,groups) && groups==2,"literal block");for(unsigned i=0;i<8;++i){check(memory.bytes[i]==i+1,"first literal word");check(memory.bytes[8+i]==((i^4)+9),"odd row swaps longs");}
 texture.tiles[0].descriptor=3ull<<51;memory.bytes.fill(0);texture.image=(3ull<<51)|256;check(!memory.load(51,command(51,0,0,0,1,0),texture,ram,groups),"split literal");check(memory.bytes[0]==1 && memory.bytes[1]==2 && memory.bytes[2]==5 && memory.bytes[3]==6 && memory.bytes[2048]==3 && memory.bytes[2049]==4 && memory.bytes[2050]==7 && memory.bytes[2051]==8,"RGBA split halves");
 texture.image=(2ull<<51)|256;texture.tiles[0].descriptor=256ull<<32;memory.bytes.fill(0);check(!memory.load(48,command(48,0,0,0,4,0),texture,ram,groups) && groups==2,"palette literal");for(unsigned i=0;i<8;++i){check(memory.bytes[2048+i]==(i%2?2:1),"palette replication");check(memory.bytes[2056+i]==(i%2?4:3),"palette entry stride");}
 // Literal regressions for the external reference comparison findings.
 texture.image=(1ull<<51)|256;texture.tiles[0].descriptor=2ull<<51;memory.bytes.fill(0);
 check(!memory.load(48,command(48,0,0,0,0,0),texture,ram,groups) && groups==1,"aligned eight-bit palette group");
 for(unsigned i=0;i<8;++i)check(memory.bytes[i]==ram[256+i%2],"palette alignment controls replication");
 texture.image=(2ull<<51)|257;memory.bytes.fill(0);
 check(!memory.load(48,command(48,0,0,0,0,0),texture,ram,groups) && groups==1,"odd palette group");
 for(unsigned i=0;i<8;++i)check(memory.bytes[i]==ram[257+i],"odd palette source uses whole word");
 texture.image=(1ull<<51)|256;texture.tiles[0].descriptor=1ull<<51;memory.bytes.fill(0);
 check(!memory.load(52,command(52,0,0,3,400,3),texture,ram,groups) && groups==1,"fraction-three empty span resets right edge");
 for(unsigned i=0;i<8;++i)check(memory.bytes[i]==ram[256+i],"empty fractional span literal");
 texture.image=(1ull<<51)|(31ull<<32)|256;memory.bytes.fill(0);
 check(!memory.load(52,command(52,0,8,3,24,3),texture,ram,groups) && groups==512,"fractional span modulo count and signed coordinates");
 for(unsigned i=0;i<8;++i)check(memory.bytes[4088+i]==ram[4346+i],"signed coordinate wrap changes destination bank");
 const auto guarded_bytes=memory.bytes;const auto state=texture.snapshot();texture.image=0;check(memory.load(51,command(51,0,0,0,3,0),texture,ram,groups)==3 && memory.bytes==guarded_bytes,"four-bit image blocks atomically");texture.restore(state);check(memory.load(48,command(48,0,0,0,4,4),texture,ram,groups)==3 && memory.bytes==guarded_bytes && texture.snapshot()==state,"multirow palette blocks atomically");
 check(argc==2,"original ROM required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};n64::Machine machine;check(machine.load(rom.data(),rom.size()),"machine ROM load");auto& dp=machine.dp_interface;
 auto put=[&](unsigned at,uint64_t word){for(unsigned i=0;i<8;++i)machine.ram[at+i]=uint8_t(word>>(56-8*i));};
 for(unsigned source=0;source<2;++source)for(unsigned op:{48u,51u,52u}){dp.reset();dp.flags=source;dp.texture.image=(2ull<<51)|0x200;dp.texture.tiles[0].descriptor=(2ull<<51)|(256ull<<32);for(unsigned i=0;i<32;++i)machine.ram[0x200+i]=uint8_t(i+1);put(0xff8,command(op,0,0,0,4,op==51?2048:0));put(0x1000,0x2900000000000000ull);if(source)for(unsigned i=0;i<16;++i)machine.sp[(0xff8+i)&4095]=machine.ram[0xff8+i];machine.dp_interrupt=false;dp.write(0,0xff8);dp.write(1,0x1000);const auto ram_before=machine.ram;dp.tick(machine.ram,machine.sp,machine.dp_interrupt);check(!dp.blocked && dp.current==0x1000 && !machine.dp_interrupt && dp.tmem_busy==(op==52?1u:2u) && machine.ram==ram_before,"load progression/counter without IRQ or RAM write");dp.write(1,0x1008);dp.tick(machine.ram,machine.sp,machine.dp_interrupt);check(machine.dp_interrupt && !dp.pipeline && dp.current==0x1008,"real FullSync after extended END");}
 dp.reset();dp.texture.image=(2ull<<51)|0x200;dp.texture.tiles[0].descriptor=(2ull<<51)|(5ull<<41)|(511ull<<32);dp.tmem.bytes.fill(0xc3);machine.rsp.vectors.divider={0,0x1234abcd,true};put(0x100,command(51,0,0,0,7,2048));put(0x108,command(52,0,0,0,4,4));dp.write(0,0x100);dp.write(1,0x110);dp.flags=2;
 std::vector<uint8_t> saved,after,replay;check(machine.save(saved),"S023 save");dp.flags=0;machine.clock();machine.clock();check(machine.save(after) && machine.restore(saved),"frozen load restore");dp.flags=0;machine.clock();machine.clock();check(machine.save(replay) && after==replay,"pending loads exact S023 replay");
 auto old=saved;old.resize(old.size()-1052944);old[7]='1';check(machine.restore(old) && dp.tmem.bytes==Memory{} && machine.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8),"S021 neutral TMEM conversion preserves descriptors and divider");
 for(bool extra:{false,true}){auto bad=saved;if(extra)bad.push_back(0);else bad.pop_back();check(machine.restore(saved) && !machine.restore(bad) && machine.save(replay) && replay==saved,"invalid state length rejects atomically");}
 machine.reset();check(dp.tmem.bytes==Memory{},"TMEM reset");
 std::cout<<cases<<" independent TMEM byte cases; literal interleave/split/palette invalid-profile guards, DP/XBUS/IRQ and S023 replay/legacy PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
