#include "serial.hpp"
#include "emu/input_types.hpp"
#include <algorithm>
namespace ppc {
namespace {
uint32_t get(const uint8_t* p) { return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]; }
void put(uint8_t* p,uint32_t v) { for(unsigned n=0;n<4;++n) p[n]=uint8_t(v>>(24-n*8)); }
constexpr uint32_t writable=0x487f7f06; // masks, lengths, selected channel
}
void Serial::reset() { buffer={};buttons={};connected=1;control=0;status=0; }
void Serial::input(unsigned port,uint32_t mask,bool present) {
    if(port>=4) return;
    buttons[port]=mask;
    if(present) connected|=1u<<port; else connected&=~(1u<<port);
}
bool Serial::read(uint32_t offset,uint32_t& value) const {
    if(offset>=0x80 && offset<=0xfc && !(offset&3)) { value=get(buffer.data()+offset-0x80);return true; }
    if(offset==0x34) { value=control;return true; }
    if(offset==0x38) { value=status;return true; }
    return false;
}
bool Serial::write(uint32_t offset,uint32_t value) {
    if(offset>=0x80 && offset<=0xfc && !(offset&3)) { put(buffer.data()+offset-0x80,value);return true; }
    if(offset==0x38) { status&=~(value&0x0f0f0f0f);return true; }
    if(offset!=0x34) return false;
    control=(control&0xa0000000u)|(value&writable);
    if(value&0x80000000u) control&=~0x80000000u;
    if(value&1) transfer();
    return true;
}
void Serial::transfer() {
    const unsigned port=(control>>1)&3;
    const unsigned requested=(control>>8)&127;
    const unsigned input_length=requested?requested:128;
    const unsigned encoded=(control>>16)&127;
    const unsigned output_length=encoded?encoded:128;
    std::array<uint8_t,8> response{};
    unsigned length=0;
    if(connected&(1u<<port)) {
        if((buffer[0]==0 || buffer[0]==255) && output_length==1) {
            // Supplied idle standard-controller ID/status (09 00 00).
            response[0]=9;length=3;
        } else if(buffer[0]==0x40 && output_length==3 && buffer[1]==3 && buffer[2]<=2) {
            // Mode 3: digital buttons, neutral sticks and digital L/R endpoints.
            constexpr emu::VirtualButton virtuals[]={emu::VirtualButton::A,emu::VirtualButton::B,
                emu::VirtualButton::X,emu::VirtualButton::Y,emu::VirtualButton::Start,
                emu::VirtualButton::Left,emu::VirtualButton::Right,emu::VirtualButton::Down,
                emu::VirtualButton::Up,emu::VirtualButton::Z,emu::VirtualButton::R,emu::VirtualButton::L};
            constexpr unsigned wire[]={8,9,10,11,12,0,1,2,3,4,5,6};
            uint32_t digital=0x80;
            for(unsigned n=0;n<12;++n) if(emu::is_button_pressed(buttons[port],virtuals[n])) digital|=1u<<wire[n];
            response[0]=uint8_t(digital>>8);response[1]=uint8_t(digital);
            response[2]=response[3]=response[4]=response[5]=128;
            response[6]=emu::is_button_pressed(buttons[port],emu::VirtualButton::L)?255:0;
            response[7]=emu::is_button_pressed(buttons[port],emu::VirtualButton::R)?255:0;
            length=8;
        }
    }
    uint32_t error=0;
    if(!length) error=8; // no response; preserve communication RAM
    else {
        std::copy_n(response.begin(),std::min(length,input_length),buffer.begin());
        if(length<input_length) error=1;
        if(length>input_length) error=2;
    }
    status|=error<<(24-port*8);
    control=(control&~0x20000000u)|0x80000000u|(error?0x20000000u:0);
}
std::array<uint8_t,Serial::state_size> Serial::save() const {
    std::array<uint8_t,state_size> state{};
    std::copy(buffer.begin(),buffer.end(),state.begin());
    for(unsigned n=0;n<4;++n) put(state.data()+128+n*4,buttons[n]);
    put(state.data()+144,connected);put(state.data()+148,control);put(state.data()+152,status);
    return state;
}
bool Serial::restore(const uint8_t* data,size_t size) {
    if(!data || size!=state_size || get(data+144)>15 || (get(data+148)&~(writable|0xa0000000u)) || (get(data+152)&~0x0f0f0f0fu)) return false;
    std::copy_n(data,128,buffer.begin());
    for(unsigned n=0;n<4;++n) buttons[n]=get(data+128+n*4);
    connected=get(data+144);control=get(data+148);status=get(data+152);return true;
}
}
