#include "emu/emulator_plugin.hpp"
class AudioCore final : public emu::IEmulatorPlugin {
public:
    emu::EmulatorInfo get_info() override {static const char* ext[]={".vaudio",nullptr};return {"Audio Fixture","1","Veloce","Test fixture",ext,60,1000000,1,1};}
    bool load_rom(const uint8_t* p,size_t n) override {loaded=n==1;streaming=loaded && p[0];return loaded;}
    void unload_rom() override {loaded=false;} bool is_rom_loaded() const override{return loaded;}
    uint32_t get_rom_crc32() const override{return 0;} void reset() override {frame=0;}
    void run_frame(const emu::InputState&) override {
        ++frame;rate=frame==1?32040:frame==2?48000:44100;
        samples[0]=float(frame)/16;samples[1]=-float(frame)/16;samples[2]=0.5f;samples[3]=-0.25f;
        if(streaming && m_audio_callback)m_audio_callback(samples,2,rate);
        if(frame==3 && sink)sink->finish(0);
    }
    uint64_t get_cycle_count() const override{return frame*100;} uint64_t get_frame_count() const override{return frame;}
    emu::FrameBuffer get_framebuffer() override {return {&pixel,1,1};}
    emu::AudioBuffer get_audio() override {return {samples,2,rate};}
    void clear_audio_buffer() override {} // retain batch to detect double playback
    uint8_t read_memory(uint16_t) override{return 0;} void write_memory(uint16_t,uint8_t) override {}
    bool save_state(std::vector<uint8_t>&) override{return false;} bool load_state(const std::vector<uint8_t>&) override{return false;}
    bool set_test_sink(emu::ITestSink* s) override {sink=s;return true;} const char* test_channels() const override{return "fixture";}
private:bool loaded=false,streaming=false;uint64_t frame=0;int rate=44100;float samples[4]={};uint32_t pixel=0xff000000;emu::ITestSink* sink=nullptr;
};
extern "C" {
EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin(){return new AudioCore;}
EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* p){delete p;}
EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version(){return EMU_PLUGIN_API_VERSION;}
}
