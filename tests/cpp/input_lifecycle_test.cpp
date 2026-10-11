#include "emu/input_plugin.hpp"
#include "core/plugin_manager.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
static void require(bool value) {if(!value)throw std::runtime_error("input lifecycle mismatch");}
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
        environment("VELOCE_INPUT_CAPTURE",capture.string().c_str());environment("VELOCE_INPUT_REJECT",nullptr);
        struct Host final:emu::IInputHost {
            uint64_t get_frame_number() const override{return 0;}
            const char* get_platform_name() const override{return "Input Fixture";}
            int get_controller_count() const override{return 2;}
            bool is_key_pressed(int) const override{return false;}
            bool is_gamepad_button_pressed(int,int) const override{return false;}
            float get_gamepad_axis(int,int) const override{return 0;}
            int get_connected_controller_count() const override{return 0;}
            emu::ControllerInfo get_controller_info(int) const override{return {-1,"",false,0,0,0};}
        } host;
        emu::PluginManager manager;require(manager.initialize());require(!manager.set_active_plugin(emu::PluginType::Input,"Capture Input"));require(manager.get_input_plugin()==nullptr);manager.set_input_host(&host);std::ofstream(capture,std::ios::trunc).close();
        require(manager.set_active_plugin(emu::PluginType::Input,"Capture Input"));auto* first=manager.get_input_plugin();
        environment("VELOCE_INPUT_REJECT","1");require(!manager.set_active_plugin(emu::PluginType::Input,"Capture Input"));
        require(manager.get_input_plugin()==first);environment("VELOCE_INPUT_REJECT",nullptr);
        first->begin_frame();require(first->get_input_state(0)==1);
        require(!manager.set_active_plugin(emu::PluginType::Input,"Missing Input"));require(manager.get_input_plugin()==first);
        require(manager.set_active_plugin(emu::PluginType::Input,"Capture Input"));require(manager.get_input_plugin()!=first);
        manager.shutdown();manager.shutdown();require(manager.get_input_plugin()==nullptr);
        std::ifstream file(capture);std::vector<std::string> lines;std::string line;while(std::getline(file,line))lines.push_back(line);
        require(lines.size()==11 && lines[4]=="begin 0" && lines[5]=="sample 0 1");
        lines.erase(lines.begin()+4,lines.begin()+6);
        require(lines==std::vector<std::string>({"initialize","initialize","shutdown","destroy","initialize","shutdown","destroy","shutdown","destroy"}));
        std::cout<<"Dynamic input replacement, rejected initialization, live fallback and single shutdown PASS\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
