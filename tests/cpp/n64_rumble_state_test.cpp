#include "machine.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#define check(x) do { if(!(x)){std::cerr<<"Rumble state mismatch at line "<<__LINE__<<'\n';return 1;} } while(false)
static unsigned crc(unsigned offset){
    unsigned result=0;
    for(int bit=15;bit>=5;--bit){
        const unsigned feedback=((offset>>bit)&1)^((result>>4)&1);
        result=(result<<1)&31;if(feedback)result^=0x15;
    }
    return result;
}
static void packet(n64::Machine& machine,unsigned port,unsigned command){
    n64::PifJoybus::Ram ram{};ram.fill(0xaa);
    for(unsigned i=0;i<port;++i)ram[i]=0;
    if(command==0){ram[port]=1;ram[port+1]=3;ram[port+2]=0;ram[port+6]=254;}
    else {ram[port]=35;ram[port+1]=1;ram[port+2]=3;
        const unsigned address=0xc000|crc(0xc000);
        ram[port+3]=uint8_t(address>>8);ram[port+4]=uint8_t(address);
        ram[port+5]=uint8_t(command==1?1:0);ram[port+38]=254;}
    ram[63]=1;machine.joybus.format(ram);machine.joybus.poll(ram,{});
}
int main(int argc,char** argv){
    check(argc==2);
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
    n64::Machine machine;check(machine.load(rom.data(),rom.size()));
    std::vector<uint8_t> s033,s034,s035,replay;
    check(machine.save(s033) && s033.size()==n64::Machine::current_state_size && s033[7]=='3');
    machine.joybus.configure_pak_mask(1);
    check(machine.save(s034) && s034.size()==n64::Machine::pak_state_size && s034[7]=='4');
    machine.joybus.configure_rumble_mask(5);
    check(machine.joybus.inserted_paks()==0 && machine.joybus.inserted_rumble_paks()==5);
    for(unsigned port:{0u,2u}){packet(machine,port,0);packet(machine,port,1);}
    check(machine.joybus.active_motors()==5);
    check(machine.save(s035) && s035.size()==n64::Machine::rumble_state_size && s035[7]=='5');
    machine.joybus.stop_rumble();
    check(machine.restore(s035) && machine.joybus.active_motors()==5 && machine.save(replay) && replay==s035);
    auto bad=s035;bad.back()=0x10;
    check(!machine.restore(bad) && machine.save(replay) && replay==s035);
    bad=s035;bad[bad.size()-8+6]=0x10;
    check(!machine.restore(bad) && machine.save(replay) && replay==s035);
    bad=s035;bad.pop_back();
    check(!machine.restore(bad) && machine.save(replay) && replay==s035);
    machine.reset();
    check(machine.joybus.active_motors()==0 && machine.joybus.inserted_rumble_paks()==5);
    check(machine.restore(s034) && machine.joybus.inserted_paks()==1 &&
          machine.joybus.inserted_rumble_paks()==0 && machine.save(replay) && replay==s034);
    machine.joybus.configure_rumble_mask(5);
    check(machine.restore(s033) && machine.joybus.inserted_rumble_paks()==5 &&
          machine.joybus.active_motors()==0);
    std::cout<<"S035 Rumble Pak motor rollback, reset, malformed-state rejection and S033/S034 import PASS\n";
}
