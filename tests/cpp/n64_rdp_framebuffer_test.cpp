#include "rdp_framebuffer.hpp"
#include "rdp_triangle_fill.hpp"
#include "rdp_blender.hpp"
#include <iostream>
#include <cstdlib>
static void verify(bool ok,unsigned line){if(!ok){std::cerr<<"framebuffer mismatch at line "<<line<<'\n';std::exit(1);}}
#define check(...) verify((__VA_ARGS__),__LINE__)
int main(){uint64_t reads=0,writes=0,fills=0;
 std::vector<uint8_t> ram(128);n64::HiddenRdram hidden(128);
 for(unsigned word=0;word<65536;++word)for(unsigned bits=0;bits<4;++bits){
  ram[0]=uint8_t(word/256);ram[1]=uint8_t(word%256);
  const unsigned expected=(word/2048)*8*16777216+(word/64%32)*8*65536+(word/2%32)*8*256+(word%2*4+bits)*32;
  check(n64::RdpFramebuffer::read16(ram.data(),bits)==expected);++reads;
 }
 for(unsigned raw=0;raw<65536;++raw)for(unsigned cvg=0;cvg<8;++cvg)for(unsigned bytes:{2u,4u}){
  const std::array<unsigned,3> rgb={raw%256,raw/256,(raw%256)^(raw/256)};
  const unsigned at=(raw/16%16)*bytes;
  n64::RdpFramebuffer::write(at,bytes,rgb,cvg,ram,&hidden);
  if(bytes==2){const unsigned packed=rgb[0]/8*2048+rgb[1]/8*64+rgb[2]/8*2+cvg/4;
   check(unsigned(ram[at])*256+ram[at+1]==packed && hidden.halfword(at)==cvg%4);
  }else check(ram[at]==rgb[0] && ram[at+1]==rgb[1] && ram[at+2]==rgb[2] && ram[at+3]==cvg*32 && hidden.halfword(at)==rgb[1]%2*3 && hidden.halfword(at+2)==0);
  ++writes;
 }
 const uint64_t mode=3ull<<52,scissor=(32ull<<12)|4,rectangle=28ull<<44;
 for(unsigned size:{1u,2u,3u})for(unsigned raw=0;raw<65536;++raw){
  const unsigned bytes=1u<<(size-1),color=(raw<<16)|(raw^65535);
  hidden.reset();std::fill(ram.begin(),ram.end(),0);
  check(n64::RdpFill::draw(rectangle,(uint64_t(size)<<51)|64,mode,scissor,color,ram,&hidden)==0);
  for(unsigned n=0;n<8*bytes;++n)check(ram[64+n]==((color>>(8*(3-n%4)))&255));
  for(unsigned n=0;n<8*bytes;n+=2)check(hidden.halfword(64+n)==(ram[65+n]&1)*3);
  check(hidden.snapshot().delayed==-1);++fills;
 }
 // A one-byte fill must flush the delayed upper bit from the retained lane.
 hidden.reset();check(hidden.rdp_halfword(64,2,true));ram[65]=1;
 check(n64::RdpFill::draw(0,(1ull<<51)|64,mode,scissor,0,ram,&hidden)==0 && hidden.halfword(64)==2 && ram[65]==1 && hidden.snapshot().delayed==-1);
 const auto before=ram;const auto metadata=hidden.snapshot();
 check(n64::RdpFill::draw(rectangle,(3ull<<51)|120,mode,scissor,0,ram,&hidden)==5 && ram==before && hidden.snapshot()==metadata);
 uint64_t rejected=0;
 for(unsigned seed=0;seed<256;++seed)for(unsigned cvg=0;cvg<8;++cvg)for(unsigned bytes:{2u,4u}){
  hidden.reset();n64::RdpBlender blender;blender.blend=128;
  const uint32_t memory=0x12003500u|(seed<<16);
  const auto result=blender.pixel((1ull<<22)|16384|512|64|1,{seed,seed^255,seed,0},0,memory,cvg,0,1);
  check(!result.write && result.coverage==7);
  const auto original=ram;const auto bits=hidden.snapshot().bits;
  n64::RdpFramebuffer::rejected(64,bytes,result.rgb,result.coverage,&hidden);
  check(ram==original && hidden.snapshot().bits==bits);
  const unsigned expected=bytes==2?3:((memory>>16)&1)*3;
  check(n64::RdpFill::draw(0,(1ull<<51)|64,mode,scissor,0,ram,&hidden)==0 && hidden.halfword(64)==(expected&2));++rejected;
 }
 uint64_t triangles=0;
 for(unsigned raw=0;raw<65536;++raw)for(bool flip:{false,true})for(unsigned size:{1u,2u,3u}){
  std::vector<uint8_t> pixels(512),expected=pixels;n64::HiddenRdram h(512);
  std::array<unsigned,512> bits{};std::array<unsigned,8> history{};int pending=-1;
  for(unsigned n=0;n<8;++n){history[n]=(raw+n)%4;check(h.rdp_halfword(n*2,history[n],true));}
  check(h.ordinary_burst(0,pixels.data(),pixels.size()));
  n64::RdpPacket p;p.count=p.needed=4;p.words[0]=0xc800002000100000ull|(flip?1ull<<55:0);
  if(flip){p.words[1]=(4ull<<48)|0xffff0000u;p.words[2]=0;p.words[3]=65536;}
  else{p.words[1]=65536;p.words[2]=4ull<<48;p.words[3]=(4ull<<48)|0xffff0000u;}
  const unsigned lengths[]={1,2,3,4,5,4,3,2},lefts[]={3,2,1,0,0,1,2,3};
  const unsigned bytes=1u<<(size-1),color=(raw<<16)|(raw^65535);
  auto flush=[&](){if(pending>=0)bits[unsigned(pending)/2*2]=history[unsigned(pending)/2%8]/2;pending=-1;};
  for(unsigned y=0;y<7;++y){const unsigned count=flip?lengths[y]:5-lefts[y];
   for(unsigned step=0;step<count;++step){const unsigned x=flip?step:4-step,a=64+(y*8+x)*bytes;
    for(unsigned n=0;n<bytes;++n)expected[a+n]=uint8_t(color>>(8*(3-(a+n)%4)));
    if(bytes==1){const unsigned low=expected[a]%2;if(flip && pending>=0 && unsigned(pending)<a)flush();
     if(a%2){bits[a]=low;if(flip && pending>=0)bits[a-1]=low;history[a/2%8]=low*3;if(flip)pending=-1;}
     else if(flip)pending=int(a+1);else bits[a]=history[a/2%8]/2;
    }else for(unsigned n=0;n<bytes;n+=2){const unsigned low=expected[a+n+1]%2;bits[a+n]=bits[a+n+1]=low;history[(a+n)/2%8]=low*3;}
   }
  }
  flush();check(n64::RdpTriangleFill::draw(p,(uint64_t(size)<<51)|(7ull<<32)|64,mode,(32ull<<12)|28,color,pixels,&h)==0 && pixels==expected);
  for(unsigned a=0;a<512;a+=2)check(h.halfword(a)==bits[a]*2+bits[a+1]);
  const auto state=h.snapshot();for(unsigned n=0;n<8;++n)check(((state.history>>(2*n))&3)==history[n]);check(state.delayed==-1);++triangles;
 }
 std::cout<<reads<<" complete RGBA16 read encodings; "<<writes<<" paired framebuffer writes; "<<fills<<" fill metadata patterns; "<<triangles<<" oriented triangle metadata captures; "<<rejected<<" rejected pixel/CI8 history captures; delayed history and atomic bounds PASS\n";
}
