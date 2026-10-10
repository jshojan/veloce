// Finite software-reference comparison; no ROM bytes or third-party code.
#include "rdp_tmem.hpp"
#include <cstdio>
#include <algorithm>
extern "C" int veloce_reference_load(const uint8_t*,uint64_t,uint64_t,uint64_t,uint8_t*);
int main(){std::vector<uint8_t> ram(65536);for(unsigned i=0;i<ram.size();++i)ram[i]=uint8_t(i*37+i/256);uint64_t cases=0;unsigned failed=0;
 auto run=[&](unsigned op,unsigned source,unsigned size,unsigned format,unsigned base,unsigned pitch,unsigned align,unsigned sl,unsigned tl,unsigned sh,unsigned th){
  n64::RdpTexture texture;texture.image=(uint64_t(source)<<51)|(31ull<<32)|(256+align);texture.tiles[0].descriptor=(uint64_t(format)<<53)|(uint64_t(size)<<51)|(uint64_t(base)<<32)|(uint64_t(pitch)<<41);
  const uint64_t cmd=(uint64_t(op)<<56)|(uint64_t(sl)<<44)|(uint64_t(tl)<<32)|(uint64_t(sh)<<12)|th;std::array<uint8_t,4096> reference;int crash=veloce_reference_load(ram.data(),texture.image,texture.tiles[0].descriptor,cmd,reference.data());n64::RdpTmem memory;memory.bytes.fill(0xa5);uint32_t groups;unsigned reason=memory.load(op,cmd,texture,ram,groups);
  if((crash && !reason) || (!crash && reason) || (!crash && memory.bytes!=reference)){if(failed<12){unsigned at=0;while(at<4096 && memory.bytes[at]==reference[at])++at;std::printf("FAIL op=%u src=%u size=%u format=%u base=%u pitch=%u align=%u sl=%u tl=%u sh=%u th=%u crash=%d reason=%u byte=%u\n",op,source,size,format,base,pitch,align,sl,tl,sh,th,crash,reason,at);}++failed;}++cases;
 };
 const unsigned bases[]={0,1,255,256,511},counts[]={1,2,3,7,8,9,17,31,128};
 for(unsigned op:{48u,51u,52u})for(unsigned source=1;source<4;++source)for(unsigned size=0;size<4;++size)for(unsigned format=0;format<8;++format)for(unsigned base:bases)for(unsigned align=0;align<8;++align)for(unsigned count:counts)run(op,source,size,format,base,3,align,0,0,op==51?count-1:4*(count-1),op==51?872:0);
 for(unsigned coord=0;coord<4096;++coord)for(unsigned op:{48u,51u,52u})for(unsigned source=1;source<4;++source)run(op,source,source,0,coord%512,coord%512,coord%8,coord,(coord*37)%4096,(coord+16)%4096,op==51?coord:(coord*37)%4096);
 for(unsigned source=1;source<4;++source)for(unsigned format=0;format<8;++format)for(unsigned size=0;size<4;++size)for(unsigned fraction=0;fraction<4;++fraction)for(unsigned rows=1;rows<=8;++rows)for(unsigned base:{0u,255u,511u})for(unsigned pitch:{0u,1u,511u})run(52,source,size,format,base,pitch,fraction,fraction,4+fraction,28+fraction,4*rows+fraction);
 std::printf("%llu reference comparisons; %u failures\n",(unsigned long long)cases,failed);return failed?1:0;}
