// Fresh captured-equation regression; see tests/data/ig00156-cap-states.md.
// Runtime barriers isolate caller specialization without relaxing residual gates.
#include "core/Ig00156Filter.h"
#include "FilterIdleFallbackBaseline.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <limits>
#include <cstring>
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
bool rootEqual(const ig00156_detail::RootPoint&a,const ig00156_detail::RootPoint&b){return bits(a.bp,b.bp)&&bits(a.hp,b.hp)&&bits(a.t1,b.t1)&&bits(a.t2,b.t2)&&bits(a.residual,b.residual);}
int bad=0;void check(bool b,const char*s){if(!b){++bad;if(bad<20)std::printf("FAIL %s\n",s);}}
template<class M>void memory(M&m,const std::vector<double>&v){m.z={v[4],v[5],v[6],v[7]};m.v={v[8],v[9],v[10],v[11]};m.slope1=v[12];m.slope2=v[13];m.halfIntegrators=v[14];m.halfCouplings=v[15];}
double residual(double x,const std::vector<double>&v){double inv=1/v[21];double hp=v[17]+v[18]*x+v[19]*v[21]*fastTanh(x*inv)+v[20]*x;return x-v[16]+v[19]*v[21]*fastTanh(hp*inv)+v[20]*hp;}
template<class S> FIXTURE_NOINLINE void setup(S& s,float fc,float q,float rate,bool idle){s.set(fc,q,rate,idle);}
template<class S> FIXTURE_NOINLINE auto step(S& s,float x){return s.process(x);}
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream in(argv[1]);if(!in)return 2;std::string line;unsigned n=0,idle=0,warped=0; unsigned clocks[3]{};std::set<std::string> keys; if(!std::getline(in,line)||line!="CS80_CAP_V1,768")return 2;
while(std::getline(in,line)){if(line.rfind("CAP,",0))return 2;std::stringstream ss(line);std::string x;std::vector<std::string>r;while(std::getline(ss,x,','))r.push_back(x);if(r.size()!=50)return 2;
for(unsigned i=1;i<=5;++i)if(r[i].empty()||r[i].find_first_not_of("0123456789")!=std::string::npos)return 2;
if(r[2]!="0"&&r[2]!="1"&&r[2]!="2")return 2;
if((r[3]!="0"&&r[3]!="1")||(r[4]!="0"&&r[4]!="1"&&r[4]!="2"))return 2;
if(!keys.insert(r[1]+","+r[2]+","+r[5]).second)return 2; ++clocks[r[2][0]-'0'];std::vector<double>v;for(unsigned i=6;i<r.size();++i){char*e;if(r[i].find("0x")==std::string::npos||r[i].find_first_of("pP")==std::string::npos)return 2; double d=strtod(r[i].c_str(),&e);if(*e||e==r[i].c_str()||!std::isfinite(d))return 2;v.push_back(d);}bool u=r[2]=="0",lp=r[3]=="1";
if(v[0]!=(u?3000.:r[2]=="1"?88200.:96000.))return 2;
ig00156_detail::RootPoint p{};bool ok=ig00156_detail::bracketedRoot(v[16],v[17],v[18],v[19],v[20],v[21],p);check(ok,"capture helper success");check(std::isfinite(p.bp)&&std::isfinite(p.hp)&&std::isfinite(p.t1)&&std::isfinite(p.t2)&&std::isfinite(p.residual),"tuple finite");check(std::abs(residual(p.bp,v))<1e-7,"independent residual");
const double independentT2=fastTanh(p.bp*(1.0/v[21]));
const double independentHp=v[17]+v[18]*p.bp+v[19]*v[21]*independentT2+v[20]*p.bp;
const double independentT1=fastTanh(independentHp*(1.0/v[21]));
const double independentResidual=residual(p.bp,v);
check(std::isfinite(independentHp)&&std::isfinite(independentT1)&&std::isfinite(independentT2)&&std::isfinite(independentResidual)
    &&std::abs(p.hp-independentHp)<=1e-12&&std::abs(p.t1-independentT1)<=1e-12
    &&std::abs(p.t2-independentT2)<=1e-12&&std::abs(p.residual-independentResidual)<=1e-12,
    "accepted tuple consistent with independent equation");
Ig00156Stage a(lp?kIg00156LowPassNetwork:kIg00156HighPassNetwork);baseline::Ig00156Stage b(lp?baseline::kIg00156LowPassNetwork:baseline::kIg00156HighPassNetwork);Ig00156Stage::Memory ma{};baseline::Ig00156Stage::Memory mb{};memory(ma,v);memory(mb,v);a.restoreMemory(ma);b.restoreMemory(mb);setup(a,v[1],v[2],v[0],u);setup(b,v[1],v[2],v[0],u);auto ao=step(a,float(v[3]));auto bo=step(b,float(v[3]));check(std::abs(b.getNodes().bp-v[40])<1e-10,"baseline original bp");check(std::abs(residual(b.getNodes().bp,v)-v[42])<1e-10,"baseline residual");
if(u)++idle;else ++warped;
check(std::isfinite(a.getNodes().bp)&&std::abs(residual(a.getNodes().bp,v))<1e-7,"all clock stage repaired");
for(int j=0;j<128;++j){auto y=step(a,0.f);check(std::isfinite(y.lp)&&std::isfinite(y.hp)&&std::isfinite(y.bp),"continuation finite");check(memoryFinite(a.getMemory()),"continuation memory finite");}
auto prior=a.halfSteps();setup(a,v[1],v[2],96000.f,false);a.reReference(prior);
for(int j=0;j<128;++j){auto y=step(a,.01f*std::sin(j*.04f));check(std::isfinite(y.lp)&&std::isfinite(y.hp)&&std::isfinite(y.bp),"re-reference finite");check(memoryFinite(a.getMemory()),"re-reference memory finite");}
++n;}
if(n!=768||clocks[0]!=256||clocks[1]!=256||clocks[2]!=256)return 2;
auto reject=[](double z,double c0,double c1,double g,double ff,double L){ig00156_detail::RootPoint p{11,22,33,44,55},old=p;check(!ig00156_detail::bracketedRoot(z,c0,c1,g,ff,L,p),"guard rejected");check(rootEqual(p,old),"failure tuple unchanged");};
for(int i=0;i<6;++i)for(double d:{std::numeric_limits<double>::quiet_NaN(),double(INFINITY),-double(INFINITY)}){double v[]={1,.2,1,2,.1,2.5};v[i]=d;reject(v[0],v[1],v[2],v[3],v[4],v[5]);}
reject(1,.2,1,2,.1,std::numeric_limits<double>::denorm_min());
reject(1,.2,1,-1,.1,2.5);reject(1,.2,1,2,-1,2.5);reject(1,.2,1,2,.1,0);reject(1,.2,1,1e308,1e308,2.5);reject(1,.2,-100,2,1,2.5);
double lo=4.97,hi=std::nextafter(lo,INFINITY),z=.5*((lo+fastTanh(fastTanh(lo)))+(hi+fastTanh(fastTanh(hi))));reject(z,0,0,1,0,1);
printf("captures=%u idle=%u warped=%u failures=%d\n",n,idle,warped,bad);return bad?1:0;}
