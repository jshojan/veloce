#include "rdp_texture_divider.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
using Divider=n64::RdpTextureDivider;
static void check(bool v){if(!v){std::cerr<<"texture divider mismatch\n";std::exit(1);}}
// Independent piecewise linear oracle: rounded reciprocal endpoints are data,
// and overflow is a signed interval test rather than the production bit mask.
static constexpr unsigned endpoints[]={
 0x4000,0x3f04,0x3e10,0x3d22,0x3c3c,0x3b5d,0x3a83,0x39b1,
 0x38e4,0x381c,0x375a,0x369d,0x35e5,0x3532,0x3483,0x33d9,
 0x3333,0x3291,0x31f4,0x3159,0x30c3,0x3030,0x2fa1,0x2f15,
 0x2e8c,0x2e06,0x2d83,0x2d03,0x2c86,0x2c0b,0x2b93,0x2b1e,
 0x2aab,0x2a3a,0x29cc,0x2960,0x28f6,0x288e,0x2828,0x27c4,
 0x2762,0x2702,0x26a4,0x2648,0x25ed,0x2594,0x253d,0x24e7,
 0x2492,0x243f,0x23ee,0x239e,0x234f,0x2302,0x22b6,0x226c,
 0x2222,0x21da,0x2193,0x214d,0x2108,0x20c5,0x2082,0x2041,0x2000};
static int64_t floor_div(int64_t x,int64_t d){return x/d-(x%d<0);}
struct Oracle {
 unsigned w,shift=0,factor=0;int64_t limit=0,divisor=1;
 explicit Oracle(unsigned raw):w(raw){unsigned n=w%32768;while(n<16384 && shift<14){n*=2;++shift;}
  const unsigned segment=n%16384/256,fraction=n%256;
  factor=unsigned(floor_div(int64_t(endpoints[segment])*(256-fraction)+int64_t(endpoints[segment+1])*fraction,256));
  limit=int64_t(1)<<(29-shift);divisor=shift==14?1:int64_t(1)<<(13-shift);
 }
 uint32_t operator()(unsigned raw)const{const int64_t input=raw<32768?raw:int64_t(raw)-65536,product=input*factor;
  const int64_t scaled=shift==14?product*2:floor_div(product,divisor);
  unsigned flags=(product>=limit || product< -limit)?(product<0?131072:262144):0;
  if(w==0 || w>=32768)flags|=262144;
  return unsigned(scaled)%131072|flags;
 }
};
static uint16_t saturated(uint32_t v){if(v>=262144)return 32767;if(v>=131072)return 32768;const int x=v<65536?int(v):int(v)-131072;return uint16_t(x>32767?32767:x< -32768?-32768:x);}
static void compare(unsigned s,unsigned w,const Oracle& oracle){const auto got=Divider::divide(uint16_t(s),uint16_t(s^65535),uint16_t(w),true);const auto a=oracle(s),b=oracle(s^65535);check(got.s==a && got.t==b);check(Divider::saturate(got.s)==saturated(a) && Divider::saturate(got.t)==saturated(b));}
int main(int argc,char** argv){
 const bool exhaustive=argc==2 && std::string(argv[1])=="--exhaustive";uint64_t cases=0;
 for(unsigned w=0;w<65536;++w){const Oracle oracle(w);
  for(unsigned i=0;i<(exhaustive?65536u:64u);++i){const unsigned s=exhaustive?i:((i*1024)+(i%4?i%4==1?1:i%4==2?511:1023:0))%65536;compare(s,w,oracle);++cases;}
 }
 if(!exhaustive)for(unsigned i=0;i<64;++i){const unsigned w=(i*1024+(i%4?i%4==1?1:i%4==2?255:1023:0))%65536;const Oracle oracle(w);for(unsigned s=0;s<65536;++s){compare(s,w,oracle);++cases;}}
 for(unsigned s=0;s<65536;++s){const auto got=Divider::divide(uint16_t(s),uint16_t(s^65535),0,false);check(Divider::saturate(got.s)==s && Divider::saturate(got.t)==(s^65535));}
 for(unsigned v=0;v<524288;++v)check(Divider::saturate(v)==saturated(v));
 check(Divider::divide(32,65504,16384,true).s==64);check(Divider::divide(32,65504,16384,true).t==131008);
 // Segment six is deliberately one below the mathematical rounded endpoint.
 check(Divider::divide(8192,0,17920,true).s==14979);
 std::cout<<cases<<" ST/W pair captures; all 524288 saturation encodings; affine identity and reciprocal goldens PASS\n";
}
