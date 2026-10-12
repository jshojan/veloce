#include "machine.hpp"
#include "save_media.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#define check(x) do {if(!(x)){std::cerr<<"N64 SRAM failure at line "<<__LINE__<<'\n';return 1;}}while(false)
static constexpr uint64_t pi=0xffffffffa4600000ull;
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
    for(const char* id:{"NTE","NFZ","CZL","NRE","NWX"}){
        std::copy_n(id,3,header.begin()+0x3b);
        check(n64::native_sram_bytes(header.data())==32768);
    }
    std::copy_n("CDZ",3,header.begin()+0x3b);
    check(n64::native_sram_bytes(header.data())==98304);
    std::copy_n("NSM",3,header.begin()+0x3b);
    check(n64::native_sram_bytes(header.data())==0);
    for(const size_t size:{size_t(32768),size_t(98304)}){
        std::copy_n(size==32768?"S32K":"S96K",4,rom.begin()+0x3c);
        n64::Machine m;check(m.load(rom.data(),rom.size()));
        check(m.sram.size()==size && std::all_of(m.sram.begin(),m.sram.end(),[](uint8_t b){return b==255;}));
        check(!m.joybus.has_eeprom());
        for(size_t offset=0;offset<size;offset+=64){
            for(unsigned i=0;i<64;++i)m.ram[0x100+i]=uint8_t(offset*17+i*37+(offset>>8));
            check(transfer(m,0x100,unsigned(offset),64,true));
            check(m.pi_interrupt && m.pi[3]==127 && m.pi[0]==0x140 &&
                  m.pi[1]==0x08000000u+offset+64);
            check(m.write(pi+16,4,2) && !m.pi_interrupt);
            std::fill_n(m.ram.begin()+0x200,64,0);
            check(transfer(m,0x200,unsigned(offset),64,false));
            for(unsigned i=0;i<64;++i)
                check(m.ram[0x200+i]==uint8_t(offset*17+i*37+(offset>>8)));
        }
        check(m.write(pi+16,4,2));
        check(!transfer(m,0x100,unsigned(size),64,true));
        const auto bank=m.sram;
        check(m.write(pi,4,0x100) && m.write(pi+4,4,0x08000000u+size-32));
        const auto pi_before=m.pi;
        check(!m.write(pi+8,4,63) && m.sram==bank && m.pi==pi_before && !m.pi_interrupt);
        check(!m.write(pi+8,4,62) && m.sram==bank && m.pi==pi_before && !m.pi_interrupt);
        check(m.write(0xffffffffa8000000ull,4,0x12345678));
        uint64_t direct=0;
        check(m.read(0xffffffffa8000000ull,4,direct) && direct==0x12345678);
        check(!m.write(0xffffffffa8000001ull,4,0) && !m.write(0xffffffffa8000000ull,2,0));
        std::vector<uint8_t> saved,replay;
        check(m.save(saved) && saved.size()==n64::Machine::rumble_state_size+size+8 && saved[7]=='6');
        m.sram[0]=0;m.reset();
        check(m.sram[0]==0 && m.restore(saved) && m.sram[0]==0x12 && m.save(replay) && replay==saved);
        auto bad=saved;bad.back()^=1;
        check(!m.restore(bad) && m.save(replay) && replay==saved);
        bad=saved;bad[bad.size()-9]^=1;
        check(m.restore(bad) && m.sram.back()!=saved[saved.size()-9]);
        check(m.restore(saved));
    }
    std::cout<<"N64 32/96 KiB SRAM PI write/read, direct I/O, S036 rollback and media IDs PASS\n";
}
