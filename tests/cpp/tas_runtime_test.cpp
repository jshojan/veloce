#include "emu/tas_plugin.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
static void require(bool value){if(!value)throw std::runtime_error("TAS runtime mismatch");}
struct Host final:emu::ITASHost{
    uint64_t frame=0,hash=7;bool paused=false,reject_save=false,reject_load=false;int fail_replay=-1,replayed=0,requests=0;
    std::array<uint32_t,4> inputs{};
    void reset_emulator() override{frame=0;hash=7;}
    void pause_emulator() override{paused=true;}void resume_emulator() override{paused=false;}
    void frame_advance() override{++requests;}bool is_emulator_paused() const override{return paused;}
    uint64_t get_current_frame() const override{return frame;}double get_fps() const override{return 60;}
    void set_controller_input(int c,uint32_t b) override{inputs.at(c)=b;}uint32_t get_controller_input(int c) const override{return inputs.at(c);}
    bool save_state(int) override{return false;}bool load_state(int) override{return false;}
    bool save_state_to_buffer(std::vector<uint8_t>& out) override{if(reject_save)return false;out.resize(16);std::memcpy(out.data(),&frame,8);std::memcpy(out.data()+8,&hash,8);return true;}
    bool load_state_from_buffer(const std::vector<uint8_t>& in) override{if(reject_load||in.size()!=16)return false;std::memcpy(&frame,in.data(),8);std::memcpy(&hash,in.data()+8,8);return true;}
    bool run_replay_frame(const emu::TASFrameData& f) override{if(fail_replay==replayed++)return false;if(f.has_reset)reset_emulator();for(auto v:f.controller_inputs)hash=(hash*33)^v;++frame;return true;}
    const char* get_rom_name() const override{return "original fixture";}uint32_t get_rom_crc32() const override{return 123;}
    const char* get_platform_name() const override{return "NES";}uint8_t read_memory(uint16_t) override{return 0;}void write_memory(uint16_t,uint8_t) override{}
};
int main(int argc,char** argv){if(argc!=2)return 2;try{
    namespace fs=std::filesystem;fs::path root=fs::absolute(argv[1]);fs::create_directories(root);Host h;auto* p=create_tas_plugin();require(!p->initialize(nullptr));require(!p->new_movie("",false));require(p->initialize(&h));
    auto path=(root/"recorded.tas").string();require(p->new_movie(path.c_str(),false));
    std::array<uint64_t,122> hashes{};hashes[0]=7;
    for(unsigned n=0;n<121;++n){h.inputs={n+1,n*3+7,n^0x80000000,n+19};
        require(p->on_frame(2)==h.inputs[2] && p->on_frame(0)==h.inputs[0] && p->on_frame(3)==h.inputs[3] && p->on_frame(1)==h.inputs[1]);
        require(p->on_frame(0)==h.inputs[0] && p->get_current_frame()==n && p->get_total_frames()==n);
        emu::TASFrameData f{};f.frame_number=n;std::copy(h.inputs.begin(),h.inputs.end(),f.controller_inputs);require(h.run_replay_frame(f));p->on_frame_completed();p->on_frame_completed();
        require(p->get_current_frame()==n+1 && p->get_total_frames()==n+1);hashes[n+1]=h.hash;
    }
    require(p->has_greenzone_at(60) && p->has_greenzone_at(120));p->stop_recording();require(p->save_movie());
    for(auto target:{0,59,60,61,120,121}){h.replayed=0;require(p->seek_to_frame(target));require(h.frame==uint64_t(target)&&h.hash==hashes[target]&&h.paused&&h.requests==0);require(h.replayed==(target<60?target:target<120?target-60:target-120));}
    h.frame=300;h.hash=456;h.fail_replay=0;h.replayed=0;auto cursor=p->get_current_frame();require(!p->seek_to_frame(61)&&h.frame==300&&h.hash==456&&p->get_current_frame()==cursor);h.fail_replay=-1;
    h.reject_load=true;require(!p->seek_to_frame(60));h.reject_load=false;h.reject_save=true;require(!p->new_movie("bad",true)&&p->get_total_frames()==121);h.reject_save=false;
    require(p->open_movie(path.c_str()));auto identity=p->get_movie_info();auto wrong=identity;wrong.rom_crc32=321;p->set_movie_info(wrong);h.frame=88;p->start_playback();require(p->get_mode()==emu::TASMode::Stopped && h.frame==88 && !p->seek_to_frame(1));
    wrong=identity;std::strcpy(wrong.platform,"GB");p->set_movie_info(wrong);p->start_playback();require(p->get_mode()==emu::TASMode::Stopped && h.frame==88 && !p->seek_to_frame(1));p->set_movie_info(identity);p->start_playback();for(unsigned n=0;n<121;++n){auto f=p->get_frame(n);for(int c:{3,0,2,1,0})require(p->on_frame(c)==f.controller_inputs[c]);require(h.run_replay_frame(f));p->on_frame_completed();}require(p->get_mode()==emu::TASMode::Stopped);
    std::ofstream(root/"invalid",std::ios::binary)<<"TAS2";require(!p->open_movie((root/"invalid").string().c_str())&&p->get_total_frames()==121);
    p->set_mode(emu::TASMode::ReadOnly);auto before=p->get_frame(0);p->delete_frame(0);p->set_frame(0,{});p->insert_frame(UINT64_MAX);require(p->get_total_frames()==121&&p->get_frame(0).controller_inputs[0]==before.controller_inputs[0]);p->set_mode(emu::TASMode::Stopped);
    p->set_selection(UINT64_MAX,UINT64_MAX);p->cut_selection();require(p->get_total_frames()==121);p->set_selection(0,0);p->copy_selection();p->paste_at(UINT64_MAX);require(p->get_total_frames()==122&&p->get_frame(121).frame_number==121);p->undo();require(p->get_total_frames()==121 && !p->has_greenzone_at(60));p->redo();require(p->get_total_frames()==122);
    // Validate old same-build struct imports and reject noncanonical bool bytes.
    emu::TASMovieInfo info{};info.frame_count=1;emu::TASFrameData f{};f.controller_inputs[0]=256;
    std::vector<uint8_t> old(4+sizeof(info)+sizeof(f));std::memcpy(old.data(),"TAS1",4);std::memcpy(old.data()+4,&info,sizeof(info));std::memcpy(old.data()+4+sizeof(info),&f,sizeof(f));
    auto write_old=[&]{std::ofstream file(root/"legacy",std::ios::binary);file.write(reinterpret_cast<const char*>(old.data()),old.size());};write_old();require(p->open_movie((root/"legacy").string().c_str())&&p->get_frame(0).controller_inputs[0]==256);
    for(size_t prefix=0;prefix<old.size();++prefix){std::ofstream file(root/"truncated_legacy",std::ios::binary);file.write(reinterpret_cast<const char*>(old.data()),prefix);file.close();require(!p->open_movie((root/"truncated_legacy").string().c_str()) && p->get_total_frames()==1);}
    old[4+offsetof(emu::TASMovieInfo,starts_from_savestate)]=2;write_old();require(!p->open_movie((root/"legacy").string().c_str())&&p->get_total_frames()==1);
    auto import=(root/"import.fm2").string();std::ofstream(import)<<"version 3\nemuVersion 0\nport0 1\nport1 0\nport2 0\n|0|.......q|||\n";
    require(p->open_movie(import.c_str()) && !p->save_movie() && !p->save_movie_as(import.c_str()));
    require(p->save_movie_as((root/"converted.tas").string().c_str()));std::ifstream original(import);std::string first;std::getline(original,first);require(first=="version 3");
    h.frame=35;h.hash=555;require(p->new_movie(path.c_str(),true));h.frame=90;h.hash=1;p->start_recording();require(h.frame==35&&h.hash==555);
    p->shutdown();destroy_tas_plugin(p);std::cout<<"121 committed four-port frames, exact greenzone/replay boundaries, failure restoration, playback, editing and legacy imports PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
