#include "core/Ig00156Batch.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>
#include <bit>
#include <cmath>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <set>
// Production-backend regression. Runtime-unavailable backend returns 77 (CTest skip),
// never a passing SIMD qualification. Optional diagnostic counters live in scratch evidence.
using namespace cs80;using S=Ig00156Stage;using B=Ig00156Batch;
unsigned failures=0,checks=0;void ck(bool b,const char*s){++checks;if(!b&&++failures<20)printf("FAIL %s\n",s);}
bool eq(double a,double b){return std::bit_cast<uint64_t>(a)==std::bit_cast<uint64_t>(b);}
auto fields(const S::Memory&m){return std::array<double,12>{m.z.input,m.z.feedback,m.z.bp,m.z.lp,m.v.input,m.v.feedback,m.v.bp,m.v.lp,m.slope1,m.slope2,m.halfIntegrators,m.halfCouplings};}
bool mem(const S&a,const S&b){auto x=fields(a.getMemory()),y=fields(b.getMemory());for(int j=0;j<12;++j)if(!eq(x[j],y[j]))return false;return true;}
bool out(const S::Output&a,const S::Output&b){return eq(a.hp,b.hp)&&eq(a.lp,b.lp)&&eq(a.bp,b.bp);}
struct C{bool lp;std::vector<double>v;};
S restore(const C&c){S s(c.lp?kIg00156LowPassNetwork:kIg00156HighPassNetwork);S::Memory m{};auto&v=c.v;m.z={v[4],v[5],v[6],v[7]};m.v={v[8],v[9],v[10],v[11]};m.slope1=v[12];m.slope2=v[13];m.halfIntegrators=v[14];m.halfCouplings=v[15];s.restoreMemory(m);s.set(v[1],v[2],v[0],false);return s;}
auto fresh(){std::array<S,4>a{S(kIg00156HighPassNetwork),S(kIg00156LowPassNetwork),S(kIg00156HighPassNetwork),S(kIg00156LowPassNetwork)};for(auto&s:a)s.set(1000,1,96000);return a;}
B::Stages ptr(std::array<S,4>&a){return{&a[0],&a[1],&a[2],&a[3]};}
int main(int argc,char**argv){
 if(argc!=2)return 2;std::ifstream f(argv[1]);if(!f)return 2;
 std::string l;if(!std::getline(f,l)||l!="CS80_CAP_V1,768")return 2;
 std::vector<C>cases;unsigned counts[3]{};std::set<std::string> keys;
 while(std::getline(f,l)){
  if(l.rfind("CAP,",0))return 2;
  std::stringstream ss(l);std::vector<std::string>r;std::string t;
  while(std::getline(ss,t,','))r.push_back(t);if(r.size()!=50)return 2;
  for(unsigned i=1;i<=5;++i)if(r[i].empty()||r[i].find_first_not_of("0123456789")!=std::string::npos)return 2;
  if(r[2]!="0"&&r[2]!="1"&&r[2]!="2")return 2;
  if((r[3]!="0"&&r[3]!="1")||(r[4]!="0"&&r[4]!="1"&&r[4]!="2"))return 2;
  if(!keys.insert(r[1]+","+r[2]+","+r[5]).second)return 2;
  C c{r[3]=="1",{}};
  for(unsigned i=6;i<50;++i){char* end=nullptr;
   if(r[i].find("0x")==std::string::npos||r[i].find_first_of("pP")==std::string::npos)return 2;
   double value=std::strtod(r[i].c_str(),&end);
   if(end==r[i].c_str()||*end||!std::isfinite(value))return 2;c.v.push_back(value);
  }
  unsigned clock=unsigned(r[2][0]-'0');if(c.v[0]!=(clock==0?3000.:clock==1?88200.:96000.))return 2;
  ++counts[clock];if(clock)cases.push_back(c);
 }
 if(counts[0]!=256||counts[1]!=256||counts[2]!=256||cases.size()!=512)return 2;
 auto kernel=B::selectKernel();if(!kernel){puts("SKIP: AVX2 unavailable or disabled; no SIMD qualification");return 77;}

unsigned long long eligibleCases=0,ineligibleCases=0;
for(const auto&c:cases)for(int lane=0;lane<4;++lane){auto a=fresh();a[lane]=restore(c);auto before=a,scalar=a;B::Inputs in{};in[lane]=float(c.v[3]);B::Outputs got;for(auto&o:got)o={11,22,33};auto sentinel=got;bool used=kernel(ptr(a),in,got);
 ++eligibleCases;ck(!used,"eligible cap rejection");for(int i=0;i<4;++i){ck(mem(a[i],before[i]),"direct state atomicity");ck(out(got[i],sentinel[i]),"direct output atomicity");}
 B::Outputs want;for(int i=0;i<4;++i)want[i]=scalar[i].process(in[i]);ck(!processIg00156Four(ptr(a),in,got,kernel),"wrapper uses scalar repair");for(int i=0;i<4;++i){ck(out(got[i],want[i]),"repair output parity");ck(mem(a[i],scalar[i]),"repair state parity");}
 for(int n=0;n<32;++n){for(int i=0;i<4;++i)in[i]=.03f*std::sin(n*.17f);processIg00156Four(ptr(a),in,got,kernel);for(int i=0;i<4;++i){auto y=scalar[i].process(in[i]);ck(out(y,got[i])&&mem(a[i],scalar[i]),"continuation parity");ck(std::isfinite(y.hp)&&std::isfinite(y.lp)&&std::isfinite(y.bp),"finite continuation");}}
}
// Four failing lanes, then duplicate pointers preserving sequential scalar meaning.
for(const auto&c:cases){auto a=fresh();for(auto&s:a)s=restore(c);auto scalar=a;B::Inputs in;in.fill(float(c.v[3]));B::Outputs got;bool used=processIg00156Four(ptr(a),in,got,kernel);for(int i=0;i<4;++i){auto y=scalar[i].process(in[i]);ck(out(y,got[i])&&mem(a[i],scalar[i]),"multifail parity");}ck(!used,"multifail scalar");
 a=fresh();a[0]=restore(c);scalar=a;B::Stages duplicate{&a[0],&a[0],&a[1],&a[2]};ck(!processIg00156Four(duplicate,in,got,kernel),"duplicates scalar");int indices[]{0,0,1,2};for(int i=0;i<4;++i){auto y=scalar[indices[i]].process(in[i]);ck(out(y,got[i]),"duplicate sequential output");}for(int i=0;i<4;++i)ck(mem(a[i],scalar[i]),"duplicate state");}
// Unsupported admissions must reject without mutations too.
for(int kind=0;kind<4;++kind){auto a=fresh();B::Inputs in{};if(kind==0)a[0].set(1000,1,7999);if(kind==1)a[0].set(1000,1,96000,true);if(kind==2)in[0]=INFINITY;if(kind==3)in[0]=65;auto before=a;B::Outputs got;for(auto&o:got)o={11,22,33};auto sentinel=got;ck(!kernel(ptr(a),in,got),"unsupported admission rejected");for(int i=0;i<4;++i){ck(mem(a[i],before[i]),"unsupported state unchanged");ck(out(got[i],sentinel[i]),"unsupported output unchanged");}}
// Exercise successful commits and rejected caps in supported reset-origin noise.
// Fifth-evaluation attribution is diagnostic-only; this production test checks parity.
uint32_t rng=803021;auto a=fresh();for(auto&s:a)s.set(43200,9,96000);unsigned success=0;
for(int n=0;n<20000;++n){B::Inputs in;for(auto&x:in){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;x=float(double(rng)/4294967295.*16.-8.);}auto scalar=a;B::Outputs got;bool used=processIg00156Four(ptr(a),in,got,kernel);if(used)++success;for(int i=0;i<4;++i){auto y=scalar[i].process(in[i]);ck(out(y,got[i])&&mem(a[i],scalar[i]),"stress parity");}}
ck(eligibleCases>0,"eligible captured failures exercised");ck(success>0,"successful vector commit exercised");printf("captures=512 tested_positions=%llu ineligible_positions=%llu vector_success=%u checks=%u failures=%u\n",eligibleCases,ineligibleCases,success,checks,failures);return failures?1:0;}
