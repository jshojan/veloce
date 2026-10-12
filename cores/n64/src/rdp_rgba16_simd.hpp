#pragma once
#include <array>
#include <cstdint>
#if !defined(VELOCE_RDP_DISABLE_SIMD) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP>=2))
#include <emmintrin.h>
#define VELOCE_RDP_RGBA16_SSE2 1
#else
#define VELOCE_RDP_RGBA16_SSE2 0
#endif
namespace n64 {
// Host-only expansion of four packed R5G5B5A1 colors. SSE2 is optional;
// the caller keeps its existing scalar decoder on other hosts.
struct RdpRgba16Simd {
    static constexpr bool available=VELOCE_RDP_RGBA16_SSE2;
    using Quad=std::array<std::array<int32_t,4>,4>;
    static bool decode(const std::array<uint16_t,4>& words,Quad& out){
#if VELOCE_RDP_RGBA16_SSE2
        const auto zero=_mm_setzero_si128();
        const auto v=_mm_unpacklo_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(words.data())),zero);
        const auto five=_mm_set1_epi32(31);
        const auto expand=[](__m128i x){return _mm_or_si128(_mm_slli_epi32(x,3),_mm_srli_epi32(x,2));};
        const auto r=expand(_mm_srli_epi32(v,11));
        const auto g=expand(_mm_and_si128(_mm_srli_epi32(v,6),five));
        const auto b=expand(_mm_and_si128(_mm_srli_epi32(v,1),five));
        const auto a=_mm_and_si128(_mm_sub_epi32(zero,_mm_and_si128(v,_mm_set1_epi32(1))),_mm_set1_epi32(255));
        // Transpose channel vectors into one RGBA vector per corner.
        const auto rg0=_mm_unpacklo_epi32(r,g),rg1=_mm_unpackhi_epi32(r,g);
        const auto ba0=_mm_unpacklo_epi32(b,a),ba1=_mm_unpackhi_epi32(b,a);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out[0].data()),_mm_unpacklo_epi64(rg0,ba0));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out[1].data()),_mm_unpackhi_epi64(rg0,ba0));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out[2].data()),_mm_unpacklo_epi64(rg1,ba1));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out[3].data()),_mm_unpackhi_epi64(rg1,ba1));
        return true;
#else
        (void)words;(void)out;return false;
#endif
    }
};
}
#undef VELOCE_RDP_RGBA16_SSE2
