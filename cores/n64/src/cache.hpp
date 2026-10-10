#pragma once
#include "memory.hpp"
#include <array>
#include <cstdint>
namespace n64 {
class PrimaryCaches {
public:
    template<unsigned Bytes> struct Line {
        uint32_t page=0;
        uint8_t flags=0; // bit 0 valid, bit 1 dirty
        std::array<uint8_t,Bytes> data{};
    };
    std::array<Line<32>,512> instruction{};
    std::array<Line<16>,512> data{};
    void reset() {instruction={};data={};}
    static bool physical(uint64_t address,uint32_t& result) {
        const auto low=uint32_t(address);
        const auto canonical=uint64_t(low)|((low&0x80000000u)?0xffffffff00000000ull:0);
        if(address!=canonical || low<0x80000000u || low>=0xc0000000u)return false;
        result=low&0x1fffffffu;return true;
    }
    bool read(Memory& bus,uint64_t address,unsigned bytes,uint64_t& value,bool fetch,uint32_t config) {
        uint32_t p=0;if(!physical(address,p))return false;
        return read_translated(bus,address,p,cached(address,config),bytes,value,fetch);
    }
    bool write(Memory& bus,uint64_t address,unsigned bytes,uint64_t value,uint8_t mask,uint32_t config) {
        uint32_t p=0;if(!physical(address,p))return false;
        return write_translated(bus,address,p,cached(address,config),bytes,value,mask);
    }
    bool read_translated(Memory& bus,uint64_t va,uint32_t p,bool use_cache,unsigned bytes,uint64_t& value,bool fetch) {
        if((bytes!=1 && bytes!=2 && bytes!=4 && bytes!=8) || (va&(bytes-1)) || p>0x1fffffffu)return false;
        if(!use_cache)return bus.read(bus_address(p),bytes,value);
        if(fetch) {const unsigned index=(va>>5)&511;auto& line=instruction[index];if(!fill(bus,line,p,index,false))return false;value=read_bytes(line,p&31,bytes);}
        else {const unsigned index=(va>>4)&511;auto& line=data[index];if(!fill(bus,line,p,index,false))return false;value=read_bytes(line,p&15,bytes);}
        return true;
    }
    bool write_translated(Memory& bus,uint64_t va,uint32_t p,bool use_cache,unsigned bytes,uint64_t value,uint8_t mask) {
        if((bytes!=1 && bytes!=2 && bytes!=4 && bytes!=8) || (va&(bytes-1)) || !mask || (unsigned(mask)>>bytes) || p>0x1fffffffu)return false;
        if(!use_cache)return mask==((1u<<bytes)-1)?bus.write(bus_address(p),bytes,value):bus.write_masked(bus_address(p),bytes,value,mask);
        const unsigned index=(va>>4)&511;auto& line=data[index];
        if(!fill(bus,line,p,index,false))return false;
        for(unsigned n=0;n<bytes;++n)if(mask&(1u<<n))line.data[(p&15)+n]=uint8_t(value>>(8*(bytes-1-n)));
        line.flags=3;return true;
    }
    bool operation(Memory& bus,unsigned op,uint64_t address,uint32_t& taglo) {
        uint32_t p=0;if(!physical(address,p))return false;
        return operation_translated(bus,op,address,p,taglo);
    }
    // Index operations only use VA index bits. Hit/fill/create operations use PA tags.
    bool operation_translated(Memory& bus,unsigned op,uint64_t va,uint32_t p,uint32_t& taglo) {
        if(op&1)return operate(bus,data[(va>>4)&511],(va>>4)&511,p,op>>2,taglo,false);
        return operate(bus,instruction[(va>>5)&511],(va>>5)&511,p,op>>2,taglo,true);
    }
    static bool defined(unsigned op) {
        switch(op) {case 0:case 1:case 4:case 5:case 8:case 9:case 13:case 16:case 17:case 20:case 21:case 24:case 25:return true;default:return false;}
    }
private:
    static bool cached(uint64_t address,uint32_t config) {
        uint32_t p=0;return physical(address,p) && uint32_t(address)<0xa0000000u && (config&7)!=2;
    }
    static uint64_t bus_address(uint32_t p) {return 0xffffffffa0000000ull|p;}
    template<unsigned B> static uint64_t read_bytes(const Line<B>& line,unsigned offset,unsigned bytes) {
        uint64_t value=0;for(unsigned n=0;n<bytes;++n)value=(value<<8)|line.data[offset+n];return value;
    }
    template<unsigned B> static bool writeback(Memory& bus,Line<B>& line,unsigned index,bool force=false) {
        if(!(line.flags&1) || (!(line.flags&2) && !force))return true;
        if(line.page>0x1ffff)return false;
        const uint32_t p=(line.page<<12)|((index*B)&4095);
        for(unsigned n=0;n<B;n+=8)if(!bus.write(bus_address(p+n),8,read_bytes(line,n,8)))return false;
        line.flags=1;return true;
    }
    template<unsigned B> static bool fill(Memory& bus,Line<B>& line,uint32_t p,unsigned index,bool force) {
        if(p>0x1fffffffu)return false;
        if(!force && (line.flags&1) && line.page==(p>>12))return true;
        Line<B> next;next.page=p>>12;next.flags=1;
        for(unsigned n=0;n<B;n+=8) {
            uint64_t value=0;if(!bus.read(bus_address((p&~(B-1))+n),8,value))return false;
            for(unsigned byte=0;byte<8;++byte)next.data[n+byte]=uint8_t(value>>(56-8*byte));
        }
        if(!writeback(bus,line,index))return false;
        line=next;return true;
    }
    template<unsigned B> static bool operate(Memory& bus,Line<B>& line,unsigned index,uint32_t p,unsigned op,uint32_t& taglo,bool icache) {
        const bool hit=(line.flags&1) && line.page==(p>>12);
        if(op==0) {if(!icache && !writeback(bus,line,index,true))return false;line.flags=0;return true;}
        if(op==1) {taglo=(line.page<<8)|((line.flags&1)?(icache?128:192):0);return true;}
        if(op==2) {line.page=(taglo>>8)&0xfffff;line.flags=(taglo&128)?1:0;if(!icache && line.flags && (taglo&64))line.flags=3;return true;}
        if(op==3) {if(!hit && !writeback(bus,line,index))return false;line.page=p>>12;line.flags=3;return true;}
        if(op==4) {if(hit)line.flags=0;return true;}
        if(op==5 && icache)return fill(bus,line,p,index,true);
        if(op==5) {if(hit) {if(!writeback(bus,line,index))return false;line.flags=0;}return true;}
        if(op==6 && hit)return writeback(bus,line,index,icache);
        return true;
    }
};
}
