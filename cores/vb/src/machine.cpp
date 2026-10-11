#include "machine.hpp"
#include <algorithm>
#include <cstring>

namespace vb {
bool Machine::load(const uint8_t* data,size_t size) {
    if(!data || size<1024 || size>16*1024*1024 || (size&(size-1)) ||
       std::memcmp(data+size-0x200,"VVBHB001",8)) return false;
    std::vector<uint8_t> image(data,data+size);
    uint32_t hash=0xffffffff; uint64_t id=14695981039346656037ull;
    for(auto byte:image) {
        hash^=byte;
        for(unsigned b=0;b<8;++b) hash=(hash>>1)^((hash&1) ? 0xedb88320u:0);
        id^=byte; id*=1099511628211ull;
    }
    rom=std::move(image); crc=~hash; identity=id; reset(); return true;
}
void Machine::reset() { ram.fill(0); cpu.reset(); controller.reset(); frames=0; done=false; }
bool Machine::read(uint32_t address,unsigned bytes,uint32_t& value) {
    if(bytes!=1 && bytes!=2 && bytes!=4) return false;
    const uint32_t p=address&0x07ffffff;
    if(p>=0x02000000 && p<0x03000000)return controller.read(p&63,value);
    if(p>=0x05000000 && uint64_t(p)+bytes<=0x06000000) {
        value=0; for(unsigned n=0;n<bytes;++n) value|=uint32_t(ram[(p+n)&0xffff])<<(8*n);
        return true;
    }
    if(!rom.empty() && p>=0x07000000 && uint64_t(p)+bytes<=0x08000000) {
        value=0; for(unsigned n=0;n<bytes;++n) value|=uint32_t(rom[(p+n)&(rom.size()-1)])<<(8*n);
        return true;
    }
    return false;
}
bool Machine::write(uint32_t address,unsigned bytes,uint32_t value) {
    if(bytes!=1 && bytes!=2 && bytes!=4) return false;
    const uint32_t p=address&0x07ffffff;
    if(p>=0x02000000 && p<0x03000000)return controller.write(p&63,value);
    // Repository-only debug addresses in an unimplemented board region.
    if(p==0x04000000 && bytes==1) { const char c=char(value); if(sink) sink->write(&c,1); return true; }
    if(p==0x04000008 && bytes==4){
        char text[8];constexpr char digits[]="0123456789abcdef";
        for(unsigned n=0;n<8;++n)text[n]=digits[(value>>(28-n*4))&15];
        if(sink)sink->write(text,8);
        return true;
    }
    if(p==0x04000004 && bytes==4) { done=true; if(sink) sink->finish(-1); return true; }
    if(p<0x05000000 || uint64_t(p)+bytes>0x06000000) return false;
    for(unsigned n=0;n<bytes;++n) ram[(p+n)&0xffff]=uint8_t(value>>(8*n));
    return true;
}
bool Machine::compare_exchange(uint32_t address,uint32_t expected,uint32_t replacement,uint32_t& previous) {
    // This profile has one serialized CPU bus owner and no asynchronous DMA.
    // Keep the full RMW in one callback; future bus owners must arbitrate here.
    if(!read(address,4,previous)) return false;
    return write(address,4,previous==expected ? replacement:previous);
}
static void append(std::vector<uint8_t>& data,uint64_t value) {
    for(unsigned n=0;n<8;++n) data.push_back(uint8_t(value>>(56-8*n)));
}
bool Machine::save(std::vector<uint8_t>& data) const {
    if(rom.empty()) return false;
    data.assign({'V','V','B','S','0','0','0','3'});
    for(auto value:{identity,uint64_t(crc),frames,uint64_t(done)}) append(data,value);
    for(auto value:cpu.state.gpr) append(data,value);
    for(auto value:cpu.state.sr) append(data,value);
    for(auto value:{uint64_t(cpu.state.pc),cpu.state.steps,uint64_t(cpu.state.halted),uint64_t(cpu.state.unsupported)}) append(data,value);
    const auto& bit=cpu.state.bitstring;
    for(auto value:{uint64_t(bit.active),uint64_t(bit.source_valid),uint64_t(bit.destination_valid),uint64_t(bit.operation),
                   uint64_t(bit.source_word),uint64_t(bit.destination_word)}) append(data,value);
    data.insert(data.end(),ram.begin(),ram.end());
    const auto pad=controller.save();data.insert(data.end(),pad.begin(),pad.end());return true;
}
bool Machine::restore(const std::vector<uint8_t>& data) {
    constexpr size_t prefix=8+(4+32+32+4+6)*8;
    if(rom.empty() || data.size()<8)return false;
    const bool legacy=!std::memcmp(data.data(),"VVBS0002",8);
    if((!legacy && std::memcmp(data.data(),"VVBS0003",8)) || data.size()!=prefix+ram.size()+(legacy?0:Controller::state_size))return false;
    size_t pos=8;
    auto take=[&]() { uint64_t value=0; for(unsigned n=0;n<8;++n) value=(value<<8)|data[pos++]; return value; };
    if(take()!=identity || take()!=crc) return false;
    const auto saved_frames=take(),saved_done=take();
    CpuState c; bool valid=true;
    for(auto& value:c.gpr) { auto v=take(); valid&=v<=0xffffffff; value=uint32_t(v); }
    for(auto& value:c.sr) { auto v=take(); valid&=v<=0xffffffff; value=uint32_t(v); }
    const auto pc=take(); c.pc=uint32_t(pc); c.steps=take(); const auto halted=take(),unsupported=take();
    const auto active=take(),source_valid=take(),destination_valid=take(),operation=take(),source_word=take(),destination_word=take();
    if(active>1 || source_valid>1 || destination_valid>1 || operation>15 || source_word>0xffffffff || destination_word>0xffffffff ||
       (!source_valid && source_word) || (!destination_valid && destination_word)) return false;
    if(active) {
        if(operation<8 || !c.gpr[28] || c.gpr[26]>31 || c.gpr[27]>31 || ((c.gpr[29]|c.gpr[30])&3)) return false;
    } else if(source_valid || destination_valid || operation || source_word || destination_word) return false;
    c.bitstring={bool(active),bool(source_valid),bool(destination_valid),uint32_t(operation),uint32_t(source_word),uint32_t(destination_word)};
    if(!valid || pc>0xffffffff || (pc&1) || saved_done>1 || halted>1 || unsupported>1 || c.gpr[0] ||
       (c.sr[5]&~0xff3ffu) || (c.sr[0]&1) || (c.sr[2]&1) || (c.sr[25]&1)) return false;
    c.halted=halted; c.unsupported=unsupported;
    Controller pad;
    if(!legacy && !pad.restore(data.data()+pos+ram.size(),Controller::state_size))return false;
    controller=pad;cpu.state=c; frames=saved_frames; done=saved_done;
    std::copy_n(data.begin()+pos,ram.size(),ram.begin()); return true;
}
}
