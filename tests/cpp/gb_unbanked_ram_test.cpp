#include "cartridge.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool b){if(!b){std::cerr<<"unbanked RAM observation mismatch\n";std::exit(1);}}
int main(){uint64_t count=0;
 for(unsigned type:{8u,9u}){
  std::vector<uint8_t> rom(32768);rom[0x147]=uint8_t(type);rom[0x149]=2;gb::Cartridge cart;check(cart.load(rom.data(),rom.size()));check(cart.has_battery()==(type==9));
  for(unsigned value=0;value<256;++value){for(uint16_t reg:{uint16_t(0),uint16_t(0x2000),uint16_t(0x4000),uint16_t(0x6000)})cart.write_mbc(reg,uint8_t(value));
   for(unsigned at=0;at<8192;++at){const auto want=uint8_t(value+at*17+(at>>8));cart.write_ram(uint16_t(at),want);check(cart.read_ram(uint16_t(at))==want);++count;}
  }
  for(uint16_t at:{uint16_t(8192),uint16_t(16384),uint16_t(65535)}){cart.write_ram(at,0);check(cart.read_ram(at)==255);}
  std::vector<uint8_t> saved;cart.save_state(saved);cart.reset();for(unsigned at=0;at<8192;++at)check(cart.read_ram(uint16_t(at))==uint8_t(255+at*17+(at>>8)));
  cart.clear_ram();const uint8_t* p=saved.data();size_t remaining=saved.size();cart.load_state(p,remaining);check(remaining==0);for(unsigned at=0;at<8192;++at)check(cart.read_ram(uint16_t(at))==uint8_t(255+at*17+(at>>8)));
  const auto battery=cart.get_save_data();check(battery.size()==8192);cart.clear_ram();check(cart.set_save_data(battery));for(unsigned at=0;at<8192;++at)check(cart.read_ram(uint16_t(at))==uint8_t(255+at*17+(at>>8)));
 }
 std::vector<uint8_t> rom(32768);gb::Cartridge empty;check(empty.load(rom.data(),rom.size()));empty.write_ram(0,1);check(empty.read_ram(0)==255 && !empty.has_battery());
 std::cout<<count<<" unbanked RAM address/byte observations; fixed decode, reset/state/save replay PASS\n";
}
