#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// SGI RDP Command Summary, tables 19, 21 and 22. Command state and
// fixed-point arithmetic and explicit source routing. Sources are supplied by
// the pixel pipeline; this kernel does not fetch textures or store pixels.
struct RdpCombiner {
    // Operands are encoded nine-bit values. A/B/D use the RDP special
    // extension (only 0x180..0x1ff are negative); C uses ordinary signed nine
    // bits. RGB retains all seventeen bits for the later key/rounding stages.
    static uint32_t rgb_equation(unsigned a,unsigned b,unsigned c,unsigned d){
        const int32_t factor=int32_t(c&511)-((c&256)?512:0);
        const int32_t fixed=(extend(a)-extend(b))*factor+extend(d)*256+128;
        return uint32_t(fixed)&131071;
    }
    static unsigned alpha_equation(unsigned a,unsigned b,unsigned c,unsigned d){
        return rgb_equation(a,b,c,d)/256;
    }
    static unsigned clamp(unsigned encoded){
        encoded&=511;
        return encoded>=384?0:encoded>=256?255:encoded;
    }
    uint64_t combine=0,primitive=0;
    uint32_t environment=0;
    struct Cycle {
        unsigned rgb_a,rgb_b,rgb_c,rgb_d,alpha_a,alpha_b,alpha_c,alpha_d;
    };
    using Color=std::array<unsigned,4>;
    struct Sources {
        Color combined{},texel0{},texel1{},shade{},key_center{},key_scale{};
        unsigned noise=0,k4=0,k5=0,lod=0;
    };
    struct Result {std::array<uint32_t,3> rgb_fixed{};Color combined{},clamped{};};
    Result evaluate(unsigned n,const Sources& s) const {
        const auto mux=cycle(n);Result result;
        for(unsigned lane=0;lane<3;++lane){
            const unsigned a=rgb_source(0,mux.rgb_a,lane,s),b=rgb_source(1,mux.rgb_b,lane,s);
            const unsigned c=rgb_source(2,mux.rgb_c,lane,s),d=rgb_source(3,mux.rgb_d,lane,s);
            result.rgb_fixed[lane]=rgb_equation(a,b,c,d);
            result.combined[lane]=result.rgb_fixed[lane]>>8;
            result.clamped[lane]=clamp(result.combined[lane]);
        }
        result.combined[3]=alpha_equation(alpha_source(mux.alpha_a,false,s),alpha_source(mux.alpha_b,false,s),alpha_source(mux.alpha_c,true,s),alpha_source(mux.alpha_d,false,s));
        result.clamped[3]=clamp(result.combined[3]);return result;
    }
    // Supported one-cycle inputs without a retained previous-pixel result,
    // LOD computation, key/conversion state or noise generation. A caller may
    // explicitly enable its current TEXEL0 input; TEXEL1 stays excluded.
    // Zero multiplier selectors make A/B electrically irrelevant.
    bool independent_one_cycle(bool texel0=false,bool coefficients=false) const {
        const auto m=cycle(1);
        const auto rgb=[texel0](unsigned v){return (texel0 && v==1) || v==3 || v==4 || v==5 || v>=8;};
        const auto add=[texel0](unsigned v){return (texel0 && v==1) || v>=3;};
        const auto alpha=[texel0](unsigned v){return (texel0 && v==1) || v>=3;};
        const auto rgb_b=[&](unsigned v){return rgb(v) || (coefficients && v==7);};
        const bool factor=(coefficients && m.rgb_c==15) || (texel0 && (m.rgb_c==1 || m.rgb_c==8)) || m.rgb_c==3 || m.rgb_c==4 || m.rgb_c==5 || m.rgb_c==10 || m.rgb_c==11 || m.rgb_c==12 || m.rgb_c==14 || m.rgb_c>=16;
        return factor && (m.rgb_c>=16 || ((rgb(m.rgb_a) || m.rgb_a==6) && rgb_b(m.rgb_b))) && add(m.rgb_d)
            && alpha(m.alpha_c) && (m.alpha_c==7 || (alpha(m.alpha_a) && alpha(m.alpha_b))) && alpha(m.alpha_d);
    }
    // Two-cycle profile with primitive-tile TEXEL0, next-tile TEXEL1 and
    // combined-color feedback. LOD, key and noise remain separate work.
    bool independent_two_cycle(bool texel0=false,bool coefficients=false,bool texel1=false) const {
        const auto supported=[&](unsigned n,bool combined,unsigned sampled){
            const auto m=cycle(n);
            const auto rgb=[&](unsigned v){return (combined && v==0) || (sampled && v==sampled) || v==3 || v==4 || v==5 || v>=8;};
            const auto add=[&](unsigned v){return (combined && v==0) || (sampled && v==sampled) || v>=3;};
            const auto alpha=add;
            const auto rgb_b=[&](unsigned v){return rgb(v) || (coefficients && v==7);};
            const bool factor=(combined && (m.rgb_c==0 || m.rgb_c==7)) ||
                (coefficients && m.rgb_c==15) ||
                (sampled && (m.rgb_c==sampled || m.rgb_c==sampled+7)) ||
                m.rgb_c==3 || m.rgb_c==4 || m.rgb_c==5 ||
                m.rgb_c==10 || m.rgb_c==11 || m.rgb_c==12 ||
                m.rgb_c==14 || m.rgb_c>=16;
            return factor && (m.rgb_c>=16 ||
                ((rgb(m.rgb_a) || m.rgb_a==6) && rgb_b(m.rgb_b))) &&
                add(m.rgb_d) && m.alpha_c!=0 && alpha(m.alpha_c) &&
                (m.alpha_c==7 || (alpha(m.alpha_a) && alpha(m.alpha_b))) &&
                alpha(m.alpha_d);
        };
        return supported(0,false,texel0?1u:0u) && supported(1,true,texel1?2u:0u);
    }
    bool uses_texel1_cycle1() const {
        const auto m=cycle(1);
        return m.rgb_a==2 || m.rgb_b==2 || m.rgb_c==2 || m.rgb_c==9 ||
               m.rgb_d==2 || m.alpha_a==2 || m.alpha_b==2 ||
               m.alpha_c==2 || m.alpha_d==2;
    }
    Cycle cycle(unsigned n) const {
        if(n==0)return {field(52,15),field(28,15),field(47,31),field(15,7),field(44,7),field(12,7),field(41,7),field(9,7)};
        return {field(37,15),field(24,15),field(32,31),field(6,7),field(21,7),field(3,7),field(18,7),field(0,7)};
    }
    unsigned minimum_lod() const{return unsigned(primitive>>40)&31;}
    unsigned lod_fraction() const{return unsigned(primitive>>32)&255;}
    uint32_t primitive_color() const{return uint32_t(primitive);}
    bool command(unsigned op,uint64_t word){
        switch(op){
        case 58:primitive=word&0x00001fffffffffffull;return true;
        case 59:environment=uint32_t(word);return true;
        case 60:combine=word&0x00ffffffffffffffull;return true;
        default:return false;
        }
    }
    using State=std::array<uint64_t,3>;
    State snapshot() const{return {combine,primitive,environment};}
    bool restore(const State& s){
        if(s[0]>>56 || s[1]>>45 || s[2]>>32)return false;
        combine=s[0];primitive=s[1];environment=uint32_t(s[2]);return true;
    }
private:
    static unsigned channel(uint32_t packed,unsigned lane){return (packed>>(24-8*lane))&255;}
    unsigned color_source(unsigned code,unsigned lane,const Sources& s) const {
        switch(code){case 0:return s.combined[lane];case 1:return s.texel0[lane];case 2:return s.texel1[lane];case 3:return channel(primitive_color(),lane);case 4:return s.shade[lane];case 5:return channel(environment,lane);default:return 0;}
    }
    unsigned rgb_source(unsigned slot,unsigned code,unsigned lane,const Sources& s) const {
        if(code<=5)return color_source(code,lane,s);
        if(slot==0)return code==6?256:code==7?s.noise:0;
        if(slot==1)return code==6?s.key_center[lane]:code==7?s.k4:0;
        if(slot==2){switch(code){case 6:return s.key_scale[lane];case 7:return s.combined[3];case 8:return s.texel0[3];case 9:return s.texel1[3];case 10:return channel(primitive_color(),3);case 11:return s.shade[3];case 12:return channel(environment,3);case 13:return s.lod;case 14:return lod_fraction();case 15:return s.k5;default:return 0;}}
        return code==6?256:0;
    }
    unsigned alpha_source(unsigned code,bool multiplier,const Sources& s) const {
        if(multiplier && code==0)return s.lod;
        if(code<=5)return color_source(code,3,s);
        return code==6?(multiplier?lod_fraction():256):0;
    }
    static int32_t extend(unsigned value){value&=511;return int32_t(value)-(value>=384?512:0);}
    unsigned field(unsigned shift,unsigned mask) const{return unsigned(combine>>shift)&mask;}
};
}
