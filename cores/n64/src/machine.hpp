#pragma once
#include "vr4300.hpp"
#include "rdram.hpp"
#include "sp_interface.hpp"
#include "rsp_scalar.hpp"
#include "audio_interface.hpp"
#include "video_interface.hpp"
#include "emu/emulator_plugin.hpp"
#include <vector>

namespace n64 {
class Machine : public Memory {
public:
    static constexpr size_t ram_size = 8 * 1024 * 1024;
    Machine() : ram(ram_size), cpu(*this) {}
    bool load(const uint8_t* data, size_t size);
    void reset();
    uint8_t interrupt_pending() const override { return ((video_interface.interrupt && (mi_mask & 8)) || (audio_interface.interrupt && (mi_mask & 4)) || (sp_interface.interrupt && (mi_mask & 1)) || (si_interrupt && (mi_mask & 2)) || (pi_interrupt && (mi_mask & 16)) || (dp_interrupt && (mi_mask & 32))) ? 4 : 0; }
    void clock() override;
    void clear_audio() {audio_head=audio_count=0;}
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
    uint16_t mi_mode = 0;
    bool dp_interrupt = false;
    enum class BootMode:uint8_t { Homebrew, OriginalIPL, Native6102 };
    BootMode boot_mode=BootMode::Homebrew;
    std::array<uint32_t,13> pi{};
    bool pi_interrupt=false;
    // mode, config, latched current control, select, refresh, latency.
    std::array<uint32_t,6> ri{};
    bool ri_current_loaded=false;
    RdramDevices rdram_devices;
    SpInterface sp_interface;
    RspScalar rsp;
    DpInterface dp_interface;
    static constexpr size_t dp_state_size=152;
    static constexpr size_t vector_state_size=536;
    static constexpr size_t accumulator_state_size=64;
    static constexpr size_t combiner_state_size=24;
    static constexpr size_t depth_image_state_size=8;
    uint64_t rsp_phase=0;
    static constexpr size_t rsp_state_size=304;
    AudioInterface audio_interface;
    VideoInterface video_interface;
    static constexpr size_t audio_capacity=8192,audio_state_size=32928;
    std::array<uint16_t,audio_capacity*2> audio_samples{};
    uint64_t crystal_phase=0,output_phase=0,audio_head=0,audio_count=0,audio_dropped=0;
    void process_pif();
private:
    static bool physical(uint64_t address, uint32_t& result);
    bool write_pi(unsigned reg,uint32_t value);
    bool write_ri(unsigned reg,uint32_t value);
};
}
