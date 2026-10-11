#include "emu/audio_plugin.hpp"
#include <cstdlib>
#include <fstream>
#include <vector>
static void log(const std::string& text) {
    if (const char* path=std::getenv("VELOCE_AUDIO_CAPTURE")) std::ofstream(path,std::ios::app)<<text<<'\n';
}
class CaptureAudio final : public emu::IAudioPlugin {
public:
    emu::AudioPluginInfo get_info() override {return {"Capture Audio","1","Veloce","Test fixture",false,false};}
    bool initialize(emu::IAudioHost* h) override {host=h;log("initialize");return !std::getenv("VELOCE_AUDIO_REJECT");}
    void shutdown() override {log("shutdown");host=nullptr;}
    ~CaptureAudio() override {log("destroy");}
    void process(const float* p,size_t n) override {
        if (!host) {log("ERROR no host");return;}
        auto f=host->get_format();std::string line="process "+std::to_string(f.sample_rate)+" "+std::to_string(f.channels)+" "+std::to_string(f.bits_per_sample)+" "+std::to_string(n);
        for(size_t i=0;i<n;++i)line+=" "+std::to_string(p[i]);log(line);
        std::vector<float> modified(p,p+n);for(auto& v:modified)v*=0.5f;host->output_samples(modified.data(),n);
    }
    void set_volume(float) override {} float get_volume() const override {return 1;}
    void set_muted(bool) override {} bool is_muted() const override {return false;}
private:emu::IAudioHost* host=nullptr;
};
extern "C" {
EMU_PLUGIN_EXPORT emu::IAudioPlugin* create_audio_plugin(){return new CaptureAudio;}
EMU_PLUGIN_EXPORT void destroy_audio_plugin(emu::IAudioPlugin* p){delete p;}
EMU_PLUGIN_EXPORT uint32_t get_audio_plugin_api_version(){return EMU_AUDIO_PLUGIN_API_VERSION;}
}
