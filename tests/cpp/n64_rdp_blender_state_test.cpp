#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void check(bool v){if(!v)throw std::runtime_error("blender state mismatch");}
static void put(std::vector<uint8_t>& memory,unsigned at,uint64_t word){for(unsigned n=0;n<8;++n)memory[at+n]=uint8_t(word>>(56-n*8));}
int main(int argc,char** argv){try{
 uint64_t commands=0;
 for(unsigned raw=0;raw<65536;++raw)for(unsigned op:{56u,57u})for(unsigned alias=0;alias<4;++alias)for(unsigned source=0;source<2;++source){
  const uint32_t payload=(raw<<16)|(raw^65535);n64::DpInterface dp;std::vector<uint8_t> ram(4096);std::array<uint8_t,8192> sp{};bool irq=false;
  put(ram,0xff8,(uint64_t(op+alias*64)<<56)|0x00a5c33c00000000ull|payload);for(unsigned n=0;n<8;++n)sp[0xff8+n]=ram[0xff8+n];put(ram,0,0xe900000000000000);for(unsigned n=0;n<8;++n)sp[n]=ram[n];
  dp.flags=source;dp.write(0,0xff8);dp.write(1,0x1000);dp.tick(ram,sp,irq);
  check(!dp.blocked && !irq && dp.current==0x1000 && dp.blender.fog==(op==56?payload:0) && dp.blender.blend==(op==57?payload:0));
  if(source){dp.write(1,0x1008);dp.tick(ram,sp,irq);check(irq && !dp.pipeline);}++commands;
 }
 n64::RdpPacket packet;packet.count=packet.needed=12;packet.words[0]=0xcc80001000080000;packet.words[1]=0x00020000ffff0000;packet.words[3]=65536;packet.words[4]=0x0080008000800080;
 n64::RdpCombiner combiner;combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(4ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|4;
 n64::RdpBlender latch;std::vector<uint8_t> ram(160,0x5a);
 const uint64_t image=(3ull<<51)|(3ull<<32)|64,scissor=(16ull<<12)|16,mode=15ull<<36;
 check(n64::RdpTriangleShade::draw(packet,image,mode|64,scissor,combiner,ram,nullptr,nullptr,nullptr,&latch)==0 && latch.memory_rgb==0x5a5a5a);
 std::fill(ram.begin(),ram.end(),0x12);auto want=ram;for(unsigned pos:{4u,8u,9u,12u}){for(unsigned lane=0;lane<3;++lane)want[64+pos*4+lane]=0x5a;want[64+pos*4+3]=(pos==4 || pos==8)?224:96;}
 check(n64::RdpTriangleShade::draw(packet,image,mode|(1ull<<30),scissor,combiner,ram,nullptr,nullptr,nullptr,&latch)==0 && ram==want && latch.memory_rgb==0x5a5a5a);
 for(uint64_t bit:{2ull,4ull,16ull,32ull,1ull<<52}){const auto before=ram;const auto state=latch.snapshot();check(n64::RdpTriangleShade::draw(packet,image,mode|bit,scissor,combiner,ram,nullptr,nullptr,nullptr,&latch)==1 && ram==before && latch.snapshot()==state);}
 // Z mode fields are inert when comparison is disabled. Comparison/update
 // still require hidden memory; those direct-call rejection checks remain above.
 for(uint64_t bits:{1024ull,2048ull,3072ull}){auto plain=ram,selected=ram;auto a=latch,b=latch;
  check(n64::RdpTriangleShade::draw(packet,image,mode|64,scissor,combiner,plain,nullptr,nullptr,nullptr,&a)==0);
  check(n64::RdpTriangleShade::draw(packet,image,mode|64|bits,scissor,combiner,selected,nullptr,nullptr,nullptr,&b)==0 && plain==selected && a.snapshot()==b.snapshot());}
 check(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine machine;check(machine.load(rom.data(),rom.size()));auto& dp=machine.dp_interface;
 dp.blender.memory_rgb=0x73519f;put(machine.ram,0x100,0xf8c33ca589abcdef);put(machine.ram,0x108,0xf900000012345678);put(machine.ram,0x110,0xe900000000000000);dp.flags=2;dp.write(0,0x100);dp.write(1,0x118);
 std::vector<uint8_t> saved,after,replay;check(machine.save(saved) && saved.size()==9528128 && saved[6]=='3' && saved[7]=='2');dp.flags=0;unsigned clocks=0;while(!machine.dp_interrupt && clocks<10){machine.clock();++clocks;}check(machine.dp_interrupt && dp.blender.snapshot()==n64::RdpBlender::State{0x89abcdef,0x12345678,0x73519f} && machine.save(after));
 check(machine.restore(saved));dp.flags=0;for(unsigned i=0;i<clocks;++i)machine.clock();check(machine.save(replay) && replay==after);
 for(unsigned field=0;field<3;++field)for(unsigned bit=field==2?24:32;bit<64;++bit){auto bad=after;bad[bad.size()-1049176+field*8+7-bit/8]|=uint8_t(1u<<(bit%8));check(!machine.restore(bad) && machine.save(replay) && replay==after);}
 auto old=after;old.resize(old.size()-1049176);old[6]='2';old[7]='5';check(machine.restore(old) && dp.blender.snapshot()==n64::RdpBlender::State{} && machine.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8));
 machine.reset();check(dp.blender.snapshot()==n64::RdpBlender::State{});
 std::cout<<commands<<" fog/blend field/alias/DRAM-XBUS captures; retained-memory framebuffer, atomic profile rejection, S030 FullSync replay/rejection/S025 import PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
