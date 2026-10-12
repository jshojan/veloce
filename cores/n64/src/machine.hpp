#pragma once
#include "vr4300.hpp"
#include "rdram.hpp"
#include "hidden_rdram.hpp"
#include "pif_joybus.hpp"
#include "sp_interface.hpp"
#include "rsp_scalar.hpp"
#include "audio_interface.hpp"
#include "video_interface.hpp"
#include "emu/emulator_plugin.hpp"
#include "emu/frame_scheduling.hpp"
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
    void run_frontend_frame();
    void clear_audio() {audio_head=audio_count=0;}
    bool read(uint64_t address, unsigned bytes, uint64_t& value) override;
    bool write(uint64_t address, unsigned bytes, uint64_t value) override;
    bool write_masked(uint64_t address, unsigned bytes, uint64_t value, uint8_t mask) override;
    bool read_cacheline(uint64_t address,unsigned bytes,uint64_t& value) override {return !(mi_mode&256) && read(address,bytes,value);}
    bool write_cacheline(uint64_t address,unsigned bytes,uint64_t value) override {return !(mi_mode&256) && write(address,bytes,value);}
    bool save(std::vector<uint8_t>& data) const;
    bool restore(const std::vector<uint8_t>& data);
    std::vector<uint8_t> ram, rom;
    HiddenRdram hidden;
    std::array<uint8_t, 8192> sp{};
    VR4300 cpu;
    emu::ITestSink* sink = nullptr;
    uint32_t crc = 0;
    uint64_t identity = 0, frames = 0;
    uint64_t entry = 0;
    bool done = false, yielded = false;
    emu::FrameScheduling frame_scheduling=emu::FrameScheduling::InstructionQuantum;
    std::array<uint8_t, 64> pif{};
    PifJoybus joybus;
    uint32_t si_address = 0, input_buttons = 0;
    std::array<uint32_t,3> other_input_buttons{};
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
    static constexpr size_t divider_state_size=8;
    static constexpr size_t texture_state_size=136;
    static constexpr size_t tmem_state_size=4096,controller_state_size=24,packet_state_size=200,conversion_state_size=8,blender_state_size=24;
    static constexpr size_t primitive_depth_state_size=8,joybus_state_size=40;
    static constexpr size_t hidden_state_size=ram_size/8+16;
    static constexpr size_t current_state_size=704+ram_size+8192+75888+304+dp_state_size+vector_state_size+accumulator_state_size+combiner_state_size+depth_image_state_size+divider_state_size+texture_state_size+tmem_state_size+controller_state_size+packet_state_size+conversion_state_size+blender_state_size+hidden_state_size+primitive_depth_state_size+joybus_state_size+PifJoybus::Eeprom{}.size();
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
