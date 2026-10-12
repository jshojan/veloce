#include "core/plugin_manager.hpp"
#include "core/paths_config.hpp"
#include "emu/netplay_plugin.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    require(argc==4,"plugin, Flash ROM and isolated artifact directory required");
    namespace fs=std::filesystem;
    const auto library=fs::absolute(argv[1]),root=fs::absolute(argv[3]);
    fs::create_directories(root/"plugins");fs::create_directories(root/"saves");
    fs::copy_file(library,root/"plugins"/library.filename(),fs::copy_options::overwrite_existing);
    const auto rom=root/"flash.z64";
    fs::copy_file(argv[2],rom,fs::copy_options::overwrite_existing);
    const auto save=root/"saves/flash.sav";fs::remove(save);
    emu::PathsConfiguration paths;paths.initialize(root);paths.set_save_directory(root/"saves");
    emu::PluginManager manager;manager.set_paths_config(&paths);
    require(manager.initialize((root/"plugins").string()) &&
            manager.set_active_plugin(emu::PluginType::Emulator,"N64") &&
            manager.load_rom(rom.string()),"frontend FlashRAM ROM load");
    auto* core=manager.get_active_plugin();
    require(core->has_battery_save() && core->get_battery_save_data().size()==131072,
            "frontend FlashRAM media selection");
    std::vector<uint8_t> content(131072);
    for(size_t n=0;n<content.size();++n)content[n]=uint8_t(n*37+(n>>8)+11);
    require(core->set_battery_save_data(content),"FlashRAM setter");
    require(!core->set_battery_save_data({1}) && core->get_battery_save_data()==content,
            "malformed FlashRAM setter atomicity");
    core->reset();require(core->get_battery_save_data()==content,"FlashRAM reset persistence");
    auto* netplay=dynamic_cast<emu::INetplayCapable*>(core);
    require(netplay,"frontend netplay state");
    std::vector<uint8_t> snapshot(netplay->save_state_fast(nullptr,0));
    require(snapshot.size()==netplay->get_max_state_size() &&
            netplay->save_state_fast(snapshot.data(),snapshot.size())==snapshot.size() &&
            snapshot[7]=='7',"frontend S037 snapshot");
    const auto blank=std::vector<uint8_t>(131072,0xff);
    require(core->set_battery_save_data(blank) &&
            netplay->load_state_fast(snapshot.data(),snapshot.size()) &&
            core->get_battery_save_data()==content,"frontend S037 rollback");
    manager.unload_rom();
    require(!core->has_battery_save() && core->get_battery_save_data().empty(),"unloaded FlashRAM contract");
    require(fs::file_size(save)==131072,"FlashRAM disk save size");
    manager.shutdown();
    emu::PluginManager fresh;fresh.set_paths_config(&paths);
    require(fresh.initialize((root/"plugins").string()) &&
            fresh.set_active_plugin(emu::PluginType::Emulator,"N64") &&
            fresh.load_rom(rom.string()),"fresh frontend FlashRAM load");
    core=fresh.get_active_plugin();
    require(core->get_battery_save_data()==content,"cross-session FlashRAM persistence");
    fresh.unload_rom();
    {std::ofstream file(save,std::ios::binary|std::ios::trunc);file.put(1);}
    require(fresh.load_rom(rom.string()) && core->get_battery_save_data()==blank,
            "malformed FlashRAM disk file was accepted");
    fresh.shutdown();
    std::cout<<"N64 128 KiB FlashRAM persistence, S037 rollback and malformed save rejection PASS\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
