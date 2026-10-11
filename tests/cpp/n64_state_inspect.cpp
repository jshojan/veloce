#include "machine.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>
static bool parse(const char* text,uint32_t& value,int base){
    const std::string word=text;const auto result=std::from_chars(word.data(),word.data()+word.size(),value,base);
    return result.ec==std::errc{} && result.ptr==word.data()+word.size();
}
// Read-only file diagnostic: restore a private checkpoint, observe physical RAM
// and the CPU cache view on a disposable machine. Never executes guest code or
// writes the input state file. No retail addresses or verdicts are built in.
int main(int argc,char** argv){
    if(argc!=5)return 2;
    uint32_t start=0,count=0;
    if(!parse(argv[3],start,16) || !parse(argv[4],count,10) || start%4 || !count || count>512 || start>=n64::Machine::ram_size || uint64_t(start)+count*4>n64::Machine::ram_size)return 2;
    std::ifstream rom_file(argv[1],std::ios::binary),state_file(argv[2],std::ios::binary);if(!rom_file || !state_file)return 2;
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(rom_file),{}},state{std::istreambuf_iterator<char>(state_file),{}};
    n64::Machine machine;if(!machine.load(rom.data(),rom.size()) || !machine.restore(state))return 2;
    std::cout<<"{\"frames\":"<<machine.frames<<",\"cpu_pc\":"<<machine.cpu.state.pc
        <<",\"instructions\":"<<machine.cpu.state.instructions<<",\"vi_fields\":"<<machine.video_interface.fields
        <<",\"vi_current\":"<<machine.video_interface.current<<",\"vi_phase\":"<<machine.video_interface.phase
        <<",\"scheduling\":\""<<(machine.frame_scheduling==emu::FrameScheduling::VideoField?"video-field":"instruction-quantum")<<'"'
        <<",\"rsp_blocked\":"<<(machine.rsp.blocked?"true":"false")
        <<",\"dp_blocked\":"<<(machine.dp_interface.blocked?"true":"false")<<",\"words\":[";
    for(unsigned n=0;n<count;++n){const uint32_t at=start+n*4;uint32_t physical=0;
        for(unsigned i=0;i<4;++i)physical=physical*256+machine.ram[at+i];
        const uint64_t address=n64::sign32(0x80000000u|at);
        const auto translation=machine.cpu.tlb.translate(address,uint32_t(machine.cpu.state.cp0[12]),uint8_t(machine.cpu.state.cp0[10]),uint32_t(machine.cpu.state.cp0[16]),false);
        uint64_t cpu_value=0;
        if(translation.fault!=n64::Tlb::Fault::none || !machine.cpu.caches.read_translated(machine,address,translation.physical,translation.cached,4,cpu_value,false))return 2;
        if(n)std::cout<<',';
        std::cout<<"{\"address\":"<<at<<",\"ram\":"<<physical<<",\"cpu_view\":"<<cpu_value<<'}';
    }
    std::cout<<"]}\n";
}
