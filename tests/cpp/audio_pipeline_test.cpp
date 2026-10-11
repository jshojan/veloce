#include "core/audio_pipeline.hpp"
#include <stdexcept>
#include <iostream>
#include <vector>
static void require(bool value) { if (!value) throw std::runtime_error("audio pipeline mismatch"); }
int main() {
    try {
        std::vector<float> output;int rate=0,pauses=0,resumes=0,calls=0;
        emu::AudioPipeline host([&](const float* p,size_t n,int r) {
            output.assign(p,p+n);rate=r;++calls;
        },[&]{++pauses;},[&]{++resumes;});
        auto* plugin=create_audio_plugin();require(plugin->initialize(&host));
        const float data[]={0.75f,-0.5f,0.25f,-1.0f};
        plugin->set_volume(0.5f);host.begin_frame();host.stream(plugin,data,4,32040,false);
        require(output==std::vector<float>({0.375f,-0.25f,0.125f,-0.5f}) && rate==32040 && calls==1);
        host.batch(plugin,data,4,32040,false);require(calls==1); // no double playback
        host.begin_frame();host.batch(plugin,data,4,48000,false);require(rate==48000 && calls==2);
        plugin->set_muted(true);host.begin_frame();host.batch(plugin,data,4,44100,false);
        require(output==std::vector<float>(4,0.0f) && rate==44100 && calls==3);
        host.begin_frame();host.stream(plugin,data,4,44100,true);host.batch(plugin,data,4,44100,false);
        require(calls==3); // suppressed replay cannot leak through the batch fallback
        host.begin_frame();host.batch(plugin,data,4,44100,true);require(calls==3);
        host.begin_frame();host.batch(nullptr,data,4,22050,false);
        require(output==std::vector<float>(data,data+4) && rate==22050 && calls==4);
        host.begin_frame();host.batch(plugin,nullptr,4,44100,false);host.batch(plugin,data,3,44100,false);
        host.batch(plugin,data,4,0,false);require(calls==4);
        host.pause_output();host.resume_output();require(pauses==1 && resumes==1);
        plugin->shutdown();destroy_audio_plugin(plugin);
        std::cout<<"Source-rate volume/mute, streaming/batch, rollback, passthrough and controls PASS\n";
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
