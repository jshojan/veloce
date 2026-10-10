#pragma once
#include "../../nds/src/arm.hpp"
#include "emu/emulator_plugin.hpp"
#include <vector>
namespace ctr {
class Machine {
    struct Bus final:nds::Memory {
        Machine& owner; unsigned id;
        Bus(Machine& machine,unsigned cpu):owner(machine),id(cpu) {}
        bool read(uint32_t a,unsigned w,uint32_t& v) override { return owner.read(id,a,w,v); }
        bool write(uint32_t a,unsigned w,uint32_t v) override { return owner.write(id,a,w,v); }
    } bus0,bus1,bus9;
public:
    Machine():bus0(*this,0),bus1(*this,1),bus9(*this,2),arm0(bus0,true,true),arm1(bus1,true,true),arm9(bus9,true),
        fcram(0x8000000),axi(0x80000),private9(0x100000) {}
    bool load(const uint8_t*,size_t);
    void reset();
    void frame();
    void set_input(uint32_t buttons);
    bool read(unsigned cpu,uint32_t address,unsigned width,uint32_t& value);
    bool write(unsigned cpu,uint32_t address,unsigned width,uint32_t value);
    size_t state_size() const { return 504+fcram.size()+axi.size()+private9.size(); }
    size_t save_into(uint8_t*,size_t) const;
    bool restore(const uint8_t*,size_t);
    uint64_t state_hash() const;
    bool save(std::vector<uint8_t>&) const;
    bool restore(const std::vector<uint8_t>&);
    nds::Arm& cpu(unsigned id) { return id==0?arm0:id==1?arm1:arm9; }
    const nds::Arm& cpu(unsigned id) const { return id==0?arm0:id==1?arm1:arm9; }
    nds::Arm arm0,arm1,arm9;
    std::vector<uint8_t> rom,fcram,axi,private9;
    emu::ITestSink* sink=nullptr;
    uint32_t crc=0;
    uint64_t identity=0,frames=0;
    uint16_t keys=0;
    uint8_t done=0;
private:
    void save_prefix(uint8_t*) const;
    uint8_t* byte(unsigned cpu,uint32_t address);
};
}
