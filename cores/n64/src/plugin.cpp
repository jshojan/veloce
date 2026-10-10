#include "machine.hpp"
#include <array>

namespace n64 {
class Plugin : public emu::IEmulatorPlugin {
public:
    emu::EmulatorInfo get_info() override {
        static const char* extensions[] = {".z64", ".v64", ".n64", nullptr};
        return {"N64", "0.1.0", "Veloce", "Experimental VR4300 integer homebrew core; no retail boot, graphics, audio or controller hardware.",
                extensions, 60.0, 93750000, 320, 240};
    }
    bool load_rom(const uint8_t* data, size_t size) override { return machine.load(data, size); }
    void unload_rom() override { machine.rom.clear(); machine.crc = 0; machine.identity = 0; machine.entry = 0; machine.reset(); }
    bool is_rom_loaded() const override { return !machine.rom.empty(); }
    uint32_t get_rom_crc32() const override { return machine.crc; }
    void reset() override { machine.reset(); }
    void run_frame(const emu::InputState&) override {
        if (!is_rom_loaded()) return;
        // Bounded frontend scheduling quantum, NOT VI/pipeline timing.
        for (unsigned i = 0; i < 100000 && !machine.done; ++i) machine.cpu.step();
        ++machine.frames;
    }
    uint64_t get_cycle_count() const override { return machine.cpu.state.instructions; }
    uint64_t get_frame_count() const override { return machine.frames; }
    emu::FrameBuffer get_framebuffer() override { return {pixels.data(), 320, 240}; }
    emu::AudioBuffer get_audio() override { return {nullptr, 0, 48000}; }
    void clear_audio_buffer() override {}
    uint8_t read_memory(uint16_t address) override { return machine.ram[address]; }
    void write_memory(uint16_t address, uint8_t value) override { machine.ram[address] = value; }
    bool save_state(std::vector<uint8_t>& data) override { return machine.save(data); }
    bool load_state(const std::vector<uint8_t>& data) override { return machine.restore(data); }
    bool set_test_sink(emu::ITestSink* sink) override { machine.sink = sink; return true; }
    const char* test_channels() const override { return "v64debug"; }
private:
    Machine machine;
    std::array<uint32_t, 320 * 240> pixels{}; // No video emulation yet.
};
}
extern "C" {
EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() { return new n64::Plugin(); }
EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) { delete plugin; }
EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() { return EMU_PLUGIN_API_VERSION; }
}
