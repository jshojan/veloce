#include "pif_joybus.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#define check(...) do { if(!(__VA_ARGS__)){std::cerr<<"Controller Pak mismatch at line "<<__LINE__<<"\n";std::exit(1);} } while(false)

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
                                   unsigned address,const std::array<uint8_t,32>& payload={}){
    n64::PifJoybus::Ram ram{};ram.fill(0xaa);
    for(unsigned n=0;n<port;++n)ram[n]=0;
    const unsigned tx=command==2?3:command==3?35:1;
    const unsigned rx=command==2?33:command==3?1:3;
    ram[port]=uint8_t(tx);ram[port+1]=uint8_t(rx);ram[port+2]=uint8_t(command);
    if(command==2 || command==3){ram[port+3]=uint8_t(address>>8);ram[port+4]=uint8_t(address);}
    if(command==3)std::copy(payload.begin(),payload.end(),ram.begin()+port+5);
    ram[port+2+tx+rx]=254;ram[63]=1;
    device.format(ram);device.poll(ram,{});
    check(ram[port+1]==rx && ram[63]==0);
    return ram;
}
int main(){
    n64::PifJoybus device;
    const auto blank=device.pak_storage();
    check(blank.size()==4*32768);
    uint64_t graded=0;
    for(unsigned port=0;port<4;++port){
        device.configure_pak_mask(uint8_t(1u<<port));
        auto status=request(device,port,0,0);
        check(status[port+3]==5 && status[port+4]==0 && status[port+5]==3);
        status=request(device,port,0,0);
        check(status[port+5]==1);
        for(unsigned block=0;block<1024;++block){
            const unsigned address=block*32|address_crc(block*32);
            std::array<uint8_t,32> payload{};
            for(unsigned lane=0;lane<32;++lane)payload[lane]=uint8_t(block*37+lane*53+port*11);
            auto write=request(device,port,3,address,payload);
            check(write[port+37]==data_crc(payload.data()));
            auto read=request(device,port,2,address);
            check(std::equal(payload.begin(),payload.end(),read.begin()+port+5) &&
                  read[port+37]==data_crc(payload.data()));
            graded+=2;
        }
        for(unsigned block=0;block<1024;++block){
            const unsigned address=block*32|address_crc(block*32);
            auto read=request(device,port,2,address);
            for(unsigned lane=0;lane<32;++lane)
                check(read[port+5+lane]==uint8_t(block*37+lane*53+port*11));
            ++graded;
        }
        std::array<uint8_t,32> payload{};payload.fill(0x5a);
        const auto before=device.pak_storage();
        auto bad=request(device,port,3,(64|address_crc(64))^1,payload);
        check(bad[port+37]==(data_crc(payload.data())^255) &&
              device.pak_storage()==before);
        device.configure_pak_mask(0);
        auto absent=request(device,port,2,64|address_crc(64));
        check(absent[port+37]==255 &&
              std::all_of(absent.begin()+port+5,absent.begin()+port+37,
                          [](uint8_t byte){return byte==0;}));
        device.reset();
        check(device.pak_storage()==before);
    }
    check(!device.restore_pak_flags(16,0) && !device.restore_pak_flags(1,2));
    check(graded==12288);
    std::cout<<graded<<" Controller Pak writes, immediate reads and full-bank rereads across four ports PASS\n";
}
