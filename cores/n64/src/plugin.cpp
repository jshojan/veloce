#include "machine.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include "emu/netplay_plugin.hpp"

namespace n64 {
class Plugin : public emu::IEmulatorPlugin, public emu::INetplayCapable, public emu::IFrameScheduling {
public:
    emu::EmulatorInfo get_info() override {
        static const char* extensions[] = {".z64", ".v64", ".n64", nullptr};
        return {"N64", "0.59.0", "Veloce", "Experimental N64 core with MIPS III, CP0, RSP, partial RDP, SI/PIF controller polling, and functional 4/16 Kbit cartridge EEPROM with frontend battery persistence. Retail course entry, complete hardware and physical timing remain pending.",
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
    bool load_rom(const uint8_t* data, size_t size) override { return machine.load(data, size); }
    emu::FrameScheduling get_frame_scheduling() const override {return machine.frame_scheduling;}
    void set_frame_scheduling(emu::FrameScheduling mode) override {machine.frame_scheduling=mode;}
    void unload_rom() override { machine.rom.clear(); machine.crc = 0; machine.identity = 0; machine.entry = 0; machine.boot_mode=Machine::BootMode::Homebrew; machine.reset(); }
    bool is_rom_loaded() const override { return !machine.rom.empty(); }
    bool has_battery_save() const override { return is_rom_loaded() && machine.joybus.has_eeprom(); }
    std::vector<uint8_t> get_battery_save_data() const override {
        if(!has_battery_save())return {};
        const auto& eeprom=machine.joybus.eeprom_data();
        return {eeprom.begin(),eeprom.begin()+machine.joybus.eeprom_bytes()};
    }
    bool set_battery_save_data(const std::vector<uint8_t>& data) override {
        // A native cartridge with an existing 16 Kbit save supplies its media
        // size before the first guest SI command. New native images still use
        // the conservative 6102/4 Kbit profile until cartridge metadata exists.
        if(machine.boot_mode==Machine::BootMode::Native6102 &&
           machine.joybus.eeprom_bytes()==512 && data.size()==2048)
            machine.joybus.configure_eeprom(2048);
        if(!has_battery_save() || data.size()!=machine.joybus.eeprom_bytes())return false;
        auto content=machine.joybus.eeprom_data();
        std::copy(data.begin(),data.end(),content.begin());
        machine.joybus.set_eeprom(content);
        return true;
    }
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
    size_t get_max_state_size() const override {return is_loaded()?Machine::current_state_size:0;}
    size_t save_state_fast(uint8_t* buffer,size_t capacity) override {
        if(!is_loaded())return 0;
        if(!buffer)return Machine::current_state_size;
        if(capacity<Machine::current_state_size || !machine.save(state_scratch) || state_scratch.size()!=Machine::current_state_size)return 0;
        std::memcpy(buffer,state_scratch.data(),state_scratch.size());return state_scratch.size();
    }
    bool load_state_fast(const uint8_t* buffer,size_t size) override {
        if(!is_loaded() || !buffer || size<8 || size>Machine::current_state_size)return false;
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
