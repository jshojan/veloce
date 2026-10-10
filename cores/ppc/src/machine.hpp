#pragma once
#include "emu/emulator_plugin.hpp"
#include <array>
#include <vector>
namespace ppc {
struct State {
    std::array<uint32_t,32> r{};
    uint32_t pc=0,cr=0,xer=0,lr=0,ctr=0;
    uint64_t steps=0;
    bool unsupported=false;
};
class Machine {
public:
    explicit Machine(bool broadway):wii(broadway),ram(24*1024*1024) {}
    bool load(const uint8_t* data,size_t size);
    void reset();
    void step();
    void frame();
    bool read(uint32_t address,unsigned width,uint32_t& value) const;
    bool write(uint32_t address,unsigned width,uint32_t value);
    bool save(std::vector<uint8_t>& data) const;
    bool restore(const std::vector<uint8_t>& data);
    bool wii;
    State cpu;
    std::vector<uint8_t> rom,ram;
    emu::ITestSink* sink=nullptr;
    uint32_t crc=0;
    uint64_t identity=0,frames=0;
    bool done=false;
private:
    bool location(uint32_t address,unsigned width,size_t& offset) const;
};
}
