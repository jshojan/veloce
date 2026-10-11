#include "core/plugin_manager.hpp"
#include "core/paths_config.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 require(argc==4,"plugin, ROM and isolated artifact directory required");namespace fs=std::filesystem;auto library=fs::absolute(argv[1]);auto image=fs::absolute(argv[2]);auto root=fs::absolute(argv[3]);fs::create_directories(root/"plugins");fs::create_directories(root/"saves");fs::copy_file(library,root/"plugins"/library.filename(),fs::copy_options::overwrite_existing);auto rom=root/"persistent.vb";fs::copy_file(image,rom,fs::copy_options::overwrite_existing);fs::remove(root/"saves/persistent.sav");
 emu::PathsConfiguration paths;paths.initialize(root);paths.set_save_directory(root/"saves");emu::PluginManager manager;manager.set_paths_config(&paths);require(manager.initialize((root/"plugins").string()),"frontend plugin discovery");require(manager.set_active_plugin(emu::PluginType::Emulator,"VB") && manager.load_rom(rom.string()),"frontend cartridge load");auto* core=manager.get_active_plugin();require(core && core->has_battery_save(),"frontend save capability");
 std::vector<uint8_t> data(8192);for(unsigned n=0;n<data.size();++n)data[n]=uint8_t(n*37+11);require(core->set_battery_save_data(data),"set exact native battery");require(!core->set_battery_save_data({1}) && core->get_battery_save_data()==data,"bad battery length atomicity");core->reset();require(core->get_battery_save_data()==data,"frontend reset keeps SRAM");manager.unload_rom();require(!core->has_battery_save() && core->get_battery_save_data().empty() && !core->set_battery_save_data(data),"unloaded battery contract");
 std::ifstream file(root/"saves/persistent.sav",std::ios::binary);std::vector<uint8_t> disk{std::istreambuf_iterator<char>(file),{}};require(disk==data,"frontend wrote exact battery bytes");require(manager.load_rom(rom.string()) && manager.get_active_plugin()->get_battery_save_data()==data,"fresh frontend session restores battery");manager.shutdown();
 emu::PluginManager fresh;fresh.set_paths_config(&paths);require(fresh.initialize((root/"plugins").string()) && fresh.set_active_plugin(emu::PluginType::Emulator,"VB") && fresh.load_rom(rom.string()),"fresh manager load");require(fresh.get_active_plugin()->get_battery_save_data()==data,"cross-session file persistence");fresh.unload_rom();{std::ofstream corrupt(root/"saves/persistent.sav",std::ios::binary);corrupt.put(1);}
 require(fresh.load_rom(rom.string()) && fresh.get_active_plugin()->get_battery_save_data()==std::vector<uint8_t>(8192,0),"malformed battery file must reject before mutation");fresh.shutdown();std::cout<<"Production frontend unload/reset/reload/shutdown battery persistence PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
