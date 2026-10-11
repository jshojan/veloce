#include "rdp_texel_fetch.hpp"
#include <cstdlib>
#include <iostream>
using Color=n64::RdpTexelFetch::Color;
static void check(bool v){if(!v){std::cerr<<"texel fetch mismatch\n";std::exit(1);}}
static unsigned mod(int64_t value,unsigned n){return unsigned((value%n+n)%n);}
static unsigned physical(int64_t address,int32_t row,unsigned size){unsigned a=mod(address,size);if(mod(row,2))a=(a/8)*8+(a%8+4)%8;return a;}
static unsigned byte(const n64::RdpTmem& m,int64_t address,int32_t row,unsigned size){return m.bytes[physical(address,row,size)];}
static unsigned word(const n64::RdpTmem& m,int64_t address,int32_t row,unsigned size){const unsigned a=physical(address,row,size);return unsigned(m.bytes[a])*256+m.bytes[a+1];}
static Color gray(unsigned x){return {int32_t(x),int32_t(x),int32_t(x),int32_t(x)};}
static Color rgba_bits(unsigned v){
    constexpr int32_t expansion[]={0,8,16,24,33,41,49,57,66,74,82,90,99,107,115,123,132,140,148,156,165,173,181,189,198,206,214,222,231,239,247,255};
    return {expansion[(v/2048)%32],expansion[(v/64)%32],expansion[(v/2)%32],v%2?255:0};
}
static Color ia(unsigned v){return {int32_t(v/256),int32_t(v/256),int32_t(v/256),int32_t(v%256)};}
static Color oracle(const n64::RdpTmem& m,unsigned f,unsigned size,unsigned base,unsigned line,unsigned pal,int32_t s,int32_t t){
    f=f<5?f:4;const int64_t origin=(base+int64_t(line)*mod(t,256))*8;
    if(f==1){
        if(size<2){unsigned v=byte(m,origin+s,t,2048);if(size==0)v=(v/16)*17;return {int32_t(v)-128,int32_t(v)-128,int32_t(v),int32_t(v)};}
        const int64_t pair=origin+(int64_t(s)>=0?s/2:-((-int64_t(s)+1)/2))*2;
        const unsigned uv=word(m,pair,t,2048);
        unsigned y=m.bytes[2048+physical(origin+s,t,2048)],a=y;
        if(size==3 && mod(s,2)==0){const unsigned pos=2048+physical(pair,t,2048);y=m.bytes[pos];a=(y%16)+(m.bytes[pos+1]/16)*16;}
        return {int32_t(uv/256)-128,int32_t(uv%256)-128,int32_t(y),int32_t(a)};
    }
    if(size==0){const int64_t nibble=origin*2+s;const int64_t pos=nibble>=0?nibble/2:-((-nibble+1)/2);const unsigned packed=byte(m,pos,t,4096),v=mod(s,2)?packed%16:packed/16;if(f==2)return gray(pal*16+v);if(f==3){const unsigned x=v/2;const unsigned i=(x*32)+(x*4)+(x/2);return {int32_t(i),int32_t(i),int32_t(i),mod(v,2)?255:0};}return gray(v*17);}
    if(size==1){const unsigned v=byte(m,origin+s,t,4096);if(f==3)return {int32_t(v/16*17),int32_t(v/16*17),int32_t(v/16*17),int32_t(v%16*17)};return gray(v);}
    const unsigned v=word(m,origin+int64_t(s)*2,t,4096);
    if(f==0){if(size==2)return rgba_bits(v);const unsigned rg=word(m,origin+int64_t(s)*2,t,2048),at=2048+physical(origin+int64_t(s)*2,t,2048);return {int32_t(rg/256),int32_t(rg%256),int32_t(m.bytes[at]),int32_t(m.bytes[at+1])};}
    if(f==3 && size==2)return ia(v);
    return {int32_t(v/256),int32_t(v%256),int32_t(v/256),int32_t(v%256)};
}
static unsigned index_oracle(const n64::RdpTmem& m,unsigned f,unsigned size,unsigned base,unsigned line,unsigned pal,int32_t s,int32_t t){
    const bool byte_kind=f==1;const int64_t origin=(base+int64_t(line)*mod(t,256))*8;
    if(size==0){unsigned v;if(byte_kind)v=byte(m,origin+s,t,2048)/16;else{const int64_t n=origin*2+s;const int64_t at=n>=0?n/2:-((-n+1)/2);const unsigned packed=byte(m,at,t,2048);v=mod(s,2)?packed%16:packed/16;}return pal*16+v;}
    if(size==1 || byte_kind)return byte(m,origin+s,t,2048);
    return word(m,origin+int64_t(s)*2,t,2048)/256;
}
static n64::RdpTexture::Tile tile(unsigned f,unsigned size,unsigned base,unsigned line,unsigned palette){n64::RdpTexture::Tile x;x.descriptor=(uint64_t(f)<<53)|(uint64_t(size)<<51)|(uint64_t(line)<<41)|(uint64_t(base)<<32)|(uint64_t(palette)<<20);return x;}
int main(){
    n64::RdpTmem m;uint32_t random=0x9164230b;
    for(auto& b:m.bytes){random^=random<<13;random^=random>>17;random^=random<<5;b=uint8_t(random);}
    uint64_t addresses=0,packed=0,palettes=0;
    for(unsigned sample=0;sample<65536;++sample){const unsigned base=sample%512,line=(sample*13)%512,pal=sample%16;const int32_t s=int32_t(sample)-32768,t=int32_t((sample*7)%65536)-32768;
        for(unsigned f=0;f<8;++f)for(unsigned size=0;size<4;++size){const auto x=tile(f,size,base,line,pal);check(n64::RdpTexelFetch::read(m,x,s,t)==oracle(m,f,size,base,line,pal,s,t));check(n64::RdpTexelFetch::palette_index(m,x,s,t)==index_oracle(m,f,size,base,line,pal,s,t));++addresses;}
    }
    for(unsigned v=0;v<65536;++v){
        for(unsigned offset:{0u,2u,4u,6u,2048u,2050u,2052u,2054u}){const unsigned value=offset<2048?v:v^65535;m.bytes[offset]=uint8_t(value>>8);m.bytes[offset+1]=uint8_t(value);}
        for(unsigned f=0;f<8;++f)for(unsigned size=0;size<4;++size)for(unsigned s=0;s<2;++s)for(unsigned t=0;t<2;++t){auto x=tile(f,size,0,0,v%16);check(n64::RdpTexelFetch::read(m,x,s,t)==oracle(m,f,size,0,0,v%16,s,t));check(n64::RdpTexelFetch::palette_index(m,x,s,t)==index_oracle(m,f,size,0,0,v%16,s,t));++packed;}
        for(unsigned index:{0u,255u})for(unsigned lane=0;lane<4;++lane){const unsigned at=2048+index*8+lane*2,value=v^(lane*0x1357);m.bytes[at]=uint8_t(value>>8);m.bytes[at+1]=uint8_t(value);for(unsigned mode=0;mode<2;++mode){check(n64::RdpTexelFetch::palette(m,index,lane,mode)==(mode?ia(value):rgba_bits(value)));++palettes;}}
    }
    for(unsigned base=0;base<512;++base)for(unsigned line=0;line<512;++line)for(int32_t s:{-1,0,1023,1024}){const int32_t t=int32_t((base+line)%512)-256;auto x=tile(0,3,base,line,0);check(n64::RdpTexelFetch::read(m,x,s,t)==oracle(m,0,3,base,line,0,s,t));++addresses;}
    for(unsigned index=0;index<256;++index)for(unsigned lane=0;lane<4;++lane)for(unsigned mode=0;mode<2;++mode){const unsigned at=2048+index*8+lane*2,v=unsigned(m.bytes[at])*256+m.bytes[at+1];check(n64::RdpTexelFetch::palette(m,index,lane,mode)==(mode?ia(v):rgba_bits(v)));++palettes;}
    uint64_t transfers=0;
    for(unsigned seed=0;seed<256;++seed)for(unsigned size:{2u,3u})for(unsigned base:{0u,255u,511u}){
        n64::RdpTexture texture;n64::RdpTmem memory;std::vector<uint8_t> ram(32);
        for(unsigned i=0;i<ram.size();++i)ram[i]=uint8_t(seed+i*37);
        texture.image=(uint64_t(size)<<51)|(3ull<<32);
        texture.tiles[0]=tile(0,size,base,size==2?1:2,0);
        uint32_t groups=0;check(memory.load(52,(12ull<<12)|4,texture,ram,groups)==0);
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<4;++x){const unsigned at=(y*4+x)*(size==2?2:4);Color expected;
            if(size==2)expected=rgba_bits(unsigned(ram[at])*256+ram[at+1]);else for(unsigned lane=0;lane<4;++lane)expected[lane]=ram[at+lane];
            check(n64::RdpTexelFetch::read(memory,texture.tiles[0],x,y)==expected);++transfers;
        }
    }
    std::cout<<addresses<<" address captures; "<<packed<<" packed-format captures; "<<palettes<<" palette decodes; "<<transfers<<" actual load/fetch captures; canonical banks and aliases PASS\n";
}
