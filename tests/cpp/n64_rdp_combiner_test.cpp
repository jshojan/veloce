#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
static void word(std::vector<uint8_t>& ram,unsigned at,uint64_t v){for(unsigned i=0;i<8;++i)ram[at+i]=uint8_t(v>>(56-i*8));}
static std::array<unsigned,16> decoded(const n64::RdpCombiner& c){const auto a=c.cycle(0),b=c.cycle(1);return {a.rgb_a,a.rgb_b,a.rgb_c,a.rgb_d,a.alpha_a,a.alpha_b,a.alpha_c,a.alpha_d,b.rgb_a,b.rgb_b,b.rgb_c,b.rgb_d,b.alpha_a,b.alpha_b,b.alpha_c,b.alpha_d};}
int main(int argc,char** argv){try{
 require(argc==2,"original ROM required");std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& d=m.dp_interface;
 const unsigned shifts[]={52,28,47,15,44,12,41,9,37,24,32,6,21,3,18,0},widths[]={4,4,5,3,3,3,3,3,4,4,5,3,3,3,3,3};
 uint64_t selectors=0;
 // Every pair of selector slots and every legal value, including aliases of zero.
 for(unsigned i=0;i<16;++i)for(unsigned j=0;j<16;++j)for(unsigned a=0;a<(1u<<widths[i]);++a)for(unsigned b=0;b<(1u<<widths[j]);++b){
  const uint64_t packed=(uint64_t(a)<<shifts[i])|(uint64_t(b)<<shifts[j]);n64::RdpCombiner c;require(c.command(60,0xfc00000000000000ull|packed),"combine accepted");std::array<unsigned,16> want{};want[i]=a;want[j]|=b;require(decoded(c)==want && c.combine==packed,"selector pair isolation");++selectors;
 }
 uint64_t streams=0;
 for(unsigned raw=0;raw<65536;++raw)for(unsigned pattern=0;pattern<4;++pattern)for(unsigned source=0;source<2;++source){
  const uint64_t payload=(uint64_t(raw)*0x1000100010001ull)^(pattern&1?~0ull:0ull);
  const uint32_t color=uint32_t(payload^(pattern&2?0xa55ac33cull:0ull));
  const uint64_t primitive=(uint64_t(raw&8191)<<32)|color,combine=payload&0x00ffffffffffffffull;
  d.reset();d.flags=source;m.dp_interrupt=false;
  word(m.ram,0x100,0xfc00000000000000ull|combine);word(m.ram,0x108,0xfa00000000000000ull|primitive);word(m.ram,0x110,0xfbffffff00000000ull|color);word(m.ram,0x118,0x2900000000000000ull);
  if(source)for(unsigned i=0;i<32;++i)m.sp[0x100+i]=m.ram[0x100+i];
  require(d.write(0,0x100) && d.write(1,0x120),"list activation");
  for(unsigned i=0;i<3;++i){d.tick(m.ram,m.sp,m.dp_interrupt);require(!d.blocked && !m.dp_interrupt && d.current==0x108+i*8,"state command progresses without IRQ");}
  require(d.combiner.snapshot()==n64::RdpCombiner::State{combine,primitive,color},"full color and selector capture");require(d.combiner.minimum_lod()==(raw>>8&31) && d.combiner.lod_fraction()==(raw&255) && d.combiner.primitive_color()==color,"LOD and RGBA fields");
  d.tick(m.ram,m.sp,m.dp_interrupt);require(m.dp_interrupt && d.current==0x120 && !d.pipeline,"FullSync alone asserts IRQ");++streams;
 }
 // Upper bits in color/primitive commands are discarded, all 32 color bits retained.
 for(unsigned bit=0;bit<64;++bit){n64::RdpCombiner c;const auto v=1ull<<bit;c.command(58,v);c.command(59,v);require(c.primitive==(bit<45?v:0) && c.environment==(bit<32?v:0),"reserved input bit convention");const auto before=c.snapshot();require(!c.command(8,v) && c.snapshot()==before,"unsupported command preserves attributes");}
 // Attribute commands must not change a RAM pixel or make an unsupported draw succeed.
 d.reset();d.combiner.command(60,0xfcfffffffffe793cull);d.combiner.command(58,0xffffffffffffffffull);d.combiner.command(59,0x12345678);word(m.ram,0x100,0x0800000000000000ull);d.write(0,0x100);d.write(1,0x120);m.dp_interrupt=false;const auto attributes=d.combiner.snapshot();const auto memory=m.ram;
 for(unsigned i=0;i<4;++i)d.tick(m.ram,m.sp,m.dp_interrupt);require(d.blocked && d.current==0x120 && d.blocked_word==0x0800000000000000ull && d.blocked_reason==1 && !m.dp_interrupt && memory==m.ram && d.combiner.snapshot()==attributes,"unsupported triangle remains explicit");
 // Save with one attribute issued and two pending behind freeze; replay and rejection.
 d.reset();word(m.ram,0x100,0xfcfffffffffe793cull);word(m.ram,0x108,0xfa001fab12345678ull);word(m.ram,0x110,0xfb000000aabbccddull);d.write(0,0x100);d.write(1,0x118);d.tick(m.ram,m.sp,m.dp_interrupt);d.flags=2;
 std::vector<uint8_t> saved,after,replay;require(m.save(saved),"save");d.flags=0;for(unsigned i=0;i<2;++i)m.clock();require(m.save(after),"save executed");require(m.restore(saved),"restore frozen");d.flags=0;for(unsigned i=0;i<2;++i)m.clock();require(m.save(replay) && replay==after,"pending attribute exact replay");
 for(unsigned field=0;field<3;++field)for(unsigned bit=(field==0?56:field==1?45:32);bit<64;++bit){auto bad=saved;bad[bad.size()-1053120+field*8+7-bit/8]|=uint8_t(1u<<(bit%8));require(m.restore(saved) && !m.restore(bad) && m.save(replay) && replay==saved,"invalid attribute state atomic rejection");}
 auto old=saved;old.resize(old.size()-1053120);old[6]='1';old[7]='7';require(m.restore(old) && d.combiner.snapshot()==n64::RdpCombiner{}.snapshot(),"S017 neutral attribute conversion");require(m.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8),"S017 preserves previous state");
 m.reset();require(d.combiner.snapshot()==n64::RdpCombiner{}.snapshot(),"reset neutral attributes");
 std::cout<<selectors<<" selector pairs, "<<streams<<" DRAM/XBUS color-state streams; IRQ, unsupported draw, frozen replay and S018/legacy rejection passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
