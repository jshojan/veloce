#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
using Bank=std::array<std::array<uint8_t,16>,32>;
using Memory=std::array<uint8_t,8192>;
static unsigned half(const Bank& bank,unsigned reg,unsigned lane){return bank[reg][lane*2]*256u+bank[reg][lane*2+1];}
// Destination-oriented byte mappings, independent of production command loops.
static void reference(Bank& bank,Memory& mem,bool store,unsigned kind,unsigned e,unsigned vt,uint32_t raw){
 const unsigned a=raw%4096,aligned=a/8*8;
 const Bank before=bank;
 if(!store){
  for(unsigned byte=0;byte<16;++byte){int source=-1,value=-1;
   if(kind<5){const unsigned length=kind==4?16-a%16:1u<<kind;if(byte>=e && byte-e<length)source=int(a+byte-e);}
   else if(kind==5){if(byte>=e+16-a%16)source=int(a/16*16+byte-e-16+a%16);}
   else if(kind<9){source=int(aligned+(a%8+16-e+(byte/2)*(kind==8?2:1))%16);const unsigned packed=unsigned(mem[source%4096])*(kind==6?256:128);value=int(packed/(byte%2?1:256)%256);source=-1;}
   else if(kind==9 && byte>=e && byte-e<8){const unsigned n=byte/2,positions[]={0,4,8,12,8,12,0,4};source=int(aligned+(a%8+positions[n]+(n?16-e:e))%16);const unsigned packed=unsigned(mem[source%4096])*128;value=int(packed/(byte%2?1:256)%256);source=-1;}
   if(source>=0)value=mem[unsigned(source)%4096];
   if(value>=0)bank[vt][byte]=uint8_t(value);
  }
  if(kind==11)for(unsigned byte=0;byte<16;++byte){const unsigned reg=vt/8*8+(e/2+byte/2)%8;bank[reg][byte]=mem[(aligned+((a/8%2)*8+e+byte)%16)%4096];}
  return;
 }
 if(kind<6){const unsigned count=kind==4?16-a%16:kind==5?a%16:1u<<kind;for(unsigned i=0;i<count;++i){const unsigned byte=kind==5?16-count+i:i;const unsigned addr=kind==5?a/16*16+i:a+i;mem[addr%4096]=before[vt][(e+byte)%16];}}
 else if(kind<8)for(unsigned i=0;i<8;++i){const bool upper=(e+i)%16<8;const unsigned divisor=kind==6?(upper?256:128):(upper?128:256);mem[(a+i)%4096]=uint8_t(half(before,vt,(e+i)%8)/divisor);}
 else if(kind==8)for(unsigned i=0;i<8;++i){const unsigned code=before[vt][(e+2*i)%16]*256u+before[vt][(e+2*i+1)%16];mem[(aligned+(a%8+2*i)%16)%4096]=uint8_t(code/128);}
 else if(kind==9){constexpr int lanes[16][4]={{0,1,2,3},{6,7,4,5},{-1,-1,-1,-1},{-1,-1,-1,-1},{1,2,3,0},{7,4,5,6},{-1,-1,-1,-1},{-1,-1,-1,-1},{4,5,6,7},{-1,-1,-1,-1},{-1,-1,-1,-1},{3,0,1,2},{5,6,7,4},{-1,-1,-1,-1},{-1,-1,-1,-1},{0,1,2,3}};for(unsigned i=0;i<4;++i)mem[(aligned+(a%8+4*i)%16)%4096]=lanes[e][i]<0?0:uint8_t(half(before,vt,unsigned(lanes[e][i]))/128);}
 else if(kind==10)for(unsigned i=0;i<16;++i)mem[(aligned+(a%8+i)%16)%4096]=before[vt][(e+i)%16];
 else for(unsigned i=0;i<16;++i){const unsigned reg=vt/8*8+(i/2+16-aligned/2%8+e/2)%8;mem[(aligned+(a+i)%16)%4096]=before[reg][(i+aligned)%16];}
}
static void opcode(n64::Machine& m,uint32_t ins){for(unsigned b=0;b<4;++b)m.sp[4096+b]=uint8_t(ins>>(24-b*8));m.sp_interface.pc=0;m.sp_interface.status=0;m.rsp.next_pc=4;m.rsp.delay_slot=false;}
static void step(n64::Machine& m){require(m.rsp.step(m.sp_interface,m.dp_interface,m.sp,m.ram),"supported vector instruction blocked");}
int main(int argc,char** argv){try{
 require(argc==2,"original ROM required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& r=m.rsp;auto& v=r.vectors;
 Bank initial{};Memory memory{};uint64_t memory_cases=0,offset_cases=0,moves=0;
 auto check=[&](unsigned store,unsigned kind,unsigned e,unsigned vt,unsigned encoded,uint32_t base){v.registers=initial;v.flags={0x8001,0xaaaa,0x81};m.sp=memory;r.gpr.fill(0);r.gpr[1]=base;const auto scalar_before=r.gpr;const auto flags_before=v.flags;v.accumulator={0x7fffffffffff,0x800000000000,0xffffffffffff,0,1,0x8000,0xffffffff,0x200000};const auto acc_before=v.accumulator;const unsigned factor=kind<4?1u<<kind:kind==6 || kind==7?8:16;const int signed_offset=int(encoded)-(encoded>=64?128:0);const uint32_t address=base+uint32_t(signed_offset*int(factor));opcode(m,((store?58u:50u)<<26)|(1<<21)|(vt<<16)|(kind<<11)|(e<<7)|encoded);auto want=initial;auto want_mem=m.sp;reference(want,want_mem,bool(store),kind,e,vt,address);step(m);require(v.registers==want && m.sp==want_mem && r.gpr==scalar_before && v.flags==flags_before && v.accumulator==acc_before && m.sp_interface.pc==4 && !m.sp_interface.interrupt,"vector byte mappings/whole banks/DMEM/IMEM/GPR guards");};
 for(unsigned seed=0;seed<2;++seed){for(unsigned reg=0;reg<32;++reg)for(unsigned b=0;b<16;++b)initial[reg][b]=uint8_t(seed*97+reg*17+b*23+b/2*7);for(unsigned b=0;b<8192;++b)memory[b]=uint8_t(seed*113+b*19+b/2*7);
  for(unsigned a=0;a<4096;++a)for(unsigned e=0;e<16;++e)for(unsigned kind=0;kind<12;++kind)for(unsigned store=0;store<2;++store){check(store,kind,e,a/128,0,a);++memory_cases;}
 }
 for(unsigned encoded=0;encoded<128;++encoded)for(unsigned vt=0;vt<32;++vt)for(unsigned e=0;e<16;++e)for(unsigned kind=0;kind<12;++kind)for(unsigned store=0;store<2;++store)for(uint32_t base:{0xfffffff0u+vt%16,0xff0u+vt%16}){check(store,kind,e,vt,encoded,base);++offset_cases;}
 const auto unchanged_acc=v.accumulator;
 // Every halfword payload and byte element. MTC2 truncates at e=15; MFC2 wraps.
 for(unsigned payload=0;payload<65536;++payload)for(unsigned e=0;e<16;++e){const unsigned vt=payload%32;v.registers=initial;r.gpr.fill(0);r.gpr[2]=0xa5a50000|payload;opcode(m,0x48820000|(vt<<11)|(e<<7));step(m);auto want=initial;want[vt][e]=uint8_t(payload/256);if(e<15)want[vt][e+1]=uint8_t(payload);require(v.registers==want,"all payload/element MTC2 bytes and guards");opcode(m,0x48030000|(vt<<11)|(e<<7));step(m);const unsigned halfword=want[vt][e]*256u+want[vt][(e+1)%16];const uint32_t result=halfword+(halfword>=32768?0xffff0000u:0);require(r.gpr[3]==result && v.registers==want,"all payload/element MFC2 sign/wrap");++moves;}
 // All low-halfword payloads through all 32 flag aliases, including unsigned VCE.
 for(unsigned payload=0;payload<65536;++payload)for(unsigned index=0;index<32;++index){v.flags={11,22,33};r.gpr[2]=0xa5a50000|payload;opcode(m,0x48c20000|(index<<11));step(m);const unsigned flag=(index%4)<2?index%4:2;std::array<uint16_t,3> want{11,22,33};want[flag]=uint16_t(payload%(flag==2?256:65536));require(v.flags==want,"CTC2 masks and aliases");opcode(m,0x48430000|(index<<11));step(m);const unsigned code=want[flag];require(r.gpr[3]==code+(code>=32768?0xffff0000u:0),"CFC2 aliases and sign");}
 // Every GPR/vector encoding at every byte element, including writes to scalar zero.
 for(unsigned scalar=0;scalar<32;++scalar)for(unsigned vt=0;vt<32;++vt)for(unsigned e=0;e<16;++e){v.registers=initial;r.gpr.fill(0);if(scalar)r.gpr[scalar]=0xdead89ab;opcode(m,0x48800000|(scalar<<16)|(vt<<11)|(e<<7));step(m);auto want=initial;want[vt][e]=scalar?0x89:0;if(e<15)want[vt][e+1]=scalar?0xab:0;require(v.registers==want && !r.gpr[0],"all MTC2 encodings/zero");opcode(m,0x48000000|(scalar<<16)|(vt<<11)|(e<<7));step(m);const unsigned code=want[vt][e]*256u+want[vt][(e+1)%16];require(r.gpr[scalar]==(scalar?code+(code>=32768?0xffff0000u:0):0),"all MFC2 encodings/zero");}
 require(v.accumulator==unchanged_acc,"scalar/vector/flag transfers preserve accumulator");
 m.reset();opcode(m,0x4bdfded2);const auto bank=v.snapshot();require(!r.step(m.sp_interface,m.dp_interface,m.sp,m.ram) && r.blocked && r.blocked_word==0x4bdfded2 && m.sp_interface.pc==0 && m.sp_interface.status==1 && !m.sp_interface.interrupt && v.snapshot()==bank,"unsupported VSUT retains PC and vector state without BREAK/IRQ");
 // Save pending vector operations, all raw storage and flags, clock phase and previous DP state.
 m.reset();v.registers=initial;v.flags={0xffff,0x8001,0xff};m.dp_interface.flags=2;m.rsp_phase=2;r.gpr[1]=0xfff;opcode(m,(50u<<26)|(1<<21)|(31<<16)|(3<<11)|(15<<7));std::vector<uint8_t> saved,after,replay;require(m.save(saved),"vector save");m.clock();m.save(after);require(m.restore(saved),"vector restore");m.clock();m.save(replay);require(after==replay,"pending vector exact replay");
 for(unsigned field=64;field<67;++field){auto bad=saved;const uint64_t value=field==66?256:65536;const size_t at=bad.size()-776+field*8;for(unsigned b=0;b<8;++b)bad[at+b]=uint8_t(value>>(56-8*b));require(m.restore(saved) && !m.restore(bad) && m.save(replay) && replay==saved,"vector invalid flags rejected atomically");}
 auto old=saved;old.resize(old.size()-776);old[6]='1';old[7]='5';require(m.restore(old) && v.snapshot()==n64::RspVector{}.snapshot() && m.dp_interface.flags==2 && r.gpr[1]==0xfff,"S015 neutral vector conversion preserves DP/scalar state");
 std::cout<<memory_cases<<" memory/address/element cases, "<<offset_cases<<" signed-offset/register cases, "<<moves<<" payload/element move pairs and 2097152 flag aliases; zero/guards, unsupported addition and S017 replay/legacy/rejection passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
