#include "machine.hpp"
namespace {
#ifdef PPC_PROFILE_WII
constexpr bool broadway=true;
constexpr const char* name="Wii";
constexpr const char* extension=".vwi";
#else
constexpr bool broadway=false;
constexpr const char* name="GameCube";
constexpr const char* extension=".vgc";
#endif
class Plugin final:public emu::IEmulatorPlugin {
public:
    emu::EmulatorInfo get_info() override {
        static const char* extensions[]={extension,nullptr};
        return {name,"0.8.0","Veloce","Experimental original PowerPC integer programs; retail boot and console peripherals pending.",extensions,60,0,640,480};
    }
    bool load_rom(const uint8_t* data,size_t size) override { return machine.load(data,size); }
    void unload_rom() override { machine.rom.clear(); machine.identity=0; machine.crc=0; machine.reset(); }
    bool is_rom_loaded() const override { return !machine.rom.empty(); }
    uint32_t get_rom_crc32() const override { return machine.crc; }
    void reset() override { machine.reset(); }
    void run_frame(const emu::InputState&) override { machine.frame(); }
    uint64_t get_cycle_count() const override { return machine.cpu.steps; }
    uint64_t get_frame_count() const override { return machine.frames; }
    emu::FrameBuffer get_framebuffer() override { return {pixels.data(),640,480}; }
    emu::AudioBuffer get_audio() override { return {nullptr,0,48000}; }
    void clear_audio_buffer() override {}
    uint8_t read_memory(uint16_t address) override { return machine.ram[address]; }
    void write_memory(uint16_t address,uint8_t value) override { machine.ram[address]=value; }
    bool save_state(std::vector<uint8_t>& data) override { return machine.save(data); }
    bool load_state(const std::vector<uint8_t>& data) override { return machine.restore(data); }
    bool set_test_sink(emu::ITestSink* sink) override { machine.sink=sink; return true; }
    const char* test_channels() const override { return "ppcdebug"; }
private:
    ppc::Machine machine{broadway};
    std::array<uint32_t,640*480> pixels{};
};
}
extern "C" {
EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() { return new Plugin; }
EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) { delete plugin; }
EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() { return EMU_PLUGIN_API_VERSION; }
}
