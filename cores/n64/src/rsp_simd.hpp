#pragma once
#include <array>
#include <cstdint>
#if !defined(VELOCE_RSP_DISABLE_SIMD) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP>=2))
#include <emmintrin.h>
#define VELOCE_RSP_SSE2 1
#else
#define VELOCE_RSP_SSE2 0
#endif
namespace n64 {
// Host acceleration only. Inputs are captured before any destination write;
// accumulators keep their raw 48-bit encoding and flags retain native lanes.
struct RspSimd {
    using Words=std::array<uint16_t,8>;
    static constexpr bool available=VELOCE_RSP_SSE2;
    static Words load_words(const std::array<uint8_t,16>& bytes){
        Words result{};
#if VELOCE_RSP_SSE2
        const auto packed=_mm_loadu_si128(reinterpret_cast<const __m128i*>(bytes.data()));
        const auto swapped=_mm_or_si128(_mm_slli_epi16(packed,8),_mm_srli_epi16(packed,8));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(result.data()),swapped);
#else
        for(unsigned i=0;i<8;++i)result[i]=uint16_t(unsigned(bytes[2*i])*256+bytes[2*i+1]);
#endif
        return result;
    }
    static void store_words(std::array<uint8_t,16>& bytes,const Words& words){
#if VELOCE_RSP_SSE2
        const auto packed=_mm_loadu_si128(reinterpret_cast<const __m128i*>(words.data()));
        const auto swapped=_mm_or_si128(_mm_slli_epi16(packed,8),_mm_srli_epi16(packed,8));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(bytes.data()),swapped);
#else
        for(unsigned i=0;i<8;++i){bytes[2*i]=uint8_t(words[i]>>8);bytes[2*i+1]=uint8_t(words[i]);}
#endif
    }
#if VELOCE_RSP_SSE2
private:
    static __m128i load(const Words& v){return _mm_loadu_si128(reinterpret_cast<const __m128i*>(v.data()));}
    static void store(Words& v,__m128i x){_mm_storeu_si128(reinterpret_cast<__m128i*>(v.data()),x);}
    static __m128i pack_unsigned(__m128i a,__m128i b){
        const auto bias=_mm_set1_epi32(32768);
        return _mm_xor_si128(_mm_packs_epi32(_mm_sub_epi32(a,bias),_mm_sub_epi32(b,bias)),_mm_set1_epi16(-32768));
    }
    static unsigned bits(__m128i v){
        unsigned m=(unsigned(_mm_movemask_epi8(v))>>1)&0x5555;
        m=(m|(m>>1))&0x3333;m=(m|(m>>2))&0x0f0f;return (m|(m>>4))&255;
    }
    static __m128i select(__m128i mask,__m128i a,__m128i b){return _mm_or_si128(_mm_and_si128(mask,a),_mm_andnot_si128(mask,b));}
    static void low_acc(std::array<uint64_t,8>& acc,__m128i words){
        const auto zero=_mm_setzero_si128(),keep=_mm_set1_epi64x(0x0000ffffffff0000ll);
        const __m128i halves[]={_mm_unpacklo_epi16(words,zero),_mm_unpackhi_epi16(words,zero)};
        for(unsigned h=0;h<2;++h){const __m128i values[]={_mm_unpacklo_epi32(halves[h],zero),_mm_unpackhi_epi32(halves[h],zero)};
            for(unsigned p=0;p<2;++p){auto* at=reinterpret_cast<__m128i*>(acc.data()+h*4+p*2);
                _mm_storeu_si128(at,_mm_or_si128(_mm_and_si128(_mm_loadu_si128(at),keep),values[p]));}}
    }
