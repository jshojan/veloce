#include "core/input_manager.hpp"
#include <SDL.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <iostream>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
#if SDL_VERSION_ATLEAST(2,24,0)
struct Motor {int on=0;int off=0;};
static int SDLCALL rumble(void* context,Uint16 low,Uint16 high){
    auto& motor=*static_cast<Motor*>(context);
    if(low==0xffff && high==0xffff)++motor.on;
    if(low==0 && high==0)++motor.off;
    return 0;
}
static int attach(Motor& motor){
    SDL_VirtualJoystickDesc desc{};
    desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes=6;desc.nbuttons=16;
    desc.name="Veloce virtual rumble pad";
    desc.userdata=&motor;desc.Rumble=rumble;
    int index=SDL_JoystickAttachVirtualEx(&desc);
    require(index>=0,SDL_GetError());
    char guid[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index),guid,sizeof(guid));
    const std::string mapping=std::string(guid)+",Veloce Virtual Rumble,a:b0,b:b1,x:b2,y:b3,"
        "back:b4,start:b6,leftshoulder:b9,rightshoulder:b10,"
        "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,";
    require(SDL_GameControllerAddMapping(mapping.c_str())>=0,SDL_GetError());
    return index;
}
static void pump(emu::InputManager& input){
    SDL_Event event{};
    while(SDL_PollEvent(&event))input.process_event(event);
    input.update();
}
#endif
int main(){
#if !SDL_VERSION_ATLEAST(2,24,0)
    std::cout<<"SDL virtual rumble requires SDL >=2.24\n";
    return 77;
#else
    const auto original=std::filesystem::current_path();
    const auto scratch=std::filesystem::temp_directory_path()/
        ("veloce-rumble-"+std::to_string(SDL_GetPerformanceCounter()));
    try{
        std::filesystem::create_directories(scratch);
        std::filesystem::current_path(scratch);
        SDL_SetMainReady();
        require(SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)==0,SDL_GetError());
        Motor first{},second{};
        attach(first);attach(second);
        emu::InputManager input;
        require(input.initialize(),"input init");
        pump(input);
        require(input.get_connected_controller_count()==2,"virtual pads absent");
        input.set_controller_for_port(0,0);
        input.set_controller_for_port(2,1);
        const int first_on=first.on,second_on=second.on;
        input.set_rumble_for_port(2,true);
        require(first.on==first_on && second.on==second_on+1,"port 2 motor routed incorrectly");
        input.set_rumble_for_port(2,false);
        require(second.off>0,"port 2 motor did not stop");
        input.set_rumble_for_port(0,true);
        require(first.on==first_on+1,"port 0 motor did not start");
        const int first_off=first.off;
        input.set_controller_for_port(0,-1);
        require(first.off>first_off,"reassignment did not stop old motor");
        input.shutdown();
        SDL_Quit();
        std::filesystem::current_path(original);
        std::filesystem::remove_all(scratch);
        std::cout<<"N64 four-port SDL Rumble Pak output routing PASS\n";
        return 0;
    }catch(const std::exception& error){
        std::filesystem::current_path(original);
        std::cerr<<error.what()<<'\n';
        return 1;
    }
#endif
}
