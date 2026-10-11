#pragma once
#include <array>
#include <cstdint>
namespace ctr {
// Functional shared-RAM monitor. The 32-byte coherent cache-line domain follows
// ARM11 MPCore's line size; CP15/MMU/cache and external monitor timing are pending.
class Exclusive {
public:
 struct Reservation {uint32_t address=0,width=0;};
 std::array<Reservation,2> slots{};
 void clear(unsigned id){if(id<2)slots[id]={};}
 void load(unsigned id,uint32_t address,unsigned width){slots[id]={address,width};}
 bool consume(unsigned id,uint32_t address,unsigned width){const auto old=slots[id];clear(id);return old.width==width && old.address==address;}
 void invalidate(uint32_t address,unsigned width){
  for(auto& r:slots)if(r.width && uint64_t(address)<uint64_t(r.address&~31u)+32 && uint64_t(address)+width>(r.address&~31u))r={};
 }
};
}
