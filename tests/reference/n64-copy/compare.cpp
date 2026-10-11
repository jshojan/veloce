#include "rdp_texture_copy.hpp"
#include <iostream>
extern "C" {void primary_copy_initialize(const uint8_t*);uint64_t primary_copy_fetch(uint64_t,uint64_t,unsigned,uint16_t,uint16_t);}
int main(){n64::RdpTmem memory;for(unsigned i=0;i<4096;++i)memory.bytes[i]=uint8_t((i*37+(i>>4)*19+3)^((i>>7)*11));primary_copy_initialize(memory.bytes.data());uint64_t count=0;
for(unsigned seed=0;seed<65536;++seed)for(unsigned size=0;size<4;++size)for(unsigned format=0;format<8;++format)for(bool palette:{false,true}){
 n64::RdpTexture::Tile tile;tile.descriptor=(uint64_t(format)<<53)|(uint64_t(size)<<51)|(uint64_t(seed&511)<<41)|(uint64_t((seed*13)&511)<<32)|(uint64_t(seed&15)<<20)|(uint64_t((seed>>4)&3)<<18)|(uint64_t((seed>>6)&15)<<14)|(uint64_t((seed>>10)&15)<<10)|(uint64_t((seed>>2)&3)<<8)|(uint64_t((seed>>8)&15)<<4)|(seed&15);tile.bounds=(uint64_t((seed*3)&4095)<<44)|(uint64_t((seed*7)&4095)<<32)|(uint64_t((seed*11)&4095)<<12)|((seed*17)&4095);
 const uint16_t s=seed,t=seed*257+0x4f39;auto got=n64::RdpTextureCopy::fetch(memory,tile,s,t,palette),want=primary_copy_fetch(tile.descriptor,tile.bounds,palette,s,t);
 if(got!=want){std::cerr<<"mismatch seed="<<seed<<" format="<<format<<" size="<<size<<" palette="<<palette<<" descriptor="<<std::hex<<tile.descriptor<<" got="<<got<<" expected="<<want<<"\n";return 1;}++count;
}std::cout<<count<<" coupled copy fetches PASS\n";}
