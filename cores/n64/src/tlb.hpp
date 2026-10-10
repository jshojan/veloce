#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// NEC VR4300 manual chapter 5: paired pages, 40-bit VA and 32-bit PA.
class Tlb {
public:
    static constexpr uint64_t hi_mask=0xc00000ffffffe0ffull;
    static constexpr uint32_t lo_mask=0x03ffffff,mask_mask=0x01ffe000;
    struct Entry {uint64_t hi=0;uint32_t mask=0,even=0,odd=0;};
    enum class Fault {none,address,miss,invalid,modified,undefined};
    struct Translation {Fault fault=Fault::none;uint32_t physical=0;bool cached=false,extended=false;};
    std::array<Entry,32> entries{};
    Tlb() {reset();}
    void reset() {for(unsigned n=0;n<32;++n)entries[n]={0xc00000ffff000000ull+n*8192,0,0,0};}
    static bool legal_mask(uint32_t m) {
        switch(m) {case 0:case 0x6000:case 0x1e000:case 0x7e000:case 0x1fe000:case 0x7fe000:case 0x1ffe000:return true;default:return false;}
    }
    bool write(unsigned index,uint64_t hi,uint32_t mask,uint32_t even,uint32_t odd) {
        mask&=mask_mask;if(!legal_mask(mask))return false;
        const uint32_t global=even&odd&1;
        entries[index&31]={hi&hi_mask&~uint64_t(mask),mask,(even&lo_mask&~1u)|global,(odd&lo_mask&~1u)|global};return true;
    }
    static bool valid(const Entry& e) {
        return legal_mask(e.mask) && !(e.hi&~hi_mask) && !(e.hi&e.mask) &&
               !(e.even&~lo_mask) && !(e.odd&~lo_mask) && ((e.even^e.odd)&1)==0;
    }
    int probe(uint64_t hi) const {
        int found=-1;
        for(unsigned n=0;n<32;++n)if(match(entries[n],hi)) {if(found!=-1)return -2;found=int(n);}
        return found;
    }
    Translation translate(uint64_t va,uint32_t status,uint8_t asid,uint32_t config,bool store) const {
        Translation t;
        const unsigned mode=(status&6)?0:(status>>3)&3;
        t.extended=status&(mode==0?128:mode==1?64:32);
        const uint32_t low=uint32_t(va);
        const uint64_t canonical=uint64_t(low)|((low&0x80000000u)?0xffffffff00000000ull:0);
        auto address_fault=[&]() {t.fault=Fault::address;return t;};
        if(mode==3)return address_fault();
        if(!t.extended && va!=canonical)return address_fault();
        // Compatibility kernel/supervisor segments remain present in 64-bit modes.
        if(va==canonical && low>=0x80000000u) {
            if(low<0xc0000000u) {
                if(mode!=0)return address_fault();
                t.physical=low&0x1fffffffu;t.cached=low<0xa0000000u && (config&7)!=2;return t;
            }
            if(mode==2 || (mode==1 && low>=0xe0000000u))return address_fault();
        } else if(t.extended) {
            const unsigned region=va>>62;const uint64_t offset=va&0x3fffffffffffffffull;
            if(region==2) {
                if(mode!=0 || (va&0x07ffffff00000000ull))return address_fault();
                t.physical=low;t.cached=((va>>59)&7)!=2;return t;
            }
            if((region==0 && offset>0xffffffffffull) ||
               (region==1 && (mode==2 || offset>0xffffffffffull)) ||
               (region==3 && (mode!=0 || offset>=0xff80000000ull)))return address_fault();
        } else if(low>=0x80000000u)return address_fault();
        if((status&4) && va<0x80000000ull) {t.physical=low;return t;}
        const uint64_t hi=(va&hi_mask&~uint64_t(255))|asid;
        const int index=probe(hi);
        if(index==-1) {t.fault=Fault::miss;return t;}
        if(index==-2) {t.fault=Fault::undefined;return t;}
        const auto& e=entries[unsigned(index)];
        const uint32_t page=(e.mask+8192)/2;
        const uint32_t lo=(va&page)?e.odd:e.even;
        if(!(lo&2)) {t.fault=Fault::invalid;return t;}
        if(store && !(lo&4)) {t.fault=Fault::modified;return t;}
        t.physical=((lo>>6)<<12)&~(page-1);t.physical|=uint32_t(va)&(page-1);
        t.cached=((lo>>3)&7)!=2;return t;
    }
private:
    static bool match(const Entry& e,uint64_t hi) {
        return ((hi^e.hi)&hi_mask&~uint64_t(e.mask|255))==0 && ((e.even&1) || uint8_t(hi)==uint8_t(e.hi));
    }
};
}
