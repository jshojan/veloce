#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void put(std::vector<uint8_t>& r,unsigned a,uint64_t v){for(unsigned i=0;i<8;++i)r[a+i]=uint8_t(v>>(56-i*8));}
static uint64_t keep(uint64_t v,const unsigned* shifts,const unsigned* widths,unsigned count){uint64_t out=0;for(unsigned i=0;i<count;++i)out|=((v>>shifts[i])% (1ull<<widths[i]))<<shifts[i];return out;}
int main(int argc,char** argv){try{
 check(argc==2,"ROM required");std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;check(m.load(rom.data(),rom.size()),"load");auto& d=m.dp_interface;
 const unsigned shifts[]={53,51,41,32,20,19,18,14,10,9,8,4,0},widths[]={3,2,9,9,4,1,1,4,4,1,1,4,4};
 const unsigned bs[]={44,32,12,0},bw[]={12,12,12,12},is[]={53,51,32,0},iw[]={3,2,10,26};
 uint64_t pairs=0;
 for(unsigned tile=0;tile<8;++tile)for(unsigned i=0;i<13;++i)for(unsigned j=0;j<13;++j)for(unsigned a=0;a<(1u<<widths[i]);++a)for(unsigned b=0;b<(1u<<widths[j]);++b){
  n64::RdpTexture t;const uint64_t word=(uint64_t(a)<<shifts[i])|(uint64_t(b)<<shifts[j]);t.command(53,word|(uint64_t(tile)<<24));const auto& x=t.tiles[tile];
  const unsigned got[]={x.format(),x.size(),x.line(),x.address(),x.palette(),x.clamp(true),x.mirror(true),x.mask(true),x.shift(true),x.clamp(false),x.mirror(false),x.mask(false),x.shift(false)};
  for(unsigned k=0;k<13;++k)check(got[k]==((k==i?a:0u)|(k==j?b:0u)),"descriptor pair fields");
  for(unsigned k=0;k<8;++k){if(k!=tile)check(!t.tiles[k].descriptor && !t.tiles[k].bounds,"tile isolation");}++pairs;
 }
 uint64_t streams=0;
 for(unsigned raw=0;raw<65536;++raw)for(unsigned source=0;source<2;++source)for(unsigned tile=0;tile<8;++tile){
  const uint64_t payload=uint64_t(raw)*0x1000100010001ull;const uint64_t descriptor=keep(payload,shifts,widths,13),bounds=keep(~payload,bs,bw,4),image=keep(payload^0xa55ac33cf00f1234ull,is,iw,4);
  d.reset();d.flags=source;m.dp_interrupt=false;put(m.ram,0x100,0xfd00000000000000ull|image);put(m.ram,0x108,0xf500000000000000ull|descriptor|(uint64_t(tile)<<24));put(m.ram,0x110,0xf200000000000000ull|bounds|(uint64_t(tile)<<24));put(m.ram,0x118,0x2900000000000000ull);
  if(source){for(unsigned n=0;n<32;++n)m.sp[0x100+n]=m.ram[0x100+n];}d.write(0,0x100);d.write(1,0x120);
  for(unsigned n=0;n<3;++n){d.tick(m.ram,m.sp,m.dp_interrupt);check(!d.blocked && !m.dp_interrupt && d.current==0x108+n*8,"command progression");}
  check(d.texture.image==image && d.texture.tiles[tile].descriptor==descriptor && d.texture.tiles[tile].bounds==bounds,"retained command state");
  const auto& x=d.texture.tiles[tile];check(x.low(false)==((~payload>>44)&4095) && x.low(true)==((~payload>>32)&4095) && x.high(false)==((~payload>>12)&4095) && x.high(true)==((~payload)&4095),"raw 10.2 bounds");
  check(d.texture.width()==((image>>32)&1023)+1 && d.texture.address()==uint32_t(image&0x3ffffff),"image width and address");d.tick(m.ram,m.sp,m.dp_interrupt);check(m.dp_interrupt && d.current==0x120 && !d.pipeline,"FullSync IRQ");++streams;
 }
 // Updates retain all neighboring descriptors, bounds and image state.
 for(unsigned target=0;target<8;++target){n64::RdpTexture t;for(unsigned slot=0;slot<8;++slot){t.command(53,(uint64_t(slot)<<24)|(uint64_t(slot+1)<<32)|(slot+1));t.command(50,(uint64_t(slot)<<24)|(uint64_t(slot+1)<<44)|(slot+1));}t.command(61,0xfd1003ff03ffffffull);
  auto before=t.snapshot();t.command(53,(uint64_t(target)<<24)|0x00fbffff00ffffffull);before[2*target]=0x00fbffff00ffffffull;check(t.snapshot()==before,"descriptor overwrite isolates neighbors and bounds");t.command(50,(uint64_t(target)<<24)|0x00ffffff00ffffffull);before[2*target+1]=0x00ffffff00ffffffull;check(t.snapshot()==before,"bounds overwrite isolates neighbors and descriptor");
 }
 uint64_t coordinates=0;
 for(unsigned slot=0;slot<8;++slot)for(unsigned field=0;field<4;++field)for(unsigned value=0;value<4096;++value){n64::RdpTexture t;t.command(50,(uint64_t(slot)<<24)|(uint64_t(value)<<bs[field]));const auto& x=t.tiles[slot];const unsigned got[]={x.low(false),x.low(true),x.high(false),x.high(true)};for(unsigned i=0;i<4;++i)check(got[i]==(i==field?value:0),"coordinate field isolation");++coordinates;}
 for(unsigned bit=0;bit<64;++bit){n64::RdpTexture t;const uint64_t v=1ull<<bit;t.command(53,v);check(t.tiles[(v>>24)&7].descriptor==keep(v,shifts,widths,13),"descriptor reserved input convention");t.command(50,v);check(t.tiles[(v>>24)&7].bounds==keep(v,bs,bw,4),"bounds input convention");t.command(61,v);check(t.image==keep(v,is,iw,4),"image input convention");const auto before=t.snapshot();check(!t.command(51,v) && t.snapshot()==before,"unsupported load preserved");}
 d.reset();put(m.ram,0x100,0xf510000007000000ull);put(m.ram,0x108,0xfd10003f00123456ull);d.write(0,0x100);d.write(1,0x110);d.tick(m.ram,m.sp,m.dp_interrupt);d.flags=2;m.rsp.vectors.divider={0,0x1234abcd,true};std::vector<uint8_t> saved,after,replay;check(m.save(saved),"save");d.flags=0;m.clock();check(m.save(after) && m.restore(saved),"restore");d.flags=0;m.clock();check(m.save(replay) && after==replay,"frozen command exact replay");
 const auto neutral=n64::RdpTexture{}.snapshot();
 for(unsigned field=0;field<17;++field){const unsigned* ss=field==16?is:field&1?bs:shifts;const unsigned* ww=field==16?iw:field&1?bw:widths;const unsigned count=field==16?4:field&1?4:13;for(unsigned bit=0;bit<64;++bit)if(!keep(1ull<<bit,ss,ww,count)){auto bad=saved;bad[bad.size()-1053080+field*8+7-bit/8]|=uint8_t(1u<<(bit%8));check(m.restore(saved) && !m.restore(bad) && m.save(replay) && replay==saved,"reserved state atomic rejection");}}
 auto old=saved;old.resize(old.size()-1053080);old[7]='0';check(m.restore(old) && d.texture.snapshot()==neutral && m.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8),"S020 neutral texture conversion preserves divider");
 for(unsigned op:{36u,8u}){d.reset();d.texture.command(53,0xf510000007000000ull);const auto texture=d.texture.snapshot();put(m.ram,0x100,uint64_t(op)<<56);d.write(0,0x100);const unsigned length=op==36?2:4;d.write(1,0x100+length*8);m.dp_interrupt=false;const auto ram=m.ram;for(unsigned i=0;i<length;++i)d.tick(m.ram,m.sp,m.dp_interrupt);check(d.blocked && d.current==0x100+length*8 && d.blocked_reason==1 && !m.dp_interrupt && ram==m.ram && d.texture.snapshot()==texture,"unimplemented load/draw preserves state and blocks");}
 m.reset();check(d.texture.snapshot()==neutral,"reset texture state");std::cout<<pairs<<" descriptor pairs; "<<streams<<" DRAM/XBUS state streams; "<<coordinates<<" isolated coordinates; reserved bits, FullSync, blocking, S021 replay/legacy PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
