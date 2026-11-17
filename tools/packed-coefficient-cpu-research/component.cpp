// Isolated one-TU component oracle: test-only access, actual backend source unchanged.
#include <bits/stdc++.h>
#include <cfenv>
#include <cerrno>
#include <xmmintrin.h>
#define private public
#include "core/Ig00156Batch.h"
#undef private
#include BATCH_SOURCE
using S=cs80::Ig00156Stage;using B=cs80::Ig00156Batch;
std::uint64_t checks=0,accepted=0,rejected=0;FILE* file;
void need(bool x){++checks;if(!x){std::fprintf(stderr,"FAIL check %llu\n",(unsigned long long)checks);std::exit(1);}}
template<class T>void put(const T&x){need(std::fwrite(&x,sizeof(x),1,file)==1);}
const std::array<double S::*,10> coeff{&S::inputScale,&S::gmPart,&S::ffPart,&S::xFromFb,&S::xFromIn,&S::xFromU,&S::xFromBp,&S::kiGs,&S::invA11,&S::inFromX};
const std::array<double S::*,28> all{&S::gs,&S::gg,&S::gf,&S::gTotal,&S::inputScale,&S::feedforwardHz,&S::kHalf,&S::kHalfCoupling,&S::gmPart,&S::ffPart,&S::qSiemensValue,&S::kiGs,&S::xFromFb,&S::xFromIn,&S::xFromU,&S::xFromBp,&S::inFromX,&S::invA11,&S::zIn,&S::zFb,&S::zBp,&S::zLp,&S::vIn,&S::vFb,&S::vBp,&S::vLp,&S::slope1,&S::slope2};
std::array<S,4> setup(){std::array<S,4>a{S(cs80::kIg00156HighPassNetwork),S(cs80::kIg00156LowPassNetwork),S(cs80::kIg00156HighPassNetwork),S(cs80::kIg00156LowPassNetwork)};for(int i=0;i<4;++i)a[i].set(500.f+i*3300.f,.5f+i*2.f,96000.f,false);return a;}
void dump(const S&s){for(double x:{s.net.sourceOhms,s.net.groundOhms,s.net.inputFarads,s.net.feedbackOhms,s.net.feedbackFarads,s.net.integratorFarads,s.net.feedforwardOhms})put(std::bit_cast<std::uint64_t>(x));for(auto m:all)put(std::bit_cast<std::uint64_t>(s.*m));put(std::bit_cast<std::uint32_t>(s.lastCutoff));put(std::bit_cast<std::uint32_t>(s.lastQ));put(std::bit_cast<std::uint32_t>(s.lastRate));put(s.lastUnwarped);put(s.cutoffCoefficientsValid);}
void run(std::array<S,4> a,B::Inputs input,B::Kernel kernel,unsigned csr,bool wrapper,bool duplicate=false){
 auto before=a;B::Outputs output;std::memset(&output,0xa5,sizeof(output));const auto outputBefore=output;B::Stages pointers{&a[0],&a[1],&a[2],&a[3]};if(duplicate)pointers[3]=pointers[0];
 std::feclearexcept(FE_ALL_EXCEPT);if(csr&0x20)std::feraiseexcept(FE_INEXACT);errno=EDOM;_mm_setcsr(csr);
 bool used=wrapper?cs80::processIg00156Four(pointers,input,output,kernel):kernel(pointers,input,output);
 const unsigned actual=_mm_getcsr();unsigned short sw;asm volatile("fnstsw %0":"=a"(sw));const int err=errno;
 _mm_setcsr(0x1f80);std::feclearexcept(FE_ALL_EXCEPT);
 if(!used&&!wrapper){need(std::memcmp(&output,&outputBefore,sizeof(output))==0);for(int i=0;i<4;++i){for(auto m:all)need(std::bit_cast<std::uint64_t>(a[i].*m)==std::bit_cast<std::uint64_t>(before[i].*m));need(std::memcmp(&a[i].net,&before[i].net,sizeof(a[i].net))==0);need(a[i].lastUnwarped==before[i].lastUnwarped&&a[i].cutoffCoefficientsValid==before[i].cutoffCoefficientsValid);need(std::bit_cast<std::uint32_t>(a[i].lastCutoff)==std::bit_cast<std::uint32_t>(before[i].lastCutoff));need(std::bit_cast<std::uint32_t>(a[i].lastQ)==std::bit_cast<std::uint32_t>(before[i].lastQ));need(std::bit_cast<std::uint32_t>(a[i].lastRate)==std::bit_cast<std::uint32_t>(before[i].lastRate));}}
 if(duplicate)need(!used);accepted+=used;rejected+=!used;put(used);put(actual);put(sw);put(err);for(auto&s:a)dump(s);for(auto&o:output){put(std::bit_cast<std::uint32_t>(o.lp));put(std::bit_cast<std::uint32_t>(o.hp));put(std::bit_cast<std::uint32_t>(o.bp));}
}
int main(int argc,char**argv){if(argc!=2)return 2;file=std::fopen(argv[1],"wb");if(!file)return 2;auto kernel=B::selectKernel();if(!kernel)return 3;std::fenv_t saved;std::feholdexcept(&saved);unsigned savedMx=_mm_getcsr();
 const std::vector<std::uint64_t> bits{0,0x8000000000000000ull,1,0x8000000000000001ull,0xfffffffffffffull,0x800fffffffffffffull,0x3ff0000000000000ull,0xbff0000000000000ull,0xbff0000000000001ull,0x412e848000000000ull,0xc12e848000000000ull,0x412e847fffffffffull,0x412e848000000001ull,0xc12e848000000001ull,0x7ff0000000000000ull,0xfff0000000000000ull,0x7ff8000000000001ull,0x7ff0000000000001ull,0x0010000000000000ull,0x8010000000000000ull,0xc12e847fffffffffull,0xfff8000000000001ull,0xfff0000000000001ull,0xbfefffffffffffffull};
 B::Inputs input{.1f,-.2f,.3f,-.4f};const auto base=setup();
 for(unsigned rounding:{0u,0x2000u,0x4000u,0x6000u})for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u})for(unsigned sticky:{0u,0x2u,0x3fu}){
  unsigned csr=0x1f80|rounding|denorm|sticky;
  for(bool wrapper:{false,true}){
   run(base,input,kernel,csr,wrapper);
   for(int field=0;field<10;++field)for(int lane=0;lane<4;++lane)for(auto bit:bits){auto a=base;a[lane].*coeff[field]=std::bit_cast<double>(bit);run(a,input,kernel,csr,wrapper);}
   // Every ordered earlier/later lane pair; each physical compare can see denormals.
   for(int early=0;early<4;++early)for(int late=early+1;late<4;++late)for(int physical:{1,2,6})for(auto tiny:{1ull,0x8000000000000001ull}){auto a=base;a[early].*coeff[physical]=std::bit_cast<double>(tiny);a[late].inputScale=std::bit_cast<double>(0x7ff0000000000001ull);run(a,input,kernel,csr,wrapper);}
   for(int lane=0;lane<4;++lane)for(std::uint32_t value:{0u,0x80000000u,1u,0x80000001u,0x42800000u,0xc2800000u,0x42800001u,0xc2800001u,0x7f800000u,0x7fc00001u,0x7f800001u}){auto in=input;in[lane]=std::bit_cast<float>(value);run(base,in,kernel,csr,wrapper);}
   for(int lane=0;lane<4;++lane)for(std::uint32_t value:{0u,0x80000000u,1u,0x80000001u,0x45fa0000u,0x45f9ffffu,0x45fa0001u,0x49bb8000u,0x49bb8001u,0x7f800000u,0x7fc00001u,0x7f800001u}){auto a=base;a[lane].lastRate=std::bit_cast<float>(value);run(a,input,kernel,csr,wrapper);}
   // Earlier metadata/rate/input rejection suppresses later physical comparisons.
   for(int early=0;early<4;++early)for(int later=early;later<4;++later)for(int bad=0;bad<4;++bad){auto a=base;auto in=input;a[later].gmPart=std::bit_cast<double>(1ull);if(bad==0)a[early].lastRate=0;if(bad==1)a[early].lastUnwarped=true;if(bad==2)a[early].cutoffCoefficientsValid=false;if(bad==3)in[early]=std::bit_cast<float>(0x7f800001u);run(a,in,kernel,csr,wrapper);}
  }
  run(base,input,kernel,csr,true,true);
  run(base,input,nullptr,csr,true);
 }
 // Unsupported trap masks: direct rejected kernel only, never execute scalar arithmetic with unmasked traps.
 for(unsigned mask:{0x80u,0x100u,0x200u,0x400u,0x800u,0x1000u})run(base,input,kernel,0x1f80&~mask,false);
 std::fclose(file);std::fesetenv(&saved);_mm_setcsr(savedMx);std::printf("PASS checks=%llu admitted=%llu rejected=%llu\n",(unsigned long long)checks,(unsigned long long)accepted,(unsigned long long)rejected);return accepted?0:4;}
