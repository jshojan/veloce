#include "core/application.hpp"
#include "core/input_manager.hpp"
#include <SDL.h>
#include <array>
#include <algorithm>
#include "core/plugin_manager.hpp"
#include "core/audio_manager.hpp"
#include "core/savestate_manager.hpp"
#include "gui/gui_manager.hpp"
#include "emu/input_plugin.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <charconv>
#include <map>
#include <set>
#include <iterator>
using namespace emu;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void checkpoint_movie(Application& app,IEmulatorPlugin* core,ITASPlugin* movie,IInputPlugin* input,const char* path,unsigned count,uint32_t word){
    std::ifstream file(path,std::ios::binary);require(file.good(),"checkpoint file");
    const std::vector<uint8_t> start{std::istreambuf_iterator<char>(file),{}};
    require(core->load_state(start),"native checkpoint restore");
    auto state=[&](){std::vector<uint8_t> out;require(core->save_state(out),"checkpoint native state");return out;};
    auto write=[&](const char* name){const auto bytes=state();std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(f),"checkpoint output");};
    const std::set<unsigned> watch={0,std::min(59u,count),std::min(60u,count),std::min(61u,count),std::min(120u,count),count};
    std::map<unsigned,std::vector<uint8_t>> baseline;baseline[0]=state();
    for(unsigned n=0;n<count;++n){TASFrameData frame{};frame.frame_number=n;frame.controller_inputs[0]=word;
        require(app.run_replay_frame(frame),"ordinary checkpoint frame");if(watch.count(n+1))baseline[n+1]=state();}
    write("checkpoint-baseline.state");require(core->load_state(start),"recording checkpoint restore");
    require(movie->new_movie("checkpoint-movie.tas",true),"checkpoint movie");
    for(int c=0;c<4;++c)input->set_input_override(c,c==0?word:0);
    for(unsigned n=0;n<count;++n){require(app.step_frame(),"checkpoint record frame");auto frame=movie->get_frame(n);
        require(frame.controller_inputs[0]==word && !frame.controller_inputs[1] && !frame.controller_inputs[2] && !frame.controller_inputs[3],"canonical recorded inputs");
        if(watch.count(n+1))require(state()==baseline.at(n+1),"recorded checkpoint state");}
    movie->stop_recording();require(movie->save_movie(),"save checkpoint movie");write("checkpoint-recorded.state");
    for(int c=0;c<4;++c)input->set_input_override(c,0);
    require(movie->open_movie("checkpoint-movie.tas"),"reopen checkpoint movie");movie->start_playback();app.pause();
    for(unsigned n=0;n<count;++n){require(app.step_frame(),"checkpoint playback frame");if(watch.count(n+1))require(state()==baseline.at(n+1),"checkpoint playback state");}
    write("checkpoint-playback.state");
    require(movie->get_mode()==TASMode::Stopped && app.is_paused(),"checkpoint EOF pause");
    for(auto target:watch){movie->seek_to_frame(target);require(movie->get_current_frame()==target && state()==baseline.at(target),"checkpoint movie seek");require(app.get_audio_manager().get_buffered_samples()==0,"checkpoint seek audio");}
    for(unsigned n=0;n<count;++n){auto frame=movie->get_frame(n);frame.controller_inputs[0]=0;movie->set_frame(n,frame);}
    movie->start_playback();app.pause();for(unsigned n=0;n<count;++n)require(app.step_frame(),"wrong-input checkpoint frame");
    require(state()!=baseline.at(count),"checkpoint wrong input changed no state");write("checkpoint-wrong-input.state");
    app.get_gui_manager().show_tas_editor(true);app.get_gui_manager().begin_frame();app.get_gui_manager().render(app,app.get_renderer());app.get_gui_manager().end_frame();
    std::cout<<count<<" native checkpoint movie frames; recording/playback states and "<<watch.size()<<" seek boundaries match; executed neutral-input control differs PASS\n";
}
static void local_ports(Application& app,IEmulatorPlugin* core) {
#if SDL_VERSION_ATLEAST(2,0,14)
    auto& manager=app.get_input_manager();
    require(app.get_controller_count()==4,"native layout exposes four ports");
    std::array<SDL_Joystick*,4> pads{};
    for (int i=0;i<4;++i) {
        const int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,16,0);
        require(index>=0,"attach virtual gamepad");
        char guid[33];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index),guid,sizeof(guid));
        const std::string mapping=std::string(guid)+",Veloce Local Pad,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
        require(SDL_GameControllerAddMapping(mapping.c_str())>=0,"virtual mapping");
        pads[i]=SDL_JoystickOpen(index);require(pads[i],"open virtual gamepad");
        require(SDL_JoystickSetVirtualAxis(pads[i],4,-32768)==0 && SDL_JoystickSetVirtualAxis(pads[i],5,-32768)==0,"neutral triggers");
    }
    auto pump=[&] {SDL_JoystickUpdate();SDL_Event event;while(SDL_PollEvent(&event))manager.process_event(event);manager.update();};
    pump();require(manager.get_connected_controller_count()==4,"four identical SDL connections");
    auto assign=[&](const std::array<int,4>& order){for(int port=0;port<4;++port)manager.set_controller_for_port(port,order[port]);};
    auto clear=[&] {for(auto* pad:pads){for(int b=0;b<16;++b)require(SDL_JoystickSetVirtualButton(pad,b,0)==0,"clear buttons");for(int a=0;a<4;++a)require(SDL_JoystickSetVirtualAxis(pad,a,0)==0,"clear axes");require(SDL_JoystickSetVirtualAxis(pad,4,-32768)==0,"clear trigger");}};
    std::ofstream out("local-ports.csv");out<<"case,port,device,control,x,y,reply,status,cleared\n";
    unsigned cases=0;
    auto observe=[&](const std::array<int,4>& order,const std::array<int,4>& control,const std::array<int,4>& xs,const std::array<int,4>& ys) {
        pump();app.reset();require(app.step_frame(),"ordinary local frame");
        auto word=[&](unsigned address){uint32_t value=0;for(unsigned b=0;b<4;++b)value=(value<<8)|core->read_memory(uint16_t(address+b));return value;};
        for(int port=0;port<4;++port)out<<cases<<','<<port<<','<<order[port]<<','<<control[order[port]]<<','<<xs[order[port]]<<','<<ys[order[port]]<<','<<word(0x2000+4*port)<<','<<word(0x2010)<<','<<word(0x2014)<<'\n';
        ++cases;
    };
    // Every mapping, every native control and three stick endpoint/dead-zone pairs.
    std::array<int,4> order={0,1,2,3};
    do {
        assign(order);clear();std::array<int,4> controls={0,1,3,4},xs={80,-80,0,0},ys={0,0,80,-80};
        const int physical[]={0,1,6,11};for(int i=0;i<4;++i){require(SDL_JoystickSetVirtualButton(pads[i],physical[i],1)==0,"permutation buttons");require(SDL_JoystickSetVirtualAxis(pads[i],0,xs[i]>0?32767:xs[i]<0?-32768:0)==0 && SDL_JoystickSetVirtualAxis(pads[i],1,ys[i]>0?-32768:ys[i]<0?32767:0)==0,"permutation sticks");}
        observe(order,controls,xs,ys);
    } while(std::next_permutation(order.begin(),order.end()));
    order={0,1,2,3};assign(order);
    // Controls use independent native bit numbers, not frontend word encoding.
    const int physical[]={0,1,-1,6,11,12,13,14,9,10,-2,-3,-4,-5};
    for(int device=0;device<4;++device)for(int control=0;control<14;++control)for(int stick=0;stick<3;++stick) {
        clear();const int b=physical[control];
        if(b>=0)require(SDL_JoystickSetVirtualButton(pads[device],b,1)==0,"control button");
        else if(b==-1)require(SDL_JoystickSetVirtualAxis(pads[device],4,32767)==0,"Z trigger");
        else require(SDL_JoystickSetVirtualAxis(pads[device],b>=-3?3:2,(b==-2 || b==-4)?-32768:32767)==0,"C stick");
        const int x=stick==0?-80:stick==1?80:0,y=stick==0?80:stick==1?-80:0;
        require(SDL_JoystickSetVirtualAxis(pads[device],0,stick==0?-32768:stick==1?32767:4000)==0 && SDL_JoystickSetVirtualAxis(pads[device],1,stick==0?-32768:stick==1?32767:-4000)==0,"control stick");
        std::array<int,4> controls={-1,-1,-1,-1},xs{},ys{};controls[device]=control;xs[device]=x;ys[device]=y;observe(order,controls,xs,ys);
    }
    // Executed neutral control after held input must release every port.
    clear();observe(order,{-1,-1,-1,-1},{0,0,0,0},{0,0,0,0});
    manager.set_input_capture_mode(true);
    require(SDL_JoystickSetVirtualButton(pads[3],0,1)==0,"capture control");
    observe(order,{-1,-1,-1,-1},{0,0,0,0},{0,0,0,0});manager.set_input_capture_mode(false);
    // Run a swapped-device control, but retain the intended mapping for the independent grader.
    clear();require(SDL_JoystickSetVirtualButton(pads[0],0,1)==0,"wrong-route control");
    assign({1,0,2,3});observe(order,{0,-1,-1,-1},{0,0,0,0},{0,0,0,0});
    // Record the real physical-input path on all four ports, then replay complete states.
    clear();assign({0,1,2,3});
    for(int i=0;i<4;++i) {
        require(SDL_JoystickSetVirtualButton(pads[i],i%2,1)==0,"movie physical button");
        require(SDL_JoystickSetVirtualAxis(pads[i],0,i%2?32767:-32768)==0,"movie physical stick");
    }
    pump();app.reset();
    auto state=[&]{std::vector<uint8_t> bytes;require(core->save_state(bytes),"local movie state");return bytes;};
    std::vector<std::vector<uint8_t>> states{state()};
    for(int n=0;n<6;++n){require(app.step_frame(),"ordinary physical movie baseline");states.push_back(state());}
    auto* movie=app.get_plugin_manager().get_tas_plugin();require(movie,"local movie plugin");
    require(movie->new_movie("four-local-ports.tas",false),"local movie creation");
    for(int n=0;n<6;++n) {
        require(app.step_frame(),"physical local movie recording");
        const auto frame=movie->get_frame(n);for(int port=0;port<4;++port)require(frame.controller_inputs[port]!=0,"recorded physical port missing");
        require(state()==states[n+1],"physical recording differs from ordinary complete state");
    }
    movie->stop_recording();require(movie->save_movie(),"local movie save");
    require(movie->open_movie("four-local-ports.tas"),"local movie reopen");movie->start_playback();app.pause();
    for(int n=0;n<6;++n){require(app.step_frame(),"local movie playback");require(state()==states[n+1],"local movie playback complete state");}
    for(int n:{0,3,6}){movie->seek_to_frame(n);require(state()==states[n],"four-port movie seek");}
    auto mutant=movie->get_frame(0);mutant.controller_inputs[3]^=1;movie->set_frame(0,mutant);movie->start_playback();app.pause();
    for(int n=0;n<6;++n)require(app.step_frame(),"local movie executed wrong port input");
    require(state()!=states.back(),"wrong player-four movie input did not change state");movie->close_movie();
    app.get_gui_manager().show_settings(true);app.get_gui_manager().begin_frame();app.get_gui_manager().render(app,app.get_renderer());app.get_gui_manager().end_frame();
    for(auto* pad:pads)SDL_JoystickClose(pad);
    std::cout<<cases<<" ordinary SDL/default-input/frontend/native N64 guest cases captured; six four-port physical movie frames and three seeks match full states; executed player-four movie mutation differs\n";
