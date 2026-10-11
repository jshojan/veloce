#include "tas_host.hpp"
#include "application.hpp"
#include "plugin_manager.hpp"
#include "savestate_manager.hpp"
#include "audio_manager.hpp"
#include <stdexcept>
namespace emu {
void TASHost::capture_reset_state(){
    m_reset_state.clear();
    if(m_app.is_rom_loaded() && !m_app.m_plugin_manager->get_active_plugin()->save_state(m_reset_state))m_reset_state.clear();
}
void TASHost::reset_emulator(){
    if(!m_app.can_run_tas())return;
    if(m_reset_state.empty() || !m_app.m_plugin_manager->get_active_plugin()->load_state(m_reset_state))
        throw std::runtime_error("Core cannot restore its ROM-load movie boundary");m_app.reset_input_timeline(false);
    if(m_app.m_audio_manager)m_app.m_audio_manager->clear_buffer();
    m_app.m_plugin_manager->notify_game_plugins_reset();
}
void TASHost::pause_emulator(){m_app.pause();}void TASHost::resume_emulator(){m_app.resume();}
void TASHost::frame_advance(){m_app.frame_advance();}bool TASHost::is_emulator_paused() const{return m_app.is_paused();}
uint64_t TASHost::get_current_frame() const{return m_app.get_frame_count();}double TASHost::get_fps() const{return m_app.get_fps();}
void TASHost::set_controller_input(int c,uint32_t b){m_app.set_controller_input(c,b);}uint32_t TASHost::get_controller_input(int c) const{return m_app.get_local_input(c);}
bool TASHost::save_state(int slot){return m_app.can_run_tas() && slot>=0 && slot<10 && m_app.m_savestate_manager->save_state(slot);}
bool TASHost::load_state(int slot){
    if(!m_app.can_run_tas() || slot<0 || slot>9 || !m_app.m_savestate_manager->load_state(slot))return false;
    m_app.reset_input_timeline(false);m_app.m_audio_manager->clear_buffer();m_app.m_plugin_manager->notify_game_plugins_state_loaded();return true;
}
bool TASHost::save_state_to_buffer(std::vector<uint8_t>& b){return m_app.can_run_tas() && m_app.m_plugin_manager->get_active_plugin()->save_state(b);}
bool TASHost::load_state_from_buffer(const std::vector<uint8_t>& b){
    if(!m_app.can_run_tas() || !m_app.m_plugin_manager->get_active_plugin()->load_state(b))return false;
    m_app.reset_input_timeline(false);m_app.m_audio_manager->clear_buffer();m_app.m_plugin_manager->notify_game_plugins_state_loaded();return true;
}
bool TASHost::run_replay_frame(const TASFrameData& frame){return m_app.run_replay_frame(frame);}
const char* TASHost::get_rom_name() const{return m_app.get_rom_name();}uint32_t TASHost::get_rom_crc32() const{return m_app.get_rom_crc32();}
const char* TASHost::get_platform_name() const{return m_app.get_platform_name();}
uint8_t TASHost::read_memory(uint16_t address){return m_app.can_run_tas()?m_app.m_plugin_manager->get_active_plugin()->read_memory(address):0;}
void TASHost::write_memory(uint16_t address,uint8_t value){if(m_app.can_run_tas())m_app.m_plugin_manager->get_active_plugin()->write_memory(address,value);}
}
