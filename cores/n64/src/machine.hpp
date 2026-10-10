#pragma once
#include "vr4300.hpp"
#include "emu/emulator_plugin.hpp"
#include <vector>

namespace n64 {
class Machine : public Memory {
public:
    static constexpr size_t ram_size = 8 * 1024 * 1024;
    Machine() : ram(ram_size), cpu(*this) {}
    bool load(const uint8_t* data, size_t size);
    void reset();
    bool read(uint64_t address, unsigned bytes, uint64_t& value) override;
    bool write(uint64_t address, unsigned bytes, uint64_t value) override;
    bool write_masked(uint64_t address, unsigned bytes, uint64_t value, uint8_t mask) override;
    bool save(std::vector<uint8_t>& data) const;
    bool restore(const std::vector<uint8_t>& data);
    std::vector<uint8_t> ram, rom;
    std::array<uint8_t, 8192> sp{};
    VR4300 cpu;
    emu::ITestSink* sink = nullptr;
    uint32_t crc = 0;
    uint64_t identity = 0, frames = 0;
    uint64_t entry = 0;
    bool done = false;
private:
    static bool physical(uint64_t address, uint32_t& result);
};
}
