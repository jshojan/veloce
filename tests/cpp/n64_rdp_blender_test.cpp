#include "rdp_blender.hpp"
#include <cstdlib>
#include <iostream>
using Blender=n64::RdpBlender;
static void check(bool v){if(!v){std::cerr<<"blender mismatch\n";std::exit(1);}}
static unsigned quotient(unsigned index){
 const int denominator=int(index/2048),numerator=int(index%2048);
 int remainder=(16-denominator+numerator/256)%8;bool carry=false;unsigned result=0;
 for(int bit=7;bit>=0;--bit){const int trial=2*remainder+(numerator/(1<<bit))%2+(carry?16-denominator:denominator);carry=trial>=16;remainder=trial%8;if(carry)result+=1u<<bit;}
 return result;
}
static const auto divisions=[](){std::array<unsigned,32768> out{};for(unsigned i=0;i<32768;++i)out[i]=quotient(i);return out;}();
struct Expected {bool write=false,enabled=false;std::array<unsigned,3> rgb{};unsigned coverage=0,latch=0;};
static std::array<unsigned,4> bytes(unsigned word){return {word/16777216,word/65536%256,word/256%256,word%256};}
static Expected oracle(uint64_t mode,Blender::Color color,unsigned shade,unsigned memory,unsigned count,unsigned cvbit,unsigned dz,unsigned fog,unsigned blend,unsigned latch){
 Expected out;const bool aa=mode%16>=8,force=mode%32768>=16384,read=mode%128>=64;
 const unsigned mem=read?memory/32%8:7;out.latch=read?memory/256:latch;
 const unsigned gain=((color[3]==255?256:color[3])*count+4)/8;
 if(mode/4096%2)count=gain/32%16;
 if(mode/8192%2)color[3]=std::min(255u,mode/4096%2?gain:count*32);
 const bool wrap=(mem+count)/8%2;out.enabled=force || (aa && !wrap);
 const unsigned dest=unsigned(mode/256)%4;
 if(dest==0){const unsigned value=out.enabled?count+mem:count-1;out.coverage=value/8%2?7:value%8;}
 else out.coverage=dest==1?(mem+count)%8:dest==2?7:mem;
 if(mode%2 && color[3]<blend%256)return out;
 if(aa?count==0:cvbit==0)return out;
 const unsigned p=unsigned(mode/(1ull<<30))%4,a=unsigned(mode/(1ull<<26))%4,m=unsigned(mode/(1ull<<22))%4,b=unsigned(mode/(1ull<<18))%4;
 const std::array<Blender::Color,4> colors={color,bytes(out.latch*256+mem*32),bytes(blend),bytes(fog)};
 const unsigned alpha=a==0?color[3]:a==1?fog%256:a==2?shade:0;
 const unsigned beta=b==0?255-alpha:b==1?mem*32:b==2?255:0;
 unsigned first=alpha/8,second=beta/8;
 if(b==1){unsigned code=0;for(unsigned i=0;i<16;++i)if(dz/(1u<<i)%2)code|=i;first=first/4*4;second=second/(1u<<(code<11?4:15-code))|3;}
 for(unsigned lane=0;lane<3;++lane){
  if(mode/128%2 && !wrap)out.rgb[lane]=colors[m][lane];
  else if(!out.enabled || (a==0 && b==0 && color[3]==255))out.rgb[lane]=colors[p][lane];
  else{const unsigned sum=colors[p][lane]*first+colors[m][lane]*(second+1);out.rgb[lane]=force?sum/32%256:divisions[(first/4*4+second/4*4+4)*512+(sum/4)%2048];}
 }
 out.write=true;return out;
}
static void compare(uint64_t mode,Blender::Color color,unsigned shade,unsigned memory,unsigned count,unsigned cvbit,unsigned dz){
 Blender state;state.fog=0x17c96b83;state.blend=0xe143297f;state.memory_rgb=0x614f33;
 const auto want=oracle(mode,color,shade,memory,count,cvbit,dz,state.fog,state.blend,state.memory_rgb);const auto got=state.pixel(mode,color,shade,memory,count,cvbit,dz);
 check(got.write==want.write && got.enabled==want.enabled && got.coverage==want.coverage && state.memory_rgb==want.latch);if(got.write)check(got.rgb==want.rgb);
}
int main(){uint64_t equations=0,decisions=0,routes=0;
 for(unsigned i=0;i<32768;++i)check(Blender::divide(i)==divisions[i]);
 for(unsigned a=0;a<32;++a)for(unsigned b=0;b<32;++b)for(unsigned p=0;p<256;++p)for(unsigned m=0;m<256;++m){const unsigned n=p*a+m*(b+1),index=(a/4*4+b/4*4+4)*512+(n/4)%2048;check(Blender::equation(p,m,a,b,false)==divisions[index] && Blender::equation(p,m,a,b,true)==n/32%256);equations+=2;}
 for(unsigned alpha=0;alpha<256;++alpha)for(unsigned count=0;count<=8;++count)for(unsigned mem=0;mem<8;++mem)for(unsigned dest=0;dest<4;++dest)for(unsigned flags=0;flags<16;++flags)for(unsigned cv=0;cv<2;++cv){
  const uint64_t mode=(1ull<<22)|(dest<<8)|64|(flags&1?8:0)|(flags&2?16384:0)|(flags&4?4096:0)|(flags&8?8192:0);
  compare(mode,{37,193,249,alpha},alpha^255,0x529ba800|(mem*32+17),count,cv,1);++decisions;
 }
 for(unsigned selectors=0;selectors<256;++selectors)for(unsigned seed=0;seed<256;++seed)for(unsigned flags=0;flags<16;++flags){
  const uint64_t mux=(uint64_t(selectors%4)<<30)|(uint64_t(selectors/4%4)<<26)|(uint64_t(selectors/16%4)<<22)|(uint64_t(selectors/64)<<18);
  compare(mux|(flags&1?64:0)|(flags&2?16384:0)|(flags&4?8:0)|(flags&8?128:0),{seed,seed^255,(seed*17)%256,(seed*73)%256},seed^127,0x19a76ce0,4,1,1);++routes;
 }
 for(unsigned alpha=0;alpha<256;++alpha)for(unsigned threshold=0;threshold<256;++threshold){Blender state;state.blend=threshold;const auto result=state.pixel(1,{1,2,3,alpha},0,0,8,1,1);check(result.write==(alpha>=threshold));++decisions;}
 for(unsigned dz=0;dz<65536;++dz)for(unsigned alpha:{0u,31u,127u,255u}){compare((1ull<<18)|64|16384,{37,191,243,alpha},alpha^255,0x69f73be0,4,1,dz);++routes;}
 Blender state;state.fog=0x12345678;state.blend=0x89abcdef;state.memory_rgb=0x345678;const auto before=state.snapshot();for(unsigned i=0;i<3;++i){auto bad=before;bad[i]|=1ull<<(i==2?24:32);check(!state.restore(bad) && state.snapshot()==before);}
 std::cout<<equations<<" complete weight/color equations; 32768 divider entries; "<<decisions<<" alpha/coverage decisions; "<<routes<<" routing/delta/latch captures; state rejection PASS\n";
}
