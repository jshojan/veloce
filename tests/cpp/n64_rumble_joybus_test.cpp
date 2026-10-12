#include "pif_joybus.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#define check(x) do { if(!(x)){std::cerr<<"Rumble Pak mismatch at line "<<__LINE__<<'\n';std::exit(1);} } while(false)
static unsigned address_crc(unsigned address){
    unsigned crc=0;
    for(int bit=15;bit>=5;--bit){
        const unsigned feedback=((address>>bit)&1)^((crc>>4)&1);
        crc=(crc<<1)&31;
        if(feedback)crc^=0x15;
    }
    return crc;
}
static unsigned data_crc(const uint8_t* data){
    constexpr unsigned table[]={0x89,0x86,0x43,0xe3,0xb3,0x9b,0x8f,0x85};
    unsigned crc=0;
    for(unsigned n=0;n<32;++n){
        const unsigned mixed=crc^data[n];crc=0;
        for(unsigned bit=0;bit<8;++bit)if(mixed&(0x80u>>bit))crc^=table[bit];
    }
    return crc;
}
static n64::PifJoybus::Ram request(n64::PifJoybus& device,unsigned port,unsigned command,
                                   unsigned offset,const std::array<uint8_t,32>& payload={},bool corrupt_crc=false){
    n64::PifJoybus::Ram ram{};ram.fill(0xaa);
    for(unsigned n=0;n<port;++n)ram[n]=0;
    const unsigned tx=command==2?3:command==3?35:1;
    const unsigned rx=command==2?33:command==3?1:3;
    const unsigned at=(offset|address_crc(offset))^(corrupt_crc?1u:0u);
    ram[port]=uint8_t(tx);ram[port+1]=uint8_t(rx);ram[port+2]=uint8_t(command);
    if(command==2 || command==3){ram[port+3]=uint8_t(at>>8);ram[port+4]=uint8_t(at);}
    if(command==3)std::copy(payload.begin(),payload.end(),ram.begin()+port+5);
    ram[port+2+tx+rx]=254;ram[63]=1;
    device.format(ram);device.poll(ram,{});
    check(ram[port+1]==rx && ram[63]==0);
    return ram;
}
int main(){
    n64::PifJoybus device;
    device.configure_rumble_mask(15);
    check(device.inserted_paks()==0 && device.inserted_rumble_paks()==15);
    unsigned graded=0;
    for(unsigned port=0;port<4;++port){
        auto status=request(device,port,0,0);
        check(status[port+5]==3);
        status=request(device,port,0,0);
        check(status[port+5]==1);
        for(unsigned phase=0;phase<3;++phase){
            const unsigned on=phase==1;
            std::array<uint8_t,32> payload{};payload.fill(uint8_t(on?0x03:0x02));
            auto write=request(device,port,3,0xc000,payload);
            check(write[port+37]==data_crc(payload.data()));
            check(bool(device.active_motors()&(1u<<port))==bool(on));
            for(unsigned block=0;block<2048;++block){
                const unsigned offset=block*32;
                auto read=request(device,port,2,offset);
                const uint8_t expected=offset<0x8000?0:offset<0x9000?0x80:on?0xff:0;
                check(std::all_of(read.begin()+port+5,read.begin()+port+37,
                    [&](uint8_t value){return value==expected;}) &&
                    read[port+37]==data_crc(read.data()+port+5));
                ++graded;
            }
        }
        std::array<uint8_t,32> payload{};payload[0]=1;
        auto bad=request(device,port,3,0xc000,payload,true);
        check(bad[port+37]==(data_crc(payload.data())^255) &&
              !(device.active_motors()&(1u<<port)));
    }
    device.configure_pak_mask(1); // Replacing a Rumble Pak stops its motor.
    check(device.inserted_paks()==1 && device.inserted_rumble_paks()==14);
    check(!device.restore_rumble_flags(1,0) && !device.restore_rumble_flags(2,4));
    check(graded==24576);
    std::cout<<graded<<" addressed Rumble Pak read replies, write control and four-port status PASS\n";
}
