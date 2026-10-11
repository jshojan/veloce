#include "rdp_triangle_shade.hpp"
#include <cstdlib>
#include <iostream>
#include <array>
static void check(bool ok){if(!ok){std::cerr<<"rectangle observation mismatch\n";std::exit(1);}}
static int sign16(unsigned n){n&=65535;return n<32768?int(n):int(n)-65536;}
static int floor32(int n){return n>=0?n/32:-((-n+31)/32);}
int main(){uint64_t count=0;
 n64::RdpTexture texture;texture.tiles[0].descriptor=(2ull<<51)|(1ull<<41)|(1ull<<14)|(2ull<<4);texture.tiles[0].bounds=(12ull<<12)|4;
 n64::RdpCombiner combiner;combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(1ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|1;
 for(unsigned raw=0;raw<65536;++raw){const unsigned words[2][4]={{raw,raw^65535,raw^43690,raw^21845},{raw^4951,raw^9320,raw^3855,raw^61680}};
  n64::RdpTmem tmem;for(unsigned y=0;y<2;++y)for(unsigned x=0;x<4;++x){unsigned at=y*8+((x*2)^(y?4:0));tmem.bytes[at]=uint8_t(words[y][x]>>8);tmem.bytes[at+1]=uint8_t(words[y][x]);}
  auto color=[&](int x,int y){unsigned word=words[unsigned(y)&1][unsigned(x)&3];std::array<int,4> c={int(word>>11),int((word>>6)&31),int((word>>1)&31),int(word&1)*255};for(unsigned n=0;n<3;++n)c[n]=c[n]*8+c[n]/4;return c;};
  const unsigned ds=(raw*73+4096)&65535,dt=(((raw*128)&65535)+(raw>>9))^1024,t=(raw*257+128)&65535;
  for(unsigned size:{2u,3u})for(bool flip:{false,true})for(bool filter:{false,true}){
   const bool alpha=raw&1;
   n64::RdpPacket packet;packet.count=packet.needed=2;packet.words[0]=(uint64_t(flip?0xe5:0xe4)<<56)|(16ull<<44)|(8ull<<32);packet.words[1]=(uint64_t(raw)<<48)|(uint64_t(t)<<32)|(uint64_t(ds)<<16)|dt;
   std::vector<uint8_t> ram(128,0x5a),expected=ram;std::array<unsigned,64> bits{};n64::HiddenRdram hidden(128);n64::RdpBlender blender;blender.blend=127;
   const uint64_t mode=(3ull<<38)|(3ull<<36)|(1ull<<43)|(filter?(1ull<<45):0)|8|512|(alpha?1:0);
   check(n64::RdpTriangleShade::rectangle(packet,(uint64_t(size)<<51)|(3ull<<32)|32,mode,(16ull<<12)|12,combiner,ram,&texture,&tmem,nullptr,&blender,&hidden)==0);
   for(int y=0;y<2;++y)for(int x=0;x<4;++x){int s=sign16(raw+floor32(sign16(ds)*(flip?y:x))),v=sign16(t+floor32(sign16(dt)*(flip?x:y))),sx=floor32(s),ty=floor32(v),sf=s-sx*32,tf=v-ty*32;auto c=color(sx,ty);
    if(filter){auto b=color(sx+1,ty),d=color(sx,ty+1),e=color(sx+1,ty+1);for(unsigned n=0;n<4;++n)c[n]=sf+tf<32?((32-sf-tf)*c[n]+sf*b[n]+tf*d[n]+16)/32:((32-tf)*b[n]+(32-sf)*d[n]+(sf+tf-32)*e[n]+16)/32;}
    if(alpha && c[3]<127)continue;
    unsigned at=32+(y*4+x)*(size==2?2:4);
    if(size==2){unsigned word=(c[0]/8)*2048+(c[1]/8)*64+(c[2]/8)*2+1;expected[at]=uint8_t(word>>8);expected[at+1]=uint8_t(word);bits[at/2]=3;}
    else{for(unsigned n=0;n<3;++n)expected[at+n]=uint8_t(c[n]);expected[at+3]=224;bits[at/2]=(c[1]&1)*3;bits[at/2+1]=0;}
   }
   check(ram==expected);for(unsigned n=0;n<bits.size();++n)check(hidden.halfword(n*2)==bits[n]);++count;
  }
 }
 std::cout<<count<<" rectangle RAM/hidden comparisons PASS\n";
}
