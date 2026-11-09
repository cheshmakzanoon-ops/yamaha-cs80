#include "experimental/MemoryVerifiedDiode.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#if defined(__SSE2__)
#include <xmmintrin.h>
#endif
namespace v=cs80::experimental::memory_sparse::verified;
namespace {
int checks=0, failures=0;
void check(bool yes,const char* label) { ++checks; if(!yes) { ++failures; std::cerr<<"FAIL "<<label<<'\n'; } }
bool sentinel(v::Interval x) { return x.lo==-123.0 && x.hi==456.0; }
int selfTests() {
 check(v::supportedArithmetic(),"default arithmetic");
 const double nan=std::numeric_limits<double>::quiet_NaN(), inf=std::numeric_limits<double>::infinity();
 for(double x:{0.,-0.,-1.,nan,inf,-inf}) {
  v::Interval out{-123.,456.}; check(!v::encloseLog(x,out)&&sentinel(out),"bad log preserves output");
 }
 for(int kind=0;kind<19;++kind) {
  v::Interval across{.7,.7}, out{-123.,456.}; double r=.568,s=2.52e-9,a=.045292704,width=1e-12;
  if(kind==0) across={1.,-1.};
  if(kind==1) across={nan,.7};
  if(kind==2) across={.7,inf};
  if(kind>=3&&kind<=6) r=std::array<double,4>{0.,-1.,nan,inf}[kind-3];
  if(kind>=7&&kind<=10) s=std::array<double,4>{0.,-1.,nan,inf}[kind-7];
  if(kind>=11&&kind<=14) a=std::array<double,4>{0.,-1.,nan,inf}[kind-11];
  if(kind>=15) width=std::array<double,4>{0.,-1.,nan,inf}[kind-15];
  check(!v::encloseDiode(across,r,s,a,0.,out,width)&&sentinel(out),"invalid diode preserves output");
 }
 {
  v::Interval out{-123.,456.};
  check(!v::encloseDiode({.7,.7},.568,2.52e-9,.045292704,0.,out,std::numeric_limits<double>::denorm_min())&&sentinel(out),"unachievable width fails closed");
 }
 {
  v::Interval out{-123.,456.};
  check(!v::encloseDiode({.1,.9},.568,2.52e-9,.045292704,0.,out,1e-12)&&sentinel(out),"uncertain voltage too wide fails closed");
 }
 for(double hint:{nan,inf,-inf,-1e200,1e200,0.}) {
  v::Interval out;
  check(v::encloseDiode({.7,.7},.568,2.52e-9,.045292704,hint,out)&&out.lo>0.&&out.hi-out.lo<=1e-12,"untrusted hint cannot bypass validation");
 }
 check(!v::valid(v::add(v::point(std::numeric_limits<double>::max()),v::point(std::numeric_limits<double>::max()))),"overflow addition rejected");
 check(!v::valid(v::mul(v::point(1e300),v::point(1e300))),"overflow multiplication rejected");
 check(!v::valid(v::div(v::point(1.),{-1.,1.})),"zero-containing denominator rejected");
 const int original=std::fegetround();
 for(int mode:{FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}) {
  check(std::fesetround(mode)==0,"set rounding test mode");
  v::Interval out{-123.,456.};
  check(!v::supportedArithmetic(),"non-nearest mode rejected");
  check(!v::encloseLog(2.,out)&&sentinel(out),"log rejects non-nearest");
  check(!v::encloseDiode({.7,.7},.568,2.52e-9,.045292704,0.,out)&&sentinel(out),"diode rejects non-nearest");
 }
 std::fesetround(original);
#if defined(__SSE2__)
 const auto csr=_mm_getcsr();
 for(unsigned bit:{0x0040u,0x8000u,0x8040u,0x2000u,0x4000u,0x6000u}) {
  _mm_setcsr(csr|bit);
  v::Interval out{-123.,456.};
  const bool supported=v::supportedArithmetic(), log=v::encloseLog(2.,out), diode=v::encloseDiode({.7,.7},.568,2.52e-9,.045292704,0.,out);
  _mm_setcsr(csr);
  check(!supported&&!log&&!diode&&sentinel(out),"DAZ/FTZ or SIMD rounding rejected without output mutation");
 }
#endif
 check(v::supportedArithmetic(),"environment restored");
 std::cout<<"Self checks: "<<checks<<", failures: "<<failures<<'\n'; return failures?1:0;
}
double number() { std::string x; std::cin>>x; return std::strtod(x.c_str(),nullptr); }
}
int main(int argc,char** argv) {
 if(argc==2&&std::string(argv[1])=="--self-tests") return selfTests();
 std::string op;
 while(std::cin>>op) {
  v::Interval out{-123.,456.}; bool ok=false;
  if(op=="log") ok=v::encloseLog(number(),out);
  else if(op=="diode") {
   double lo=number(),hi=number(),r=number(),s=number(),a=number(),hint=number(),width=number();
   ok=v::encloseDiode({lo,hi},r,s,a,hint,out,width);
  } else {
   double al=number(),ah=number(),bl=number(),bh=number(); v::Interval a{al,ah},b{bl,bh};
   if(op=="add") out=v::add(a,b); else if(op=="sub") out=v::sub(a,b);
   else if(op=="mul") out=v::mul(a,b); else if(op=="div") out=v::div(a,b); else return 2;
   ok=v::valid(out);
  }
  std::cout<<ok<<' '<<std::hexfloat<<out.lo<<' '<<out.hi<<std::defaultfloat<<'\n';
 }
}
