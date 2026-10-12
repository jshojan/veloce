#include "machine.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#define check(x) do { if(!(x)){std::cerr<<"Controller Pak state failure at line "<<__LINE__<<'\n';return 1;} } while(false)
int main(int argc,char** argv){
    check(argc==2);
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
    n64::Machine machine;
    check(machine.load(rom.data(),rom.size()));
    std::vector<uint8_t> legacy;
    check(machine.save(legacy) && legacy.size()==n64::Machine::current_state_size);
    check(std::equal(legacy.begin(),legacy.begin()+8,"VN64S033"));
    machine.joybus.configure_pak_mask(5);
    check(machine.joybus.pending_pak_changes()==5);
    auto first=machine.joybus.pak_storage();
    first[0x140]=0x7b;first[2*32768+0x7fe0]=0xc9;
    machine.joybus.set_pak_storage(first);
    std::vector<uint8_t> saved,replay;
    check(machine.save(saved) && saved.size()==n64::Machine::pak_state_size);
    check(std::equal(saved.begin(),saved.begin()+8,"VN64S034"));
    machine.joybus.configure_pak_mask(1);
    auto second=first;second[0x140]=0x33;machine.joybus.set_pak_storage(second);
    check(machine.restore(saved));
    check(machine.joybus.pak_storage()==first && machine.joybus.inserted_paks()==5 &&
          machine.joybus.pending_pak_changes()==5);
    check(machine.save(replay) && replay==saved);
    auto malformed=saved;malformed.back()=0x10;
    check(!machine.restore(malformed) && machine.save(replay) && replay==saved);
    malformed=saved;malformed.pop_back();
    check(!machine.restore(malformed) && machine.save(replay) && replay==saved);
    check(machine.restore(legacy));
    check(machine.joybus.inserted_paks()==5 && machine.joybus.pak_storage()!=first);
    check(machine.joybus.pak_storage()[0x140]==0 && machine.joybus.pak_storage()[2*32768+0x7fe0]==0);
    check(machine.load(rom.data(),rom.size()));
    check(machine.joybus.inserted_paks()==0 && machine.save(replay) && replay==legacy);
    std::cout<<"S034 four-Pak snapshot, rollback, malformed rejection and S033 import PASS\n";
}
