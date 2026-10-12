#include "rdp_combiner.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool v){if(!v){std::cerr<<"combiner routing mismatch\n";std::exit(1);}}
static int64_t extend(unsigned v){v%=512;return v<384?v:int64_t(v)-512;}
static unsigned equation(unsigned a,unsigned b,unsigned c,unsigned d){const int64_t factor=(c%512)<256?c%512:int64_t(c%512)-512;return unsigned((extend(a)-extend(b))*factor+extend(d)*256+128)&131071;}
static unsigned clamp(unsigned raw){const unsigned v=raw%512;return v<256?v:v<384?255:0;}
static uint64_t encode(unsigned cycle,unsigned a,unsigned b,unsigned c,unsigned d,unsigned aa,unsigned ab,unsigned ac,unsigned ad){
    const unsigned shifts[2][8]={{52,28,47,15,44,12,41,9},{37,24,32,6,21,3,18,0}};
    const unsigned values[]={a,b,c,d,aa,ab,ac,ad};uint64_t word=0;for(unsigned i=0;i<8;++i)word|=uint64_t(values[i])<<shifts[cycle][i];return word;
}
int main(){
    uint64_t rgb_cases=0,alpha_cases=0;
    for(unsigned pattern=0;pattern<512;++pattern){
        n64::RdpCombiner p;p.primitive=(uint64_t((pattern*19)&255)<<32)|0x1379b5e7;p.environment=0xf2a64109;
        n64::RdpCombiner::Sources s;
        for(unsigned lane=0;lane<4;++lane){s.combined[lane]=(pattern+lane*17)&511;s.texel0[lane]=(pattern*3+lane*23)&511;s.texel1[lane]=(pattern*5+lane*31)&511;s.shade[lane]=(pattern*7+lane*37)&511;s.key_center[lane]=(pattern*11+lane*41)&511;s.key_scale[lane]=(pattern*13+lane*43)&511;}
        s.noise=(pattern*17)&511;s.k4=(pattern*23)&511;s.k5=(pattern*29)&511;s.lod=(pattern*31)&511;
        const unsigned prim[]={0x13,0x79,0xb5,0xe7},env[]={0xf2,0xa6,0x41,9};
        std::array<std::array<unsigned,32>,3> a{},b{},c{},d{};
        for(unsigned lane=0;lane<3;++lane){
            const unsigned colors[]={s.combined[lane],s.texel0[lane],s.texel1[lane],prim[lane],s.shade[lane],env[lane]};
            for(unsigned index=0;index<6;++index)a[lane][index]=b[lane][index]=c[lane][index]=d[lane][index]=colors[index];
            a[lane][6]=d[lane][6]=256;a[lane][7]=s.noise;b[lane][6]=s.key_center[lane];b[lane][7]=s.k4;c[lane][6]=s.key_scale[lane];
            const unsigned alpha[]={s.combined[3],s.texel0[3],s.texel1[3],prim[3],s.shade[3],env[3],s.lod,(pattern*19)&255,s.k5};
            for(unsigned index=0;index<9;++index)c[lane][7+index]=alpha[index];
        }
        const unsigned normal_alpha[]={s.combined[3],s.texel0[3],s.texel1[3],prim[3],s.shade[3],env[3],256,0};
        const unsigned multiply_alpha[]={s.lod,s.texel0[3],s.texel1[3],prim[3],s.shade[3],env[3],(pattern*19)&255,0};
        for(unsigned cycle=0;cycle<2;++cycle){
            // Whole RGB selector product under sixteen varied source patterns;
            // every individual selector additionally sees all nine-bit patterns.
            if(pattern<16)for(unsigned ma=0;ma<16;++ma)for(unsigned mb=0;mb<16;++mb)for(unsigned mc=0;mc<32;++mc)for(unsigned md=0;md<8;++md){
                p.combine=encode(cycle,ma,mb,mc,md,7,7,7,7);const auto r=p.evaluate(cycle,s);
                for(unsigned lane=0;lane<3;++lane){const unsigned want=equation(a[lane][ma],b[lane][mb],c[lane][mc],d[lane][md]);check(r.rgb_fixed[lane]==want && r.combined[lane]==want/256 && r.clamped[lane]==clamp(want/256));}++rgb_cases;
            }
            for(unsigned slot=0;slot<4;++slot)for(unsigned selector=0;selector<(slot==2?32u:slot==3?8u:16u);++selector){
                unsigned m[]={4,5,10,3};m[slot]=selector;p.combine=encode(cycle,m[0],m[1],m[2],m[3],7,7,7,7);const auto r=p.evaluate(cycle,s);
                for(unsigned lane=0;lane<3;++lane){const unsigned want=equation(a[lane][m[0]],b[lane][m[1]],c[lane][m[2]],d[lane][m[3]]);check(r.rgb_fixed[lane]==want && r.clamped[lane]==clamp(want/256));}++rgb_cases;
            }
            for(unsigned ma=0;ma<8;++ma)for(unsigned mb=0;mb<8;++mb)for(unsigned mc=0;mc<8;++mc)for(unsigned md=0;md<8;++md){
                p.combine=encode(cycle,15,15,31,7,ma,mb,mc,md);const auto r=p.evaluate(cycle,s);const unsigned want=equation(normal_alpha[ma],normal_alpha[mb],multiply_alpha[mc],normal_alpha[md])/256;
                check(r.combined[3]==want && r.clamped[3]==clamp(want));++alpha_cases;
            }
        }
    }
    n64::RdpCombiner p;p.combine=encode(1,0,0,31,4,0,0,7,4);check(p.independent_one_cycle());
    p.combine=encode(1,0,8,4,4,7,7,7,4);check(!p.independent_one_cycle());
    // The two common shaded-texture profiles have direct evaluators. Grade
    // every nine-bit source pair against the independent equation above,
    // including special negative encodings and clamp boundaries.
    n64::RdpCombiner::Sources common{};
    n64::RdpCombiner pass,multiply,first_cycle;
    first_cycle.combine=0x127ffffffff838ull;
    check(n64::RdpCombiner::texel_shade_cycle(first_cycle.cycle(0)));
    first_cycle.combine^=1ull<<52;
    check(!n64::RdpCombiner::texel_shade_cycle(first_cycle.cycle(0)));
    first_cycle.combine^=1ull<<52;
    for(unsigned variant=0;variant<2;++variant){
        pass.combine=variant?0x127ffffffff238ull:0x127ffffffff838ull;
        multiply.combine=variant?(0x127e24fffff9fcull^(1ull<<52)):0x127e24fffff9fcull;
        for(unsigned texel=0;texel<512;++texel){
            for(unsigned lane=0;lane<4;++lane)common.combined[lane]=(texel+lane*17)&511;
            const auto passed=pass.evaluate(1,common);
            for(unsigned lane=0;lane<3;++lane){
                const unsigned fixed=equation(0,0,0,common.combined[lane]),value=fixed/256;
                check(passed.rgb_fixed[lane]==fixed && passed.combined[lane]==value && passed.clamped[lane]==clamp(value));
            }
            const unsigned alpha=equation(0,0,0,common.combined[3])/256;
            check(passed.combined[3]==alpha && passed.clamped[3]==clamp(alpha));
            for(unsigned lane=0;lane<4;++lane)common.texel0[lane]=(texel+lane*29)&511;
            for(unsigned shade=0;shade<512;++shade){
                for(unsigned lane=0;lane<4;++lane)common.shade[lane]=(shade+lane*37)&511;
                const auto multiplied=multiply.evaluate(1,common);
                const auto first=n64::RdpCombiner::texel_shade_combined(common);
                check(first==first_cycle.evaluate(0,common).combined);
                const unsigned a=equation(0,0,0,common.shade[3])/256;
                for(unsigned lane=0;lane<3;++lane){
                    const unsigned rgb=equation(common.texel0[lane],0,common.shade[lane],0);
                    check(multiplied.rgb_fixed[lane]==rgb && multiplied.combined[lane]==rgb/256 && multiplied.clamped[lane]==clamp(rgb/256));
                    check(first[lane]==rgb/256);
                }
                check(multiplied.combined[3]==a && multiplied.clamped[3]==clamp(a) && first[3]==a);
            }
        }
    }
    std::cout<<rgb_cases<<" RGB routing captures; "<<alpha_cases<<" alpha routing captures; 525312 common-profile source cases; both cycles and stateless profile checks PASS\n";
}
