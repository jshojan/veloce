#pragma once
#include <array>
#include <cstdint>

namespace n64 {
// Functional 8 MiB topology: four 2 MiB devices. Analog calibration is pending.
class RdramDevices {
public:
    using State=std::array<std::array<uint32_t,9>,4>;
    RdramDevices() {reset();}
    void reset() {chips={};for(auto& chip:chips)chip[1]=0x20;}
    uint32_t read(uint32_t address,bool active) const {
        if(!active || (address&0x80000))return 0;
        const int chip=select((address>>10)&511);
        if(chip<0)return 0;
        const auto& c=chips[unsigned(chip)];
        if(address&0x200)return c[8];
        if(!(c[2]&0x02000000))return 0;
        const unsigned reg=(address>>2)&15;
        if(reg==0)return 0xb4190010;
        if(reg==9)return 0x500;
        if(reg>=10)return 0;
        if(reg==2)return c[1]|0x03030203;
        if(reg==3)return c[2]^0xc0c0c0c0;
        return c[reg-1];
    }
    void write(uint32_t address,uint32_t value,bool active,unsigned repeat_length) {
        if(!active)return;
        if(address&0x80000) {
            for(auto& c:chips)write_chip(c,address,value,repeat_length);
        } else {
            const int chip=select((address>>10)&511);
            if(chip>=0)write_chip(chips[unsigned(chip)],address,value,repeat_length);
        }
    }
    State snapshot() const {return chips;}
    bool restore(const State& state) {
        for(const auto& c:state)if(c[1]&~0x38381838u)return false;
        chips=state;return true;
    }
private:
    State chips{};
    static uint16_t id(uint32_t word) {
        constexpr unsigned positions[]={26,27,28,29,30,31,23,8,9,10,11,12,13,14,15,7};
        uint16_t result=0;
        for(unsigned bit=0;bit<16;++bit)result|=((word>>positions[bit])&1)<<bit;
        return result;
    }
    int select(unsigned wanted) const {
        bool disabled_seen=false;
        for(unsigned n=0;n<chips.size();++n) {
            const auto& c=chips[n];const bool enabled=c[2]&0x02000000;
            if(!enabled && disabled_seen)continue;
            if(!enabled)disabled_seen=true;
            if((id(c[0])>>1)==(wanted>>1))return int(n);
        }
        return -1;
    }
    static void write_chip(std::array<uint32_t,9>& c,uint32_t address,uint32_t value,unsigned repeat_length) {
        if(address&0x200) {c[8]=value;return;}
        const unsigned reg=(address>>2)&15;
        if(!reg || reg>=9)return;
        if(((c[1]>>3)&7)!=1) {
            if(repeat_length<16)return;
            value=(value>>16)|(value<<16);
        }
        c[reg-1]=reg==2?value&0x38381838u:value;
    }
};
}
