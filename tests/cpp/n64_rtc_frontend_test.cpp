#include "core/plugin_manager.hpp"
#include "core/paths_config.hpp"
#include "emu/netplay_plugin.hpp"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    require(argc==4,"plugin, native NAF RTC ROM and isolated directory required");
    namespace fs=std::filesystem;
    const auto library=fs::absolute(argv[1]),root=fs::absolute(argv[3]);
    fs::create_directories(root/"plugins");fs::create_directories(root/"saves");
    fs::copy_file(library,root/"plugins"/library.filename(),fs::copy_options::overwrite_existing);
    const auto rom=root/"rtc.z64";
    fs::copy_file(argv[2],rom,fs::copy_options::overwrite_existing);
    const auto save=root/"saves/rtc.sav";fs::remove(save);
    emu::PathsConfiguration paths;paths.initialize(root);paths.set_save_directory(root/"saves");
    emu::PluginManager manager;manager.set_paths_config(&paths);
    require(manager.initialize((root/"plugins").string()) &&
            manager.set_active_plugin(emu::PluginType::Emulator,"N64") &&
            manager.load_rom(rom.string()),"frontend RTC cartridge load");
    auto* core=manager.get_active_plugin();
    auto content=core->get_battery_save_data();
    require(content.size()==131112 && content[131072+19]==1,"NAF RTC battery profile");
    for(size_t n=0;n<131072;++n)content[n]=uint8_t(n*29+(n>>8)+5);
    const uint64_t yesterday=uint64_t(std::time(nullptr))-86400;
    for(unsigned n=0;n<8;++n)content[131104+n]=uint8_t(yesterday>>(56-8*n));
    require(core->set_battery_save_data(content),"RTC battery import and offline advancement");
    auto advanced=core->get_battery_save_data();
    require(advanced[131072+19]==2 &&
            std::equal(content.begin(),content.begin()+131072,advanced.begin()),
            "one offline day did not advance RTC and preserve FlashRAM");
    require(!core->set_battery_save_data({1}) &&
            core->get_battery_save_data()[131072+19]==2,"malformed RTC setter atomicity");
    auto invalid=advanced;invalid[131072+21]=0x99;
    require(!core->set_battery_save_data(invalid) &&
            core->get_battery_save_data()[131072+19]==2,"invalid RTC date was accepted");
    auto* netplay=dynamic_cast<emu::INetplayCapable*>(core);
    require(netplay,"RTC netplay state interface");
    std::vector<uint8_t> snapshot(netplay->save_state_fast(nullptr,0));
    require(snapshot.size()==netplay->get_max_state_size() &&
            netplay->save_state_fast(snapshot.data(),snapshot.size())==snapshot.size() &&
            snapshot[7]=='8',"frontend S038 snapshot");
    auto changed=advanced;changed[131072+19]=3;
    const uint64_t now=uint64_t(std::time(nullptr));
    for(unsigned n=0;n<8;++n)changed[131104+n]=uint8_t(now>>(56-8*n));
    require(core->set_battery_save_data(changed) &&
            netplay->load_state_fast(snapshot.data(),snapshot.size()) &&
            core->get_battery_save_data()[131072+19]==2,"frontend RTC rollback");
    manager.unload_rom();require(fs::file_size(save)==131112,"RTC battery disk size");
    manager.shutdown();
    emu::PluginManager fresh;fresh.set_paths_config(&paths);
    require(fresh.initialize((root/"plugins").string()) &&
            fresh.set_active_plugin(emu::PluginType::Emulator,"N64") &&
            fresh.load_rom(rom.string()),"fresh RTC load");
    core=fresh.get_active_plugin();
    auto reloaded=core->get_battery_save_data();
    require(reloaded.size()==131112 && reloaded[131072+19]==2 &&
            std::equal(content.begin(),content.begin()+131072,reloaded.begin()),
            "cross-session FlashRAM or RTC persistence");
    fresh.unload_rom();
    {std::ofstream file(save,std::ios::binary|std::ios::trunc);file.put(1);}
    require(fresh.load_rom(rom.string()) &&
            core->get_battery_save_data()[131072+19]==1,"malformed disk RTC save was accepted");
    fresh.unload_rom();
    {std::ofstream file(save,std::ios::binary|std::ios::trunc);
     file.write(reinterpret_cast<const char*>(content.data()),131072);}
    require(fresh.load_rom(rom.string()) &&
            core->get_battery_save_data()[131072+19]==1 &&
            std::equal(content.begin(),content.begin()+131072,
                       core->get_battery_save_data().begin()),
            "legacy raw FlashRAM save import");
    fresh.shutdown();
    std::cout<<"N64 RTC cross-session persistence, offline day, S038 rollback, malformed and legacy save PASS\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
