#include "emu/emulator_plugin.hpp"
#include "retail_input_schedule.hpp"
#include <SDL.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
static bool write_capture(const std::string& path,const std::vector<uint32_t>& pixels,int width,int height) {
    std::ofstream output(path,std::ios::binary);if(!output)return false;
    output<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(auto pixel:pixels){output.put(char(pixel&255));output.put(char((pixel>>8)&255));output.put(char((pixel>>16)&255));}
    return bool(output);
}
int main(int argc,char** argv) {
    if(argc<3 || argc>6)return 2;
    RetailInputSchedule input;try{if(argc==6)input.load(argv[5]);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
    const bool capture=argc>=5 && std::string(argv[4])!="-";
    unsigned limit=120;
    if(argc>=4){try{size_t used=0;const auto value=std::stoul(argv[3],&used);if(argv[3][used] || !value || value>100000)return 2;limit=unsigned(value);}catch(...){return 2;}}
    std::ifstream file(argv[2],std::ios::binary);if(!file.good())return 2;
    std::vector<uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    void* library=SDL_LoadObject(argv[1]);if(!library)return 2;
    const auto version=reinterpret_cast<uint32_t(*)()>(SDL_LoadFunction(library,"get_plugin_api_version"));
    const auto create=reinterpret_cast<emu::IEmulatorPlugin*(*)()>(SDL_LoadFunction(library,"create_emulator_plugin"));
    const auto destroy=reinterpret_cast<void(*)(emu::IEmulatorPlugin*)>(SDL_LoadFunction(library,"destroy_emulator_plugin"));
    if(!version || version()!=EMU_PLUGIN_API_VERSION || !create || !destroy) {SDL_UnloadObject(library);return 2;}
    auto* plugin=create();if(!plugin) {SDL_UnloadObject(library);return 2;}
    const bool loaded=plugin->load_rom(image.data(),image.size());
    std::vector<uint32_t> captured;std::vector<unsigned> checkpoints;int captured_width=0,captured_height=0;
    bool capture_written=false,nonuniform=false;uint64_t audio_samples=0,frames=0,capture_quantum=0;
    if(loaded) {
        for(unsigned frame=0;frame<limit;++frame) {
            plugin->run_frame({input.at(frame)});const auto fb=plugin->get_framebuffer();
            if(fb.pixels && fb.width>0 && fb.height>0) {
                if(capture){captured.assign(fb.pixels,fb.pixels+size_t(fb.width)*fb.height);captured_width=fb.width;captured_height=fb.height;capture_quantum=frame+1;}
                const auto first=fb.pixels[0];
                for(size_t n=1;n<size_t(fb.width)*fb.height;++n) if(fb.pixels[n]!=first) {nonuniform=true;
                    break;}
            }
            if(capture && limit>10000 && (frame+1)%10000==0 && capture_quantum==frame+1) {
                if(!write_capture(std::string(argv[4])+".q"+std::to_string(frame+1)+".ppm",captured,captured_width,captured_height)){destroy(plugin);SDL_UnloadObject(library);return 2;}
                checkpoints.push_back(frame+1);
            }
            const auto audio=plugin->get_audio();if(audio.samples && audio.sample_count>0)audio_samples+=audio.sample_count;
            plugin->clear_audio_buffer();
        }
        frames=plugin->get_frame_count();
    }
    if(capture && !captured.empty()){
        if(!write_capture(argv[4],captured,captured_width,captured_height)){destroy(plugin);SDL_UnloadObject(library);return 2;}
        capture_written=true;
    }
    // A video/audio smoke signal is evidence of execution, never a playability verdict.
    std::cout<<"{\"loaded\":"<<(loaded?"true":"false")<<",\"frames\":"<<frames
             <<",\"nonuniform_video_observed\":"<<(nonuniform?"true":"false")
             <<",\"capture_written\":"<<(capture_written?"true":"false")
             <<",\"capture_quantum\":"<<capture_quantum
             <<",\"audio_sample_pairs_observed\":"<<audio_samples<<",\"capture_checkpoints\":[";
    for(size_t n=0;n<checkpoints.size();++n){if(n)std::cout<<',';std::cout<<checkpoints[n];}
    std::cout<<"],\"playability_verified\":false}\n";
    destroy(plugin);SDL_UnloadObject(library);return 0;
}
