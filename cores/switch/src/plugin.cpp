#include "machine.hpp"
namespace nx {
class Plugin final : public emu::IEmulatorPlugin {
public:
    emu::EmulatorInfo get_info() override {
        static const char* extensions[]={".nso",nullptr};
        return {"Switch","0.3.0","Veloce","Experimental uncompressed NSO0/AArch64 integer guest core with multiply/divide and conditional select; firmware, full CPU, graphics/audio, native devices, controllers, timing and retail compatibility pending.",extensions,60.0,0,320,240};
    }
    bool load_rom(const uint8_t* data,size_t size) override{return machine.load(data,size);}
    void unload_rom() override{machine.rom.clear();machine.crc=0;machine.identity=0;machine.reset();}
    bool is_rom_loaded() const override{return !machine.rom.empty();}
    uint32_t get_rom_crc32() const override{return machine.crc;}
    void reset() override{machine.reset();}
    void run_frame(const emu::InputState&) override{if(!is_rom_loaded())return;for(unsigned n=0;n<100000 && !machine.done && !machine.cpu.state.fault;++n)machine.cpu.step();++machine.frames;}
    uint64_t get_cycle_count() const override{return machine.cpu.state.steps;}
    uint64_t get_frame_count() const override{return machine.frames;}
    emu::FrameBuffer get_framebuffer() override{return {nullptr,0,0};}
    emu::AudioBuffer get_audio() override{return {nullptr,0,48000};}
    void clear_audio_buffer() override{}
    uint8_t read_memory(uint16_t address) override{return machine.ram[address];}
    void write_memory(uint16_t address,uint8_t value) override{machine.write(address,1,value);}
    bool save_state(std::vector<uint8_t>& data) override{return machine.save(data);}
    bool load_state(const std::vector<uint8_t>& data) override{return machine.restore(data);}
    bool set_test_sink(emu::ITestSink* sink) override{machine.sink=sink;return true;}
    const char* test_channels() const override{return "vnxdebug";}
private:
    Machine machine;
};
}
extern "C" {
EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin(){return new nx::Plugin();}
EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin){delete plugin;}
EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version(){return EMU_PLUGIN_API_VERSION;}
}
