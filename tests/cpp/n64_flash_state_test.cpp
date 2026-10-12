#include "machine.hpp"
#include "save_media.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#define check(x) do {if(!(x)){std::cerr<<"N64 FlashRAM failure at line "<<__LINE__<<'\n';return 1;}}while(false)
static constexpr uint64_t pi=0xffffffffa4600000ull,command=0xffffffffa8010000ull;
static bool transfer(n64::Machine& m,unsigned ram,unsigned cart,unsigned bytes,bool write){
    return m.write(pi,4,ram) && m.write(pi+4,4,0x08000000u+cart) &&
           m.write(pi+(write?8:12),4,bytes-1);
}
int main(int argc,char** argv){
    check(argc==2);
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
    check(rom.size()>0x1004);
    std::array<uint8_t,64> header{};
    for(const char* id:{"NCC","NAF","NZS","NPF"}){
        std::copy_n(id,3,header.begin()+0x3b);
        check(n64::native_flash_model(header.data())==1);
    }
    for(const char* id:{"NMQ","NPO","NP3","NW4"}){
        std::copy_n(id,3,header.begin()+0x3b);
        check(n64::native_flash_model(header.data())==0);
    }
    std::copy_n("NSM",3,header.begin()+0x3b);
    check(n64::native_flash_model(header.data())==-1);
    std::copy_n("F128",4,rom.begin()+0x3c);
    n64::Machine m;check(m.load(rom.data(),rom.size()));
    check(m.flash.enabled && m.sram.empty() && !m.joybus.has_eeprom());
    check(m.write(command,4,0xe1000000) && transfer(m,0x100,0,8,false));
    const std::array<uint8_t,8> id={0x11,0x11,0x80,0x01,0,0xc2,0,0x1d};
    check(std::equal(id.begin(),id.end(),m.ram.begin()+0x100));
    unsigned graded=0;
    for(unsigned page=0;page<1024;++page){
        for(unsigned lane=0;lane<128;++lane)m.ram[0x100+lane]=uint8_t(page*17+lane*37+(page>>8));
        check(m.write(command,4,0xb4000000) && transfer(m,0x100,0,128,true));
        check(m.write(command,4,0xa5000000|page) && m.flash.status&4);
        check(m.write(command,4,0xf0000000));
        const unsigned offset=page*128;
        check(transfer(m,0x200,offset,128,false));
        for(unsigned lane=0;lane<128;++lane)
            check(m.ram[0x200+lane]==uint8_t(page*17+lane*37+(page>>8)));
        ++graded;
    }
    const auto bank=m.flash.data;
    check(!m.write(command,4,0xa5000400) && m.flash.data==bank);
    check(!transfer(m,0x100,0,128,true) && m.flash.data==bank);
    for(unsigned sector=0;sector<8;++sector){
        const unsigned base=sector*0x4000;
        check(m.write(command,4,0x4b000000|(sector*128+1)) &&
              m.write(command,4,0x78000000) && (m.flash.status&8) &&
              m.flash.mode==n64::FlashRam::Mode::Status);
        check(std::all_of(m.flash.data.begin()+base,m.flash.data.begin()+base+0x4000,
                          [](uint8_t byte){return byte==255;}));
        if(sector<7)check(m.flash.data[base+0x4000]==bank[base+0x4000]);
    }
    m.ram[0x100]=0;
    check(m.write(command,4,0xb4000000) && transfer(m,0x100,0,128,true) &&
          m.write(command,4,0xa5000000) && m.flash.data[0]==0);
    check(m.write(command,4,0x3c000000) && m.write(command,4,0x78000000) &&
          std::all_of(m.flash.data.begin(),m.flash.data.end(),[](uint8_t b){return b==255;}));
    for(unsigned lane=0;lane<128;++lane)m.ram[0x100+lane]=uint8_t(lane*19);
    check(m.write(command,4,0xb4000000) && transfer(m,0x100,0,128,true));
    std::vector<uint8_t> saved,replay;
    check(m.save(saved) && saved[7]=='7' && saved.size()==n64::Machine::flash_state_size);
    m.reset();check(m.flash.mode==n64::FlashRam::Mode::Array && m.restore(saved));
    check(m.save(replay) && replay==saved && m.write(command,4,0xa5000005));
    for(unsigned lane=0;lane<128;++lane)check(m.flash.data[5*128+lane]==uint8_t(lane*19));
    check(m.restore(saved));
    auto bad=saved;bad[bad.size()-9]=0xff;
    check(!m.restore(bad) && m.save(replay) && replay==saved);
    m.reset();check(m.flash.data[5*128]==255);
    std::copy_n("FOLD",4,rom.begin()+0x3c);
    n64::Machine old;check(old.load(rom.data(),rom.size()) && old.flash.model==n64::FlashRam::Model::Old);
    old.ram[0x100]=0x42;
    check(old.write(command,4,0xb4000000) && transfer(old,0x100,0,128,true) &&
          old.write(command,4,0xa5000200) && old.write(command,4,0xf0000000) &&
          transfer(old,0x200,0x8000,128,false) && old.ram[0x200]==0x42);
    std::cout<<graded<<" FlashRAM pages, ID, program, sector/chip erase, S037 rollback and old model address PASS\n";
}
