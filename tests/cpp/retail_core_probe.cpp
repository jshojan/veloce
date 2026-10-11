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
    if(argc<3)return 2;
    std::string load_state,save_state;unsigned capture_every=10000;
    try{for(int at=6;at<argc;at+=2){
        if(at+1>=argc)return 2;
        const std::string option=argv[at];
        if(option=="--load-state" && load_state.empty())load_state=argv[at+1];
        else if(option=="--save-state" && save_state.empty())save_state=argv[at+1];
        else if(option=="--capture-every"){
            size_t used=0;const auto n=std::stoul(argv[at+1],&used);
            if(argv[at+1][used] || !n || n>100000)return 2;
            capture_every=unsigned(n);
        }else return 2;
    }}catch(...){return 2;}
    RetailInputSchedule input;try{if(argc>=6 && std::string(argv[5])!="-")input.load(argv[5]);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
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
    if(!load_state.empty()){
        std::ifstream state_file(load_state,std::ios::binary);
        if(!state_file.good()){destroy(plugin);SDL_UnloadObject(library);return 2;}
        const std::vector<uint8_t> state{std::istreambuf_iterator<char>(state_file),{}};
        if(!loaded || !plugin->load_state(state)){destroy(plugin);SDL_UnloadObject(library);return 2;}
    }
    const auto starting_quantum=plugin->get_frame_count();
    if(starting_quantum>100000 || limit>100000-starting_quantum){destroy(plugin);SDL_UnloadObject(library);return 2;}
    std::vector<uint32_t> captured;std::vector<unsigned> checkpoints;int captured_width=0,captured_height=0;
    bool capture_written=false,nonuniform=false;uint64_t audio_samples=0,frames=0,capture_quantum=0;
    if(loaded) {
        for(unsigned frame=0;frame<limit;++frame) {
            plugin->run_frame({input.at(unsigned(starting_quantum)+frame)});const auto fb=plugin->get_framebuffer();
            if(fb.pixels && fb.width>0 && fb.height>0) {
                if(capture){captured.assign(fb.pixels,fb.pixels+size_t(fb.width)*fb.height);captured_width=fb.width;captured_height=fb.height;capture_quantum=starting_quantum+frame+1;}
                const auto first=fb.pixels[0];
                for(size_t n=1;n<size_t(fb.width)*fb.height;++n) if(fb.pixels[n]!=first) {nonuniform=true;
                    break;}
            }
            const unsigned quantum=unsigned(starting_quantum)+frame+1;
            if(capture && limit>capture_every && quantum%capture_every==0 && capture_quantum==quantum) {
                if(!write_capture(std::string(argv[4])+".q"+std::to_string(quantum)+".ppm",captured,captured_width,captured_height)){destroy(plugin);SDL_UnloadObject(library);return 2;}
                checkpoints.push_back(quantum);
            }
            const auto audio=plugin->get_audio();if(audio.samples && audio.sample_count>0)audio_samples+=audio.sample_count;
            plugin->clear_audio_buffer();
        }
        frames=plugin->get_frame_count();
    }
    bool state_written=false;
    if(!save_state.empty()){
        std::vector<uint8_t> state;
        if(!loaded || !plugin->save_state(state)){destroy(plugin);SDL_UnloadObject(library);return 2;}
        std::ofstream output(save_state,std::ios::binary);output.write(reinterpret_cast<const char*>(state.data()),state.size());
        if(!output){destroy(plugin);SDL_UnloadObject(library);return 2;}state_written=true;
    }
    if(capture && !captured.empty()){
        if(!write_capture(argv[4],captured,captured_width,captured_height)){destroy(plugin);SDL_UnloadObject(library);return 2;}
        capture_written=true;
    }
    // A video/audio smoke signal is evidence of execution, never a playability verdict.
    std::cout<<"{\"loaded\":"<<(loaded?"true":"false")<<",\"frames\":"<<frames
             <<",\"starting_quantum\":"<<starting_quantum<<",\"requested_quanta\":"<<limit
             <<",\"state_written\":"<<(state_written?"true":"false")
             <<",\"nonuniform_video_observed\":"<<(nonuniform?"true":"false")
             <<",\"capture_written\":"<<(capture_written?"true":"false")
             <<",\"capture_quantum\":"<<capture_quantum
             <<",\"audio_sample_pairs_observed\":"<<audio_samples<<",\"capture_checkpoints\":[";
    for(size_t n=0;n<checkpoints.size();++n){if(n)std::cout<<',';std::cout<<checkpoints[n];}
    std::cout<<"],\"playability_verified\":false}\n";
    destroy(plugin);SDL_UnloadObject(library);return 0;
}
