#pragma once
#include "v810.hpp"
#include "controller.hpp"
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
    bool read_port(uint32_t address,unsigned bytes,uint32_t& value) override { return read(address,bytes,value); }
    bool write_port(uint32_t address,unsigned bytes,uint32_t value) override { return write(address,bytes,value); }
    bool compare_exchange(uint32_t address,uint32_t expected,uint32_t replacement,uint32_t& previous) override;
    bool save(std::vector<uint8_t>& data) const;
    bool restore(const std::vector<uint8_t>& data);
    std::array<uint8_t,65536> ram{};
    std::vector<uint8_t> rom, cartridge_ram;
    V810 cpu;
    Controller controller;
    emu::ITestSink* sink=nullptr;
    uint32_t crc=0;
    uint64_t identity=0,frames=0;
    bool done=false;
};
}
