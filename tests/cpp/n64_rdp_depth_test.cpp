#include "rdp_depth.hpp"
#include <iostream>
#include <cstdlib>
static void check(bool ok){if(!ok){std::cerr<<"depth mismatch\n";std::exit(1);}}
using Depth=n64::RdpDepth;
static constexpr unsigned edges[]={0,131072,196608,229376,245760,253952,258048,260096,262144};
static constexpr unsigned steps[]={64,32,16,8,4,2,1,1};
static unsigned compress(unsigned z){z%=262144;unsigned band=0;while(z>=edges[band+1])++band;return (band*2048+(z-edges[band])/steps[band])*4;}
static unsigned decompress(unsigned word){const unsigned n=word/4%16384;return (edges[n/2048]+n%2048*steps[n/2048])%262144;}
static unsigned encoding(unsigned value){unsigned out=0;for(unsigned n=0;n<16;++n)if(value/(1u<<n)%2)out|=n;return out;}
static unsigned highest(unsigned value){unsigned out=0;while(value){out=out?out*2:1;value/=2;}return out;}
static Depth::Result oracle(bool enabled,unsigned kind,bool aa,bool force,unsigned z,unsigned delta,unsigned word,unsigned hidden,unsigned count,unsigned memory){
 Depth::Result out;out.coverage=count;out.overflow=(count+memory)/8%2;
 const unsigned dzcode=encoding(delta);z%=262144;
 if(!enabled){out.pass=true;out.blend=force || (aa && !out.overflow);out.shift_b=dzcode<11?4:15-dzcode;return out;}
 out.stored_delta=(word%4)*4+hidden;
 out.shift_a=dzcode>out.stored_delta?std::min(4u,dzcode-out.stored_delta):0;
 out.shift_b=out.stored_delta>dzcode?std::min(4u,out.stored_delta-dzcode):0;
 unsigned stored=1u<<out.stored_delta;const unsigned precision=word/8192;bool coplanar=false;
 if(precision<3){if(stored==32768){stored=65535;coplanar=true;}else stored=std::max(stored*2,16u/(1u<<precision));}
 const unsigned tolerance=highest(delta|stored)*8,old=decompress(word);
 const bool front=z<old,far=coplanar || z+tolerance>=old,near=coplanar || int64_t(z)-tolerance<=old,max=old==262143;
 out.blend=force || (aa && !out.overflow && far);
 if(kind==2)out.pass=front || max;
 else if(kind==3)out.pass=far && near && !max;
 else if(kind==1 && front && far && out.overflow){const unsigned divisor=1u<<encoding(tolerance/8);out.coverage=((old/divisor-z/divisor)%16*count/8)%16;out.pass=true;}
 else out.pass=max || (out.overflow?front:near);
 return out;
}
#ifdef PRIMARY_DEPTH
extern "C" {void reference_init();unsigned reference_compress(unsigned);unsigned reference_decompress(unsigned);unsigned reference_delta(unsigned);unsigned reference_highest(unsigned);void reference_compare(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned*);}
#endif
int main(){uint64_t codecs=0,decisions=0,stores=0;
#ifdef PRIMARY_DEPTH
 reference_init();
#endif
 for(unsigned z=0;z<262144;++z){check(Depth::compress(z)==compress(z) && Depth::compress(z+262144)==compress(z));
#ifdef PRIMARY_DEPTH
 check(Depth::compress(z)==reference_compress(z));
#endif
 ++codecs;}
 for(unsigned value=0;value<65536;++value){check(Depth::decompress(uint16_t(value))==decompress(value) && Depth::delta_encoding(uint16_t(value))==encoding(value) && Depth::highest(uint16_t(value))==highest(value));
#ifdef PRIMARY_DEPTH
 check(Depth::decompress(uint16_t(value))==reference_decompress(value) && Depth::delta_encoding(uint16_t(value))==reference_delta(value) && Depth::highest(uint16_t(value))==reference_highest(value));
#endif
 ++codecs;}
 for(unsigned word=0;word<65536;++word)for(unsigned h=0;h<4;++h)for(unsigned mode=0;mode<4;++mode)for(unsigned partner=0;partner<16;++partner){
  constexpr unsigned deltas[]={0,1,2,3,4,7,8,15,16,31,127,255,1024,16384,32768,65535};
  const unsigned old=decompress(word),delta=deltas[partner],count=partner%9,memory=(word+h)%8;
  const unsigned distance=1u<<(partner%16);
  const unsigned z=partner%4==0?old:partner%4==1?(old+distance)%262144:partner%4==2?(old>=distance?old-distance:0):partner*17476;
  const bool aa=partner%2,force=partner/2%2;const auto got=Depth::compare(true,mode,aa,force,z,uint16_t(delta),uint16_t(word),h,count,memory);
  check(got==oracle(true,mode,aa,force,z,delta,word,h,count,memory));
#ifdef PRIMARY_DEPTH
  unsigned r[7]{};reference_compare(1,mode,aa,force,z,delta,word,h,count,memory,r);check(got.pass==bool(r[0]) && got.blend==bool(r[1]) && got.overflow==bool(r[2]) && got.coverage==r[3] && got.shift_a==r[4] && got.shift_b==r[5] && got.stored_delta==r[6]);
#endif
  ++decisions;
 }
 for(unsigned delta=0;delta<65536;++delta)for(unsigned flags=0;flags<4;++flags)for(unsigned count=0;count<=8;++count){const auto got=Depth::compare(false,0,flags%2,flags/2,0,uint16_t(delta),0,0,count,delta%8);check(got==oracle(false,0,flags%2,flags/2,0,delta,0,0,count,delta%8));++decisions;}
 std::vector<uint8_t> ram(16);n64::HiddenRdram hidden(16);check(hidden.rdp_halfword(0,2,true));const auto history=hidden.snapshot().history;
 for(unsigned z=0;z<262144;++z)for(unsigned dz=0;dz<16;++dz){check(Depth::store(0,z,dz,ram,hidden) && unsigned(ram[0])*256+ram[1]==compress(z)+dz/4 && hidden.halfword(0)==dz%4 && hidden.snapshot().history==history);++stores;}
 const auto data=ram;const auto state=hidden.snapshot();for(size_t a:{size_t(1),size_t(16),size_t(-1)})check(!Depth::store(a,0,0,ram,hidden) && ram==data && hidden.snapshot()==state);check(!Depth::store(0,0,16,ram,hidden) && ram==data && hidden.snapshot()==state);
 std::cout<<codecs<<" complete encoding domains; "<<decisions<<" independent depth decisions; "<<stores<<" paired stores; atomic bounds/history preservation PASS\n";
}
