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
    uint8_t interrupt_pending() const override { return ((si_interrupt && (mi_mask & 2)) || (pi_interrupt && (mi_mask & 16))) ? 4 : 0; }
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
    bool done = false, yielded = false;
    std::array<uint8_t, 64> pif{};
    uint32_t si_address = 0, input_buttons = 0;
    bool si_interrupt = false;
    uint8_t mi_mask = 0;
    enum class BootMode:uint8_t { Homebrew, OriginalIPL, Native6102 };
    BootMode boot_mode=BootMode::Homebrew;
    std::array<uint32_t,13> pi{};
    bool pi_interrupt=false;
    void process_pif();
private:
    static bool physical(uint64_t address, uint32_t& result);
    bool write_pi(unsigned reg,uint32_t value);
};
}
