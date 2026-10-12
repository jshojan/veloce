#include "machine.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <charconv>
#include <fstream>
#include <iterator>
#include <string>
#include "emu/controller_pak_storage.hpp"
#include "emu/rumble_output.hpp"
#include "emu/netplay_plugin.hpp"
#ifdef VELOCE_N64_IMGUI
#include <imgui.h>
#endif

namespace n64 {
class Plugin : public emu::IEmulatorPlugin, public emu::INetplayCapable,
               public emu::IFrameScheduling, public emu::IControllerPakStorage,
               public emu::IRumbleOutput {
public:
    emu::EmulatorInfo get_info() override {
        static const char* extensions[] = {".z64", ".v64", ".n64", nullptr};
        return {"N64", "0.92.0", "Veloce", "Experimental N64 core with cached idle-loop, active-RSP clock and interrupt polling acceleration and inactive-clock acceleration, MIPS III, CP0, RSP with host vector acceleration where available and optional local GCC profile-guided compilation, faster exact RDP perspective division, RGBA16 quad decode and settled DP clock acceleration, partial RDP including bounded two-cycle fog and PASS blender modes, dual-texel color/alpha and filtering plus four-axis affine subtexel LOD and per-pixel perspective LOD with bounded mixed S/T/W gradients and division-overflow clamp, detail/sharpen tile selection and minimum-LOD fractions, SI/PIF controller polling, per-port Controller or Rumble Paks, 4/16 Kbit EEPROM, 32/96 KiB SRAM, 128 KiB FlashRAM and functional cartridge RTC. Bounded retail course movement passes; complete hardware, full-title playability and physical timing remain pending.",
                extensions, 60.0, 93750000, 320, 240};
    }
    const emu::ControllerLayoutInfo* get_controller_layout() override {
        using emu::VirtualButton;
        static const emu::ButtonLayout buttons[] = {
            {VirtualButton::Up,"Up",.18f,.32f,.09f,.12f,true},
            {VirtualButton::Down,"Down",.18f,.57f,.09f,.12f,true},
            {VirtualButton::Left,"Left",.08f,.45f,.09f,.12f,true},
            {VirtualButton::Right,"Right",.28f,.45f,.09f,.12f,true},
            {VirtualButton::A,"A",.65f,.68f,.1f,.12f,false},
            {VirtualButton::B,"B",.56f,.48f,.1f,.12f,false},
            {VirtualButton::Z,"Z",.45f,.74f,.1f,.12f,false},
            {VirtualButton::Start,"Start",.45f,.40f,.1f,.12f,false},
            {VirtualButton::L,"L",.10f,.06f,.15f,.12f,false},
            {VirtualButton::R,"R",.75f,.06f,.15f,.12f,false},
            {VirtualButton::CUp,"C Up",.82f,.25f,.1f,.12f,false},
            {VirtualButton::CDown,"C Down",.82f,.53f,.1f,.12f,false},
            {VirtualButton::CLeft,"C Left",.71f,.39f,.1f,.12f,false},
            {VirtualButton::CRight,"C Right",.93f,.39f,.1f,.12f,false}};
        static const emu::ControllerLayoutInfo layout = {"N64","N64 digital and analog controls",
            emu::ControllerShape::Modern,2.0f,buttons,14,4};
        return &layout;
    }
    bool load_rom(const uint8_t* data, size_t size) override {
        if(!machine.load(data,size))return false;
        machine.joybus.configure_pak_mask(configured_paks);
        machine.joybus.configure_rumble_mask(configured_rumble);
        return true;
    }
    emu::FrameScheduling get_frame_scheduling() const override {return machine.frame_scheduling;}
    void set_frame_scheduling(emu::FrameScheduling mode) override {machine.frame_scheduling=mode;}
    void unload_rom() override { machine.rom.clear(); machine.crc = 0; machine.identity = 0; machine.entry = 0; machine.boot_mode=Machine::BootMode::Homebrew; machine.reset(); }
    bool is_rom_loaded() const override { return !machine.rom.empty(); }
    bool has_battery_save() const override { return is_rom_loaded() && (machine.joybus.has_eeprom() || !machine.sram.empty() || machine.flash.enabled); }
    std::vector<uint8_t> get_battery_save_data() const override {
        if(!has_battery_save())return {};
        if(machine.flash.enabled){
            std::vector<uint8_t> content(machine.flash.data.begin(),machine.flash.data.end());
            if(machine.joybus.rtc.present){
                content.insert(content.end(),machine.joybus.rtc.ram.begin(),machine.joybus.rtc.ram.end());
                const uint64_t timestamp=uint64_t(std::time(nullptr));
                for(unsigned n=0;n<8;++n)content.push_back(uint8_t(timestamp>>(56-8*n)));
            }
            return content;
        }
        if(!machine.sram.empty())return machine.sram;
        const auto& eeprom=machine.joybus.eeprom_data();
        return {eeprom.begin(),eeprom.begin()+machine.joybus.eeprom_bytes()};
    }
    bool set_battery_save_data(const std::vector<uint8_t>& data) override {
        if(!is_rom_loaded())return false;
        if(machine.boot_mode==Machine::BootMode::Native6102 && !machine.flash.enabled &&
           machine.sram.empty() && data.size()==FlashRam::bytes){
            machine.flash.configure(true);
            machine.joybus.configure_eeprom(0);
        }
        if(machine.flash.enabled){
            const bool rtc=machine.joybus.rtc.present;
            if(data.size()!=FlashRam::bytes && !(rtc && data.size()==FlashRam::bytes+40))return false;
            CartridgeRtc incoming=machine.joybus.rtc;
            if(rtc && data.size()==FlashRam::bytes+40){
                std::copy_n(data.begin()+FlashRam::bytes,32,incoming.ram.begin());
                if(!incoming.valid_time())return false;
                uint64_t timestamp=0;
                for(unsigned n=0;n<8;++n)timestamp=(timestamp<<8)|data[FlashRam::bytes+32+n];
                const uint64_t now=uint64_t(std::time(nullptr));
                if(now>timestamp)incoming.advance(now-timestamp);
            }
            std::copy_n(data.begin(),FlashRam::bytes,machine.flash.data.begin());
            machine.joybus.rtc=incoming;return true;
        }
        // Existing saves can identify a native board absent from the ID table.
        if(machine.boot_mode==Machine::BootMode::Native6102 && machine.sram.empty() &&
           (data.size()==32*1024 || data.size()==96*1024)){
            machine.sram.assign(data.size(),0xff);
            machine.joybus.configure_eeprom(0);
        }
        if(!machine.sram.empty()){
            if(data.size()!=machine.sram.size())return false;
            machine.sram=data;return true;
        }
        // An existing 16 Kbit save also selects that media for a native cart
        // whose header ID is absent from the known 16 Kbit table.
        if(machine.boot_mode==Machine::BootMode::Native6102 &&
           machine.joybus.eeprom_bytes()==512 && data.size()==2048)
            machine.joybus.configure_eeprom(2048);
        if(!has_battery_save() || data.size()!=machine.joybus.eeprom_bytes())return false;
        auto content=machine.joybus.eeprom_data();
        std::copy(data.begin(),data.end(),content.begin());
        machine.joybus.set_eeprom(content);
        return true;
    }
    unsigned controller_pak_ports() const override {return 4;}
    bool rumble_active(unsigned port) const override {
        return is_rom_loaded() && port<4 && (machine.joybus.active_motors()&(1u<<port));
    }
    bool controller_pak_inserted(unsigned port) const override {
        return is_rom_loaded() && port<4 && (machine.joybus.inserted_paks()&(1u<<port));
    }
    std::vector<uint8_t> get_controller_pak_data(unsigned port) const override {
        if(!controller_pak_inserted(port))return {};
        const auto& storage=machine.joybus.pak_storage();
        return {storage.begin()+port*32768,storage.begin()+(port+1)*32768};
    }
    bool set_controller_pak_data(unsigned port,const std::vector<uint8_t>& data) override {
        if(!controller_pak_inserted(port) || data.size()!=32768)return false;
        auto storage=machine.joybus.pak_storage();
        std::copy(data.begin(),data.end(),storage.begin()+port*32768);
        machine.joybus.set_pak_storage(storage);
        return true;
    }
    bool save_config(const char* path) override {
        std::ofstream file(path,std::ios::trunc);
        if(!file)return false;
        file<<"{\"controller_paks\":"<<unsigned(configured_paks)
            <<",\"rumble_paks\":"<<unsigned(configured_rumble)<<"}\n";
        return bool(file);
    }
    bool load_config(const char* path) override {
        std::ifstream file(path);
        if(!file)return true;
        const std::string contents{std::istreambuf_iterator<char>(file),{}};
        const auto key=contents.find("\"controller_paks\"");
        if(key==std::string::npos)return false;
        const auto colon=contents.find(':',key+17);
        if(colon==std::string::npos)return false;
        const char* begin=contents.data()+colon+1;
        const char* end=contents.data()+contents.size();
        while(begin<end && (*begin==' ' || *begin=='\t'))++begin;
        unsigned mask=0;
        const auto result=std::from_chars(begin,end,mask);
        if(result.ec!=std::errc{} || mask>15)return false;
        unsigned rumble=0;
        const auto rumble_key=contents.find("\"rumble_paks\"");
        if(rumble_key!=std::string::npos){
            const auto rumble_colon=contents.find(':',rumble_key+13);
            if(rumble_colon==std::string::npos)return false;
            begin=contents.data()+rumble_colon+1;
            while(begin<end && (*begin==' ' || *begin=='\t'))++begin;
            const auto parsed=std::from_chars(begin,end,rumble);
            if(parsed.ec!=std::errc{} || rumble>15 || (rumble&mask))return false;
        }
        configured_paks=uint8_t(mask);
        configured_rumble=uint8_t(rumble);
        return true;
    }
#ifdef VELOCE_N64_IMGUI
    bool has_config_gui() const override {return true;}
    void set_imgui_context(void* context) override {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));
    }
    void render_config_gui(bool& visible) override {
        if(ImGui::Begin("Nintendo 64 Settings",&visible))render_config_gui_content();
        ImGui::End();
    }
    void render_config_gui_content() override {
        ImGui::TextUnformatted("Controller accessories");
        for(unsigned port=0;port<4;++port){
            int accessory=(configured_paks&(1u<<port))?1:(configured_rumble&(1u<<port))?2:0;
            const std::string label="Port "+std::to_string(port+1);
            if(ImGui::Combo(label.c_str(),&accessory,"None\0Controller Pak\0Rumble Pak\0")){
                configured_paks&=uint8_t(~(1u<<port));
                configured_rumble&=uint8_t(~(1u<<port));
                if(accessory==1)configured_paks|=uint8_t(1u<<port);
                if(accessory==2)configured_rumble|=uint8_t(1u<<port);
            }
        }
        if(is_rom_loaded())ImGui::TextUnformatted("Reload the ROM to apply accessory changes.");
    }
    const char* get_config_window_name() const override {return "Nintendo 64 Settings";}
