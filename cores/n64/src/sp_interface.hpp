#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "hidden_rdram.hpp"
namespace n64 {
// CPU view of RSP controls. DMA settles synchronously; this is not an RSP CPU.
class SpInterface {
public:
    using State=std::array<uint32_t,9>;
    uint32_t pending_sp=0,pending_dram=0,active_sp=0,active_dram=0,length=0;
    uint32_t status=1,pc=0,semaphore=0,interrupt=0;
    // Host-only instruction-memory generation; save states restore a fresh cache.
    uint64_t imem_generation=1;
    void invalidate_imem(){++imem_generation;if(!imem_generation)++imem_generation;}
    void reset() {const auto generation=imem_generation;*this=SpInterface{};imem_generation=generation;invalidate_imem();}
    bool read(unsigned reg,uint64_t& value) {
        switch(reg) {
        case 0:value=active_sp;break;case 1:value=active_dram;break;
        case 2:case 3:value=length;break;case 4:value=status;break;
        case 5:case 6:value=0;break; // No outstanding DMA in settled model.
        case 7:value=semaphore;semaphore=1;break;
        default:return false;
        }return true;
    }
    bool write(unsigned reg,uint32_t value,std::vector<uint8_t>& ram,std::array<uint8_t,8192>& memory,HiddenRdram* hidden=nullptr) {
        if(reg==0) {pending_sp=value&0x1ff8;return true;}
        if(reg==1) {pending_dram=value&0xfffff8;return true;}
        if(reg==2 || reg==3)return dma(reg==3,value,ram,memory,hidden);
        if(reg==4) {commands(value);return true;}
        if(reg==7) {semaphore=0;return true;}
        return false; // Read-only writes are outside the supported profile.
    }
    State snapshot() const {return {pending_sp,pending_dram,active_sp,active_dram,length,status,pc,semaphore,interrupt};}
    bool restore(const State& s) {
        constexpr uint32_t masks[]={0x1ff8,0xfffff8,0x1ff8,0xfffff8,0xfff00fff,0x7fe3,0xffc,1,1};
        for(unsigned n=0;n<s.size();++n)if(s[n]&~masks[n])return false;
        // Settled lengths are reset zero or terminal count/length with retained stride.
        if(s[4] && (s[4]&0xfffff)!=0xff8)return false;
        pending_sp=s[0];pending_dram=s[1];active_sp=s[2];active_dram=s[3];length=s[4];
        status=s[5];pc=s[6];semaphore=s[7];interrupt=s[8];invalidate_imem();return true;
    }
private:
    static void pair(uint32_t command,unsigned bit,uint32_t& target,uint32_t mask) {
        const auto bits=(command>>bit)&3;
        if(bits==1)target&=~mask;else if(bits==2)target|=mask;
        // Simultaneous set/clear preserves the previous value (hardware tests).
    }
    void commands(uint32_t value) {
        pair(value,0,status,1);if(value&4)status&=~2u;
        pair(value,3,interrupt,1);pair(value,5,status,32);pair(value,7,status,64);
        for(unsigned n=0;n<8;++n)pair(value,9+2*n,status,128u<<n);
    }
    bool dma(bool to_ram,uint32_t value,std::vector<uint8_t>& ram,std::array<uint8_t,8192>& memory,HiddenRdram* hidden) {
        const uint32_t bytes=((value&4095)|7)+1,rows=((value>>12)&255)+1,skip=(value>>20)&4095;
        const uint32_t bank=pending_sp&4096,start=pending_sp&4095;
        // Reject any unsupported RAM address before changing memory or controls.
        // Non-multiple-of-eight strides have unimplemented bus alignment details.
        if(skip&7)return false;
        const uint64_t end=uint64_t(pending_dram)+uint64_t(rows-1)*(bytes+skip)+bytes;
        if(end>ram.size())return false;
        if(hidden && hidden->size()!=ram.size())return false;
        for(uint32_t row=0;row<rows;++row)for(uint32_t byte=0;byte<bytes;++byte) {
            auto& sp=memory[bank|((start+row*bytes+byte)&4095)];
            auto& dram=ram[pending_dram+row*(bytes+skip)+byte];
            if(to_ram)dram=sp;else sp=dram;
        }
        if(!to_ram && bank)invalidate_imem();
        if(to_ram && hidden)for(uint32_t row=0;row<rows;++row){const auto address=pending_dram+row*(bytes+skip);hidden->ordinary_burst(address,ram.data()+address,bytes);}
        active_sp=bank|((start+rows*bytes)&4095);
        active_dram=(pending_dram+rows*bytes+(rows-1)*skip)&0xfffff8;
        length=(value&0xfff00000)|0xff8;
        return true;
    }
};
}
