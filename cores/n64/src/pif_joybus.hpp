#pragma once
#include "emu/input_types.hpp"
#include "emu/n64_input.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
namespace n64 {
// Packet formatting latches five channel offsets. SI reads communicate through
// that layout with current input; clearing the format bit does not disable it.
// This functional profile has four controllers with signed-byte analog input
// and a cartridge EEPROM. Paks and physical timing are not modeled.
class PifJoybus {
public:
    using Ram=std::array<uint8_t,64>;
    using State=std::array<uint64_t,5>;
    using Eeprom=std::array<uint8_t,2048>;
    PifJoybus(){erase_eeprom();reset();}
    void reset(){channels.fill(64);}
    void configure_eeprom(size_t bytes){eeprom_size=bytes==512 || bytes==2048?bytes:0;}
    bool has_eeprom() const {return eeprom_size!=0;}
    size_t eeprom_bytes() const {return eeprom_size;}
    void erase_eeprom(){eeprom.fill(0xff);}
    const Eeprom& eeprom_data() const {return eeprom;}
    void set_eeprom(const Eeprom& data){eeprom=data;}
    void format(Ram& ram){
        if(!(ram[63]&1))return;
        reset();unsigned at=0,channel=0;
        while(at<63 && channel<5){const unsigned marker=ram[at];
            if(marker==254)break;
            if(marker==255){++at;continue;}
            if(marker==0 || marker==253){channels[channel++]=marker==253?65:64;++at;continue;}
            if(at+2>=63)break;
            const unsigned bytes=2+(marker&63)+(ram[at+1]&63);
            if(at+bytes>63){ram[at+1]=uint8_t((ram[at+1]&63)|64);break;}
            channels[channel++]=at;at+=bytes;
        }
        ram[63]&=~uint8_t(1);
    }
    void poll(Ram& ram,const std::array<uint32_t,4>& input) {
        for(unsigned port=0;port<5;++port){const auto at=unsigned(channels[port]);if(at>=64)continue;
            const unsigned raw=ram[at];if(raw&192)continue; // Skip/reset flags, no digital side effects.
            const unsigned tx=raw&63,rx=ram[at+1]&63,reply=at+2+tx;
            ram[at+1]=uint8_t(rx);
            if(!tx || reply+rx>63){ram[at+1]|=64;continue;}
            const unsigned command=ram[at+2];
            if(port==4){
                if(!has_eeprom()){ram[at+1]|=128;continue;}
                if(command==0 || command==255){
                    if(tx!=1 || rx!=3){ram[at+1]|=64;continue;}
                    ram[reply]=0;ram[reply+1]=eeprom_size==2048?0xc0:0x80;ram[reply+2]=0;continue;
                }
                if(command==4){
                    if(tx!=2 || rx!=8 || unsigned(ram[at+3])*8>=eeprom_size){ram[at+1]|=64;continue;}
                    std::copy_n(eeprom.begin()+unsigned(ram[at+3])*8,8,ram.begin()+reply);
                    continue;
                }
                if(command==5){
                    if(tx!=10 || rx!=1 || unsigned(ram[at+3])*8>=eeprom_size){ram[at+1]|=64;continue;}
                    std::copy_n(ram.begin()+at+4,8,eeprom.begin()+unsigned(ram[at+3])*8);
                    ram[reply]=0;continue;
                }
                ram[at+1]|=128;continue;
            }
            if(command!=0 && command!=255 && command!=1){ram[at+1]|=128;continue;}
            if(tx!=1 || rx!=(command==1?4u:3u)){ram[at+1]|=64;continue;}
            if(command!=1){ram[reply]=5;ram[reply+1]=0;ram[reply+2]=2;continue;}
            const unsigned buttons=wire(input[port]);ram[reply]=uint8_t(buttons>>8);ram[reply+1]=uint8_t(buttons);
            ram[reply+2]=uint8_t(emu::n64_input::axis(input[port],0));
            ram[reply+3]=uint8_t(emu::n64_input::axis(input[port],1));
        }
    }
    State snapshot() const {return channels;}
    bool restore(const State& state){
        unsigned next=0;
        for(const auto at:state){if(at==64 || at==65)continue;if(at>60 || at<next)return false;next=unsigned(at)+2;}
        channels=state;return true;
    }
private:
    State channels{};
    Eeprom eeprom{};
    size_t eeprom_size=0;
    static unsigned wire(uint32_t buttons){
        buttons=emu::n64_input::buttons(buttons);
        using emu::VirtualButton;
        constexpr std::array<VirtualButton,14> controls={VirtualButton::A,VirtualButton::B,VirtualButton::Z,VirtualButton::Start,VirtualButton::Up,VirtualButton::Down,VirtualButton::Left,VirtualButton::Right,VirtualButton::L,VirtualButton::R,VirtualButton::CUp,VirtualButton::CDown,VirtualButton::CLeft,VirtualButton::CRight};
        constexpr std::array<unsigned,14> masks={32768,16384,8192,4096,2048,1024,512,256,32,16,8,4,2,1};
        unsigned value=0;for(unsigned n=0;n<14;++n)if(emu::is_button_pressed(buttons,controls[n]))value|=masks[n];return value;
    }
};
}
