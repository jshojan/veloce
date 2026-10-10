#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <iomanip>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ifstream file(argv[1],std::ios::binary);if(!file.good())return 2;
    const std::vector<uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    n64::Machine machine;const bool loaded=machine.load(image.data(),image.size());
    if(!loaded) {std::cout<<"{\"loaded\":false}\n";return 0;}
    bool fault=false;
    for(unsigned n=0;n<12000000 && !machine.done;++n) {
        machine.cpu.step();
        if(machine.cpu.state.cp0[12]&2) {fault=true;break;}
    }
    const auto& s=machine.cpu.state;
    uint64_t instruction=0;const bool readable=fault && machine.read(s.cp0[14],4,instruction);
    uint64_t fetched=instruction;bool fetched_valid=readable && !(s.cp0[14]&3);
    if(fault && (s.cp0[16]&7)!=2 && uint32_t(s.cp0[14])<0xa0000000u) {
        const uint32_t physical=uint32_t(s.cp0[14])&0x1fffffffu;
        const auto& line=machine.cpu.caches.instruction[(physical>>5)&511];
        fetched_valid=!(physical&3) && (line.flags&1) && line.page==(physical>>12);
        fetched=0;if(fetched_valid)for(unsigned n=0;n<4;++n)fetched=(fetched<<8)|line.data[(physical&31)+n];
    }
    std::cout<<"{\"loaded\":true,\"boot_mode\":"<<unsigned(machine.boot_mode)
             <<",\"attempts\":"<<s.instructions<<",\"exception_observed\":"<<(fault?"true":"false")
             <<",\"fault_backing_readable\":"<<(readable?"true":"false")<<",\"fault_backing_word\":\""<<std::hex<<instruction<<std::dec<<"\""
             <<",\"fault_instruction_valid\":"<<(fetched_valid?"true":"false")<<",\"fault_instruction_word\":\""<<std::hex<<fetched<<std::dec<<"\""
             <<",\"done\":"<<(machine.done?"true":"false")<<",\"pc\":\""<<std::hex<<s.pc
             <<"\",\"epc\":\""<<s.cp0[14]<<"\",\"cause\":\""<<s.cp0[13]<<"\",\"status\":\""<<s.cp0[12]
             <<"\",\"bad_address\":\""<<s.cp0[8]<<"\",\"gpr\":[";
    for(unsigned n=0;n<32;++n) {if(n)std::cout<<',';std::cout<<'"'<<s.gpr[n]<<'"';}
    std::cout<<"],\"retail_boot_verified\":false,\"playability_verified\":false}\n";
    return 0;
}
