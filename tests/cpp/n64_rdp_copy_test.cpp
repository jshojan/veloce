#include "rdp_copy.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool ok){if(!ok){std::cerr<<"copy observation mismatch\n";std::exit(1);}}
int main(){uint64_t draws=0,fetches=0;
 n64::RdpTexture texture;auto& tile=texture.tiles[0];tile.descriptor=(2ull<<51)|(1ull<<41)|(1ull<<14)|(2ull<<4);tile.bounds=(12ull<<12)|4;
 for(unsigned seed=0;seed<65536;++seed){n64::RdpTmem memory;unsigned words[2][4];
  for(unsigned row=0;row<2;++row)for(unsigned col=0;col<4;++col){const unsigned word=seed^(row?0x1357:0)^(col*0x5555);words[row][col]=word;const unsigned at=row*8+((col*2)^(row?4:0));memory.bytes[at]=uint8_t(word>>8);memory.bytes[at+1]=uint8_t(word);}
  for(unsigned row=0;row<2;++row){uint64_t expected=0;for(unsigned col=0;col<4;++col)expected=(expected<<16)|words[row][col];check(n64::RdpTextureCopy::fetch(memory,tile,0,uint16_t(row*32),false)==expected);++fetches;}
  for(unsigned size:{0u,1u,2u})for(bool flip:{false,true})for(bool alpha:{false,true}){
   n64::RdpPacket packet;packet.count=packet.needed=2;packet.words[0]=(uint64_t(flip?0xe5:0xe4)<<56)|(28ull<<44)|(4ull<<32);packet.words[1]=(4096ull<<16)|1024;
   std::vector<uint8_t> ram(128,0x5a),expected=ram;n64::HiddenRdram hidden(128);
   const uint64_t mode=(2ull<<52)|(alpha?1:0),image=(uint64_t(size)<<51)|(7ull<<32)|32;
   check(n64::RdpCopy::rectangle(packet,image,mode,(32ull<<12)|8,texture,memory,127,ram,hidden)==0);
   const unsigned bursts=size==2?2:1;
   for(unsigned row=0;row<2;++row)for(unsigned burst=0;burst<bursts;++burst){const unsigned source=flip?burst%2:row;
    for(unsigned pair=0;pair<4;++pair){const unsigned word=size?words[source][pair]:0;
     const unsigned testbyte=(words[source][2+pair/2]>>(pair%2?0:8))&255;
     const bool accepted=!alpha || (size==2?bool(word&1):size==1?testbyte>=127:false);
     const unsigned at=32+row*(size==2?16:8)+burst*8+pair*2;
     if(accepted){expected[at]=uint8_t(word>>8);expected[at+1]=uint8_t(word);check(hidden.halfword(at)==(word&1)*3);}else check(hidden.halfword(at)==0);
    }
   }
   check(ram==expected && hidden.snapshot().delayed==-1);++draws;
  }
 }
 // Bounds and unsupported profiles reject atomically, preserving hidden history.
 n64::RdpPacket p;p.count=p.needed=2;p.words[0]=(0xe4ull<<56)|(28ull<<44)|(4ull<<32);p.words[1]=(4096ull<<16)|1024;
 n64::RdpTmem memory;std::vector<uint8_t> ram(128,0x5a);n64::HiddenRdram hidden(128);auto state=hidden.snapshot();state.history=65535;check(hidden.restore(state));const auto before=ram;
 for(const auto image:{(3ull<<51)|(7ull<<32)|32,(2ull<<51)|(7ull<<32)|112,(2ull<<51)|(7ull<<32)|33}){check(n64::RdpCopy::rectangle(p,image,2ull<<52,(32ull<<12)|8,texture,memory,0,ram,hidden)!=0);check(ram==before && hidden.snapshot()==state);}
 check(n64::RdpCopy::rectangle(p,(1ull<<51)|(7ull<<32)|32,(2ull<<52)|3,(32ull<<12)|8,texture,memory,0,ram,hidden)==1);check(ram==before && hidden.snapshot()==state);
 // Empty CI8 commands still clear retained lanes zero/four.
 check(n64::RdpCopy::rectangle(p,(1ull<<51)|(7ull<<32)|32,2ull<<52,0,texture,memory,0,ram,hidden)==0);check(hidden.snapshot().history==(65535u&~514u) && ram==before);
 // Copy ignores clamp bounds: masks wrap each neighbor at the seam.
 tile.descriptor|=(2ull<<18)|(2ull<<8);tile.bounds=0;memory.bytes[6]=0x12;memory.bytes[7]=0x35;memory.bytes[0]=0x46;memory.bytes[1]=0x79;memory.bytes[2]=0x8a;memory.bytes[3]=0xbd;memory.bytes[4]=0xce;memory.bytes[5]=0xf1;
 check(n64::RdpTextureCopy::fetch(memory,tile,96,0,false)==0x123546798abdcef1ull);
 // Palette lanes use four physical entries, preserving raw RGBA/IA words.
 tile.descriptor=(1ull<<51)|(1ull<<41);memory.bytes[0]=7;memory.bytes[1]=7;memory.bytes[2]=7;memory.bytes[3]=7;
 for(unsigned lane=0;lane<4;++lane){const unsigned at=2048+7*8+lane*2;memory.bytes[at]=uint8_t(0x10+lane);memory.bytes[at+1]=uint8_t(0x31+lane*2);}
 check(n64::RdpTextureCopy::fetch(memory,tile,0,0,true)==0x1031113312351337ull);
 std::cout<<fetches<<" RGBA16 bank fetches; "<<draws<<" copy rectangle RAM/hidden comparisons; atomic bounds/profile rejection, clamp seam, palette lanes and empty CI8 history PASS\n";
}
