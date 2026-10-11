#pragma once
#include "emu/input_types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace vb {
// Software-clocked standard pad. Hardware polling/timing is still unsupported.
class Controller {
public:
    static constexpr size_t state_size=40;
    void reset(){buttons=0;latched=0;data=0;control=0;count=0;}
    void input(uint32_t value){buttons=value;}
    bool read(unsigned offset,uint32_t& value) const {
        if(offset==0x10)value=data&255;
        else if(offset==0x14)value=data>>8;
        else if(offset==0x28)value=control|0x4c; // Reserved ones and HW-SI readback.
        else return false;
        return true;
    }
    bool write(unsigned offset,uint32_t value){
        if(offset==0x10 || offset==0x14)return true; // Read-only data registers.
        if(offset!=0x28 || ((value&4) && !(value&1)))return false;
        const unsigned next=value&0xb1;
        if(next&0x20){latched=wire();count=0;}
        else if((control&0x10) && !(next&0x10) && count<16){
            if(++count==16)data=latched;
        }
        control=next;return true;
    }
    std::array<uint8_t,state_size> save() const {
        std::array<uint8_t,state_size> out{};unsigned at=0;
        for(uint64_t v:{uint64_t(buttons),uint64_t(latched),uint64_t(data),uint64_t(control),uint64_t(count)})
            for(unsigned n=0;n<8;++n)out[at++]=uint8_t(v>>(56-n*8));
        return out;
    }
    bool restore(const uint8_t* in,size_t size){
        if(!in || size!=state_size)return false;
        std::array<uint64_t,5> v{};unsigned at=0;
        for(auto& x:v)for(unsigned n=0;n<8;++n)x=x*256+in[at++];
        if(v[0]>0xffffffffu || v[1]>65535 || v[2]>65535 || (v[3]&~uint64_t(0xb1)) || v[4]>16)return false;
        buttons=uint32_t(v[0]);latched=unsigned(v[1]);data=unsigned(v[2]);control=unsigned(v[3]);count=unsigned(v[4]);return true;
    }
private:
    uint16_t wire() const {
        constexpr emu::VirtualButton keys[]={emu::VirtualButton::A,emu::VirtualButton::B,emu::VirtualButton::L,emu::VirtualButton::R,
            emu::VirtualButton::Start,emu::VirtualButton::Select,emu::VirtualButton::Up,emu::VirtualButton::Down,
            emu::VirtualButton::Left,emu::VirtualButton::Right,emu::VirtualButton::CUp,emu::VirtualButton::CDown,
            emu::VirtualButton::CLeft,emu::VirtualButton::CRight};
        constexpr unsigned bits[]={2,3,5,4,12,13,11,10,9,8,6,15,14,7};
        uint16_t result=2;for(unsigned n=0;n<14;++n)if(emu::is_button_pressed(buttons,keys[n]))result|=uint16_t(1u<<bits[n]);return result;
    }
    uint32_t buttons=0;unsigned latched=0,data=0,control=0,count=0;
};
}
