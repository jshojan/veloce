#include "machine.hpp"
#include <array>

namespace nds {
class Plugin : public emu::IEmulatorPlugin {
public:
    emu::EmulatorInfo get_info() override {
        static const char* extensions[] = {".nds", nullptr};
        return {"DS", "0.2.0", "Veloce", "Experimental dual-CPU ARM/Thumb integer homebrew core with digital keypad; firmware, video, audio and other peripherals pending.",
                extensions, 60.0, 67000000, 256, 384};
    }
    const emu::ControllerLayoutInfo* get_controller_layout() override {
        using emu::VirtualButton;
        static const emu::ButtonLayout buttons[] = {
            {VirtualButton::Up,"Up",.14f,.30f,.08f,.12f,true},
            {VirtualButton::Down,"Down",.14f,.55f,.08f,.12f,true},
            {VirtualButton::Left,"Left",.05f,.42f,.08f,.12f,true},
            {VirtualButton::Right,"Right",.23f,.42f,.08f,.12f,true},
            {VirtualButton::A,"A",.90f,.42f,.08f,.12f,false},
            {VirtualButton::B,"B",.80f,.55f,.08f,.12f,false},
            {VirtualButton::X,"X",.80f,.30f,.08f,.12f,false},
            {VirtualButton::Y,"Y",.70f,.42f,.08f,.12f,false},
            {VirtualButton::L,"L",.06f,.06f,.16f,.10f,false},
            {VirtualButton::R,"R",.78f,.06f,.16f,.10f,false},
            {VirtualButton::Start,"Start",.52f,.75f,.12f,.06f,false},
            {VirtualButton::Select,"Select",.36f,.75f,.12f,.06f,false}};
        static const emu::ControllerLayoutInfo layout={"DS","DS digital controls (touch pending)",
            emu::ControllerShape::Modern,2.0f,buttons,12,1};
        return &layout;
    }
    bool load_rom(const uint8_t* data, size_t size) override { return machine.load(data, size); }
    void unload_rom() override { machine.rom.clear(); machine.crc = 0; machine.identity = 0; machine.reset(); }
    bool is_rom_loaded() const override { return !machine.rom.empty(); }
    uint32_t get_rom_crc32() const override { return machine.crc; }
    void reset() override { machine.reset(); }
    void run_frame(const emu::InputState& input) override {
        if (!is_rom_loaded()) return;
        machine.set_input(input.buttons);
        machine.frame();
    }
    uint64_t get_cycle_count() const override { return machine.cpu9.state.steps+machine.cpu7.state.steps; }
    uint64_t get_frame_count() const override { return machine.frames; }
    emu::FrameBuffer get_framebuffer() override { return {pixels.data(), 256, 384}; }
    emu::AudioBuffer get_audio() override { return {nullptr, 0, 48000}; }
    void clear_audio_buffer() override {}
    uint8_t read_memory(uint16_t address) override { return machine.main[address]; }
    void write_memory(uint16_t address, uint8_t value) override { machine.main[address] = value; }
    bool save_state(std::vector<uint8_t>& data) override { return machine.save(data); }
    bool load_state(const std::vector<uint8_t>& data) override { return machine.restore(data); }
    bool set_test_sink(emu::ITestSink* sink) override { machine.sink = sink; return true; }
    const char* test_channels() const override { return "dsdebug"; }
private:
    Machine machine;
    std::array<uint32_t, 256 * 384> pixels{}; // No video emulation yet.
};
}
extern "C" {
EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() { return new nds::Plugin(); }
EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) { delete plugin; }
EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() { return EMU_PLUGIN_API_VERSION; }
}
