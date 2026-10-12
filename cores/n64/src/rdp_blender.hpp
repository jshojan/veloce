#pragma once
#include "rdp_coverage.hpp"
#include "rdp_depth.hpp"
namespace n64 {
// One-cycle color/depth decisions, plus two-cycle fog/pass then framebuffer blend,
// with an optional ordered alpha seed.
// The caller applies final RGB dithering after these decisions.
// Memory RGB is a retained latch; disabling image reads only replaces its alpha.
struct RdpBlender {
    using Color=std::array<unsigned,4>;
    uint32_t fog=0,blend=0,memory_rgb=0;
    struct Result {bool write=false,enabled=false;std::array<unsigned,3> rgb{};unsigned coverage=0;};
    bool command(unsigned op,uint64_t word){if(op==56)fog=uint32_t(word);else if(op==57)blend=uint32_t(word);else return false;return true;}
    using State=std::array<uint64_t,3>;
    State snapshot()const{return {fog,blend,memory_rgb};}
    bool restore(const State& s){if(s[0]>0xffffffffu || s[1]>0xffffffffu || s[2]>0xffffffu)return false;fog=uint32_t(s[0]);blend=uint32_t(s[1]);memory_rgb=uint32_t(s[2]);return true;}
    static unsigned divide(unsigned index){return division()[index&32767];}
    static unsigned equation(unsigned first,unsigned second,unsigned a,unsigned b,bool force){
        const unsigned sum=first*a+second*(b+1);
        return force?(sum>>5)&255:divide(((a&~3u)+(b&~3u)+4)*512|((sum>>2)&2047));
    }
    struct DepthInput {unsigned z=0;uint16_t word=0;unsigned hidden=0;};
    static bool fog_two_cycle_supported(uint64_t mode){
        const unsigned fog_alpha=unsigned(mode>>26)&3;
        return ((mode>>30)&3)==3 && (fog_alpha==1 || fog_alpha==2) &&
               ((mode>>22)&3)==0 && ((mode>>18)&3)==0;
    }
    static bool pass_two_cycle_supported(uint64_t mode){
        return ((mode>>30)&3)==0 && ((mode>>26)&3)==3 &&
               ((mode>>22)&3)==0 && ((mode>>18)&3)==2;
    }
    Result pixel_two_cycle(uint64_t mode,Color color,unsigned shade_alpha,uint32_t memory,unsigned count,unsigned cvbit,unsigned delta_z,const DepthInput* depth=nullptr,unsigned alpha_dither=0){
        // BL0 receives the second combiner result. It either fogs or passes
        // that RGB; BL1 source zero receives the BL0 result.
        if(!pass_two_cycle_supported(mode)){
            const unsigned alpha=((mode>>26)&3)==1?(fog&255):std::min(255u,shade_alpha+alpha_dither);
            const Color fog_color=unpack(fog);
            for(unsigned lane=0;lane<3;++lane)
                color[lane]=equation(fog_color[lane],color[lane],alpha>>3,(alpha^255)>>3,true);
        }
        const uint64_t first_mux=(3ull<<30)|(3ull<<26)|(3ull<<22)|(3ull<<18);
        const uint64_t second_mux=(3ull<<28)|(3ull<<24)|(3ull<<20)|(3ull<<16);
        const uint64_t selected=(mode&~first_mux)|((mode&second_mux)<<2);
        return pixel(selected,color,shade_alpha,memory,count,cvbit,delta_z,depth,alpha_dither);
    }
    Result pixel(uint64_t mode,Color color,unsigned shade_alpha,uint32_t memory,unsigned count,unsigned cvbit,unsigned delta_z,const DepthInput* depth=nullptr,unsigned alpha_dither=0){
        Result out;
        const unsigned memory_coverage=(mode&64)?(memory>>5)&7:7;
        if(mode&64)memory_rgb=memory>>8;
        const unsigned alpha256=color[3]==255?256:color[3];
        const unsigned scaled=(alpha256*count+4)>>3;
        if(mode&4096)count=(scaled>>5)&15;
        color[3]=(mode&8192)?std::min(255u,(mode&4096)?scaled:count*32):std::min(255u,alpha256+alpha_dither);
        shade_alpha=std::min(255u,shade_alpha+alpha_dither);
        const auto decision=RdpDepth::compare(depth && (mode&16),unsigned(mode>>10)&3,mode&8,mode&16384,depth?depth->z:0,uint16_t(delta_z),depth?depth->word:0,depth?depth->hidden:0,count,memory_coverage);
        count=decision.coverage;
        const bool overflow=decision.overflow;
        out.enabled=decision.blend;
        out.coverage=RdpCoverage::stored(unsigned(mode>>8)&3,out.enabled,count,memory_coverage);
        const bool accepted=decision.pass && (!(mode&1) || color[3]>=(blend&255)) && ((mode&8)?count!=0:cvbit!=0);
        const unsigned p=unsigned(mode>>30)&3,a_source=unsigned(mode>>26)&3;
        const unsigned m=unsigned(mode>>22)&3,b_source=unsigned(mode>>18)&3;
        const std::array<Color,4> colors={color,unpack((memory_rgb<<8)|(memory_coverage<<5)),unpack(blend),unpack(fog)};
        const unsigned alpha=a_source==0?color[3]:a_source==1?fog&255:a_source==2?shade_alpha:0;
        const unsigned beta=b_source==0?(alpha^255):b_source==1?memory_coverage*32:b_source==2?255:0;
        const bool bypass=!out.enabled || (a_source==0 && b_source==0 && color[3]>=255);
        unsigned a=alpha>>3,b=beta>>3;
        if(b_source==1){
            a=(a>>decision.shift_a)&60;b=(b>>decision.shift_b)|3;
        }
        for(unsigned lane=0;lane<3;++lane){
            if((mode&128) && !overflow)out.rgb[lane]=colors[m][lane];
            else out.rgb[lane]=bypass?colors[p][lane]:equation(colors[p][lane],colors[m][lane],a,b,mode&16384);
        }
        out.write=accepted;return out;
    }
private:
    static Color unpack(uint32_t word){return {word>>24,(word>>16)&255,(word>>8)&255,word&255};}
    static const std::array<uint8_t,32768>& division(){
        static const auto table=[](){std::array<uint8_t,32768> values{};
            for(unsigned i=0;i<32768;++i){const unsigned d=i>>11,n=i&2047;
                unsigned residue=((15-d)+(n>>8)+1)&7,result=0;
                for(unsigned step=0;step<8;++step){const unsigned bit=7-step;
                    const unsigned next=2*residue+((n>>bit)&1)+(step && (result&(1u<<(bit+1)))?16-d:d);
                    residue=next&7;if(next&16)result|=1u<<bit;
                }values[i]=uint8_t(result);
            }return values;
        }();return table;
    }
};
}
