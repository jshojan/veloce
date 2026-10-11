#include "emu/tas_plugin.hpp"
#include <cstdlib>
#include <fstream>
extern "C" {
EMU_PLUGIN_EXPORT emu::ITASPlugin* create_tas_plugin(){if(auto* path=std::getenv("VELOCE_TAS_FACTORY_MARKER"))std::ofstream(path)<<"unsafe factory invoked";return nullptr;}
EMU_PLUGIN_EXPORT void destroy_tas_plugin(emu::ITASPlugin*){}
EMU_PLUGIN_EXPORT uint32_t get_tas_plugin_api_version(){return 1;}
}
