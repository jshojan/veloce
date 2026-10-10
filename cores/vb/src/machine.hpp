#pragma once
#include "v810.hpp"
#include "emu/emulator_plugin.hpp"
#include <vector>

namespace vb {
class Machine : public Memory {
public:
    Machine() : cpu(*this) {}
    bool load(const uint8_t* data,size_t size);
    void reset();
    bool read(uint32_t address,unsigned bytes,uint32_t& value) override;
    bool write(uint32_t address,unsigned bytes,uint32_t value) override;
    bool save(std::vector<uint8_t>& data) const;
    bool restore(const std::vector<uint8_t>& data);
    std::array<uint8_t,65536> ram{};
    std::vector<uint8_t> rom;
    V810 cpu;
    emu::ITestSink* sink=nullptr;
    uint32_t crc=0;
    uint64_t identity=0,frames=0;
    bool done=false;
};
}
