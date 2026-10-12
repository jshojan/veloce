#include "core/plugin_manager.hpp"
#include "core/paths_config.hpp"
#include "emu/netplay_plugin.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    require(argc==5,"plugin, 96 KiB ROM, isolated artifact directory and 32 KiB ROM required");
    namespace fs=std::filesystem;
    const auto library=fs::absolute(argv[1]),root=fs::absolute(argv[3]);
    fs::create_directories(root/"plugins");fs::create_directories(root/"saves");
    fs::copy_file(library,root/"plugins"/library.filename(),fs::copy_options::overwrite_existing);
    emu::PathsConfiguration paths;paths.initialize(root);paths.set_save_directory(root/"saves");
    for(const auto [size,source]:std::array<std::pair<size_t,const char*>,2>{{
        {98304,argv[2]},{32768,argv[4]}}}){
        const auto rom=root/(std::to_string(size)+".z64");
        fs::copy_file(source,rom,fs::copy_options::overwrite_existing);
        const auto save=root/"saves"/(std::to_string(size)+".sav");
        fs::remove(save);
        emu::PluginManager manager;manager.set_paths_config(&paths);
        require(manager.initialize((root/"plugins").string()) &&
                manager.set_active_plugin(emu::PluginType::Emulator,"N64") &&
                manager.load_rom(rom.string()),"frontend SRAM ROM load");
        auto* core=manager.get_active_plugin();
        require(core->has_battery_save() && core->get_battery_save_data().size()==size,
                "frontend SRAM media selection");
        std::vector<uint8_t> content(size);
        for(size_t n=0;n<size;++n)content[n]=uint8_t(n*37+(n>>8)+11);
        require(core->set_battery_save_data(content),"SRAM setter");
        require(!core->set_battery_save_data({1}) && core->get_battery_save_data()==content,
                "malformed SRAM setter atomicity");
        core->reset();require(core->get_battery_save_data()==content,"SRAM reset persistence");
        auto* netplay=dynamic_cast<emu::INetplayCapable*>(core);
        require(netplay,"frontend netplay state");
        std::vector<uint8_t> snapshot(netplay->save_state_fast(nullptr,0));
        require(snapshot.size()==netplay->get_max_state_size() &&
                netplay->save_state_fast(snapshot.data(),snapshot.size())==snapshot.size() &&
                snapshot[7]=='6',"frontend S036 snapshot");
        const auto blank=std::vector<uint8_t>(size,0xff);
        require(core->set_battery_save_data(blank) &&
                netplay->load_state_fast(snapshot.data(),snapshot.size()) &&
                core->get_battery_save_data()==content,"frontend S036 rollback");
        manager.unload_rom();
        require(!core->has_battery_save() && core->get_battery_save_data().empty(),"unloaded SRAM contract");
        require(fs::file_size(save)==size,"SRAM disk save size");
        manager.shutdown();
        emu::PluginManager fresh;fresh.set_paths_config(&paths);
        require(fresh.initialize((root/"plugins").string()) &&
                fresh.set_active_plugin(emu::PluginType::Emulator,"N64") &&
                fresh.load_rom(rom.string()),"fresh frontend SRAM load");
        core=fresh.get_active_plugin();
        require(core->get_battery_save_data()==content,"cross-session SRAM persistence");
        fresh.unload_rom();
        {std::ofstream file(save,std::ios::binary|std::ios::trunc);file.put(1);}
        require(fresh.load_rom(rom.string()) && core->get_battery_save_data()==blank,
                "malformed SRAM disk file was accepted");
        fresh.shutdown();
    }
    std::cout<<"N64 32/96 KiB SRAM frontend persistence, S036 rollback and malformed saves PASS\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
