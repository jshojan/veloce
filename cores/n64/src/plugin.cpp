#include "machine.hpp"
#include <array>

namespace n64 {
class Plugin : public emu::IEmulatorPlugin {
public:
    emu::EmulatorInfo get_info() override {
        static const char* extensions[] = {".z64", ".v64", ".n64", nullptr};
        return {"N64", "0.25.0", "Veloce", "Experimental integer core with TLB translation, FPU register/memory transfers and controls/branches, conversions and S/D arithmetic/comparisons, primary caches, settled PI/SI/SP, SP scalar execution and vector transfers/multiply/accumulate/comparison/integer-ALU/reciprocal/square-root/move/clipping, shared DP controls and RDP raw fill/color-depth clears, scissoring and sync and combiner/color and tile/texture-image command state and RI/MI/RDRAM device controls and CIC-6102 IPL3 handoff; retail boot/playability, RSP/RDP graphics, RSP audio microcode, exact timing and analog input pending; functional AI PCM and unfiltered VI framebuffer output.",
                extensions, 60.0, 93750000, 320, 240};
    }
    const emu::ControllerLayoutInfo* get_controller_layout() override {
        using emu::VirtualButton;
        static const emu::ButtonLayout buttons[] = {
            {VirtualButton::Up,"Up",.18f,.32f,.09f,.12f,true},
            {VirtualButton::Down,"Down",.18f,.57f,.09f,.12f,true},
            {VirtualButton::Left,"Left",.08f,.45f,.09f,.12f,true},
            {VirtualButton::Right,"Right",.28f,.45f,.09f,.12f,true},
            {VirtualButton::A,"A",.65f,.68f,.1f,.12f,false},
            {VirtualButton::B,"B",.56f,.48f,.1f,.12f,false},
            {VirtualButton::Z,"Z",.45f,.74f,.1f,.12f,false},
            {VirtualButton::Start,"Start",.45f,.40f,.1f,.12f,false},
            {VirtualButton::L,"L",.10f,.06f,.15f,.12f,false},
            {VirtualButton::R,"R",.75f,.06f,.15f,.12f,false},
            {VirtualButton::CUp,"C Up",.82f,.25f,.1f,.12f,false},
            {VirtualButton::CDown,"C Down",.82f,.53f,.1f,.12f,false},
            {VirtualButton::CLeft,"C Left",.71f,.39f,.1f,.12f,false},
            {VirtualButton::CRight,"C Right",.93f,.39f,.1f,.12f,false}};
        static const emu::ControllerLayoutInfo layout = {"N64","N64 digital controls (stick pending)",
            emu::ControllerShape::Modern,2.0f,buttons,14,1};
        return &layout;
    }
    bool load_rom(const uint8_t* data, size_t size) override { return machine.load(data, size); }
    void unload_rom() override { machine.rom.clear(); machine.crc = 0; machine.identity = 0; machine.entry = 0; machine.boot_mode=Machine::BootMode::Homebrew; machine.reset(); }
    bool is_rom_loaded() const override { return !machine.rom.empty(); }
    uint32_t get_rom_crc32() const override { return machine.crc; }
    void reset() override { machine.reset(); }
    void run_frame(const emu::InputState& input) override {
        if (!is_rom_loaded()) return;
        machine.input_buttons = input.buttons; machine.yielded = false;
        // Bounded frontend scheduling quantum, NOT VI/pipeline timing.
        for (unsigned i = 0; i < 100000 && !machine.done && !machine.yielded; ++i) machine.cpu.step();
        ++machine.frames;
        if(m_audio_callback) {const auto a=get_audio();if(a.sample_count)m_audio_callback(a.samples,size_t(a.sample_count),a.sample_rate);clear_audio_buffer();}
    }
    uint64_t get_cycle_count() const override { return machine.cpu.state.instructions; }
    uint64_t get_frame_count() const override { return machine.frames; }
    emu::FrameBuffer get_framebuffer() override {int width=0,height=0;machine.video_interface.render(machine.ram,pixels,width,height);return {pixels.data(),width,height};}
    emu::AudioBuffer get_audio() override {
        audio.resize(size_t(machine.audio_count)*2);
        for(size_t n=0;n<audio.size();++n) {const uint16_t raw=machine.audio_samples[(machine.audio_head*2+n)%(Machine::audio_capacity*2)];const int value=raw<32768?int(raw):int(raw)-65536;audio[n]=float(value)/32768.0f;}
        return {audio.empty()?nullptr:audio.data(),int(machine.audio_count),48000};
    }
    void clear_audio_buffer() override {machine.clear_audio();audio.clear();}
    uint8_t read_memory(uint16_t address) override { return machine.ram[address]; }
    void write_memory(uint16_t address, uint8_t value) override { machine.ram[address] = value; }
    bool save_state(std::vector<uint8_t>& data) override { return machine.save(data); }
    bool load_state(const std::vector<uint8_t>& data) override { return machine.restore(data); }
    bool set_test_sink(emu::ITestSink* sink) override { machine.sink = sink; return true; }
    const char* test_channels() const override { return "v64debug"; }
private:
    Machine machine;
    std::vector<float> audio;
    std::vector<uint32_t> pixels;
};
}
extern "C" {
EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() { return new n64::Plugin(); }
EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) { delete plugin; }
EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() { return EMU_PLUGIN_API_VERSION; }
}
