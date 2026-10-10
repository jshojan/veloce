#include "rdp_combiner.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using n64::RdpCombiner;
#ifdef N64_COMBINER_REFERENCE
extern "C" uint32_t reference_combiner(unsigned,unsigned,unsigned,unsigned,unsigned*);
extern "C" unsigned reference_clamp(unsigned);
#endif
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// Independent signed-value enumeration and Euclidean remainder oracle.
static unsigned encoded(int value){return unsigned(value<0?value+512:value);}
static uint32_t expected(int a,int b,int c,int d){
    const int64_t value=(int64_t(a)-b)*c+int64_t(d)*256+128;
    const int64_t rem=value%131072;
    return uint32_t(rem<0?rem+131072:rem);
}
int main(){try{
 std::array<std::array<unsigned,2>,1023> representatives{};
 std::array<bool,1023> present{};
 uint64_t pairs=0,cases=0;
 for(int a=-128;a<=383;++a)for(int b=-128;b<=383;++b){
  const unsigned ae=encoded(a),be=encoded(b),index=unsigned(a-b+511);
  require(RdpCombiner::rgb_equation(ae,be,1,0)==expected(a,b,1,0),"A/B special extension");
  representatives[index]={ae,be};present[index]=true;++pairs;
 }
 // All possible A-B values, all 512 signed C factors, all 512 D addends.
 // A and B affect the equation only through their extended difference;
 // enumerating every A/B encoding above proves this finite reduction.
 for(int difference=-511;difference<=511;++difference){
  const auto pair=representatives[size_t(difference+511)];
  require(present[size_t(difference+511)],"difference domain gap");
  for(int c=-256;c<=255;++c)for(int d=-128;d<=383;++d){
   const uint32_t want=expected(difference,0,c,d);
   const unsigned ce=encoded(c),de=encoded(d);
   require(RdpCombiner::rgb_equation(pair[0],pair[1],ce,de)==want,"RGB fixed-point residue");
   require(RdpCombiner::alpha_equation(pair[0],pair[1],ce,de)==want/256,"alpha signed rounding");
#ifdef N64_COMBINER_REFERENCE
   unsigned alpha;
   require(reference_combiner(pair[0],pair[1],ce,de,&alpha)==want && alpha==want/256,"external reference arithmetic");
#endif
   ++cases;
  }
 }
 for(int value=-128;value<=383;++value){
  const unsigned want=value<0?0:value>255?255:unsigned(value);
  require(RdpCombiner::clamp(encoded(value))==want,"special nine-bit clamp");
#ifdef N64_COMBINER_REFERENCE
  require(reference_clamp(encoded(value))==want,"external reference clamp");
#endif
 }
 require(RdpCombiner::rgb_equation(0,0,0,255)==65408,"retained half-unit RGB rounding");
 require(RdpCombiner::alpha_equation(0,1,128,0)==0,"negative half rounds to zero");
 require(RdpCombiner::alpha_equation(0,1,129,0)==511,"negative below half wraps");
 require(RdpCombiner::rgb_equation(256,0,256,0)==65664,"C is ordinary signed nine-bit");
 require(RdpCombiner::rgb_equation(383,384,0,384)==98432,"A/B/D special negative region");
 std::cout<<pairs<<" A/B pairs; "<<cases<<" difference/C/D cases; 512 clamp values and literal fixed-point boundaries PASS\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
