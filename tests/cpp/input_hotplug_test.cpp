#include "core/input_manager.hpp"
#include "emu/input_plugin.hpp"
#include "emu/n64_input.hpp"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <string>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

#if SDL_VERSION_ATLEAST(2, 0, 14)
static int attach() {
    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 16, 0);
    require(index >= 0, SDL_GetError());
    char guid[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
    std::string mapping = std::string(guid) + ",Veloce Virtual Pad,a:b0,b:b1,x:b2,y:b3,"
        "back:b4,start:b6,leftshoulder:b9,rightshoulder:b10,"
        "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,";
    require(SDL_GameControllerAddMapping(mapping.c_str()) >= 0, SDL_GetError());
    require(SDL_IsGameController(index), "virtual pad has no game-controller mapping");
    return index;
}

struct MappedHost final : emu::IInputHost, emu::IMappedInputHost, emu::IControllerInputEncodingHost {
    emu::InputManager& input;
    explicit MappedHost(emu::InputManager& i):input(i) {}
    uint64_t get_frame_number() const override{return 0;}
    const char* get_platform_name() const override{return input.get_current_platform().c_str();}
    int get_controller_count() const override{return 4;}
    bool is_key_pressed(int c) const override{return input.is_key_pressed(c);}
    bool is_gamepad_button_pressed(int c,int b) const override{return input.is_gamepad_button_pressed(c,b);}
    float get_gamepad_axis(int c,int a) const override{return input.get_gamepad_axis(c,a);}
    int get_connected_controller_count() const override{return input.get_connected_controller_count();}
    emu::ControllerInfo get_controller_info(int) const override{return {-1,"",false,0,0,0};}
    uint32_t get_mapped_input(int c) const override{return input.get_button_state(c);}
    uint32_t encode_controller_input(int c,uint32_t b) const override{return input.encode_controller_input(c,b);}
    bool get_mapped_binding(int c,emu::VirtualButton button,emu::InputBinding& binding) const override{
        auto* b=input.get_binding(button,c);if(!b)return false;binding=*b;
        if(c>0 && binding.type!=emu::InputSourceType::Keyboard){binding.device_id=input.get_controller_for_port(c);if(binding.device_id<0)binding.device_id=-2;return true;}
        if(binding.type!=emu::InputSourceType::Keyboard && binding.device_id>=0){
            int instance=binding.device_id;binding.device_id=-2;
            for(int i=0;i<input.get_connected_controller_count();++i)if(input.get_controller_instance_id(i)==instance){binding.device_id=i;break;}
        }
        return true;
    }
};

static void pump(emu::InputManager& input) {
    SDL_JoystickUpdate();
    SDL_Event event;
    while (SDL_PollEvent(&event)) input.process_event(event);
    input.update();
    MappedHost host(input);
    auto* plugin=create_input_plugin();
    require(plugin->initialize(&host), "mapped plugin initialization failed");
    plugin->begin_frame();
    std::array<uint32_t,4> states{};for(int port=0;port<4;++port)states[port]=plugin->get_input_state(port);
    for(int port=0;port<4;++port)for(unsigned i=0;i<static_cast<unsigned>(emu::VirtualButton::COUNT);++i){
        auto button=static_cast<emu::VirtualButton>(i);auto* b=input.get_binding(button,port);
        if(b){emu::InputBinding expected{};host.get_mapped_binding(port,button,expected);auto actual=plugin->get_binding(port,button);require(actual==expected && actual.axis_threshold==expected.axis_threshold && actual.axis_positive==expected.axis_positive,"plugin binding differs from saved profile");}
    }
    plugin->shutdown();destroy_input_plugin(plugin);
    for(int port=0;port<4;++port)require(states[port]==input.encode_controller_input(port,input.get_button_state(port)), "default plugin lost mapped hot-plug/profile input");
}
#endif

