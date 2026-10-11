#pragma once
#include "emu/emulator_plugin.hpp"
#include "cpu.hpp"
#include "serial.hpp"
#include <array>
#include <vector>
namespace ppc {
class Machine:public Memory {
public:
    static constexpr size_t state_size=360+24*1024*1024+Serial::state_size;
    explicit Machine(bool broadway):wii(broadway),ram(24*1024*1024) {}
    bool load(const uint8_t* data,size_t size);
    void reset();
    void step();
    void frame();
    bool read(uint32_t address,unsigned width,uint32_t& value) const override;
    bool write(uint32_t address,unsigned width,uint32_t value) override;
    bool save(std::vector<uint8_t>& data) const;
    bool restore(const std::vector<uint8_t>& data);
    size_t save_into(uint8_t* out,size_t capacity) const;
    bool restore(const uint8_t* in,size_t size);
    uint64_t state_hash() const;
    bool wii;
    State cpu;
    Serial serial;
    std::vector<uint8_t> rom,ram;
    emu::ITestSink* sink=nullptr;
    uint32_t crc=0;
    uint64_t identity=0,frames=0;
    bool done=false;
private:
    void header(uint8_t* out) const;
    bool location(uint32_t address,unsigned width,size_t& offset) const;
};
}
