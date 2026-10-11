#include "core/plugin_registry.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){if(argc!=4)return 2;try{
    namespace fs=std::filesystem;auto old=fs::absolute(argv[1]),current=fs::absolute(argv[2]),marker=fs::absolute(argv[3]);fs::remove(marker);
#ifdef _WIN32
    _putenv_s("VELOCE_TAS_FACTORY_MARKER",marker.string().c_str());
#else
    setenv("VELOCE_TAS_FACTORY_MARKER",marker.string().c_str(),1);
#endif
    emu::PluginRegistry registry;registry.scan_directory(old.parent_path());
    if(!registry.get_plugins_of_type(emu::PluginType::TAS).empty() || fs::exists(marker))throw std::runtime_error("legacy TAS vtable was invoked");
    registry.scan_directory(current.parent_path());auto plugins=registry.get_plugins_of_type(emu::PluginType::TAS);
    if(plugins.size()!=1 || plugins[0].name!="TAS Editor" || plugins[0].api_version!=2)throw std::runtime_error("TAS v2 not discovered");
    std::cout<<"Legacy TAS ABI1 rejected before factory; real TAS ABI2 discovered PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
