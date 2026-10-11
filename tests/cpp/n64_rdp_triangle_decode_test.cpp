#include "rdp_triangle.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool value){if(!value){std::cerr<<"triangle decoder mismatch\n";std::exit(1);}}
static bool same(const n64::RdpTriangle& a,const n64::RdpTriangle& b){return a.shade==b.shade && a.texture==b.texture && a.depth==b.depth && a.flip==b.flip && a.level==b.level && a.tile==b.tile && a.y==b.y && a.edge==b.edge && a.color==b.color && a.tex==b.tex && a.z==b.z;}
int main(){
    constexpr unsigned lengths[]={4,6,12,14,12,14,20,22};
    constexpr unsigned ints[]={0,1,4,5},fracs[]={2,3,6,7};
    uint64_t basis=0,halves=0,ys=0,headers=0,rejections=0;
    for(unsigned op=8;op<16;++op)for(unsigned alias=0;alias<4;++alias){
        n64::RdpPacket p;p.count=p.needed=lengths[op-8];p.words[0]=uint64_t(op+64*alias)<<56;
        n64::RdpTriangle base;check(n64::RdpTriangle::decode(p,base));
        check(base.shade==bool(op&4) && base.texture==bool(op&2) && base.depth==bool(op&1));
        for(unsigned meta=0;meta<128;++meta){auto q=p;q.words[0]|=uint64_t(meta&63)<<48|uint64_t(meta>>6)<<55;n64::RdpTriangle t;check(n64::RdpTriangle::decode(q,t));check(t.tile==(meta&7) && t.level==((meta>>3)&7) && t.flip==bool(meta&64));++headers;}
        for(unsigned field=0;field<3;++field)for(unsigned raw=0;raw<16384;++raw){auto q=p;q.words[0]|=uint64_t(raw)<<(32-16*field);auto expected=base;expected.y[field]=raw<8192?int32_t(raw):int32_t(raw)-16384;n64::RdpTriangle t;check(n64::RdpTriangle::decode(q,t) && same(t,expected));++ys;}
        for(unsigned word=1;word<p.count;++word)for(unsigned bit=0;bit<64;++bit){
            auto q=p;q.words[word]=uint64_t(1)<<bit;auto expected=base;
            if(word<4)expected.edge[word-1][bit>=32?0:1]=uint32_t(1)<<(bit%32);
            unsigned cursor=4;
            if(op&4){for(unsigned g=0;g<4;++g)for(unsigned c=0;c<4;++c){const unsigned lane=48-c*16;if(bit>=lane && bit<lane+16){if(word==cursor+ints[g])expected.color[g][c]=uint32_t(1)<<(bit-lane+16);if(word==cursor+fracs[g])expected.color[g][c]=uint32_t(1)<<(bit-lane);}}cursor+=8;}
            if(op&2){for(unsigned g=0;g<4;++g)for(unsigned c=0;c<3;++c){const unsigned lane=48-c*16;if(bit>=lane && bit<lane+16){if(word==cursor+ints[g])expected.tex[g][c]=uint32_t(1)<<(bit-lane+16);if(word==cursor+fracs[g])expected.tex[g][c]=uint32_t(1)<<(bit-lane);}}cursor+=8;}
            if((op&1) && word>=cursor)expected.z[(word-cursor)*2+(bit>=32?0:1)]=uint32_t(1)<<(bit%32);
            n64::RdpTriangle t;check(n64::RdpTriangle::decode(q,t) && same(t,expected));++basis;
        }
        for(unsigned count=0;count<p.count;++count){auto q=p;q.count=count;auto t=base;check(!n64::RdpTriangle::decode(q,t) && same(t,base));++rejections;}
    }
    // Every halfword value, separately routed integer/fractional shade/STW lanes.
    for(unsigned raw=0;raw<65536;++raw)for(unsigned channel=0;channel<4;++channel)for(unsigned group=0;group<4;++group)for(unsigned fraction=0;fraction<2;++fraction){
        n64::RdpPacket p;p.count=p.needed=22;p.words[0]=uint64_t(15)<<56;
        auto expected=n64::RdpTriangle{};expected.shade=expected.texture=expected.depth=true;
        const unsigned index=(fraction?fracs:ints)[group];
        p.words[4+index]=uint64_t(raw)<<(48-channel*16);expected.color[group][channel]=uint32_t(raw)<<(fraction?0:16);
        p.words[12+index]=uint64_t(raw)<<(48-channel*16);if(channel<3)expected.tex[group][channel]=uint32_t(raw)<<(fraction?0:16);
        n64::RdpTriangle t;check(n64::RdpTriangle::decode(p,t) && same(t,expected));++halves;
    }
    for(unsigned op=0;op<256;++op)if((op&63)<8 || (op&63)>15){n64::RdpPacket p;p.count=p.needed=4;p.words[0]=uint64_t(op)<<56;n64::RdpTriangle t;t.tile=7;auto old=t;check(!n64::RdpTriangle::decode(p,t) && same(t,old));++rejections;}
    std::cout<<basis<<" coefficient basis packets; "<<halves<<" halfword routing packets; "<<ys<<" signed Y packets; "<<headers<<" header combinations; "<<rejections<<" atomic rejections PASS\n";
}
