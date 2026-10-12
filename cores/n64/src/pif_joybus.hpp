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
// and a cartridge EEPROM. Each port can hold a 32 KiB Controller Pak.
// Physical serial timing and other accessory types are not modeled.
class PifJoybus {
public:
    using Ram=std::array<uint8_t,64>;
    using State=std::array<uint64_t,5>;
    using Eeprom=std::array<uint8_t,2048>;
    using PakStorage=std::array<uint8_t,4*32768>;
    PifJoybus(){erase_eeprom();format_paks();reset();}
    void reset(){channels.fill(64);}
    void configure_pak_mask(uint8_t mask){
        mask&=15;
        if(mask)pak_state_active=true;
        pak_latch|=uint8_t(mask&~pak_mask);
        pak_mask=mask;
        pak_latch&=mask;
    }
    uint8_t inserted_paks() const {return pak_mask;}
    uint8_t pending_pak_changes() const {return pak_latch;}
    bool has_pak_state() const {return pak_state_active;}
    bool restore_pak_flags(uint8_t mask,uint8_t latch){
        if((mask|latch)>15 || (latch&~mask))return false;
        pak_mask=mask;pak_latch=latch;
        if(mask)pak_state_active=true;
        return true;
    }
    const PakStorage& pak_storage() const {return paks;}
    void set_pak_storage(const PakStorage& data){paks=data;pak_state_active=true;}
    void format_paks(){
        paks.fill(0);
        pak_state_active=false;
        for(unsigned port=0;port<4;++port){
            const size_t base=port*32768;
            // Four redundant ID sectors and two inode tables. Fixed serials
            // make fresh Paks deterministic for netplay and state replay.
            for(const unsigned sector:{1u,3u,4u,6u}){
                const size_t at=base+sector*0x20;
                paks[at+1]=uint8_t(0x21+port);
                paks[at+4]=0x56;paks[at+5]=0x4c;paks[at+6]=0x4f;paks[at+7]=uint8_t(0x30+port);
                paks[at+8]=0x50;paks[at+9]=0x41;paks[at+10]=0x4b;paks[at+11]=uint8_t(0x31+port);
                paks[at+0x19]=1;paks[at+0x1a]=1;
                uint16_t sum=0,inverted=0;
                for(unsigned n=0;n<14;++n){
                    const uint16_t word=uint16_t(paks[at+n*2])<<8|paks[at+n*2+1];
                    sum=uint16_t(sum+word);inverted=uint16_t(inverted+uint16_t(~word));
                }
                paks[at+0x1c]=uint8_t(sum>>8);paks[at+0x1d]=uint8_t(sum);
                paks[at+0x1e]=uint8_t(inverted>>8);paks[at+0x1f]=uint8_t(inverted);
            }
            for(const unsigned page:{1u,2u}){
                unsigned checksum=0;
                for(unsigned slot=5;slot<128;++slot){paks[base+page*256+slot*2+1]=3;checksum+=3;}
                paks[base+page*256+1]=uint8_t(checksum);
            }
        }
    }
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
            if(command==2 || command==3){
                if(tx!=(command==2?3u:35u) || rx!=(command==2?33u:1u)){
                    ram[at+1]|=64;continue;
                }
                const uint16_t address=uint16_t(ram[at+3])<<8|ram[at+4];
                const uint16_t offset=address&~uint16_t(31);
                const bool available=(pak_mask&(1u<<port)) && !(pak_latch&(1u<<port)) &&
                    (address&31)==accessory_address_crc(offset);
                if(command==2){
                    if(available && offset<32768)
                        std::copy_n(paks.begin()+port*32768+offset,32,ram.begin()+reply);
                    else std::fill_n(ram.begin()+reply,32,0);
                    ram[reply+32]=uint8_t(accessory_data_crc(ram.data()+reply)^(available?0:0xff));
                } else {
                    if(available && offset<32768)
                        std::copy_n(ram.begin()+at+5,32,paks.begin()+port*32768+offset);
                    ram[reply]=uint8_t(accessory_data_crc(ram.data()+at+5)^(available?0:0xff));
                }
                continue;
            }
            if(command!=0 && command!=255 && command!=1){ram[at+1]|=128;continue;}
            if(tx!=1 || rx!=(command==1?4u:3u)){ram[at+1]|=64;continue;}
            if(command!=1){
                ram[reply]=5;ram[reply+1]=0;
                ram[reply+2]=(pak_mask&(1u<<port))?(pak_latch&(1u<<port))?3:1:2;
                pak_latch&=uint8_t(~(1u<<port));
                continue;
            }
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
    static uint8_t accessory_address_crc(uint16_t address){
        constexpr uint8_t bits[]={0,0,0,0,0,0x15,0x1f,0x0b,
            0x16,0x19,0x07,0x0e,0x1c,0x0d,0x1a,0x01};
        uint8_t crc=0;
        for(unsigned n=5;n<16;++n)if(address&(1u<<n))crc^=bits[n];
        return crc;
    }
    static uint8_t accessory_data_crc(const uint8_t* data){
        uint8_t crc=0;
        for(unsigned byte=0;byte<32;++byte)
            for(int bit=7;bit>=0;--bit){
                const bool carry=crc&0x80;
                crc=uint8_t((crc<<1)|((data[byte]>>bit)&1));
                if(carry)crc^=0x85;
            }
        for(unsigned bit=0;bit<8;++bit){
            const bool carry=crc&0x80;
            crc=uint8_t(crc<<1);
            if(carry)crc^=0x85;
        }
        return crc;
    }
    State channels{};
    Eeprom eeprom{};
    PakStorage paks{};
    uint8_t pak_mask=0,pak_latch=0;
    bool pak_state_active=false;
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
