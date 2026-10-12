#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace n64 {
// Functional settled N64 FlashRAM command profile. Electrical timing and
// model-specific busy/stale-status behavior are outside this device profile.
// Command/address behavior follows ares and Mupen64Plus N64 FlashRAM sources.
class FlashRam {
public:
    static constexpr size_t bytes=128*1024;
    enum class Model:uint8_t {New=0,Old=1};
    enum class Mode:uint8_t {Array=0,Id=1,Status=2,Program=3,EraseSector=4,EraseChip=5};
    std::array<uint8_t,bytes> data{};
    std::array<uint8_t,128> page{};
    bool enabled=false;
    Model model=Model::New;
    Mode mode=Mode::Array;
    uint8_t status=0;
    uint16_t erase_page=0;

    FlashRam(){data.fill(0xff);page.fill(0xff);}
    void configure(bool inserted,Model variant=Model::New){
        enabled=inserted;model=variant;data.fill(0xff);reset();
    }
    void reset(){mode=Mode::Array;status=0;erase_page=0;page.fill(0xff);}
    uint32_t id_word(unsigned word) const {
        return word==0?0x11118001u:model==Model::Old?0x00c2001eu:0x00c2001du;
    }
    bool read_word(uint32_t offset,uint32_t& value) const {
        if(!enabled)return false;
        if(offset==0 && mode==Mode::Status){value=status;return true;}
        if(offset==0 && mode==Mode::Array){value=0;return true;}
        return false;
    }
    bool write_word(uint32_t offset,uint32_t value){
        if(!enabled)return false;
        if(offset==0 && mode==Mode::Status){status=uint8_t(value);return true;}
        if(offset!=0x10000)return false;
        const uint8_t command=uint8_t(value>>24);
        const unsigned selected=value&0xffff;
        switch(command){
        case 0x3c:mode=Mode::EraseChip;return true;
        case 0x4b:
            if(selected>=1024)return false;
            erase_page=uint16_t(selected);mode=Mode::EraseSector;return true;
        case 0x78:
            if(mode==Mode::EraseChip)data.fill(0xff);
            else if(mode==Mode::EraseSector){
                const size_t base=(size_t(erase_page)*128)&~size_t(0x3fff);
                std::fill_n(data.begin()+base,0x4000,0xff);
            }else return false;
            mode=Mode::Status;status=uint8_t(status|8);return true;
        case 0xa5:
            if(selected>=1024 || mode!=Mode::Program)return false;
            for(unsigned n=0;n<128;++n)data[selected*128+n]&=page[n];
            page.fill(0xff);mode=Mode::Status;status=uint8_t(status|4);return true;
        case 0xb4:page.fill(0xff);mode=Mode::Program;return true;
        case 0xd2:mode=Mode::Status;return true;
        case 0xe1:status=uint8_t(status|1);mode=Mode::Id;return true;
        case 0xf0:mode=Mode::Array;return true;
        default:return false;
        }
    }
    bool dma_write(uint32_t offset,const uint8_t* source,size_t length){
        if(!enabled || mode!=Mode::Program || offset!=0 || length!=128)return false;
        std::copy_n(source,128,page.begin());return true;
    }
    bool dma_read(uint32_t offset,uint8_t* destination,size_t length) const {
        if(!enabled)return false;
        if(mode==Mode::Id && offset==0 && length==8){
            for(unsigned word=0;word<2;++word){const auto value=id_word(word);
                for(unsigned n=0;n<4;++n)destination[word*4+n]=uint8_t(value>>(24-8*n));}
            return true;
        }
        if(mode!=Mode::Array || offset>=0x20000)return false;
        const size_t address=model==Model::Old?size_t(offset)*2:offset;
        if(address>data.size() || length>data.size()-address)return false;
        std::copy_n(data.begin()+address,length,destination);return true;
    }
    std::array<uint64_t,4> flags() const {
        return {uint64_t(enabled),uint64_t(model),uint64_t(mode),
                uint64_t(status)|(uint64_t(erase_page)<<8)};
    }
    bool restore_flags(const std::array<uint64_t,4>& f){
        if(f[0]!=1 || f[1]>1 || f[2]>5 || f[3]>0xffffff)return false;
        enabled=true;model=Model(f[1]);mode=Mode(f[2]);
        status=uint8_t(f[3]);erase_page=uint16_t(f[3]>>8);
        if(erase_page>=1024)return false;
        return true;
    }
};
}