#endif
    uint32_t get_rom_crc32() const override { return machine.crc; }
    void reset() override { machine.reset(); }
    void run_frame(const emu::InputState& input) override {
        machine.input_buttons=input.buttons;machine.other_input_buttons.fill(0);step_quantum();
    }
    int get_max_players() const override {return 4;}
    void run_frame_netplay(uint32_t host,uint32_t client) override {
        machine.input_buttons=host;machine.other_input_buttons={client,0,0};step_quantum(true);
    }
    void run_frame_netplay_n(const std::vector<uint32_t>& inputs) override {
        machine.input_buttons=inputs.empty()?0:inputs[0];
        for(unsigned port=1;port<4;++port)machine.other_input_buttons[port-1]=port<inputs.size()?inputs[port]:0;
        step_quantum(true);
    }
    size_t get_max_state_size() const override {return is_loaded()?machine.state_size():0;}
    size_t save_state_fast(uint8_t* buffer,size_t capacity) override {
        if(!is_loaded())return 0;
        if(!buffer)return machine.state_size();
        if(capacity<machine.state_size() || !machine.save(state_scratch) || state_scratch.size()!=machine.state_size())return 0;
        std::memcpy(buffer,state_scratch.data(),state_scratch.size());return state_scratch.size();
    }
    bool load_state_fast(const uint8_t* buffer,size_t size) override {
        if(!is_loaded() || !buffer || size<8 || size>Machine::rtc_state_size)return false;
        return machine.restore(std::vector<uint8_t>(buffer,buffer+size));
    }
    uint64_t get_state_hash() const override {
        if(!machine.save(state_scratch))return 0;
        uint64_t hash=0xcbf29ce484222325ULL;
        for(auto byte:state_scratch)hash=(hash^byte)*0x100000001b3ULL;
        return hash;
    }
    uint64_t get_cycle_count() const override { return machine.cpu.state.instructions; }
    uint64_t get_frame_count() const override { return machine.frames; }
    emu::FrameBuffer get_framebuffer() override {int width=0,height=0;machine.video_interface.render(machine.ram,pixels,width,height);return {pixels.data(),width,height};}
    emu::AudioBuffer get_audio() override {
        audio.resize(size_t(machine.audio_count)*2);
        for(size_t n=0;n<audio.size();++n) {const uint16_t raw=machine.audio_samples[(machine.audio_head*2+n)%(Machine::audio_capacity*2)];const int value=raw<32768?int(raw):int(raw)-65536;audio[n]=float(value)/32768.0f;}
        return {audio.empty()?nullptr:audio.data(),int(machine.audio_count),48000};
    }
    void clear_audio_buffer() override {machine.clear_audio();audio.clear();}
    uint8_t read_memory(uint16_t address) override { return machine.ram[address]; }
    void write_memory(uint16_t address, uint8_t value) override { machine.ram[address] = value; }
    bool save_state(std::vector<uint8_t>& data) override { return machine.save(data); }
    bool load_state(const std::vector<uint8_t>& data) override { return machine.restore(data); }
    bool set_test_sink(emu::ITestSink* sink) override { machine.sink = sink; return true; }
    const char* test_channels() const override { return "v64debug"; }
private:
    uint8_t configured_paks=0;
    uint8_t configured_rumble=0;
    bool is_loaded() const {return !machine.rom.empty();}
    void step_quantum(bool canonical_audio=false){
        if(!is_loaded())return;
        machine.run_frontend_frame();
        if(m_audio_callback){const auto a=get_audio();if(a.sample_count)m_audio_callback(a.samples,size_t(a.sample_count),a.sample_rate);clear_audio_buffer();}
        else if(canonical_audio)clear_audio_buffer();
    }
    Machine machine;
    mutable std::vector<uint8_t> state_scratch;
    std::vector<float> audio;
    std::vector<uint32_t> pixels;
};
}
extern "C" {
EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin() { return new n64::Plugin(); }
EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin) { delete plugin; }
EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version() { return EMU_PLUGIN_API_VERSION; }
}
