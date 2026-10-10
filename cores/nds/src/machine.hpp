#pragma once
#include "arm.hpp"
#include "emu/emulator_plugin.hpp"
#include <vector>
namespace nds {
class Machine {
    struct Bus : Memory {
        Machine& machine; bool arm9;
        Bus(Machine& owner,bool nine):machine(owner),arm9(nine) {}
        bool read(uint32_t address,unsigned bytes,uint32_t& value) override { return machine.read(arm9,address,bytes,value); }
        bool write(uint32_t address,unsigned bytes,uint32_t value) override { return machine.write(arm9,address,bytes,value); }
    } bus9,bus7;
public:
    Machine():bus9(*this,true),bus7(*this,false),cpu9(bus9,true),cpu7(bus7,false),main(0x400000),shared(0x8000),private7(0x10000) {}
    bool load(const uint8_t* data,size_t size);
    void reset();
    void frame();
    void set_input(uint32_t buttons);
    bool read(bool arm9,uint32_t address,unsigned bytes,uint32_t& value);
    bool write(bool arm9,uint32_t address,unsigned bytes,uint32_t value);
    bool save(std::vector<uint8_t>& data) const;
    bool restore(const std::vector<uint8_t>& data);
    Arm cpu9,cpu7;
    std::vector<uint8_t> rom,main,shared,private7;
    emu::ITestSink* sink=nullptr;
    uint32_t crc=0;
    uint64_t identity=0,frames=0;
    uint8_t wramcnt=3,done=0;
    uint16_t keys=0; // Pressed buttons in DS KEYINPUT/X/Y order, latched per frame.
private:
    uint8_t* byte(bool arm9,uint32_t address);
};
}
