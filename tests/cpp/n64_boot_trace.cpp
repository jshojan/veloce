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
    std::cout<<"{\"loaded\":true,\"boot_mode\":"<<unsigned(machine.boot_mode)
             <<",\"attempts\":"<<s.instructions<<",\"exception_observed\":"<<(fault?"true":"false")
             <<",\"done\":"<<(machine.done?"true":"false")<<",\"pc\":\""<<std::hex<<s.pc
             <<"\",\"epc\":\""<<s.cp0[14]<<"\",\"cause\":\""<<s.cp0[13]<<"\",\"status\":\""<<s.cp0[12]
             <<"\",\"bad_address\":\""<<s.cp0[8]<<"\",\"gpr\":[";
    for(unsigned n=0;n<32;++n) {if(n)std::cout<<',';std::cout<<'"'<<s.gpr[n]<<'"';}
    std::cout<<"],\"retail_boot_verified\":false,\"playability_verified\":false}\n";
    return 0;
}
