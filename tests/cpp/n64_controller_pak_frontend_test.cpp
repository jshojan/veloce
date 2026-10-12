#include "core/plugin_manager.hpp"
#include "core/paths_config.hpp"
#include "emu/controller_pak_storage.hpp"
#include "emu/netplay_plugin.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    require(argc==4,"plugin, ROM and isolated artifact directory required");
    namespace fs=std::filesystem;
    const auto library=fs::absolute(argv[1]),image=fs::absolute(argv[2]),root=fs::absolute(argv[3]);
    fs::create_directories(root/"plugins");fs::create_directories(root/"saves");
    fs::copy_file(library,root/"plugins"/library.filename(),fs::copy_options::overwrite_existing);
    const auto rom=root/"pak.z64";fs::copy_file(image,rom,fs::copy_options::overwrite_existing);
    const auto config=root/"pak-config.json";
    {std::ofstream file(config);file<<"{\"controller_paks\":5}\n";}
    fs::remove(root/"saves/pak.mpk1");fs::remove(root/"saves/pak.mpk3");
    emu::PathsConfiguration paths;paths.initialize(root);paths.set_save_directory(root/"saves");
    emu::PluginManager manager;manager.set_paths_config(&paths);
    require(manager.initialize((root/"plugins").string()) && manager.set_active_plugin(emu::PluginType::Emulator,"N64"),"frontend plugin discovery");
    require(manager.get_active_plugin()->load_config(config.c_str()) && manager.load_rom(rom.string()),"configured ROM load");
    auto* storage=dynamic_cast<emu::IControllerPakStorage*>(manager.get_active_plugin());
    require(storage && storage->controller_pak_ports()==4 && storage->controller_pak_inserted(0) &&
            !storage->controller_pak_inserted(1) && storage->controller_pak_inserted(2),"insertion mask");
    const auto blank=storage->get_controller_pak_data(0);
    require(blank.size()==32768 && !storage->set_controller_pak_data(0,{1}) &&
            storage->get_controller_pak_data(0)==blank,"malformed setter atomicity");
    auto content=blank;for(size_t i=0;i<content.size();++i)content[i]=uint8_t(i*37+11);
    require(storage->set_controller_pak_data(0,content),"set Pak bytes");
    auto* netplay=dynamic_cast<emu::INetplayCapable*>(manager.get_active_plugin());
    require(netplay && netplay->get_max_state_size()>9'529'672,"Pak state capacity");
    std::vector<uint8_t> snapshot(netplay->save_state_fast(nullptr,0));
    require(snapshot.size()==netplay->get_max_state_size() &&
            netplay->save_state_fast(snapshot.data(),snapshot.size())==snapshot.size() &&
            snapshot[7]=='4',"S034 frontend snapshot");
    require(storage->set_controller_pak_data(0,blank) &&
            netplay->load_state_fast(snapshot.data(),snapshot.size()) &&
            storage->get_controller_pak_data(0)==content,"frontend Pak rollback");
    manager.unload_rom();
    require(!storage->controller_pak_inserted(0) && storage->get_controller_pak_data(0).empty(),"unloaded contract");
    require(fs::file_size(root/"saves/pak.mpk1")==32768 && fs::file_size(root/"saves/pak.mpk3")==32768 &&
            !fs::exists(root/"saves/pak.mpk2"),"disk files");
    manager.shutdown();
    emu::PluginManager fresh;fresh.set_paths_config(&paths);
    require(fresh.initialize((root/"plugins").string()) && fresh.set_active_plugin(emu::PluginType::Emulator,"N64"),"fresh frontend");
    require(fresh.get_active_plugin()->load_config(config.c_str()) && fresh.load_rom(rom.string()),"fresh ROM load");
    storage=dynamic_cast<emu::IControllerPakStorage*>(fresh.get_active_plugin());
    require(storage && storage->get_controller_pak_data(0)==content,"cross-session Pak persistence");
    fresh.unload_rom();
    {std::ofstream file(root/"saves/pak.mpk1",std::ios::binary|std::ios::trunc);file.put(1);}
    require(fresh.load_rom(rom.string()) && storage->get_controller_pak_data(0)==blank,"malformed Pak rejected atomically");
    fresh.shutdown();
    std::cout<<"N64 frontend Pak insertion, disk persistence and malformed file rejection PASS\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
