#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
using emulator_test::require;
static uint32_t get(n64::Machine& m,uint32_t p) {uint64_t v=0;require(m.read(n64::sign32(0xa0000000u|p),4,v),"read MMIO");return uint32_t(v);}
static bool put(n64::Machine& m,uint32_t p,uint32_t v) {return m.write(n64::sign32(0xa0000000u|p),4,v);}
static std::vector<uint8_t> read_file(const char* path) {std::ifstream f(path,std::ios::binary);require(f.good(),"fixture missing");return {std::istreambuf_iterator<char>(f),{}};}
static uint8_t pattern(uint32_t n) {return uint8_t(91+37*(n&255)+11*(n>>8));}
static void submit(n64::Machine& m,uint32_t destination,uint32_t source,uint32_t length) {
    require(put(m,0x04600000,destination) && put(m,0x04600004,source),"DMA setup");
    require(put(m,0x0460000c,length-1),"valid DMA rejected");
}
int main(int argc,char** argv) {
    try {
        require(argc==4,"core, homebrew and original IPL fixtures required");
        auto image=read_file(argv[2]);n64::Machine m;require(m.load(image.data(),image.size()),"homebrew load");
        // Replace immutable source only in this component fixture, then exercise
        // every RDRAM halfword destination with an address-dependent source.
        m.rom.resize(8*1024*1024);
        for(uint32_t n=0;n<m.rom.size();++n)m.rom[n]=pattern(n);
        uint64_t destinations=0,lengths=0,masks=0;
        for(uint32_t destination=0;destination<m.ram.size();destination+=8) {
            const uint32_t source=(destination*13)&0x7ffffe;
            submit(m,destination,0x10000000+source,2);
            require(m.ram[destination]==pattern(source) && m.ram[destination+1]==pattern(source+1),"RDRAM coverage/source mapping");
            require(get(m,0x04600000)==destination+8 && get(m,0x04600004)==0x10000002+source && get(m,0x0460000c)==127,"post-DMA registers");++destinations;
        }
        for(uint32_t length=2;length<=65536;length+=2) {
            submit(m,0x700008,0x1001fffe,length);
            const uint32_t source=0x1fffe;
            require(m.ram[0x700008]==pattern(source) && m.ram[0x700008+length-1]==pattern(source+length-1),"all low16 even length payload endpoints");
            require(get(m,0x04600000)==((0x700008+length+7)&~7u) && get(m,0x04600004)==0x1001fffe + length && get(m,0x0460000c)==127,"length advancement");++lengths;
        }
        // Full 8 MiB transfer: inspect every byte, not just endpoints/guest samples.
        submit(m,0,0x10000000,8*1024*1024);
        require(m.ram==m.rom,"full RDRAM DMA payload mismatch");
        for(uint32_t value=0;value<65536;++value) {
            require(put(m,0x04600000,0xff000000u|value) && get(m,0x04600000)==(value&0xfffe),"DRAM address mask");
            require(put(m,0x04600004,0xffff0000u|value) && get(m,0x04600004)==(0xffff0000u|(value&0xfffe)),"cart address mask");++masks;
        }
        for(unsigned index=5;index<=12;++index) for(unsigned value=0;value<256;++value) {
            constexpr unsigned widths[]={8,8,4,2,8,8,4,2};
            const auto mask=(1u<<widths[index-5])-1;
            require(put(m,0x04600000+index*4,0xffffff00u|value) && get(m,0x04600000+index*4)==(value&mask),"domain timing register mask");
        }
        for(unsigned si=0;si<2;++si) for(unsigned pi=0;pi<2;++pi) for(unsigned mi=0;mi<64;++mi) {
            m.si_interrupt=si;m.pi_interrupt=pi;m.mi_mask=mi;
            require(get(m,0x04300008)==(si*2u|pi*16u),"MI pending independent sources");
            require(m.interrupt_pending()==((((mi&2)&&si)||((mi&16)&&pi))?4:0),"combined IP2 gate");
            put(m,0x04600010,1);require(m.pi_interrupt==bool(pi),"PI reset incorrectly acknowledged interrupt");
            put(m,0x04600010,2);require(!m.pi_interrupt && m.si_interrupt==bool(si),"PI ack cleared other device");
        }
        m.si_interrupt=false;m.mi_mask=16;submit(m,0x700000,0x10000000,16);
        m.cpu.reset(n64::sign32(0x80001000));m.cpu.state.cp0[12]=0x401;
        require(m.write(n64::sign32(0x80001000),4,0x24031234) &&
                m.write(n64::sign32(0x80000180),4,0x42000018),"interrupt probe instruction placement");
        m.cpu.step();
        require(m.cpu.state.pc==n64::sign32(0x80000180) && m.cpu.state.cp0[14]==n64::sign32(0x80001000) &&
                (m.cpu.state.cp0[13]&0x47c)==0x400 && m.cpu.state.gpr[3]==0,"PI IRQ did not suppress instruction/enter exception");
        put(m,0x04600010,2);m.cpu.step();m.cpu.step();
        require(m.cpu.state.gpr[3]==0x1234 && !(m.cpu.state.cp0[12]&2) && !(m.cpu.state.cp0[13]&0x400),"PI ack/ERET resumption");
        // Rejected transactions must leave all RAM and every device field unchanged.
        m.mi_mask=16;m.si_interrupt=false;m.pi_interrupt=false;
        for(const auto fixture:std::array<std::array<uint32_t,3>,8>{{
            {0,0x10000000,1},{2,0x10000000,2},{0x800000,0x10000000,2},{0x7ffff8,0x10000000,10},
            {0,0x0ffffffe,2},{0,0x10800000,2},{0,0xfffffffe,2},{0,0x107ffffe,4}}}) {
            put(m,0x04600000,fixture[0]);put(m,0x04600004,fixture[1]);
            std::vector<uint8_t> before,after;require(m.save(before),"save before invalid DMA");
            require(!put(m,0x0460000c,fixture[2]-1) && m.save(after) && before==after,"invalid DMA mutated full state");
        }
        for(unsigned width:{1u,2u,8u}) {uint64_t sentinel=0xdeadbeef;require(!m.read(n64::sign32(0xa4600000),width,sentinel) && sentinel==0xdeadbeef && !m.write(n64::sign32(0xa4600000),width,0),"narrow/wide PI MMIO accepted");}
        submit(m,0x700000,0x10000000,16);m.mi_mask=16;
        std::vector<uint8_t> state,after,replay;require(m.save(state),"PI state save");
        put(m,0x04600010,2);submit(m,0x700008,0x10000200,512);m.save(after);
        require(m.restore(state),"PI state restore");put(m,0x04600010,2);submit(m,0x700008,0x10000200,512);m.save(replay);require(after==replay,"pending PI continuation");
        const auto unchanged=[&] {m.save(replay);require(replay==after,"invalid PI state mutation");};
        for(unsigned index=0;index<15;++index) {
            auto bad=after;const size_t pos=bad.size()-480+index*8;
            if(index==1)bad[pos+7]|=1;else bad[pos]=1;
            require(!m.restore(bad),"invalid PI scalar/identity accepted");unchanged();
        }
        auto legacy=after;legacy.resize(legacy.size()-480);legacy[7]='2';require(m.restore(legacy) && !m.pi_interrupt && std::all_of(m.pi.begin(),m.pi.end(),[](uint32_t v){return !v;}),"legacy PI reset");
        const auto bootstrap=read_file(argv[3]);
        require(m.load(bootstrap.data(),bootstrap.size()) && m.boot_mode==n64::Machine::BootMode::OriginalIPL,"original IPL load");
        require(m.cpu.state.pc==n64::sign32(0xa4000040) && m.cpu.state.gpr[22]==63 && m.cpu.state.cp0[12]==0x34000000,"IPL handoff state");
        require(std::equal(bootstrap.begin()+0x40,bootstrap.begin()+0x1000,m.sp.begin()+0x40) &&
                std::all_of(m.ram.begin(),m.ram.end(),[](uint8_t v){return !v;}),"IPL copy / no bulk payload load");
        std::vector<uint8_t> native_state;m.save(native_state);auto invalid=bootstrap;invalid[8]=0;
        require(!m.load(invalid.data(),invalid.size()) && m.save(replay) && replay==native_state,"invalid native image mutated active machine");
        auto large=bootstrap;large.resize(64*1024*1024);require(m.load(large.data(),large.size()),"native64MiB loader");
        large.resize(large.size()+4);require(!m.load(large.data(),large.size()),"over64MiB image accepted");
        require(m.load(bootstrap.data(),bootstrap.size()),"restore original IPL image");
        m.save(state);m.cpu.step();m.save(after);require(m.restore(state),"native state restore");m.cpu.step();m.save(replay);require(after==replay,"native exact step replay");
        auto native_legacy=state;native_legacy.resize(state.size()-480);native_legacy[7]='2';require(!m.restore(native_legacy),"legacy state accepted for native profile");
        for(unsigned byte_order=0;byte_order<3;++byte_order) {
            auto bytes=bootstrap;
            if(byte_order==1)for(size_t n=0;n<bytes.size();n+=2)std::swap(bytes[n],bytes[n+1]);
            if(byte_order==2)for(size_t n=0;n<bytes.size();n+=4) {std::swap(bytes[n],bytes[n+3]);std::swap(bytes[n+1],bytes[n+2]);}
            require(m.load(bytes.data(),bytes.size()) && m.rom==bootstrap && m.boot_mode==n64::Machine::BootMode::OriginalIPL,"native image normalization");
        }
        emulator_test::LoadedCore core(argv[1],bootstrap);core.plugin->run_frame({});require(core.plugin->save_state(state),"loaded native plugin state");
        n64::Machine mirror;require(mirror.load(bootstrap.data(),bootstrap.size()) && mirror.restore(state) && mirror.done,"native actual-plugin execution/complete state");
        std::cout<<destinations<<" RDRAM destinations, "<<lengths<<" even length values, "<<masks<<" address masks and every byte of an 8MiB DMA matched; MI/IRQ, atomic rejection, legacy/native states,64MiB formats and loaded native plugin passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
