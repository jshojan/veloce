#include "machine.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#define check(x) do {if(!(x)){std::cerr<<"N64 RTC failure at line "<<__LINE__<<'\n';return 1;}}while(false)
static n64::PifJoybus::Ram packet(n64::PifJoybus& device,uint8_t command,uint8_t block=0,
                                  const std::array<uint8_t,8>& payload={}){
    n64::PifJoybus::Ram ram{};
    const unsigned tx=command==8?10:command==7?2:1;
    const unsigned rx=command==8?1:command==7?9:3;
    ram[4]=uint8_t(tx);ram[5]=uint8_t(rx);ram[6]=command;
    if(tx>1)ram[7]=block;
    if(command==8)std::copy(payload.begin(),payload.end(),ram.begin()+8);
    ram[6+tx+rx]=0xfe;ram[63]=1;
    device.format(ram);device.poll(ram,{});
    return ram;
}
int main(int argc,char** argv){
    check(argc==2);
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    check(image.size()==0x1004);
    std::copy_n("NAF",3,image.begin()+0x3b);
    n64::Machine machine;
    check(machine.load(image.data(),image.size()) && machine.flash.enabled && machine.joybus.rtc.present);
    auto& rtc=machine.joybus.rtc;
    auto status=packet(machine.joybus,6);
    check(status[5]==3 && status[7]==0 && status[8]==0x10 && status[9]==0);
    auto initial=packet(machine.joybus,7,2);
    check(initial[5]==9 && initial[8]==0 && initial[9]==0 && initial[10]==0x80 &&
          initial[11]==1 && initial[12]==6 && initial[13]==1 && initial[14]==0 && initial[15]==1);
    constexpr std::array<uint8_t,8> leap={0x59,0x59,0xa3,0x28,0x03,0x02,0x24,0x01};
    auto write=packet(machine.joybus,8,2,leap);
    check(write[5]==1 && write[16]==0 && rtc.valid_time());
    rtc.advance(1);
    auto next=packet(machine.joybus,7,2);
    check(next[8]==0 && next[9]==0 && next[10]==0x80 && next[11]==0x29 &&
          next[12]==4 && next[13]==2 && next[14]==0x24 && next[15]==1);
    rtc.advance(86400);
    next=packet(machine.joybus,7,2);
    check(next[11]==1 && next[12]==5 && next[13]==3);
    n64::CartridgeRtc calendar;calendar.configure(true);
    calendar.ram[16]=0x59;calendar.ram[17]=0x59;calendar.ram[18]=0xa3;
    calendar.ram[19]=0x28;calendar.ram[21]=2;calendar.ram[22]=0;calendar.ram[23]=2;
    check(calendar.valid_time());calendar.advance(1);
    check(calendar.ram[19]==1 && calendar.ram[21]==3); // 2100 is not leap.
    calendar.ram[16]=0x59;calendar.ram[17]=0x59;calendar.ram[18]=0xa3;
    calendar.ram[19]=0x28;calendar.ram[21]=2;calendar.ram[23]=5;
    check(calendar.valid_time());calendar.advance(1);
    check(calendar.ram[19]==0x29 && calendar.ram[21]==2); // 2400 is leap.
    std::array<uint8_t,8> control{};control[1]=4;
    check(packet(machine.joybus,8,0,control)[16]==0x80);
    rtc.advance(86400);check(packet(machine.joybus,7,2)[11]==1);
    control[1]=0;control[0]=2;
    check(packet(machine.joybus,8,0,control)[16]==0);
    check(packet(machine.joybus,8,2,leap)[5]==(1|64) && packet(machine.joybus,7,2)[11]==1);
    control[0]=0;check(packet(machine.joybus,8,0,control)[16]==0);
    machine.reset();check(machine.joybus.rtc.ram[19]==1 && machine.joybus.rtc.ram[21]==3);
    rtc.clocks=n64::CartridgeRtc::clocks_per_second-1;
    std::vector<uint8_t> snapshot,replay;
    check(machine.save(snapshot) && snapshot[7]=='8' && snapshot.size()==n64::Machine::rtc_state_size);
    rtc.tick();check(rtc.ram[16]==1 && machine.restore(snapshot));
    check(machine.save(replay) && replay==snapshot && rtc.clocks==n64::CartridgeRtc::clocks_per_second-1);
    auto bad=snapshot;bad[bad.size()-9]=0xff;
    check(!machine.restore(bad) && machine.save(replay) && replay==snapshot);
    auto previous=snapshot;previous.resize(previous.size()-48);previous[7]='7';
    check(machine.restore(previous) && machine.joybus.rtc.ram[19]==1 &&
          machine.joybus.rtc.ram[21]==1 && machine.restore(snapshot));
    std::cout<<"N64 cartridge RTC Joybus status/read/write, leap rollover, locks, stop and S038 rollback PASS\n";
}
