#include "core/input_manager.hpp"
#include "emu/input_plugin.hpp"
#include <SDL.h>
#include <filesystem>
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

struct MappedHost final : emu::IInputHost, emu::IMappedInputHost {
    emu::InputManager& input;
    explicit MappedHost(emu::InputManager& i):input(i) {}
    uint64_t get_frame_number() const override{return 0;}
    const char* get_platform_name() const override{return input.get_current_platform().c_str();}
    int get_controller_count() const override{return 1;}
    bool is_key_pressed(int c) const override{return input.is_key_pressed(c);}
    bool is_gamepad_button_pressed(int c,int b) const override{return input.is_gamepad_button_pressed(c,b);}
    float get_gamepad_axis(int c,int a) const override{return input.get_gamepad_axis(c,a);}
    int get_connected_controller_count() const override{return input.get_connected_controller_count();}
    emu::ControllerInfo get_controller_info(int) const override{return {-1,"",false,0,0,0};}
    uint32_t get_mapped_input(int c) const override{return c==0?input.get_button_state():0;}
    bool get_mapped_binding(int c,emu::VirtualButton button,emu::InputBinding& binding) const override{
        auto* b=input.get_binding(button);if(c || !b)return false;binding=*b;
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
    const auto state=plugin->get_input_state(0);
    for(unsigned i=0;i<static_cast<unsigned>(emu::VirtualButton::COUNT);++i){
        auto button=static_cast<emu::VirtualButton>(i);auto* b=input.get_binding(button);
        if(b){emu::InputBinding expected{};host.get_mapped_binding(0,button,expected);auto actual=plugin->get_binding(0,button);require(actual==expected && actual.axis_threshold==expected.axis_threshold && actual.axis_positive==expected.axis_positive,"plugin binding differs from saved profile");}
    }
    plugin->shutdown();destroy_input_plugin(plugin);
    require(state==input.get_button_state(), "default plugin lost mapped hot-plug/profile input");
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
        input.set_binding(emu::VirtualButton::CRight,
            {emu::InputSourceType::GamepadButton, input.get_controller_instance_id(0), SDL_CONTROLLER_BUTTON_Y, 0, false});
        require(input.save_platform_config("N64"), "N64 binding save failed");
        input.set_current_platform("NES"); input.set_current_platform("N64");
        require(input.get_binding(emu::VirtualButton::CRight)->code==SDL_CONTROLLER_BUTTON_Y &&
                emu::InputManager::get_button_name(emu::VirtualButton::CRight)=="CRight", "N64 binding lost");
        input.shutdown();
        input.initialize();
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
