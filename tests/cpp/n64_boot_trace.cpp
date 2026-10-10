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
    bool fault=false;uint64_t handled_exceptions=0;uint32_t last_status=0;
    for(unsigned n=0;n<12000000 && !machine.done;++n) {
        last_status=uint32_t(machine.cpu.state.cp0[12]);machine.cpu.step();
        if(machine.cpu.exception_taken) {
            const unsigned code=(machine.cpu.state.cp0[13]>>2)&31;
            // Interrupts, TLB refill/permission and FPE may be handled by guest code.
            if(code==4 || code==5 || code==6 || code==7 || code==10) {fault=true;break;}
            ++handled_exceptions;
        }
    }
    const auto& s=machine.cpu.state;
    const auto translation=machine.cpu.tlb.translate(s.cp0[14],last_status,uint8_t(s.cp0[10]),uint32_t(s.cp0[16]),false);
    uint64_t instruction=0;
    const bool readable=fault && translation.fault==n64::Tlb::Fault::none && translation.physical<=0x1fffffffu &&
        machine.read(0xffffffffa0000000ull|translation.physical,4,instruction);
    const uint64_t fetched=fault?machine.cpu.instruction_word:0;const bool fetched_valid=fault && machine.cpu.instruction_valid;
    std::cout<<"{\"loaded\":true,\"boot_mode\":"<<unsigned(machine.boot_mode)
             <<",\"attempts\":"<<s.instructions<<",\"nonterminal_exception_entries\":"<<handled_exceptions<<",\"exception_observed\":"<<(fault?"true":"false")
             <<",\"fault_backing_readable\":"<<(readable?"true":"false")<<",\"fault_backing_word\":\""<<std::hex<<instruction<<std::dec<<"\""
             <<",\"fault_instruction_valid\":"<<(fetched_valid?"true":"false")<<",\"fault_instruction_word\":\""<<std::hex<<fetched<<std::dec<<"\""
             <<",\"fault_access_address\":\""<<std::hex<<(fault?machine.cpu.access_address:0)<<std::dec<<"\""
             <<",\"fault_access_bytes\":"<<(fault?machine.cpu.access_bytes:0)<<",\"fault_access_store\":"<<(fault && machine.cpu.access_store?"true":"false")
             <<",\"done\":"<<(machine.done?"true":"false")<<",\"pc\":\""<<std::hex<<s.pc
             <<"\",\"epc\":\""<<s.cp0[14]<<"\",\"cause\":\""<<s.cp0[13]<<"\",\"status\":\""<<s.cp0[12]
             <<"\",\"bad_address\":\""<<s.cp0[8]<<"\",\"gpr\":[";
    for(unsigned n=0;n<32;++n) {if(n)std::cout<<',';std::cout<<'"'<<s.gpr[n]<<'"';}
    std::cout<<"],\"sp_status\":\""<<machine.sp_interface.status<<"\",\"sp_pc\":\""<<machine.sp_interface.pc
             <<"\",\"sp_instruction\":\"";
    uint32_t sp_word=0;for(unsigned n=0;n<4;++n)sp_word=(sp_word<<8)|machine.sp[4096+((machine.sp_interface.pc+n)&4095)];
    std::cout<<sp_word<<"\",\"mi_mask\":\""<<unsigned(machine.mi_mask)<<"\",\"vi_registers\":[";
    for(unsigned n=0;n<14;++n){if(n)std::cout<<',';std::cout<<'"'<<machine.video_interface.snapshot()[n]<<'"';}
    std::cout<<"],\"rsp_instructions\":"<<std::dec<<machine.rsp.instructions<<",\"rsp_blocked\":"<<(machine.rsp.blocked?"true":"false")
             <<",\"rsp_blocked_word\":\""<<std::hex<<machine.rsp.blocked_word<<"\",\"retail_boot_verified\":false,\"playability_verified\":false}\n";
    return 0;
}
