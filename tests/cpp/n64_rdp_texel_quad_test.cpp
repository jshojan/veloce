#include "rdp_texel_quad.hpp"
#include <cstdlib>
#include <iostream>
using Color=n64::RdpTexelFetch::Color;
static void check(bool v){if(!v){std::cerr<<"texel quad mismatch\n";std::exit(1);}}
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
static Color oracle(const n64::RdpTmem& m,unsigned f,unsigned size,int64_t origin,unsigned pal,int32_t s,int32_t t){
    f=f<5?f:4;
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
static unsigned index_oracle(const n64::RdpTmem& m,unsigned f,unsigned size,int64_t origin,unsigned pal,int32_t s,int32_t t){
    const bool byte_kind=f==1;
    if(size==0){unsigned v;if(byte_kind)v=byte(m,origin+s,t,2048)/16;else{const int64_t n=origin*2+s;const int64_t at=n>=0?n/2:-((-n+1)/2);const unsigned packed=byte(m,at,t,2048);v=mod(s,2)?packed%16:packed/16;}return pal*16+v;}
    if(size==1 || byte_kind)return byte(m,origin+s,t,2048);
    return word(m,origin+int64_t(s)*2,t,2048)/256;
}
static n64::RdpTexture::Tile tile(unsigned f,unsigned size,unsigned base,unsigned line,unsigned palette){n64::RdpTexture::Tile x;x.descriptor=(uint64_t(f)<<53)|(uint64_t(size)<<51)|(uint64_t(line)<<41)|(uint64_t(base)<<32)|(uint64_t(palette)<<20);return x;}
using Fetch=n64::RdpTexelQuad;
using Sample=n64::RdpTextureCoordinates::Sample;
static Fetch::Quad expected(const n64::RdpTmem& m,unsigned f,unsigned size,unsigned base,unsigned line,unsigned pal,const Sample& p,Fetch::Palette mode,bool nearest){
    const unsigned uv=f==1?p.s.fraction/2+mod(p.s.texel,2)*16:p.s.fraction;
    const bool high_y=p.s.fraction+p.t.fraction>=32,high_uv=uv+p.t.fraction>=32;
    const int32_t delta_s=p.s.neighbor-p.s.texel,delta_t=p.t.neighbor-p.t.texel;
    Fetch::Quad colors{};
    for(unsigned point=0;point<4;++point){
        const int32_t row=int32_t(mod(p.t.texel,256))+((point/2)?delta_t:0);
        const int32_t x=(point%2)?p.s.neighbor:p.s.texel;
        const int64_t origin=(int64_t(base)+int64_t(line)*row)*8;
        if(mode!=Fetch::Palette::none){
            const int64_t start=nearest?(int64_t(base)+int64_t(line)*p.t.texel)*8:origin;
            const auto index=index_oracle(m,f,size,start,pal,nearest?p.s.texel:x+((f==1 && point%2)?delta_s:0),nearest?p.t.texel:row);
            const unsigned bank=high_uv?3-point:point;
            const unsigned at=2048+index*8+bank*2,v=unsigned(m.bytes[at])*256+m.bytes[at+1];
            colors[point]=mode==Fetch::Palette::ia?ia(v):rgba_bits(v);
        }else{
            colors[point]=oracle(m,f,size,origin,pal,x,row);
            if(f==1 && point%2){
                const auto c=oracle(m,f,size,origin,pal,x+delta_s,row);
                colors[point][0]=c[0];colors[point][1]=c[1];
                if(size<2){colors[point][2]=c[2];colors[point][3]=c[3];}
            }
        }
    }
    if(high_y!=high_uv && (mode!=Fetch::Palette::none || (f==1 && size<2))){
        const auto original=colors;
        for(unsigned point=0;point<4;++point){colors[point][2]=original[3-point][2];colors[point][3]=original[3-point][3];}
    }
    return colors;
}
int main(){
    n64::RdpTmem m;uint32_t random=0x392642b1;
    for(auto& b:m.bytes){random^=random<<13;random^=random>>17;random^=random<<5;b=uint8_t(random);}
    uint64_t addresses=0,palettes=0;
    constexpr int32_t ss[]={-1,0,1,-1023},tt[]={-1,0,1,-255};
    for(unsigned seed=0;seed<65536;++seed){
        const unsigned base=seed%512,line=(seed*13)%512,pal=seed%16;
        Sample p;p.s={int32_t(seed%2048)-1024,0,(seed/16)%32};p.s.neighbor=p.s.texel+ss[(seed/1024)%4];
        p.t={int32_t((seed*7)%2048)-1024,0,seed%32};p.t.neighbor=p.t.texel+tt[(seed/4096)%4];
        for(unsigned f=0;f<8;++f)for(unsigned size=0;size<4;++size){
            const auto x=tile(f,size,base,line,pal);
            check(Fetch::read(m,x,p)==expected(m,f,size,base,line,pal,p,Fetch::Palette::none,false));++addresses;
            for(auto mode:{Fetch::Palette::rgba,Fetch::Palette::ia})for(bool nearest:{false,true}){
                check(Fetch::read(m,x,p,mode,nearest)==expected(m,f,size,base,line,pal,p,mode,nearest));++palettes;
            }
        }
    }
    uint64_t boundaries=0;
    for(unsigned base=0;base<512;++base)for(unsigned line=0;line<512;++line){
        const Sample p{{-1,0,25},{255,256,17}};
        for(unsigned f:{0u,1u,4u})for(unsigned size=0;size<4;++size){
            const auto x=tile(f,size,base,line,0);
            check(Fetch::read(m,x,p)==expected(m,f,size,base,line,0,p,Fetch::Palette::none,false));++boundaries;
        }
    }
    // Real loader -> coordinate processing -> quad fetch -> filtering. A
    // fixed independently averaged 2x2 RGBA tile anchors the composed path.
    uint64_t transfers=0;
    for(unsigned seed=0;seed<256;++seed)for(unsigned size:{2u,3u})for(unsigned base:{0u,255u,511u}){
        n64::RdpTexture texture;n64::RdpTmem memory;std::vector<uint8_t> ram(32);
        for(unsigned i=0;i<ram.size();++i)ram[i]=uint8_t(seed+i*37);
        texture.image=(uint64_t(size)<<51)|(3ull<<32);
        texture.tiles[0]=tile(0,size,base,size==2?1:2,0);texture.tiles[0].bounds=(12ull<<12)|4;
        uint32_t groups=0;check(memory.load(52,(12ull<<12)|4,texture,ram,groups)==0);
        const auto p=n64::RdpTextureCoordinates::sample(texture.tiles[0],16,16);
        const auto q=Fetch::read(memory,texture.tiles[0],p);
        Fetch::Quad golden{};
        for(unsigned point=0;point<4;++point){const unsigned at=(point/2*4+point%2)*(size==2?2:4);
            if(size==2)golden[point]=rgba_bits(unsigned(ram[at])*256+ram[at+1]);else for(unsigned lane=0;lane<4;++lane)golden[point][lane]=ram[at+lane];
        }
        check(q==golden);n64::RdpTextureFilter::Options options;options.s=p.s.fraction;options.t=p.t.fraction;options.mid=true;
        const auto filtered=n64::RdpTextureFilter::filter(q,options);
        for(unsigned lane=0;lane<4;++lane){unsigned sum=2;for(const auto& color:golden)sum+=unsigned(color[lane]);check(filtered[lane]==sum/4);}
        ++transfers;
    }
    uint64_t tlut_loads=0;
    for(unsigned seed=0;seed<256;++seed){
        n64::RdpTexture texture;n64::RdpTmem memory;std::vector<uint8_t> ram(512);
        for(unsigned i=0;i<ram.size();++i)ram[i]=uint8_t(seed+i*37);
        texture.image=(2ull<<51)|(255ull<<32);texture.tiles[0]=tile(0,0,256,0,0);
        uint32_t groups=0;check(memory.load(48,1020ull<<12,texture,ram,groups)==0 && groups==256);
        texture.tiles[1]=tile(2,1,0,1,0);
        memory.bytes[0]=uint8_t(seed);memory.bytes[1]=uint8_t(seed+71);
        memory.bytes[12]=uint8_t(seed+139);memory.bytes[13]=uint8_t(seed+223);
        const Sample p{{0,1,16},{0,1,16}};
        for(auto mode:{Fetch::Palette::rgba,Fetch::Palette::ia})for(bool nearest:{false,true}){
            const auto q=Fetch::read(memory,texture.tiles[1],p,mode,nearest);
            for(unsigned point=0;point<4;++point){const unsigned increments[]={0,71,139,223},index=(seed+(nearest?0:increments[point]))%256;
                const unsigned value=unsigned(ram[index*2])*256+ram[index*2+1];
                check(q[point]==(mode==Fetch::Palette::ia?ia(value):rgba_bits(value)));
            }
            ++tlut_loads;
        }
    }
    std::cout<<addresses<<" raw quad captures; "<<palettes<<" palette quad captures; "<<boundaries<<" row-carry captures; "<<transfers<<" composed load/filter captures; "<<tlut_loads<<" actual TLUT load/fetch captures; coupled banks and lanes PASS\n";
}
