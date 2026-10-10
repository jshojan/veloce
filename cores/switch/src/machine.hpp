#pragma once
#include "aarch64.hpp"
#include "emu/emulator_plugin.hpp"
#include <vector>
namespace nx {
class Machine final : public Memory {
public:
    static constexpr size_t ram_size=4*1024*1024,state_size=344+ram_size;
    Machine():ram(ram_size),cpu(*this){}
    bool load(const uint8_t* data,size_t size);
    void reset();
    bool read(uint64_t address,unsigned size,uint64_t& value,bool instruction=false) override;
    bool write(uint64_t address,unsigned size,uint64_t value) override;
    bool save(std::vector<uint8_t>& data) const;
    bool restore(const std::vector<uint8_t>& data);
    std::vector<uint8_t> ram,rom;
    Aarch64 cpu;
    emu::ITestSink* sink=nullptr;
    uint64_t frames=0,identity=0;
    uint32_t crc=0;
    bool done=false;
private:
    struct Segment {uint32_t file=0,address=0,size=0;};
    std::array<Segment,3> segments{};
    uint32_t bss=0;
    bool permitted(uint64_t address,unsigned size,bool instruction,bool writing) const;
};
}
