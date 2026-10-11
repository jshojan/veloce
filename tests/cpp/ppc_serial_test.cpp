#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <iostream>
#include <cstring>
using emulator_test::require;
static uint32_t read(const ppc::Serial& si,uint32_t offset) { uint32_t v=0;require(si.read(offset,v),"SI read rejected");return v; }
static void command(ppc::Serial& si,unsigned port,unsigned out,unsigned in,uint32_t first=0x400300a5) {
    require(si.write(0x80,first) && si.write(0x84,0xdecafbad),"buffer seed");
    require(si.write(0x34,0xc0000001u|((out%128)<<16)|((in%128)<<8)|(port<<1)),"command submission");
}
static void word(std::vector<uint8_t>& v,size_t pos,uint32_t n) { for(unsigned b=0;b<4;++b)v[pos+b]=uint8_t(n>>(24-b*8)); }
static std::vector<uint8_t> rom(bool wii) {
    std::vector<uint8_t> v(40);std::memcpy(v.data(),wii?"VWIP0001":"VGCP0001",8);
    word(v,8,0x80000000);word(v,12,0x80000000);word(v,16,8);word(v,32,0x38630001);word(v,36,0x4bfffffc);return v;
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"two core paths required");
        uint64_t digital_checks=0,length_checks=0;
        constexpr unsigned inputs[]={0,1,2,3,6,10,11,9,8,20,5,4};
        constexpr unsigned wires[]={8,9,10,11,12,0,1,2,3,4,5,6};
        for(unsigned present=0;present<16;++present) for(unsigned port=0;port<4;++port) for(unsigned sample=0;sample<4096;++sample) {
            ppc::Serial si;uint32_t mask=0,want=128;
            for(unsigned n=0;n<12;++n) if(sample&(1u<<n)) {mask|=1u<<inputs[n];want|=1u<<wires[n];}
            for(unsigned other=0;other<4;++other) si.input(other,other==port?mask:~mask,(present>>other)&1);
            command(si,port,3,8);
            const bool plugged=present&(1u<<port);
            require(read(si,0x80)==(plugged?(want<<16|0x8080):0x400300a5),"controller wire/port selection");
            const uint32_t low=0x80800000u|(sample&2048?0xff00u:0)|(sample&1024?0xffu:0);
            require(read(si,0x84)==(plugged?low:0xdecafbad),"mode-3 analog endpoints");
            require(read(si,0x38)==(plugged?0:8u<<(24-port*8)),"absent-port error");
            require(read(si,0x34)==(0xc0030800u|(port<<1)|(plugged?0:0x20000000u)),"settled completion/masks");
            const auto state=si.save();ppc::Serial restored;require(restored.restore(state.data(),state.size()) && restored.save()==state,"SI roundtrip");
            si.write(0x34,0x00030800u|(port<<1));require(read(si,0x34)&0x80000000u,"zero cleared TCINT");
            si.write(0x34,0x80030800u|(port<<1));require(!(read(si,0x34)&0x80000000u),"W1C TCINT");
            si.write(0x38,0);require(read(si,0x38)==(plugged?0:8u<<(24-port*8)),"zero cleared status");
            si.write(0x38,0x0f0f0f0f);require(read(si,0x38)==0,"W1C status");++digital_checks;
        }
        for(unsigned port=0;port<4;++port) for(unsigned out=1;out<=128;++out) for(unsigned in=1;in<=128;++in) for(unsigned kind=0;kind<3;++kind) {
            ppc::Serial si;si.input(port,0xffffffff,true);
            command(si,port,out,in,kind==0?0x00aabbcc:kind==1?0xffaabbcc:0x400300a5);
            const bool valid=out==(kind<2?1u:3u);const unsigned length=kind<2?3:8;
            const uint32_t error=!valid?8:length<in?1:length>in?2:0;
            require(read(si,0x38)==error<<(24-port*8),"length error/zero-128 semantics");
            require(bool(read(si,0x34)&0x20000000u)==bool(error),"COMERR length result");
            auto bytes=si.save();
            const uint8_t seed[]={uint8_t(kind==0?0:kind==1?255:64),uint8_t(kind<2?0xaa:3),uint8_t(kind<2?0xbb:0),uint8_t(kind<2?0xcc:0xa5),0xde,0xca,0xfb,0xad};
            const uint8_t identity[]={9,0,0};const uint8_t pad[]={0x1f,0xff,128,128,128,128,255,255};
            for(unsigned n=0;n<128;++n) {
                const uint8_t wanted=valid && n<in && n<length?(kind<2?identity[n]:pad[n]):n<8?seed[n]:0;
                require(bytes[n]==wanted,"receive bound/preserved communication RAM");
            }
            ++length_checks;
        }
        ppc::Serial si;uint32_t sentinel=0xdeadbeef;
        for(unsigned offset=0;offset<256;++offset) {
            const bool supported=offset==0x34 || offset==0x38 || (offset>=0x80 && !(offset&3));
            auto saved=si.save();const bool write=si.write(offset,0);require(write==supported,"invalid register write decoding");
            if(!supported)require(si.save()==saved,"invalid write mutation");
            sentinel=0xdeadbeef;const bool ok=si.read(offset,sentinel);require(ok==supported,"invalid register read decoding");
            if(!ok)require(sentinel==0xdeadbeef,"failed read mutated output");
        }
        for(unsigned b=144;b<156;++b) for(unsigned bit=0;bit<8;++bit) {
            auto saved=si.save(),bad=saved;bad[b]^=1u<<bit;ppc::Serial trial;
            const bool accepted=trial.restore(bad.data(),bad.size());
            if(!accepted)require(trial.save()==ppc::Serial{}.save(),"invalid peripheral restore mutation");
            else require(trial.save()==bad,"accepted state not canonical");
        }
        require(!si.restore(nullptr,156) && !si.restore(si.save().data(),155),"null/short restore accepted");
        for(unsigned profile=0;profile<2;++profile) {
            const bool wii=profile;auto image=rom(wii);ppc::Machine m(wii);require(m.load(image.data(),image.size()),"load machine");
            const uint32_t base=wii?0xcd006400:0xcc006400;
            uint32_t v=0xdeadbeef;
            require(m.write(base+0x80,4,0x400300a5) && m.write(base+0x34,4,0xc0030801) && m.read(base+0x80,4,v) && v==0x00808080,"machine SI wiring");
            const auto peripheral=m.serial.save();
            for(unsigned width:{1u,2u})require(!m.write(base+0x80,width,0) && !m.read(base+0x80,width,v),"narrow SI access accepted");
            require(!m.write(base+0x81,4,0) && !m.read(base+0x81,4,v) && m.serial.save()==peripheral,"misaligned SI access mutated");
            std::vector<uint8_t> saved,replay;require(m.save(saved),"machine save");
            auto invalid=saved;invalid[360+m.ram.size()+144]=1;require(!m.restore(invalid) && m.save(replay) && replay==saved,"invalid SI state atomicity");
            auto legacy=saved;legacy.resize(360+m.ram.size());legacy[7]='1';require(m.restore(legacy) && m.serial.save()==ppc::Serial{}.save(),"legacy state peripheral initialization");
            require(m.restore(saved) && m.serial.save()==peripheral,"machine peripheral restore");
            emulator_test::LoadedCore loaded(argv[profile+1],image);
            const auto* layout=loaded.plugin->get_controller_layout();
            require(layout && layout->button_count==12 && layout->controller_count==2,"controller layout metadata");
            bool z=false,select=false;for(int n=0;n<layout->button_count;++n) {
                z|=layout->buttons[n].button==emu::VirtualButton::Z;select|=layout->buttons[n].button==emu::VirtualButton::Select;
            }
            require(z && !select,"native controller Z/Select layout");
            loaded.plugin->run_frame({1u<<20});require(loaded.plugin->save_state(saved),"plugin save");
            ppc::Serial inspect;require(inspect.restore(saved.data()+saved.size()-156,156),"plugin state peripheral");
            command(inspect,0,3,8);require(read(inspect,0x80)==0x00908080,"frontend Z routing");
            loaded.plugin->run_frame({0});require(loaded.plugin->save_state(replay),"plugin altered save");
            require(loaded.plugin->load_state(saved),"plugin restore");loaded.plugin->run_frame({0});std::vector<uint8_t> continued;
            require(loaded.plugin->save_state(continued) && continued==replay,"plugin input-state replay");
        }
        std::cout<<digital_checks<<" digital/connection/port snapshots and "<<length_checks<<" transfer-length snapshots matched; receive bounds, W1C, state compatibility and both plugins passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
