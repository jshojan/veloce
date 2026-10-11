#include "hidden_rdram.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
// Minimal host types for the unchanged, separately fetched ares header.
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using u64=uint64_t;using n1=uint8_t;
constexpr unsigned Byte=1,Half=2,Word=4,Dual=8,ICache=32;
static std::array<unsigned,4> range(unsigned){return {0,1,2,3};}
namespace memory {template<unsigned Bytes> void writel(uint8_t* p,uint64_t value){for(unsigned n=0;n<Bytes;++n)p[n]=uint8_t(value>>(8*n));}}
#include "hidden.hpp"
extern "C" {
void reference_reset(unsigned,unsigned);
unsigned reference_hidden(unsigned);
unsigned reference_history(unsigned);
int reference_delayed(void);
void reference_byte(unsigned,unsigned,int);
void reference_half(unsigned,unsigned,int);
void reference_word(unsigned,unsigned,unsigned);
void reference_finish(void);
}
static void check(bool ok){if(!ok){std::cerr<<"primary hidden RDRAM mismatch\n";std::exit(1);}}
static void compare(const n64::HiddenRdram& actual){
    for(unsigned a=0;a<32;++a)check(actual.halfword(a)==reference_hidden(a));
    const auto s=actual.snapshot();for(unsigned n=0;n<8;++n)check(((s.history>>(2*n))&3)==reference_history(n));
    check(s.delayed==reference_delayed());
}
int main(){uint64_t cpu=0,ebus=0,rdp=0;
    n64::HiddenRdram actual(32);std::array<uint8_t,16> bits{};HiddenRAM reference;reference.data=bits.data();
    auto seed=[&](unsigned pattern,unsigned history){
        auto s=actual.snapshot();s.history=0;s.delayed=-1;
        for(unsigned n=0;n<16;++n){bits[n]=uint8_t((pattern+n)%4);check(actual.rdp_halfword(n*2,bits[n],false));}
        s=actual.snapshot();s.history=0;s.delayed=-1;
        for(unsigned n=0;n<8;++n)s.history|=uint64_t((history+n)%4)<<(n*2);
        check(actual.restore(s));reference_reset(pattern,history);
    };
    auto compare_ares=[&](){for(unsigned n=0;n<16;++n)check(actual.halfword(n*2)==bits[n]);for(unsigned a=0;a<32;a+=4){uint64_t word=0;check(actual.ebus_read(a,4,word) && word==reference.nibble(a));}};
    for(unsigned initial=0;initial<4;++initial)for(unsigned raw=0;raw<65536;++raw)for(unsigned bytes:{1u,2u,4u,8u}){
        const unsigned a=raw/16%(32/bytes)*bytes;
        const uint64_t v=uint64_t(raw)|(uint64_t(raw/2)<<16)|(uint64_t(raw/4)<<32)|(uint64_t(raw/8)<<48);seed(initial,0);
        check(actual.ordinary_write(a,bytes,v));
        switch(bytes){case Byte:reference.update<Byte>(a,v);break;case Half:reference.update<Half>(a,v);break;case Word:reference.update<Word>(a,v);break;case Dual:reference.update<Dual>(a,v);break;}
        compare_ares();++cpu;
    }
    for(unsigned initial=0;initial<16;++initial)for(unsigned raw=0;raw<256;++raw)for(unsigned bytes:{1u,2u,4u,8u})for(unsigned a=0;a<32;a+=bytes){
        seed(initial/4,initial%4);const uint64_t v=(uint64_t(raw)<<32)|(raw^255);check(actual.ebus_write(a,bytes,v));
        switch(bytes){case Byte:reference.ebusScatter<Byte>(a,v);break;case Half:reference.ebusScatter<Half>(a,v);break;case Word:reference.ebusScatter<Word>(a,v);break;case Dual:reference.ebusScatter<Dual>(a,v);break;}
        compare_ares();++ebus;
    }
    for(unsigned initial=0;initial<4;++initial)for(unsigned history=0;history<4;++history)for(unsigned raw=0;raw<256;++raw)for(unsigned lane=0;lane<8;++lane)for(bool flip:{false,true}){
        seed(initial,history);
        for(unsigned a:{lane*2,lane*2+1,24u,25u,8u,7u,6u,31u,30u,30u,0u,17u}){check(actual.rdp_byte(a,uint8_t(raw),flip));reference_byte(a,raw,flip);compare(actual);++rdp;}
        actual.finish_rdp_bytes();reference_finish();compare(actual);
    }
    for(unsigned lane=0;lane<8;++lane)for(unsigned first=0;first<4;++first)for(unsigned second=0;second<4;++second){
        seed(1,2);const unsigned a=lane*4;
        check(actual.rdp_word(a,first,second));reference_word(a,first,second);compare(actual);
        check(actual.rdp_halfword(a,second,false));reference_half(a,second,false);compare(actual);
        check(actual.rdp_halfword(a,first,true));reference_half(a,first,true);compare(actual);rdp+=3;
    }
    std::cout<<cpu<<" ares CPU stores; "<<ebus<<" ares EBUS transactions; "<<rdp<<" Angrylion RDP history writes; zero mismatches PASS\n";
}
