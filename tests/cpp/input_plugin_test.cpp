#include "emu/input_plugin.hpp"
#include <iostream>
#include <stdexcept>
struct Host : emu::IInputHost,emu::IMappedInputHost {
    uint64_t frame=0;uint32_t mask=1;bool key=false,pad=false;float axis=0;
    uint64_t get_frame_number() const override{return frame;}
    const char* get_platform_name() const override{return "NES";}
    int get_controller_count() const override{return 2;}
    bool is_key_pressed(int code) const override{return key && code==29;}
    bool is_gamepad_button_pressed(int controller,int button) const override{return pad && controller==1 && button==3;}
    float get_gamepad_axis(int controller,int) const override{return controller>=0 && controller<2?axis:0;}
    int get_connected_controller_count() const override{return 2;}
    emu::ControllerInfo get_controller_info(int) const override{return {-1,"",false,0,0,0};}
    uint32_t get_mapped_input(int controller) const override{return controller==0?mask:0;}
};
static void require(bool value){if(!value)throw std::runtime_error("input plugin mismatch");}
int main(){try{
    Host host;auto* p=create_input_plugin();require(p->initialize(&host));
    p->begin_frame();require(p->get_input_state(0)==1 && p->get_input_state(1)==0);
    p->set_binding(0,emu::VirtualButton::A,{emu::InputSourceType::Keyboard,-1,29,0,false});
    require(p->get_input_state(0)==0);host.key=true;require(p->get_input_state(0)==1);
    p->set_binding(0,emu::VirtualButton::B,{emu::InputSourceType::GamepadAxis,0,0,0.5f,true});host.axis=0.75f;
    require(p->get_input_state(0)==3);host.axis=-0.75f;require(p->get_input_state(0)==1);
    p->set_binding(0,emu::VirtualButton::B,{emu::InputSourceType::GamepadAxis,-1,0,0.5f,true});host.axis=0.75f;require(p->get_input_state(0)==3);host.axis=0;
    p->set_binding(0,emu::VirtualButton::CUp,{emu::InputSourceType::GamepadButton,-1,3,0,false});host.pad=true;require(p->get_input_state(0)==(1|(1u<<21)));host.pad=false;
    p->set_input_override(0,0x80000000);require(p->get_input_state(0)==0x80000000);p->clear_input_override(0);
    require(p->start_playback({{2,10},{256,12}}));host.frame=10;p->begin_frame();require(p->get_input_state(0)==2 && p->get_input_state(0)==2 && p->get_input_state(1)==0);
    host.frame=11;p->begin_frame();require(p->get_input_state(0)==2);host.frame=12;p->begin_frame();require(p->get_input_state(0)==256);
    host.frame=13;p->begin_frame();require(!p->is_playing() && p->get_input_state(0)==1);
    require(!p->start_playback({{1,-1}}) && !p->start_playback({{1,5},{2,4}}) && !p->start_playback({{1,5},{2,5}}));
    require(p->start_recording());host.frame=20;p->begin_frame();p->get_input_state(0);p->get_input_state(0);p->get_input_state(1);p->stop_recording();
    auto recording=p->get_recording();require(recording.size()==1 && recording[0].buttons==1 && recording[0].frame_number==20);
    require(p->start_recording());p->set_input_override(0,0x40000000);host.frame=21;p->begin_frame();p->get_input_state(0);p->stop_recording();
    require(p->get_recording().size()==1 && p->get_recording()[0].buttons==0x40000000);
    require(p->start_recording() && p->start_playback({{2,25}}));host.frame=0;p->begin_frame();require(!p->is_recording() && !p->is_playing());
    p->shutdown();destroy_input_plugin(p);
    // A fresh plugin exercises turbo independently of the previous test phases.
    p=create_input_plugin();require(p->initialize(&host));p->set_turbo_rate(2);p->set_turbo_enabled(0,emu::VirtualButton::A,true);
    for(unsigned i=0;i<8;++i){host.frame=i;p->begin_frame();require(p->get_input_state(0)==(((i+1)/2)%2?1u:0u));}
    p->shutdown();destroy_input_plugin(p);std::cout<<"Mapped bindings, overrides, turbo, controller-zero recording and stable sparse playback PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
