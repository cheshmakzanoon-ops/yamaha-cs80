// Default Source Audio V2 scalar-history regression against frozen pre-recovery arithmetic.
// Captured capped states must remain exact, including continuation and clock re-reference.
#include "core/Ig00156Filter.h"
#include "FilterIdleFallbackBaseline.h"
#if CS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY
#error "Default-path regression must use the default (recovery-disabled) core configuration"
#endif
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <set>
#include <bit>
#include <cstdint>
#if defined(_MSC_VER)
#define FIXTURE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define FIXTURE_NOINLINE __attribute__((noinline))
#else
#define FIXTURE_NOINLINE
#endif
using namespace cs80;
bool bits(double a,double b){return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);}
template<class M> auto named(const M&m){return std::array<double,12>{m.z.input,m.z.feedback,m.z.bp,m.z.lp,m.v.input,m.v.feedback,m.v.bp,m.v.lp,m.slope1,m.slope2,m.halfIntegrators,m.halfCouplings};}
template<class A,class B> bool memoryEqual(const A&a,const B&b){auto x=named(a),y=named(b);for(unsigned i=0;i<x.size();++i)if(!bits(x[i],y[i]))return false;return true;}
template<class M> bool memoryFinite(const M& m){for(double x:named(m))if(!std::isfinite(x))return false;return true;}
int bad=0;void check(bool b,const char*s){if(!b){++bad;if(bad<20)std::printf("FAIL %s\n",s);}}
template<class M>void memory(M&m,const std::vector<double>&v){m.z={v[4],v[5],v[6],v[7]};m.v={v[8],v[9],v[10],v[11]};m.slope1=v[12];m.slope2=v[13];m.halfIntegrators=v[14];m.halfCouplings=v[15];}
template<class S> FIXTURE_NOINLINE void setup(S& s,float fc,float q,float rate,bool idle){s.set(fc,q,rate,idle);}
template<class S> FIXTURE_NOINLINE auto step(S& s,float x){return s.process(x);}
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream in(argv[1]);if(!in)return 2;std::string line;unsigned n=0,idle=0,warped=0; unsigned clocks[3]{};std::set<std::string> keys; if(!std::getline(in,line)||line!="CS80_CAP_V1,768")return 2;
while(std::getline(in,line)){if(line.rfind("CAP,",0))return 2;std::stringstream ss(line);std::string x;std::vector<std::string>r;while(std::getline(ss,x,','))r.push_back(x);if(r.size()!=50)return 2;
for(unsigned i=1;i<=5;++i)if(r[i].empty()||r[i].find_first_not_of("0123456789")!=std::string::npos)return 2;
if(r[2]!="0"&&r[2]!="1"&&r[2]!="2")return 2;
if((r[3]!="0"&&r[3]!="1")||(r[4]!="0"&&r[4]!="1"&&r[4]!="2"))return 2;
if(!keys.insert(r[1]+","+r[2]+","+r[5]).second)return 2; ++clocks[r[2][0]-'0'];std::vector<double>v;for(unsigned i=6;i<r.size();++i){char*e;if(r[i].find("0x")==std::string::npos||r[i].find_first_of("pP")==std::string::npos)return 2; double d=strtod(r[i].c_str(),&e);if(*e||e==r[i].c_str()||!std::isfinite(d))return 2;v.push_back(d);}bool u=r[2]=="0",lp=r[3]=="1";
if(v[0]!=(u?3000.:r[2]=="1"?88200.:96000.))return 2;

Ig00156Stage a(lp?kIg00156LowPassNetwork:kIg00156HighPassNetwork);
baseline::Ig00156Stage b(lp?baseline::kIg00156LowPassNetwork:baseline::kIg00156HighPassNetwork);
Ig00156Stage::Memory ma{};baseline::Ig00156Stage::Memory mb{};
memory(ma,v);memory(mb,v);a.restoreMemory(ma);b.restoreMemory(mb);
setup(a,v[1],v[2],v[0],u);setup(b,v[1],v[2],v[0],u);
auto compare=[&](float input){
 const auto x=step(a,input);const auto y=step(b,input);
 check(std::bit_cast<std::uint32_t>(x.hp)==std::bit_cast<std::uint32_t>(y.hp)
    &&std::bit_cast<std::uint32_t>(x.lp)==std::bit_cast<std::uint32_t>(y.lp)
    &&std::bit_cast<std::uint32_t>(x.bp)==std::bit_cast<std::uint32_t>(y.bp),"default output exact");
 check(memoryEqual(a.getMemory(),b.getMemory()),"default named history exact");
 check(memoryFinite(a.getMemory())&&std::isfinite(x.hp)&&std::isfinite(x.lp)&&std::isfinite(x.bp),"default state/output finite");
};
check(memoryEqual(a.getMemory(),b.getMemory()),"restored history exact");
compare(float(v[3]));
for(int j=0;j<128;++j)compare(0.f);
const auto ap=a.halfSteps();const auto bp=b.halfSteps();
setup(a,v[1],v[2],96000.f,false);setup(b,v[1],v[2],96000.f,false);
a.reReference(ap);b.reReference(bp);
check(memoryEqual(a.getMemory(),b.getMemory()),"re-reference history exact");
for(int j=0;j<128;++j)compare(.01f*std::sin(j*.04f));
if(u)++idle;else ++warped;++n;
}
if(n!=768||clocks[0]!=256||clocks[1]!=256||clocks[2]!=256)return 2;
printf("default captures=%u idle=%u warped=%u samples=%u failures=%d\n",n,idle,warped,n*257,bad);
return bad?1:0;
}
