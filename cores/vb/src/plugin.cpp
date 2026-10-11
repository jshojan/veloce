#include "machine.hpp"
#include <array>

namespace vb {
class Plugin : public emu::IEmulatorPlugin {
public:
    emu::EmulatorInfo get_info() override {
        static const char* extensions[] = {".vb", nullptr};
        return {"VB", "0.4.0", "Veloce", "Experimental V810 integer homebrew core; original cartridge SRAM persistence, Nintendo integer extensions and software-clocked dual D-pad input; video, audio, hardware polling/timing and retail compatibility pending.",
                extensions, 50.0, 20000000, 384, 224};
    }
    const emu::ControllerLayoutInfo* get_controller_layout() override {
        using emu::VirtualButton;
        static const emu::ButtonLayout buttons[]={
            {VirtualButton::Up,"Left Up",.14f,.24f,.08f,.12f,true},{VirtualButton::Down,"Left Down",.14f,.50f,.08f,.12f,true},
            {VirtualButton::Left,"Left Left",.04f,.37f,.08f,.12f,true},{VirtualButton::Right,"Left Right",.24f,.37f,.08f,.12f,true},
            {VirtualButton::CUp,"Right Up",.80f,.24f,.08f,.12f,true},{VirtualButton::CDown,"Right Down",.80f,.50f,.08f,.12f,true},
            {VirtualButton::CLeft,"Right Left",.70f,.37f,.08f,.12f,true},{VirtualButton::CRight,"Right Right",.90f,.37f,.08f,.12f,true},
            {VirtualButton::A,"A",.60f,.62f,.08f,.12f,false},{VirtualButton::B,"B",.49f,.68f,.08f,.12f,false},
            {VirtualButton::Start,"Start",.38f,.56f,.08f,.10f,false},{VirtualButton::Select,"Select",.27f,.62f,.08f,.10f,false},
            {VirtualButton::L,"L",.10f,.04f,.20f,.10f,false},{VirtualButton::R,"R",.70f,.04f,.20f,.10f,false}};
        static const emu::ControllerLayoutInfo layout={"VB","Virtual Boy dual digital D-pads",emu::ControllerShape::Modern,2.0f,buttons,14,1};return &layout;
    }
    bool load_rom(const uint8_t* data, size_t size) override { return machine.load(data, size); }
    void unload_rom() override { machine.rom.clear(); machine.cartridge_ram.clear(); machine.crc = 0; machine.identity = 0; machine.reset(); }
    bool is_rom_loaded() const override { return !machine.rom.empty(); }
    uint32_t get_rom_crc32() const override { return machine.crc; }
    void reset() override { machine.reset(); }
    void run_frame(const emu::InputState& input) override {
        if (!is_rom_loaded()) return;
        machine.controller.input(input.buttons);
        // Bounded frontend scheduling quantum, NOT VIP/pipeline timing.
        for (unsigned i = 0; i < 100000 && !machine.done && !machine.cpu.state.halted && !machine.cpu.state.unsupported; ++i) machine.cpu.step();
        ++machine.frames;
    }
    uint64_t get_cycle_count() const override { return machine.cpu.state.steps; }
    uint64_t get_frame_count() const override { return machine.frames; }
    emu::FrameBuffer get_framebuffer() override { return {pixels.data(), 384, 224}; }
    emu::AudioBuffer get_audio() override { return {nullptr, 0, 48000}; }
    void clear_audio_buffer() override {}
    uint8_t read_memory(uint16_t address) override { return machine.ram[address]; }
    void write_memory(uint16_t address, uint8_t value) override { machine.ram[address] = value; }
    bool save_state(std::vector<uint8_t>& data) override { return machine.save(data); }
    bool load_state(const std::vector<uint8_t>& data) override { return machine.restore(data); }
    bool has_battery_save() const override { return is_rom_loaded() && !machine.cartridge_ram.empty(); }
    std::vector<uint8_t> get_battery_save_data() const override { return machine.cartridge_ram; }
    bool set_battery_save_data(const std::vector<uint8_t>& data) override {
        if(!has_battery_save() || data.size()!=machine.cartridge_ram.size())return false;
        machine.cartridge_ram=data;return true;
    }
    bool set_test_sink(emu::ITestSink* sink) override { machine.sink = sink; return true; }
    const char* test_channels() const override { return "v810debug"; }
private:
    Machine machine;
    std::array<uint32_t, 384 * 224> pixels{}; // No video emulation yet.
};
}
extern "C" {
EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() { return new vb::Plugin(); }
EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) { delete plugin; }
EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() { return EMU_PLUGIN_API_VERSION; }
}
