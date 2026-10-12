#include "machine.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}

int main(int argc,char** argv){
    try{
        require(argc==2,"original VI cartridge path");
        std::ifstream source(argv[1],std::ios::binary);
        std::vector<uint8_t> original{std::istreambuf_iterator<char>(source),{}};
        require(original.size()==0x1004,"original VI cartridge size");
        const auto check=[&](uint8_t code,bool pal){
            auto rom=original;rom[0x3e]=code;
            for(unsigned format=0;format<3;++format){
                auto image=rom;
                if(format==1)for(size_t n=0;n<image.size();n+=2)std::swap(image[n],image[n+1]);
                if(format==2)for(size_t n=0;n<image.size();n+=4){std::swap(image[n],image[n+3]);std::swap(image[n+1],image[n+2]);}
                n64::Machine machine;
                require(machine.load(image.data(),image.size()),"region cartridge load");
                require(machine.pal_region==pal && machine.video_crystal_rate==(pal?49656530u:48681818u),"normalized region clock");
                machine.reset();
                require(machine.pal_region==pal && machine.video_crystal_rate==(pal?49656530u:48681818u),"reset region clock");
            }
        };
        for(const char* p="DFHILPSUWXYZ";*p;++p)check(uint8_t(*p),true);
        for(const char* p="ABCEGJKN";*p;++p)check(uint8_t(*p),false);
        check(0,false);check('?',false);
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