int main() {
#if !SDL_VERSION_ATLEAST(2, 0, 14)
    std::cout << "SDL virtual controllers require SDL >=2.0.14\n";
    return 77;
#else
    const auto old_cwd = std::filesystem::current_path();
    const auto scratch = std::filesystem::temp_directory_path() /
        ("veloce-input-" + std::to_string(SDL_GetPerformanceCounter()));
    int result = 0;
    try {
        std::filesystem::create_directories(scratch);
        std::filesystem::current_path(scratch);
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetMainReady();
        require(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) == 0, SDL_GetError());
        int first = attach();
        int second = attach();
        emu::InputManager input;
        require(input.initialize(), "input initialization failed");
        pump(input);
        require(input.get_connected_controller_count() == 2, "identical pads were deduplicated");
        require(input.get_controller_instance_id(0) != input.get_controller_instance_id(1),
                "two pads share a connection ID");
        SDL_Event duplicate{};
        duplicate.type = SDL_CONTROLLERDEVICEADDED;
        duplicate.cdevice.which = first;
        input.process_event(duplicate);
        require(input.get_connected_controller_count() == 2, "duplicate event added a pad twice");

        SDL_Joystick* joystick = SDL_JoystickOpen(second);
        require(joystick != nullptr, SDL_GetError());
        const int old_id = input.get_controller_instance_id(1);
        input.set_binding(emu::VirtualButton::A,
            {emu::InputSourceType::GamepadButton, old_id, SDL_CONTROLLER_BUTTON_B, 0, false});
        require(SDL_JoystickSetVirtualButton(joystick, 1, 1) == 0, SDL_GetError());
        pump(input);
        require(input.is_button_pressed(emu::VirtualButton::A), "custom gamepad binding did not fire");
        require(!input.is_button_pressed(emu::VirtualButton::B), "default mapping overrode custom binding");
        input.set_input_capture_mode(true);
        pump(input);
        require(input.get_button_state() == 0, "capture mode leaked input to game");
        input.set_input_capture_mode(false);
        input.set_active_controller(1);
        require(input.save_platform_config("NES"), "could not save controller identity");

        require(SDL_JoystickDetachVirtual(first) == 0, SDL_GetError());
        pump(input);
        require(input.get_active_controller() == 0, "selection changed when preceding pad was removed");
        require(input.is_button_pressed(emu::VirtualButton::A), "remaining pad lost its binding");
        SDL_JoystickClose(joystick);
        require(SDL_JoystickDetachVirtual(0) == 0, SDL_GetError());
        pump(input);
        require(input.get_connected_controller_count() == 0 && input.get_button_state() == 0,
                "disconnect retained a controller or held button");
        require(input.get_active_controller() == -1, "removed selection did not fall back to keyboard");

        first = attach();
        pump(input);
        require(input.get_binding(emu::VirtualButton::A)->device_id == input.get_controller_instance_id(0),
                "unique-model reconnect did not restore custom binding");
        require(input.get_controller_instance_id(0) != old_id, "reconnect reused connection ID");
        joystick = SDL_JoystickOpen(first);
        require(joystick != nullptr, SDL_GetError());
        require(SDL_JoystickSetVirtualButton(joystick, 1, 1) == 0, SDL_GetError());
        require(SDL_JoystickSetVirtualAxis(joystick, 0, -32768) == 0, SDL_GetError());
        pump(input);
        require(input.is_button_pressed(emu::VirtualButton::A), "reconnected pad did not drive input");
        require(input.get_gamepad_axis(0, SDL_CONTROLLER_AXIS_LEFTX) == -1.0f,
                "negative axis endpoint exceeded normalized range");
        require(SDL_JoystickSetVirtualAxis(joystick, 0, 32767) == 0, SDL_GetError());
        pump(input);
        require(input.get_gamepad_axis(0, SDL_CONTROLLER_AXIS_LEFTX) == 1.0f,
                "positive axis endpoint did not normalize to one");

        input.shutdown();
        require(input.get_button_state() == 0 && !input.is_key_pressed(SDL_SCANCODE_Z),
                "shutdown retained input state");
        require(input.initialize(), "input reinitialization failed");
        pump(input);
        require(input.is_button_pressed(emu::VirtualButton::A), "saved identity did not survive restart");
        second = attach();
        pump(input);
        input.shutdown();
        require(input.initialize(), "ambiguous input initialization failed");
        pump(input);
        require(input.get_binding(emu::VirtualButton::A)->device_id == -2,
                "ambiguous model silently bound to a different physical pad");
        require(input.get_button_state() == 0, "ambiguous saved binding leaked input");
        input.shutdown();
        // N64's appended controls must remain configurable and survive a platform switch.
        input.initialize();
        input.set_current_platform("N64");
        require(emu::InputManager::get_platform_buttons("N64").size()==14, "N64 controls missing");
        require(input.get_binding(emu::VirtualButton::Z)->code==SDL_SCANCODE_C &&
                input.get_binding(emu::VirtualButton::CUp)->code==SDL_SCANCODE_I &&
                !input.get_binding(emu::VirtualButton::Select), "N64 keyboard defaults wrong");
        input.set_active_controller(0);
        require(SDL_JoystickSetVirtualAxis(joystick,0,-32768)==0 &&
                SDL_JoystickSetVirtualAxis(joystick,1,32767)==0,SDL_GetError());
        pump(input);
        auto analog=input.encode_controller_input(0,1);
        require(analog==0xb0b0c001,"N64 selected pad axis direction/endpoint wrong");
        input.set_input_capture_mode(true);
        require(input.encode_controller_input(0,1)==0,"N64 analog leaked during mapping capture");
        input.set_input_capture_mode(false);
        require(SDL_JoystickSetVirtualAxis(joystick,0,4000)==0 &&
                SDL_JoystickSetVirtualAxis(joystick,1,-4000)==0,SDL_GetError());
        pump(input);require(input.encode_controller_input(0,1)==0xc001,"N64 dead zone not neutral");
        input.set_active_controller(-1);require(input.encode_controller_input(0,1)==0xc001,"keyboard selection retained pad analog");
        input.set_binding(emu::VirtualButton::CRight,
            {emu::InputSourceType::GamepadButton, input.get_controller_instance_id(0), SDL_CONTROLLER_BUTTON_Y, 0, false});
        require(input.save_platform_config("N64"), "N64 binding save failed");
        input.set_current_platform("NES"); input.set_current_platform("N64");
        require(input.get_binding(emu::VirtualButton::CRight)->code==SDL_CONTROLLER_BUTTON_Y &&
                emu::InputManager::get_button_name(emu::VirtualButton::CRight)=="CRight", "N64 binding lost");
        // Two identical connections may be explicitly assigned independently.
        input.load_default_bindings_for_platform("N64");
        input.set_controller_for_port(1,0);
        require(input.get_active_controller()==-1 && input.get_controller_for_port(1)==0,"port move retained primary selection");
        require(SDL_JoystickSetVirtualButton(joystick,1,1)==0,SDL_GetError());
        require(SDL_JoystickSetVirtualAxis(joystick,0,-32768)==0,SDL_GetError());
        pump(input);
        require(input.get_button_state()==0 && emu::is_button_pressed(input.get_button_state(1),emu::VirtualButton::B),"secondary digital input leaked to primary");
        require(input.encode_controller_input(1,input.get_button_state(1))==0x00b0c002,"assigned secondary analog did not follow device");
        require(input.encode_controller_input(2,0)==0xc000,"unassigned port borrowed another stick");
        input.set_input_capture_mode(true);
        require(input.get_button_state(1)==0 && input.encode_controller_input(1,1)==0,"secondary capture leaked input");
        input.set_input_capture_mode(false);
        input.set_controller_for_port(2,0);pump(input);
        require(input.get_controller_for_port(1)==-1 && input.get_button_state(1)==0 && input.get_button_state(2)!=0,"moving device duplicated its port");
        input.set_binding(emu::VirtualButton::A,{emu::InputSourceType::GamepadButton,999,SDL_CONTROLLER_BUTTON_B,0,false},2);
        input.clear_binding(emu::VirtualButton::B,2);
        input.set_binding(emu::VirtualButton::Z,{emu::InputSourceType::Keyboard,-1,SDL_SCANCODE_V,0,false},3);
        pump(input);
        require(input.get_button_state(2)==emu::button_to_mask(emu::VirtualButton::A),"remapped secondary fired cleared default");
        require(input.get_binding(emu::VirtualButton::Z)->code==SDL_SCANCODE_C,"secondary keyboard profile changed primary");
        emu::InputManager::StickProfile stick;stick.axes={1,-1};stick.inverted={true,false};stick.deadzone=0;stick.range=127;
        require(input.set_stick_profile(2,stick),"secondary analog profile setter");
        require(SDL_JoystickSetVirtualAxis(joystick,1,-32768)==0,SDL_GetError());pump(input);
        require(input.encode_controller_input(2,input.get_button_state(2))==0x007fc001,"remapped stick did not reach default input plugin");
        auto bad_stick=stick;bad_stick.axes[0]=6;require(!input.set_stick_profile(2,bad_stick),"invalid source accepted");
        bad_stick=stick;bad_stick.deadzone=1;require(!input.set_stick_profile(2,bad_stick),"invalid dead zone accepted");
        bad_stick=stick;bad_stick.range=128;require(!input.set_stick_profile(2,bad_stick) && !input.set_stick_profile(-1,stick) && !input.get_stick_profile(4),"invalid range/port accepted");
        require(input.save_platform_config("N64"),"secondary persistence save");
        input.shutdown();input.initialize();pump(input);
        require(input.get_controller_for_port(2)==-1,"ambiguous identical model restored secondary assignment");
        require(input.get_stick_profile(2)->axes==stick.axes && input.get_stick_profile(2)->range==127 && input.encode_controller_input(2,0)==0xc000,"restart/disconnected analog profile changed or leaked input");
        require(input.get_binding(emu::VirtualButton::A,2)->code==SDL_CONTROLLER_BUTTON_B && !input.get_binding(emu::VirtualButton::B,2) &&
            input.get_binding(emu::VirtualButton::Z,3)->code==SDL_SCANCODE_V,"restart changed independent maps or cleared binding");

        require(SDL_JoystickDetachVirtual(1)==0,SDL_GetError());pump(input);
        input.shutdown();input.initialize();pump(input);
        require(input.get_controller_for_port(2)==0 && input.get_button_state(2)!=0,"unique saved secondary identity did not restore");
        require(input.encode_controller_input(2,input.get_button_state(2))==0x007fc001,"unique restored analog source/profile changed");
        input.reset_port_bindings(1);
        require(input.get_stick_profile(2)->range==127,"reset altered another player stick");
        require(input.get_binding(emu::VirtualButton::A,2)->code==SDL_CONTROLLER_BUTTON_B && !input.get_binding(emu::VirtualButton::B,2) &&
            input.get_binding(emu::VirtualButton::Z,3)->code==SDL_SCANCODE_V,"reset changed another player profile");
        input.set_binding(emu::VirtualButton::A,{emu::InputSourceType::GamepadAxis,-1,SDL_CONTROLLER_AXIS_MAX,.5f,true},2);
        require(input.get_binding(emu::VirtualButton::A,2)->type==emu::InputSourceType::GamepadButton,"invalid binding replaced live player profile");
        nlohmann::json saved_profile;{std::ifstream file("config/input_n64.json");file>>saved_profile;}
        for(bool axis:{false,true}) {
            auto bad=saved_profile;
            if(axis)bad["n64_sticks"][0]["axes"][0]=4294967296ull;
            else bad["n64_sticks"][0]["range"]=4294967376ull;
            {std::ofstream file("config/input_n64.json");file<<bad;}
            require(!input.load_platform_config("N64") && input.get_stick_profile(2)->range==127 && input.get_controller_for_port(2)==0,"oversized stick integer accepted or changed live configuration");
        }
        for(int mutant=0;mutant<7;++mutant) {
            auto bad=saved_profile;
            auto& map=bad["player_bindings"][1]["A"];
            switch(mutant) {
                case 0:bad["player_bindings"]=nlohmann::json::array();break;
                case 1:map["type"]="Invalid";break;
                case 2:map["code"]=-1;break;
                case 3:map["code"]=SDL_CONTROLLER_BUTTON_MAX;break;
                case 4:map["axis_threshold"]=1.01;break;
                case 5:map["axis_threshold"]=nullptr;break;
                case 6:bad["player_bindings"][1]["Unknown"] = map;break;
            }
            {std::ofstream file("config/input_n64.json");file<<bad.dump(2);}
            require(!input.load_platform_config("N64"),"invalid player profile accepted");
            require(input.get_controller_for_port(2)==0 && input.get_button_state(2)==emu::button_to_mask(emu::VirtualButton::A) &&
                input.get_binding(emu::VirtualButton::A,2)->code==SDL_CONTROLLER_BUTTON_B && !input.get_binding(emu::VirtualButton::B,2),"invalid player profile mutated live ownership/map/state");
        }
        saved_profile.erase("player_bindings");
        {std::ofstream file("config/input_n64.json");file<<saved_profile.dump(2);}
        require(input.load_platform_config("N64"),"legacy profile migration failed");pump(input);
        require(input.get_binding(emu::VirtualButton::A,2)->code==SDL_CONTROLLER_BUTTON_A &&
            input.get_binding(emu::VirtualButton::B,2)->code==SDL_CONTROLLER_BUTTON_B,"legacy extra players did not receive defaults");
        input.set_active_controller(0);pump(input);
        require(input.get_controller_for_port(2)==-1 && input.get_button_state(2)==0,"primary reselection duplicated secondary device");

        input.shutdown();
        std::filesystem::remove("config/input_n64.json");
        input.initialize();
        require(input.get_binding(emu::VirtualButton::Z)->code==SDL_SCANCODE_C &&
            input.get_binding(emu::VirtualButton::Z,2)->code==SDL_CONTROLLER_AXIS_TRIGGERLEFT,"missing config used another platform's defaults");
        for(const std::string platform:{"GameCube","Wii"}) {
            input.set_current_platform(platform);
            require(emu::InputManager::get_platform_buttons(platform).size()==12,"GameCube digital controls missing");
            require(input.get_binding(emu::VirtualButton::Z)->code==SDL_SCANCODE_C &&
                    input.get_binding(emu::VirtualButton::X)->code==SDL_SCANCODE_A &&
                    input.get_binding(emu::VirtualButton::Y)->code==SDL_SCANCODE_S &&
                    !input.get_binding(emu::VirtualButton::Select),"GameCube keyboard defaults wrong");
            input.set_binding(emu::VirtualButton::Z,
                {emu::InputSourceType::Keyboard,-1,SDL_SCANCODE_V,0,false});
            require(input.save_platform_config(platform),"GameCube input persistence failed");
            input.set_current_platform("NES");input.set_current_platform(platform);
            require(input.get_binding(emu::VirtualButton::Z)->code==SDL_SCANCODE_V,"GameCube Z binding lost");
        }
        input.shutdown();
        SDL_JoystickClose(joystick);
        std::cout << "Input hotplug: identical pads, custom bindings, reconnect, persistence, "
                     "capture, axis endpoints and ambiguity verified\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << " (SDL: " << SDL_GetError() << ")\n";
        result = 1;
    }
    SDL_Quit();
    std::filesystem::current_path(old_cwd);
    std::filesystem::remove_all(scratch);
    return result;
#endif
}
