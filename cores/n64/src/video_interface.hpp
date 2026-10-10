#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
namespace n64 {
class VideoInterface {
public:
    // CURRENT has separate counter/ack semantics. Diagnostic line-buffer data is pending.
    static constexpr std::array<uint32_t,15> masks={0x1fbff,0xffffff,0xfff,0x3ff,0,
        0x3fffffff,0x3ff,0x1f0fff,0x0fff0fff,0x03ff03ff,0x03ff03ff,0x03ff03ff,0x0fff0fff,0x0fff0fff,0x7f};
    using State=std::array<uint64_t,20>;
    std::array<uint32_t,15> registers{};
    uint32_t current=0,phase=0;
    uint64_t fields=0;
    unsigned pattern=0;
    bool interrupt=false;
    VideoInterface() {reset();}
    void reset() {registers={};registers[3]=0x3ff;registers[5]=1;registers[7]=0x7ff;current=phase=0;fields=0;pattern=0;interrupt=false;}
    bool active() const {return (registers[0]&3)>=2;}
    bool read(unsigned index,uint64_t& value) const {
        if(index>=registers.size())return false;
        value=index==4?current:registers[index];return true;
    }
    bool write(unsigned index,uint32_t value) {
        if(index>=registers.size())return false;
        if(index==4) {interrupt=false;return true;}
        registers[index]=value&masks[index];
        if(!active())current=phase=0;
        else {if(current>registers[6])current=0;phase%=period();}
        return true;
    }
    unsigned period() const {
        // One programmable leap scanline per field. Sub-line sync waveform is pending.
        if((current>>1)==1) {
            const unsigned leap=(registers[7]>>(16+pattern))&1;
            return ((registers[8]>>(leap?0:16))&0xfff)+1;
        }
        return (registers[7]&0xfff)+1;
    }
    void tick() {
        if(!active())return;
        if(++phase<period())return;
        phase=0;current+=2;
        if(current>registers[6]) {
            const unsigned field=(registers[6]&1)?(current&1):((current&1)^1);
            current=field<=registers[6]?field:0;
            if(fields!=~uint64_t(0))++fields;
            pattern=(pattern+1)%5;
        }
        const unsigned target=registers[3],total=registers[6];bool match=false;
        if((total&1) || (target&1))match=(current>>1)==(target>>1);
        else if(target==0)match=current==0 || current==total;
        else match=current==target-unsigned(current&1);
        if(match)interrupt=true;
    }
    State snapshot() const {
        State out{};for(unsigned n=0;n<15;++n)out[n]=registers[n];
        out[15]=current;out[16]=phase;out[17]=fields;out[18]=pattern;out[19]=interrupt;return out;
    }
    bool restore(const State& data) {
        VideoInterface next;
        for(unsigned n=0;n<15;++n) {if(data[n]&~uint64_t(masks[n]))return false;next.registers[n]=uint32_t(data[n]);}
        if(data[15]>1023 || data[16]>4095 || data[18]>=5 || data[19]>1)return false;
        next.current=uint32_t(data[15]);next.phase=uint32_t(data[16]);next.fields=data[17];next.pattern=unsigned(data[18]);next.interrupt=data[19];
        if((!next.active() && (next.current || next.phase)) || next.current>next.registers[6] || next.phase>=next.period())return false;
        *this=next;return true;
    }
    // Derived, unfiltered scanout at host observation time. DMA/filter pipeline is pending.
    void render(const std::vector<uint8_t>& ram,std::vector<uint32_t>& pixels,int& width,int& height) const {
        const unsigned hstart=registers[9]>>16,hend=registers[9]&1023;
        const unsigned vstart=registers[10]>>16,vend=registers[10]&1023;
        const unsigned first=vstart+((vstart^(current&1))&1);
        if(!active() || !registers[2] || hend<=hstart || vend<=first) {width=320;height=240;pixels.assign(320*240,rgba(0,0,0));return;}
        width=int(hend-hstart);height=int((vend-first+1)/2);
        pixels.assign(size_t(width)*height,rgba(0,0,0));
        const unsigned bytes=(registers[0]&3)==2?2:4;
        const uint32_t xstep=registers[12]&4095,ystep=registers[13]&4095;
        const uint32_t xoffset=registers[12]>>16,yoffset=(registers[13]>>16)&1023;
        for(unsigned y=0;y<unsigned(height);++y)for(unsigned x=0;x<unsigned(width);++x) {
            const uint64_t sx=(xoffset+uint64_t(x)*xstep)/1024,sy=(yoffset+uint64_t(y)*ystep)/1024;
            const uint64_t at=uint64_t(registers[1])+((sy*registers[2])+sx)*bytes;
            if(at>ram.size() || bytes>ram.size()-at)continue;
            unsigned r,g,b;
            if(bytes==2) {const unsigned value=(unsigned(ram[size_t(at)])<<8)|ram[size_t(at)+1];
                r=(value>>11)&31;g=(value>>6)&31;b=(value>>1)&31;
                r=(r<<3)|(r>>2);g=(g<<3)|(g>>2);b=(b<<3)|(b>>2);
            } else {r=ram[size_t(at)];g=ram[size_t(at)+1];b=ram[size_t(at)+2];}
            pixels[size_t(y)*width+x]=rgba(r,g,b);
        }
    }
private:
    static uint32_t rgba(unsigned r,unsigned g,unsigned b) {
        const uint8_t bytes[]={uint8_t(r),uint8_t(g),uint8_t(b),255};uint32_t value;std::memcpy(&value,bytes,4);return value;
    }
};
}
