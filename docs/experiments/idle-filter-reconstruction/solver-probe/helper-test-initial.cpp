#include "core/Ig00156Filter.h"
#include "baseline.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <limits>
#include <cstring>
using namespace cs80;
int bad=0;void check(bool b,const char*s){if(!b){++bad;if(bad<20)std::printf("FAIL %s\n",s);}}
template<class M>void memory(M&m,const std::vector<double>&v){m.z={v[4],v[5],v[6],v[7]};m.v={v[8],v[9],v[10],v[11]};m.slope1=v[12];m.slope2=v[13];m.halfIntegrators=v[14];m.halfCouplings=v[15];}
double residual(double x,const std::vector<double>&v){double inv=1/v[21];double hp=v[17]+v[18]*x+v[19]*v[21]*fastTanh(x*inv)+v[20]*x;return x-v[16]+v[19]*v[21]*fastTanh(hp*inv)+v[20]*hp;}
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream in(argv[1]);if(!in)return 2;std::string line;unsigned n=0,idle=0,warped=0;
while(std::getline(in,line)){if(line.rfind("CAP,",0))continue;std::stringstream ss(line);std::string x;std::vector<std::string>r;while(std::getline(ss,x,','))r.push_back(x);if(r.size()!=50)return 2;std::vector<double>v;for(unsigned i=6;i<r.size();++i){char*e;double d=strtod(r[i].c_str(),&e);if(*e||e==r[i].c_str()||!std::isfinite(d))return 2;v.push_back(d);}bool u=r[2]=="0",lp=r[3]=="1";
ig00156_detail::RootPoint p{};bool ok=ig00156_detail::bracketedRoot(v[16],v[17],v[18],v[19],v[20],v[21],p);check(ok,"capture helper success");check(std::isfinite(p.bp)&&std::isfinite(p.hp)&&std::isfinite(p.t1)&&std::isfinite(p.t2)&&std::isfinite(p.residual),"tuple finite");check(std::abs(residual(p.bp,v))<1e-7,"independent residual");
Ig00156Stage a(lp?kIg00156LowPassNetwork:kIg00156HighPassNetwork);baseline::Ig00156Stage b(lp?baseline::kIg00156LowPassNetwork:baseline::kIg00156HighPassNetwork);Ig00156Stage::Memory ma{};baseline::Ig00156Stage::Memory mb{};memory(ma,v);memory(mb,v);a.restoreMemory(ma);b.restoreMemory(mb);a.set(v[1],v[2],v[0],u);b.set(v[1],v[2],v[0],u);auto ao=a.process(float(v[3]));auto bo=b.process(float(v[3]));check(std::abs(b.getNodes().bp-v[40])<1e-10,"baseline original bp");check(std::abs(residual(b.getNodes().bp,v)-v[42])<1e-10,"baseline residual");
if(u){++idle;check(std::isfinite(a.getNodes().bp)&&std::abs(residual(a.getNodes().bp,v))<1e-7,"idle stage repaired");}else{++warped;auto am=a.getMemory();auto bm=b.getMemory();check(!std::memcmp(&ao,&bo,sizeof ao)&&!std::memcmp(&am,&bm,sizeof am),"warped unchanged");}++n;}
check(n==768,"capture count");
auto reject=[](double z,double c0,double c1,double g,double ff,double L){ig00156_detail::RootPoint p{11,22,33,44,55},old=p;check(!ig00156_detail::bracketedRoot(z,c0,c1,g,ff,L,p),"guard rejected");check(!std::memcmp(&p,&old,sizeof p),"failure tuple unchanged");};
for(int i=0;i<6;++i)for(double d:{std::numeric_limits<double>::quiet_NaN(),double(INFINITY),-double(INFINITY)}){double v[]={1,.2,1,2,.1,2.5};v[i]=d;reject(v[0],v[1],v[2],v[3],v[4],v[5]);}
reject(1,.2,1,-1,.1,2.5);reject(1,.2,1,2,-1,2.5);reject(1,.2,1,2,.1,0);reject(1,.2,1,1e308,1e308,2.5);reject(1,.2,-100,2,1,2.5);
double lo=4.97,hi=std::nextafter(lo,INFINITY),z=.5*((lo+fastTanh(fastTanh(lo)))+(hi+fastTanh(fastTanh(hi))));reject(z,0,0,1,0,1);
printf("captures=%u idle=%u warped=%u failures=%d\n",n,idle,warped,bad);return bad?1:0;}
