#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
static uint64_t box(unsigned l,unsigned t,unsigned r,unsigned b){return (uint64_t(r)<<44)|(uint64_t(b)<<32)|(uint64_t(l)<<12)|t;}
static uint64_t clip(unsigned l,unsigned t,unsigned r,unsigned b,unsigned flags=0){return (uint64_t(l)<<44)|(uint64_t(t)<<32)|(uint64_t(flags)<<24)|(uint64_t(r)<<12)|b;}
static void put(std::vector<uint8_t>& ram,unsigned at,uint64_t v){for(unsigned i=0;i<8;++i)ram[at+i]=uint8_t(v>>(56-i*8));}
// Independent sample enumeration: rows are present if any quarter scanline
// survives clipping; a fill span includes the truncated clipped X endpoint.
static std::vector<uint8_t> oracle(unsigned l,unsigned t,unsigned r,unsigned b,
 unsigned sl,unsigned st,unsigned sr,unsigned sb,unsigned flags,unsigned bytes,unsigned width){
 std::vector<uint8_t> out(128,0x5a);if(l>r || sl>=sr || r<sl || l>=sr)return out;
 for(unsigned y=0;y<4;++y){bool row=false;for(unsigned q=4*y;q<4*y+4;++q)if(q>=t && q>=st && q<(b|3) && q<sb)row=true;
  if((flags&2) && (y&1)!=(flags&1))row=false;
  if(!row)continue;
  for(unsigned x=0;x<4;++x){bool hit=false;for(unsigned q=4*x;q<4*x+4;++q)if(q>=l && q>=sl && q<=r && q<=sr)hit=true;
   if(!hit)continue;
   const unsigned at=32+(y*width+x)*bytes;
   for(unsigned n=0;n<bytes;++n)out[at+n]=uint8_t(0x12345679u>>(8*(3-((at+n)&3))));
  }
 }
 return out;
}
int main(int argc,char** argv){try{
 require(argc==2,"original ROM required");std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");
 const std::array<std::array<unsigned,5>,8> scissors={{{0,0,16,16,0},{4,4,12,12,0},{5,2,11,10,1},{0,0,16,16,2},{0,0,16,16,3},{4,4,4,12,0},{12,12,4,4,0},{8,0,16,3,0}}};
 uint64_t geometry=0;
 for(unsigned encoded=0;encoded<65536;++encoded){const unsigned l=encoded&15,t=encoded>>4&15,r=encoded>>8&15,b=encoded>>12;
  for(const auto& s:scissors)for(unsigned size=1;size<=3;++size){std::vector<uint8_t> ram(128,0x5a);const unsigned width=(encoded&1)?3:4,bytes=1u<<(size-1);
   const uint64_t image=(uint64_t(size)<<51)|(uint64_t(width-1)<<32)|32;
   require(!n64::RdpFill::draw(box(l,t,r,b),image,0xb82c3f0f0a4000ull,clip(s[0],s[1],s[2],s[3],s[4]),0x12345679,ram),"fractional draw accepted");
   require(ram==oracle(l,t,r,b,s[0],s[1],s[2],s[3],s[4],bytes,width),"fractional/field/stride pixels and guards");++geometry;
  }
 }
 uint64_t modes=0;
 for(unsigned raw=0;raw<65536;++raw)for(unsigned size=1;size<=3;++size){std::vector<uint8_t> ram(128,0x5a),want=ram;const uint64_t mode=0xb82c3f0f0a0000ull|raw;
  const bool unsafe=(raw&0x50) || ((raw&0x20) && !(raw&4));
  if(!unsafe)for(unsigned n=0;n<(1u<<(size-1));++n)want[32+n]=uint8_t(0x12345679u>>(24-8*n));
  const auto reason=n64::RdpFill::draw(box(0,0,0,0),(uint64_t(size)<<51)|32,mode,clip(0,0,16,16),0x12345679,ram);
  require(reason==(unsafe?3u:0u) && ram==want,"all low mode encodings/safe bypass/atomic hazard");++modes;
 }
 for(unsigned bit=0;bit<56;++bit){std::vector<uint8_t> ram(128,0x5a);const uint64_t mode=(3ull<<52)^(1ull<<bit);const bool unsafe=bit==4 || bit==5 || bit==6 || bit==52 || bit==53;require(n64::RdpFill::draw(box(0,0,0,0),(2ull<<51)|32,mode,clip(0,0,16,16),0x12345679,ram)==(unsafe?3u:0u),"all mode bit positions");}
 // All fill data bytes, all widths and absolute-address parity, including row overlap.
 for(unsigned raw=0;raw<65536;++raw)for(unsigned size=1;size<=3;++size){std::vector<uint8_t> ram(128,0x5a);const uint32_t color=raw*65536+(65535-raw);require(!n64::RdpFill::draw(box(0,0,12,12),(uint64_t(size)<<51)|(2ull<<32)|32,3ull<<52,clip(0,0,16,16),color,ram),"raw fill code");for(unsigned at=32;at<32+13*(1u<<(size-1));++at)require(ram[at]==uint8_t(color>>(8*(3-(at&3)))),"raw bits and odd stride phase");}
 for(unsigned width=1;width<=1024;++width){std::vector<uint8_t> ram(16384,0x5a);const uint64_t image=(2ull<<51)|(uint64_t(width-1)<<32)|32;require(!n64::RdpFill::draw(box(4092,4,4092,4),image,3ull<<52,clip(0,0,4095,4095),0x12345679,ram),"all row strides");const auto at=32+(width+1023)*2;require(ram[at]==((at&2)?0x56:0x12) && ram[at+1]==((at&2)?0x79:0x34),"row address beyond width");}
 for(unsigned format=0;format<8;++format)for(unsigned size=0;size<4;++size){std::vector<uint8_t> ram(128,0x5a);require(n64::RdpFill::draw(box(0,0,0,0),(uint64_t(format)<<53)|(uint64_t(size)<<51)|32,3ull<<52,clip(0,0,16,16),0x12345679,ram)==(size?0u:3u),"raw fill ignores format; 4-bit blocked");}
 // Selected field rows determine RAM bounds, not rows excluded by scissoring.
 std::vector<uint8_t> ram(128,0x5a);require(!n64::RdpFill::draw(box(0,0,12,4),(2ull<<51)|(3ull<<32)|120,3ull<<52,clip(0,0,16,16,2),0xffffffff,ram),"excluded out-of-RAM odd row");const auto before=ram;require(n64::RdpFill::draw(box(0,0,12,4),(2ull<<51)|(3ull<<32)|120,3ull<<52,clip(0,0,16,16,3),0xffffffff,ram)==5 && ram==before,"selected out-of-RAM row atomicity");
 auto& d=m.dp_interface;uint64_t addresses=0;put(m.ram,0,0xfeffffffff000000ull);
 for(uint32_t raw=0;raw<0x4000000;++raw){for(unsigned i=0;i<4;++i)m.ram[4+i]=uint8_t(raw>>(24-i*8));d.current=0;d.active_end=8;m.dp_interrupt=false;d.tick(m.ram,m.sp,m.dp_interrupt);require(d.depth_image==raw && !d.blocked && d.current==8 && !m.dp_interrupt,"every 26-bit depth address");++addresses;}
 for(unsigned bit=26;bit<56;++bit){put(m.ram,0,0xfe00000000000000ull|(1ull<<bit)|0x123456);d.current=0;d.active_end=8;d.tick(m.ram,m.sp,m.dp_interrupt);require(d.depth_image==0x123456,"depth reserved/opcode alias input convention");}
 for(unsigned alias=0;alias<4;++alias){put(m.ram,0,(uint64_t(alias)<<62)|(62ull<<56)|0x00ffffff00123456ull);d.current=0;d.active_end=8;d.tick(m.ram,m.sp,m.dp_interrupt);require(d.depth_image==0x123456,"opcode upper-bit alias convention");}
 // Actual command replay clears the selected color/depth RAM, never another buffer.
 m.reset();const uint64_t list[]={0xfe00000000700048ull,0xff10000300700048ull,0xed00000000010010ull,0xefb82c3f0f0a4000ull,0xf7000000fffcfffcull,0xf600c00c00000000ull,0xe900000000000000ull};
 for(unsigned i=0;i<7;++i){put(m.ram,0x100+i*8,list[i]);}
 std::fill(m.ram.begin()+0x6ffff8,m.ram.begin()+0x700090,0x5a);d.write(0,0x100);d.write(1,0x138);d.tick(m.ram,m.sp,m.dp_interrupt);d.flags=2;
 std::vector<uint8_t> saved,after,replay;require(m.save(saved),"S019 save");d.flags=0;for(unsigned i=0;i<9;++i)m.clock();require(m.save(after),"executed save");require(m.restore(saved),"frozen restore");d.flags=0;for(unsigned i=0;i<9;++i)m.clock();require(m.save(replay) && after==replay && m.dp_interrupt && d.depth_image==0x700048,"pending depth clear exact replay/IRQ");
 for(unsigned at=0x6ffff8;at<0x700090;++at){uint8_t want=0x5a;if(at>=0x700048 && at<0x700068)want=(at&1)?0xfc:0xff;require(m.ram[at]==want,"depth clear pixels and untouched color/guards");}
 for(unsigned bit=26;bit<64;++bit){auto bad=saved;bad[bad.size()-1053137-bit/8]|=uint8_t(1u<<(bit%8));require(m.restore(saved) && !m.restore(bad) && m.save(replay) && replay==saved,"invalid depth state atomic rejection");}
 auto old=saved;old.resize(old.size()-1053144);old[6]='1';old[7]='8';require(m.restore(old) && !d.depth_image && m.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8),"S018 neutral depth conversion preserves previous state");
 std::cout<<geometry<<" fractional/scissor/field/stride cases; "<<modes<<" mode cases; 196608 raw fills, all 1024 widths, "<<addresses<<" depth addresses; bounds/guards/IRQ/S019 replay/legacy/atomic rejection passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
