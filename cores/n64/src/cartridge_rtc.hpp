#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace n64 {
// Functional cartridge Joybus RTC. Time advances from emulated CPU clocks;
// snapshots replay deterministically, and battery import can advance offline.
class CartridgeRtc {
public:
    static constexpr uint64_t clocks_per_second=93750000;
    std::array<uint8_t,32> ram{};
    bool present=false;
    uint8_t status=0;
    uint64_t clocks=0;

    void configure(bool inserted){present=inserted;ram.fill(0);status=0;clocks=0;
        ram[18]=0x80;ram[19]=1;ram[20]=6;ram[21]=1;ram[22]=0;ram[23]=1;}
    void reset(){} // Battery and oscillator survive console reset.
    static unsigned decode(uint8_t value){return (value>>4)*10+(value&15);}
    static uint8_t encode(unsigned value){return uint8_t(((value/10)<<4)|(value%10));}
    static bool bcd(uint8_t value){return (value&15)<10 && (value>>4)<10;}
    static unsigned month_days(unsigned month,unsigned year){
        static constexpr unsigned days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
        return days[month]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
    }
    bool valid_time() const {
        for(unsigned at:{16u,17u,19u,20u,21u,22u,23u})if(!bcd(ram[at]))return false;
        if(!bcd(ram[18]&0x7f))return false;
        const unsigned year=1900+decode(ram[22])+100*decode(ram[23]);
        const unsigned month=decode(ram[21]),day=decode(ram[19]);
        return decode(ram[16])<60 && decode(ram[17])<60 && decode(ram[18]&0x7f)<24 &&
               decode(ram[20])<7 && month>=1 && month<=12 && day>=1 &&
               day<=month_days(month,year);
    }
    void advance(uint64_t seconds){
        if(!present || (status&0x80) || !valid_time() || !seconds)return;
        uint64_t total=uint64_t(decode(ram[16]))+60ull*decode(ram[17])+3600ull*decode(ram[18]&0x7f)+seconds;
        uint64_t days=total/86400;total%=86400;
        const unsigned weekday=(decode(ram[20])+days)%7;
        unsigned year=1900+decode(ram[22])+100*decode(ram[23]);
        unsigned month=decode(ram[21]),day=decode(ram[19]);
        while(days){
            const unsigned remaining=month_days(month,year)-day+1;
            if(days<remaining){day+=unsigned(days);break;}
            days-=remaining;day=1;
            if(++month==13){month=1;++year;}
            if(year>11899)return; // Two BCD bytes cannot represent later dates.
        }
        const unsigned hour=unsigned(total/3600),minute=unsigned(total/60%60),second=unsigned(total%60);
        ram[16]=encode(second);ram[17]=encode(minute);ram[18]=uint8_t(encode(hour)|0x80);
        ram[19]=encode(day);ram[20]=encode(weekday);ram[21]=encode(month);
        ram[22]=encode((year-1900)%100);ram[23]=encode((year-1900)/100);
    }
    void tick(){if(!present || (status&0x80))return;
        if(++clocks==clocks_per_second){clocks=0;advance(1);}}
    bool read(unsigned block,uint8_t* output) const {
        if(!present || block>2)return false;
        for(unsigned n=0;n<8;++n)output[n]=ram[block*8+n];
        return true;
    }
    bool write(unsigned block,const uint8_t* input){
        if(!present || block>2 || (block && (ram[0]&(1u<<(block-1)))))return false;
        auto candidate=ram;
        for(unsigned n=0;n<8;++n)candidate[block*8+n]=input[n];
        if(block==2){const auto previous=ram;ram=candidate;
            if(!valid_time()){ram=previous;return false;}clocks=0;}
        else ram=candidate;
        if(block==0)status=uint8_t((status&~0x80u)|((ram[1]&4)?0x80:0));
        return true;
    }
};
}