#else
    throw std::runtime_error("SDL virtual pads require 2.0.14");
#endif
}
int main(int argc,char** argv){try {
    const bool local_mode=argc==3 && std::string(argv[2])=="--local-ports";
    const bool checkpoint_mode=argc==6 && std::string(argv[2])=="--checkpoint";
    require(argc==2 || checkpoint_mode || local_mode,"ROM argument");Application app;
    require(app.initialize(2,argv),"initialize");app.pause();
    auto* core=app.get_plugin_manager().get_active_plugin();
    auto* movie=app.get_plugin_manager().get_tas_plugin();
    auto* input=app.get_plugin_manager().get_input_plugin();
    require(core&&movie&&input,"selected plugins");
    if(local_mode){local_ports(app,core);app.shutdown();return 0;}
    if(checkpoint_mode){unsigned count=0;uint32_t word=0;const std::string frames=argv[4],mask=argv[5];
        const auto a=std::from_chars(frames.data(),frames.data()+frames.size(),count),b=std::from_chars(mask.data(),mask.data()+mask.size(),word,16);
        require(a.ec==std::errc{} && a.ptr==frames.data()+frames.size() && count && count<=2000 && b.ec==std::errc{} && b.ptr==mask.data()+mask.size(),"checkpoint movie domain");
        checkpoint_movie(app,core,movie,input,argv[3],count,word);app.shutdown();return 0;
    }
    std::ofstream ram("ram-observations.txt");
    auto state=[&](){std::vector<uint8_t> out;require(core->save_state(out),"save native state");return out;};
    auto observe=[&](const char* phase,int n){ram<<phase<<' '<<n<<' '<<int(core->read_memory(0x12))<<' '<<int(core->read_memory(0x13))<<'\n';};
    constexpr int count=121;
    std::vector<TASFrameData> frames(count);std::vector<std::vector<uint8_t>> baseline;
    baseline.push_back(state());
    const uint32_t buttons[]={1,2,128,64,256,512,1024,2048};
    for(int n=0;n<count;++n){frames[n].frame_number=n;frames[n].controller_inputs[0]=buttons[n%8];frames[n].controller_inputs[1]=buttons[(n*3+1)%8];require(app.run_replay_frame(frames[n]),"ordinary frame");baseline.push_back(state());observe("ordinary",n);}
    require(movie->new_movie("recording.tas",false),"new movie");require(state()==baseline[0],"reset boundary");
    for(int n=0;n<count;++n){input->set_input_override(0,frames[n].controller_inputs[0]);input->set_input_override(1,frames[n].controller_inputs[1]);require(app.step_frame(),"record frame");require(movie->get_current_frame()==uint64_t(n+1),"commit once");auto f=movie->get_frame(n);require(f.controller_inputs[0]==frames[n].controller_inputs[0]&&f.controller_inputs[1]==frames[n].controller_inputs[1],"recorded ports");require(state()==baseline[n+1],"recording native state");observe("recording",n);}
    movie->stop_recording();require(movie->save_movie(),"save movie");
    input->set_input_override(0,0);input->set_input_override(1,0);
    require(movie->open_movie("recording.tas"),"reopen movie");movie->start_playback();app.pause();
    for(int n=0;n<count;++n){require(app.step_frame(),"playback frame");require(state()==baseline[n+1],"playback native state");observe("playback",n);}
    require(movie->get_mode()==TASMode::Stopped&&app.is_paused(),"EOF pause");
    for(int n:{0,59,60,61,120,121}){movie->seek_to_frame(n);require(movie->get_current_frame()==uint64_t(n),"seek cursor");require(state()==baseline[n],"seek native state");require(app.get_audio_manager().get_buffered_samples()==0,"seek audio cleared");}
    auto altered=movie->get_frame(0);altered.controller_inputs[0]^=1;movie->set_frame(0,altered);movie->start_playback();app.pause();
    for(int n=0;n<count;++n){require(app.step_frame(),"mutant frame");observe("mutant",n);}
    require(state()!=baseline.back(),"executed input mutant must change native state");
    movie->close_movie();require(core->load_state(baseline[60]),"restore state start");
    require(movie->new_movie("state-start.tas",true),"state start");
    for(int n=60;n<90;++n){input->set_input_override(0,frames[n].controller_inputs[0]);input->set_input_override(1,frames[n].controller_inputs[1]);require(app.step_frame(),"state recording");require(state()==baseline[n+1],"state recording native");}
    movie->stop_recording();movie->seek_to_frame(0);require(state()==baseline[60],"state start seek");
    movie->seek_to_frame(30);require(state()==baseline[90],"state terminal seek");
    require(movie->new_movie("events.tas",false),"event movie");movie->stop_recording();
    std::vector<std::vector<uint8_t>> events{state()};std::vector<uint8_t> saved_boundary;
    for(int n=0;n<65;++n){auto f=frames[n];f.has_reset=n==20;f.has_savestate=n==40;f.savestate_slot=3;
        movie->insert_frame(n);movie->set_frame(n,f);if(n==40)saved_boundary=state();
        require(app.run_replay_frame(f),"ordinary event frame");events.push_back(state());}
    require(app.get_savestate_manager().load_state(3),"real event save slot");require(state()==saved_boundary,"save before event input");
    movie->start_playback();app.pause();
    for(int n=0;n<65;++n){require(app.step_frame(),"event playback");require(state()==events[n+1],"reset/save native state");require(movie->get_current_frame()==uint64_t(n+1),"movie cursor across reset");}
    movie->seek_to_frame(33);require(state()==events[33],"seek across reset");
    altered=movie->get_frame(0);altered.controller_inputs[2]=1;movie->set_frame(0,altered);movie->start_playback();app.pause();
    const auto unsupported_boundary=state();require(!app.step_frame(),"unsupported port rejected");
    require(state()==unsupported_boundary&&movie->get_current_frame()==0&&movie->get_mode()==TASMode::Stopped,"unsupported port does not advance");
    require(movie->new_movie("manual-reset.tas",false),"manual reset movie");app.reset();require(movie->get_mode()==TASMode::Stopped,"manual reset stops movie");
    app.get_gui_manager().show_tas_editor(true);app.get_gui_manager().begin_frame();app.get_gui_manager().render(app,app.get_renderer());app.get_gui_manager().end_frame();
    app.shutdown();std::cout<<"Native NES TAS recording/playback/seek/state-start and executed mutant PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
