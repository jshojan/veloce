#include "core/input_manager.hpp"
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

static void pump(emu::InputManager& input) {
    SDL_JoystickUpdate();
    SDL_Event event;
    while (SDL_PollEvent(&event)) input.process_event(event);
    input.update();
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