public:
#endif
    static bool alu(unsigned fn,const Words& left,const Words& right,std::array<uint64_t,8>& acc,std::array<uint16_t,3>& flags,Words& output){
#if VELOCE_RSP_SSE2
        if(!(fn==16 || fn==17 || fn==19 || fn==20 || fn==21 || fn==39 || (fn>=40 && fn<=45)))return false;
        const auto a=load(left),b=load(right),zero=_mm_setzero_si128(),ones=_mm_set1_epi16(-1);
        __m128i value=zero,word=zero;
        if(fn==16 || fn==17){
            const auto carry=_mm_setr_epi16(flags[0]&1,(flags[0]>>1)&1,(flags[0]>>2)&1,(flags[0]>>3)&1,
                (flags[0]>>4)&1,(flags[0]>>5)&1,(flags[0]>>6)&1,(flags[0]>>7)&1);
            const auto as=_mm_srai_epi16(a,15),bs=_mm_srai_epi16(b,15);
            __m128i x=_mm_unpacklo_epi16(a,as),y=_mm_unpackhi_epi16(a,as);
            if(fn==16){x=_mm_add_epi32(_mm_add_epi32(x,_mm_unpacklo_epi16(b,bs)),_mm_unpacklo_epi16(carry,zero));
                y=_mm_add_epi32(_mm_add_epi32(y,_mm_unpackhi_epi16(b,bs)),_mm_unpackhi_epi16(carry,zero));}
            else{x=_mm_sub_epi32(_mm_sub_epi32(x,_mm_unpacklo_epi16(b,bs)),_mm_unpacklo_epi16(carry,zero));
                y=_mm_sub_epi32(_mm_sub_epi32(y,_mm_unpackhi_epi16(b,bs)),_mm_unpackhi_epi16(carry,zero));}
            word=_mm_packs_epi32(x,y);const auto low=_mm_set1_epi32(65535);
            value=pack_unsigned(_mm_and_si128(x,low),_mm_and_si128(y,low));flags[0]=0;
        }else if(fn==19){
            const auto negative=_mm_cmpgt_epi16(zero,a),positive=_mm_cmpgt_epi16(a,zero);
            value=_mm_or_si128(_mm_and_si128(negative,_mm_sub_epi16(zero,b)),_mm_and_si128(positive,b));
            word=_mm_or_si128(_mm_and_si128(negative,_mm_subs_epi16(zero,b)),_mm_and_si128(positive,b));
        }else if(fn==20 || fn==21){
            const auto bias=_mm_set1_epi16(-32768);
            value=fn==20?_mm_add_epi16(a,b):_mm_sub_epi16(a,b);word=value;
            if(fn==20)flags[0]=uint16_t(bits(_mm_cmpgt_epi16(_mm_xor_si128(a,bias),_mm_xor_si128(value,bias))));
            else flags[0]=uint16_t(bits(_mm_cmpgt_epi16(_mm_xor_si128(b,bias),_mm_xor_si128(a,bias)))|
                (bits(_mm_xor_si128(_mm_cmpeq_epi16(a,b),ones))<<8));
        }else if(fn==39){
            const auto mask=_mm_setr_epi16(-int(flags[1]&1),-int((flags[1]>>1)&1),-int((flags[1]>>2)&1),-int((flags[1]>>3)&1),
                -int((flags[1]>>4)&1),-int((flags[1]>>5)&1),-int((flags[1]>>6)&1),-int((flags[1]>>7)&1));
            value=word=select(mask,a,b);flags[0]=0;
        }else{
            if(fn==40 || fn==41)value=_mm_and_si128(a,b);
            else if(fn==42 || fn==43)value=_mm_or_si128(a,b);
            else value=_mm_xor_si128(a,b);
            if(fn&1)value=_mm_xor_si128(value,ones);
            word=value;
        }
        low_acc(acc,value);store(output,word);return true;
#else
        (void)fn;(void)left;(void)right;(void)acc;(void)flags;(void)output;return false;
#endif
    }
    static bool multiply(unsigned fn,const Words& left,const Words& right,std::array<uint64_t,8>& acc,Words& output){
#if VELOCE_RSP_SSE2
        if(!(fn<=1 || (fn>=4 && fn<=9) || (fn>=12 && fn<=15)))return false;
        const auto a=load(left),b=load(right),zero=_mm_setzero_si128();
        const bool unsigned_product=fn==4 || fn==12;
        const auto low=_mm_mullo_epi16(a,b);
        __m128i high;
        if(unsigned_product)high=_mm_mulhi_epu16(a,b);
        else if(fn==5 || fn==13)high=_mm_sub_epi16(_mm_mulhi_epu16(a,b),_mm_and_si128(_mm_srai_epi16(a,15),b));
        else if(fn==6 || fn==14)high=_mm_sub_epi16(_mm_mulhi_epu16(a,b),_mm_and_si128(_mm_srai_epi16(b,15),a));
        else high=_mm_mulhi_epi16(a,b);
        __m128i product[]={_mm_unpacklo_epi16(low,high),_mm_unpackhi_epi16(low,high)};
        if(unsigned_product){product[0]=_mm_srli_epi32(product[0],16);product[1]=_mm_srli_epi32(product[1],16);}
        const auto mask=_mm_set1_epi64x(0x0000ffffffffffffll);
        __m128i middle[4],lower[4];
        for(unsigned h=0;h<2;++h){const auto sign=unsigned_product?zero:_mm_srai_epi32(product[h],31);
            __m128i values[]={_mm_unpacklo_epi32(product[h],sign),_mm_unpackhi_epi32(product[h],sign)};
            for(unsigned p=0;p<2;++p){
                auto value=values[p];
                if(fn<=1 || fn==8 || fn==9)value=_mm_slli_epi64(value,1);
                if(fn<=1)value=_mm_add_epi64(value,_mm_set1_epi64x(32768));
                if(fn==7 || fn==15)value=_mm_slli_epi64(value,16);
                auto* at=reinterpret_cast<__m128i*>(acc.data()+h*4+p*2);
                if(fn>=8)value=_mm_add_epi64(value,_mm_loadu_si128(at));
                value=_mm_and_si128(value,mask);_mm_storeu_si128(at,value);
                middle[h*2+p]=_mm_shuffle_epi32(_mm_srli_epi64(value,16),_MM_SHUFFLE(2,0,2,0));
                lower[h*2+p]=_mm_shuffle_epi32(value,_MM_SHUFFLE(2,0,2,0));
            }
        }
        __m128i m[]={_mm_unpacklo_epi64(middle[0],middle[1]),_mm_unpacklo_epi64(middle[2],middle[3])};
        __m128i l[]={_mm_unpacklo_epi64(lower[0],lower[1]),_mm_unpacklo_epi64(lower[2],lower[3])};
        if(fn==1 || fn==9 || fn==12 || fn==14){
            for(unsigned h=0;h<2;++h){const auto below=_mm_cmpgt_epi32(_mm_set1_epi32(fn<=9?0:-32768),m[h]);
                const auto above=_mm_cmpgt_epi32(m[h],_mm_set1_epi32(32767));
                const auto normal=fn<=9?m[h]:_mm_and_si128(l[h],_mm_set1_epi32(65535));
                l[h]=_mm_or_si128(_mm_andnot_si128(_mm_or_si128(below,above),normal),_mm_and_si128(above,_mm_set1_epi32(65535)));}
            store(output,pack_unsigned(l[0],l[1]));
        }else if(fn==4 || fn==6){
            store(output,pack_unsigned(_mm_and_si128(l[0],_mm_set1_epi32(65535)),_mm_and_si128(l[1],_mm_set1_epi32(65535))));
        }else store(output,_mm_packs_epi32(m[0],m[1]));
        return true;
#else
        (void)fn;(void)left;(void)right;(void)acc;(void)output;return false;
#endif
    }
};
}
#undef VELOCE_RSP_SSE2
