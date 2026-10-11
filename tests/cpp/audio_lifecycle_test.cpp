#include "core/audio_pipeline.hpp"
#include "core/plugin_manager.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
static void require(bool value) {if(!value)throw std::runtime_error("audio lifecycle mismatch");}
static void environment(const char* key,const char* value) {
#ifdef _WIN32
    _putenv_s(key,value?value:"");
#else
    if(value)setenv(key,value,1);else unsetenv(key);
#endif
}
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    try {
        namespace fs=std::filesystem;auto library=fs::absolute(argv[1]);auto root=fs::absolute(argv[2]);
        fs::create_directories(root/"plugins");fs::copy_file(library,root/"plugins"/library.filename(),fs::copy_options::overwrite_existing);
        fs::current_path(root);auto capture=root/"lifecycle.txt";fs::remove(capture);
        environment("VELOCE_AUDIO_CAPTURE",capture.string().c_str());environment("VELOCE_AUDIO_REJECT",nullptr);
        int outputs=0;emu::AudioPipeline host([&](const float* p,size_t n,int rate){require(n==2 && rate==48000 && p[0]==0.25f && p[1]==-0.25f);++outputs;},[]{},[]{});
        emu::PluginManager manager;require(manager.initialize());require(!manager.set_active_plugin(emu::PluginType::Audio,"Capture Audio"));require(manager.get_audio_plugin()==nullptr);manager.set_audio_host(&host);std::ofstream(capture,std::ios::trunc).close();
        require(manager.set_active_plugin(emu::PluginType::Audio,"Capture Audio"));auto* first=manager.get_audio_plugin();
        environment("VELOCE_AUDIO_REJECT","1");require(!manager.set_active_plugin(emu::PluginType::Audio,"Capture Audio"));
        require(manager.get_audio_plugin()==first);environment("VELOCE_AUDIO_REJECT",nullptr);
        const float p[]={0.5f,-0.5f};host.begin_frame();host.batch(first,p,2,48000,false);require(outputs==1);
        require(!manager.set_active_plugin(emu::PluginType::Audio,"Missing Audio"));require(manager.get_audio_plugin()==first);
        require(manager.set_active_plugin(emu::PluginType::Audio,"Capture Audio"));require(manager.get_audio_plugin()!=first);
        manager.shutdown();manager.shutdown();require(manager.get_audio_plugin()==nullptr);
        std::ifstream file(capture);std::vector<std::string> lines;std::string line;while(std::getline(file,line))lines.push_back(line);
        require(lines.size()==10 && lines[4].find("process 48000 2 32 2 ")==0);
        lines.erase(lines.begin()+4);
        require(lines==std::vector<std::string>({"initialize","initialize","shutdown","destroy","initialize","shutdown","destroy","shutdown","destroy"}));
        std::cout<<"Dynamic audio replacement, rejected initialization, live fallback and single shutdown PASS\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
