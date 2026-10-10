#include "machine.hpp"
#include <algorithm>
#include <cstring>
namespace nx {
static uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool Machine::load(const uint8_t* data,size_t size){
    // NSO0 segment descriptors. This first loader rejects compression/hash
    // flags explicitly; it never executes a guessed or partially loaded image.
    if(!data || size<256 || size>16*1024*1024 || std::memcmp(data,"NSO0",4) || u32(data+4) || u32(data+8) || u32(data+12))return false;
    std::array<Segment,3> parsed{};
    const uint32_t parsed_bss=u32(data+60);
    for(unsigned n=0;n<3;++n){
        const auto p=data+16+n*16;parsed[n]={u32(p),u32(p+4),u32(p+8)};
        const auto& s=parsed[n];
        const uint64_t extent=uint64_t(s.size)+(n==2?parsed_bss:0);
        if((s.address&4095) || s.address>ram_size-65536 || extent>ram_size-65536-s.address ||
           (s.file>size || s.size>size-s.file || (s.size && s.file<256)))return false;
        for(unsigned old=0;old<n;++old){
            const auto& a=parsed[old];
            if(extent && a.size && s.address<uint64_t(a.address)+a.size && a.address<uint64_t(s.address)+extent)return false;
            if(s.size && a.size && s.file<uint64_t(a.file)+a.size && a.file<uint64_t(s.file)+s.size)return false;
        }
    }
    if(parsed[0].address || parsed[0].size<4 || (parsed[0].size&3))return false;
    const uint32_t name_offset=u32(data+28),name_size=u32(data+44);
    if(name_size && (name_offset<256 || name_offset>size || name_size>size-name_offset))return false;
    std::vector<uint8_t> image(data,data+size);
    uint32_t hash=0xffffffff;uint64_t id=0xcbf29ce484222325ULL;
    for(auto byte:image){hash^=byte;for(unsigned bit=0;bit<8;++bit)hash=(hash>>1)^((hash&1)?0xedb88320u:0);id=(id^byte)*0x100000001b3ULL;}
    rom=std::move(image);segments=parsed;bss=parsed_bss;crc=~hash;identity=id;reset();return true;
}
void Machine::reset(){
    std::fill(ram.begin(),ram.end(),0);
    if(!rom.empty())for(const auto& s:segments)if(s.size)std::copy_n(rom.begin()+s.file,s.size,ram.begin()+s.address);
    cpu.state={};cpu.state.sp=ram_size;frames=0;done=false;
}
bool Machine::permitted(uint64_t address,unsigned size,bool instruction,bool writing) const{
    if(rom.empty() || (size!=1 && size!=2 && size!=4 && size!=8) || address>ram.size() || size>ram.size()-address)return false;
    if(!instruction && address>=ram_size-65536)return true; // Explicit test-profile stack.
    for(unsigned n=0;n<3;++n){const auto& s=segments[n];
        const uint64_t end=uint64_t(s.address)+s.size+(n==2?bss:0);
        if(address>=s.address && address+size<=end)return instruction?n==0:!writing || n==2;
    }
    return false;
}
bool Machine::read(uint64_t address,unsigned size,uint64_t& value,bool instruction){
    if(!permitted(address,size,instruction,false))return false;
    uint64_t result=0;for(unsigned n=0;n<size;++n)result|=uint64_t(ram[size_t(address)+n])<<(n*8);value=result;return true;
}
bool Machine::write(uint64_t address,unsigned size,uint64_t value){
    // Emulator instrumentation outside the process map. Actual STR bus values
    // are encoded without expected results or a guest correctness verdict.
    if(!rom.empty() && address==0xffff0000 && size==1){const char byte=char(value);if(sink)sink->write(&byte,1);return true;}
    if(!rom.empty() && address==0xffff0008 && size==8){
        char encoded[16];const char* digits="0123456789abcdef";for(unsigned n=0;n<16;++n)encoded[n]=digits[(value>>(60-4*n))&15];
        if(sink)sink->write(encoded,16);
        return true;
    }
    if(!rom.empty() && address==0xffff0010 && size==4){if(sink)sink->write("REG r=",6);return true;}
    if(!rom.empty() && address==0xffff0004 && size==4){done=true;if(sink)sink->finish(-1);return true;}
    if(!permitted(address,size,false,true))return false;
    for(unsigned n=0;n<size;++n)ram[size_t(address)+n]=uint8_t(value>>(n*8));
    return true;
}
static void append(std::vector<uint8_t>& data,uint64_t value){for(unsigned n=0;n<8;++n)data.push_back(uint8_t(value>>(56-n*8)));}
bool Machine::save(std::vector<uint8_t>& data) const{
    if(rom.empty())return false;
    data.assign({'V','N','X','S','0','0','0','1'});data.reserve(state_size);
    for(auto v:{identity,uint64_t(crc),frames,uint64_t(done)})append(data,v);
    for(auto v:cpu.state.x)append(data,v);
    const auto& s=cpu.state;
    for(auto v:{s.sp,s.pc,s.steps,s.fault_address,uint64_t(s.fault_word),uint64_t(s.nzcv),uint64_t(s.fault)})append(data,v);
    data.insert(data.end(),ram.begin(),ram.end());return true;
}
bool Machine::restore(const std::vector<uint8_t>& data){
    if(rom.empty() || data.size()!=state_size || std::memcmp(data.data(),"VNXS0001",8))return false;
    size_t at=8;auto take=[&](){uint64_t v=0;for(unsigned n=0;n<8;++n)v=v*256+data[at++];return v;};
    if(take()!=identity || take()!=crc)return false;
    const auto saved_frames=take(),saved_done=take();CpuState s;
    for(auto& v:s.x)v=take();
    s.sp=take();s.pc=take();s.steps=take();s.fault_address=take();
    const auto word=take(),flags=take(),fault=take();
    if(saved_done>1 || word>0xffffffffu || flags>15 || fault>5 || ((s.pc&3) && fault!=1) || (!fault && (word || s.fault_address)))return false;
    // Executable/RO bytes are immutable and fixed by the loaded image identity.
    for(unsigned n=0;n<2;++n){const auto& seg=segments[n];if(seg.size && !std::equal(rom.begin()+seg.file,rom.begin()+seg.file+seg.size,data.begin()+at+seg.address))return false;}
    s.fault_word=uint32_t(word);s.nzcv=unsigned(flags);s.fault=unsigned(fault);
    cpu.state=s;frames=saved_frames;done=saved_done;std::copy_n(data.begin()+at,ram_size,ram.begin());return true;
}
}
