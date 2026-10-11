#include "pif_joybus.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool ok){if(!ok){std::cerr<<"Joybus observation mismatch\n";std::exit(1);}}
static uint32_t frontend(unsigned raw){constexpr unsigned bits[]={0,1,20,6,8,9,10,11,4,5,21,22,23,24};uint32_t out=0;for(unsigned n=0;n<14;++n)if(raw&(1u<<n))out+=1u<<bits[n];return out;}
static unsigned wire(unsigned raw){constexpr unsigned bits[]={15,14,13,12,11,10,9,8,5,4,3,2,1,0};unsigned out=0;for(unsigned n=0;n<14;++n)if(raw&(1u<<n))out+=1u<<bits[n];return out;}
int main(){uint64_t reads=0,lengths=0,controls=0;
 n64::PifJoybus pif;n64::PifJoybus::Ram ram{};ram.fill(0xaa);for(unsigned port=0;port<4;++port){const unsigned at=port*8;ram[at]=255;ram[at+1]=1;ram[at+2]=4;ram[at+3]=1;}ram[32]=254;ram[63]=1;
 const auto before=ram;pif.format(ram);auto formatted=before;formatted[63]=0;check(ram==formatted && pif.snapshot()==n64::PifJoybus::State{1,9,17,25,64});
 for(unsigned port=0;port<4;++port)for(unsigned raw=0;raw<16384;++raw){std::array<uint32_t,4> input{};input[port]=frontend(raw);
  for(unsigned phase=0;phase<2;++phase){if(phase)input[port]=frontend(raw^16383);pif.poll(ram,input);for(unsigned target=0;target<4;++target){const unsigned at=target*8,value=target==port?wire(raw^(phase?16383:0)):0;check(ram[at]==255 && ram[at+1]==1 && ram[at+2]==4 && ram[at+3]==1 && ram[at+4]==value/256 && ram[at+5]==value%256 && !ram[at+6] && !ram[at+7]);}check(ram[32]==254 && ram[63]==0);++reads;}
 }
 // All control bytes: only the format flag rebuilds the retained layout.
 for(unsigned flags=0;flags<256;++flags){n64::PifJoybus test;auto packet=formatted;packet[63]=1;test.format(packet);const auto layout=test.snapshot();packet.fill(0xaa);packet[0]=254;packet[63]=uint8_t(flags);const auto original=packet;test.format(packet);check(test.snapshot()==(flags&1?n64::PifJoybus::State{64,64,64,64,64}:layout));auto want=original;if(flags&1)want[63]&=254;check(packet==want);++controls;}
 // Full command byte/receive-length domains, including malformed and cartridge channel.
 for(unsigned channel=0;channel<5;++channel)for(unsigned command=0;command<256;++command)for(unsigned rx=0;rx<64;++rx){n64::PifJoybus test;n64::PifJoybus::Ram packet{};packet.fill(0xaa);for(unsigned n=0;n<channel;++n)packet[n]=0;packet[channel]=1;packet[channel+1]=uint8_t(rx|192);packet[channel+2]=uint8_t(command);if(channel+3+rx<63)packet[channel+3+rx]=254;packet[63]=1;test.format(packet);const auto unpolled=packet;test.poll(packet,{});
  const bool fits=channel+3+rx<=63,recognized=channel<4 && (command==0 || command==1 || command==255),valid=recognized && rx==(command==1?4u:3u);
  const unsigned expected=rx|(!fits?64:!recognized?128:!valid?64:0);check(packet[channel+1]==expected);
  for(unsigned n=0;n<63;++n){if(n==channel+1)continue;const unsigned value=valid && n>=channel+3 && n<channel+3+rx?(command==1?0:n==channel+3?5:n==channel+5?2:0):unpolled[n];check(packet[n]==value);}check(packet[63]==0);++lengths;
 }
 // Tx skip/reset flags preserve responses; received error flags clear on a fresh poll.
 for(unsigned flag:{64u,128u,192u}){auto packet=formatted;packet[1]=uint8_t(flag|1);packet[2]=196;const auto original=packet;pif.poll(packet,{});check(packet[2]==original[2] && packet[4]==original[4]);}
 // Reformat changes offsets, and disabled/reset channels do not retain prior packets.
 ram.fill(0xaa);ram[0]=253;ram[1]=0;ram[2]=255;ram[3]=1;ram[4]=4;ram[5]=1;ram[10]=254;ram[63]=1;pif.format(ram);check(pif.snapshot()==n64::PifJoybus::State{65,64,3,64,64});pif.poll(ram,{0,0,64,0});check(ram[6]==16 && ram[7]==0 && !ram[8] && !ram[9]);
 const auto state=pif.snapshot();for(const auto bad:{n64::PifJoybus::State{62,64,64,64,64},n64::PifJoybus::State{0,0,64,64,64},n64::PifJoybus::State{3,2,64,64,64},n64::PifJoybus::State{66,64,64,64,64}}){check(!pif.restore(bad) && pif.snapshot()==state);}pif.reset();check(pif.snapshot()==n64::PifJoybus::State{64,64,64,64,64});
 std::cout<<reads<<" cached digital reads; "<<lengths<<" command/length/channel captures; "<<controls<<" control flags; formatting, skip/reset and atomic state rejection PASS\n";
}
