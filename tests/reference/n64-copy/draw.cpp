#include "rdp_copy.hpp"
#include <iostream>
extern "C" int primary_copy_draw(const uint64_t*,unsigned,uint64_t,uint64_t,uint64_t,const uint64_t*,const uint8_t*,unsigned,uint8_t*,uint8_t*,unsigned*);
int main(){n64::RdpTmem tmem;for(unsigned i=0;i<4096;++i)tmem.bytes[i]=uint8_t((i*37+(i>>4)*19+3)^((i>>7)*11));uint64_t count=0;
for(unsigned seed=0;seed<8192;++seed)for(unsigned size:{0u,1u,2u})for(bool flip:{false,true})for(bool triangular:{false,true}){
 n64::RdpPacket packet;packet.count=packet.needed=2;const unsigned left=seed%4+8,right=left+seed%33,top=(seed/4)%4+8,bottom=top+seed%17;
 packet.words[0]=(uint64_t(flip?0xe5:0xe4)<<56)|(uint64_t(right)<<44)|(uint64_t(bottom)<<32)|(uint64_t(left)<<12)|top;packet.words[1]=(uint64_t(uint16_t(seed*19))<<48)|(uint64_t(uint16_t(seed*257+4096))<<32)|(uint64_t(uint16_t(seed*31+1024))<<16)|uint16_t(seed*127+1024);
 if(triangular){
  const unsigned op=8+(seed%8);packet.clear();packet.count=packet.needed=n64::RdpPacket::length(op);
  packet.words[0]=(uint64_t(0xc0+op)<<56)|(uint64_t(flip)<<55)|(uint64_t(seed%8)<<51)|(uint64_t(24+(seed%8))<<32)|(uint64_t(16+(seed%8))<<16)|uint64_t(seed%8);
  packet.words[1]=uint64_t(flip?8:2)<<48;packet.words[2]=(uint64_t(flip?2:8)<<48)|uint32_t(seed&1?65536:-65536);packet.words[3]=uint64_t(flip?8:2)<<48;
  if(op&2){const unsigned offset=4+((op&4)?8:0);std::array<std::array<uint32_t,3>,4> values{};
   for(unsigned group=0;group<4;++group)for(unsigned lane=0;lane<3;++lane)values[group][lane]=uint32_t(seed*(group*31+lane*257+17)+group*0x102040+lane*0x1020304);
   for(unsigned group=0;group<4;++group)for(unsigned lane=0;lane<3;++lane){const unsigned index=offset+(group/2)*4+group%2,shift=48-lane*16;packet.words[index]|=uint64_t(values[group][lane]>>16)<<shift;packet.words[index+2]|=uint64_t(values[group][lane]&65535)<<shift;}
  }
 }
 n64::RdpTexture texture;for(unsigned tile=0;tile<8;++tile){texture.tiles[tile].descriptor=(uint64_t(seed%8)<<53)|(uint64_t(seed%4)<<51)|(uint64_t((seed+tile)&511)<<41)|(uint64_t((seed*13+tile*73)&511)<<32)|(uint64_t(seed%16)<<20)|(uint64_t(seed%4)<<18)|(uint64_t(seed%16)<<14)|(uint64_t((seed/16)%16)<<10)|(uint64_t(seed%4)<<8)|(uint64_t((seed/256)%16)<<4)|((seed/32)%16);texture.tiles[tile].bounds=(uint64_t(seed%16)<<44)|(uint64_t((seed/16)%16)<<32)|(64ull<<12)|64;}
 const uint64_t mode=(2ull<<52)|((seed&1)?1:0)|((seed&2)?(1ull<<47):0)|((seed&4)?(1ull<<51):0)|((seed&8)?(1ull<<48):0)|((seed&16)?(1ull<<50):0),image=(uint64_t(size)<<51)|(15ull<<32)|4096;
 const uint64_t scissor=(uint64_t((seed/32)%8)<<44)|(uint64_t((seed/64)%8)<<32)|(48ull<<12)|32|((seed&32)?(1ull<<25):0)|((seed&64)?(1ull<<24):0);
 std::vector<uint8_t> ram(65536,0x5a),ref=ram;n64::HiddenRdram hidden(65536);std::vector<uint8_t> bits(32768);for(unsigned i=0;i<bits.size();++i){bits[i]=(seed+i)%4;hidden.rdp_halfword(i*2,bits[i],false);}auto state=hidden.snapshot();state.history=seed&65535;hidden.restore(state);unsigned history=state.history;
 const auto result=(triangular?n64::RdpCopy::triangle:n64::RdpCopy::rectangle)(packet,image,mode,scissor,texture,tmem,seed%256,ram,hidden);auto tiles=texture.snapshot();const auto crash=primary_copy_draw(packet.words.data(),packet.count,image,mode,scissor,tiles.data(),tmem.bytes.data(),seed%256,ref.data(),bits.data(),&history);
 bool mismatch=ram!=ref || hidden.snapshot().history!=history || result || crash;for(unsigned i=0;i<bits.size();++i)if(hidden.halfword(i*2)!=bits[i])mismatch=true;
 if(mismatch){std::cerr<<"draw mismatch seed="<<seed<<" size="<<size<<" flip="<<flip<<" triangular="<<triangular<<" status="<<result<<" crash="<<crash<<" history="<<std::hex<<hidden.snapshot().history<<" expected="<<history<<"\n";for(unsigned i=0;i<ram.size();++i)if(ram[i]!=ref[i]){std::cerr<<"RAM "<<std::hex<<i<<" got="<<unsigned(ram[i])<<" expected="<<unsigned(ref[i])<<"\n";break;}for(unsigned i=0;i<bits.size();++i)if(hidden.halfword(i*2)!=bits[i]){std::cerr<<"hidden "<<std::hex<<i*2<<" got="<<hidden.halfword(i*2)<<" expected="<<unsigned(bits[i])<<"\n";break;}return 1;}++count;
}std::cout<<count<<" copy rectangle/triangle framebuffer/hidden/history comparisons PASS\n";}
