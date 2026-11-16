// Inspection only: no memo, Engine change, or timing.
#include <cmath>
#include <cerrno>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <dlfcn.h>
#include <link.h>
#include <gnu/libc-version.h>
#include <xmmintrin.h>
#if !defined(__linux__) || !defined(__x86_64__) || !defined(__GNUC__) || defined(__clang__)
#error This bounded inspection fixture requires GNU C++ on Linux x86-64.
#endif
extern "C" __attribute__((noinline,noipa,noclone)) float raw_pow(float x) {return std::pow(2.0f,x);}
static unsigned short sw(){unsigned short x;asm volatile("fnstsw %0":"=a"(x)::"memory");return x;}
static unsigned short cw(){unsigned short x;asm volatile("fnstcw %0":"=m"(x)::"memory");return x;}
static int object(struct dl_phdr_info* info,size_t,void*) {if(info->dlpi_name && info->dlpi_name[0])std::printf("OBJECT\t%s\n",info->dlpi_name);return 0;}
int main() {
 for(auto name:{"LD_PRELOAD","LD_AUDIT","LD_LIBRARY_PATH"})if(const char* x=std::getenv(name);x&&*x){std::fprintf(stderr,"Unexpected %s: inspection refused\n",name);return 2;}
 fenv_t saved;if(fegetenv(&saved))return 3;
 // Read the exact powf GOT slot. Its relocation and the raw_pow -> powf@plt
 // edge are independently retained and checked by the runner.
 void** slot;asm volatile("lea powf@GOTPCREL(%%rip),%0":"=r"(slot));
 void* resolved=*slot;void* lookup=dlsym(RTLD_DEFAULT,"powf");Dl_info info{};
 if(!resolved||resolved!=lookup||!dladdr(resolved,&info)||!info.dli_fname)return 4;
 std::printf("BIND\t%p\t%p\t%p\t0x%llx\t%s\n",static_cast<void*>(slot),resolved,info.dli_fbase,(unsigned long long)((uintptr_t)resolved-(uintptr_t)info.dli_fbase),info.dli_fname);
 std::printf("GLIBC\t%s\n",gnu_get_libc_version());dl_iterate_phdr(object,nullptr);
 std::uint64_t calls=0,errors=0;const auto initial=cw();
 for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u})for(unsigned rounding:{0u,0x400u,0x800u,0xc00u}){
  const unsigned short word=(initial&~0xc00u)|rounding|0x3fu;asm volatile("fldcw %0"::"m"(word):"memory");
  for(int i=-65536;i<=65536;++i){
   const float x=float(i)*(120.f/65536.f);_mm_setcsr(0x1f80u|denorm|((i&1)?0x3fu:0u));errno=i%3==0?0:i%3==1?EDOM:ERANGE;
   const int en=errno;const auto before=sw(),control=cw();const float y=raw_pow(x);const auto after=sw();++calls;
   if(!std::isnormal(y)||errno!=en||after!=before||cw()!=control||(_mm_getcsr()&~63u)!=(0x1f80u|denorm)){
    if(errors++<10)std::printf("FAIL\tx=%a y=%a before=%x after=%x errno=%d/%d\n",x,y,before,after,en,errno);
   }
  }
 }
 const bool bindingStable=*slot==resolved;fesetenv(&saved);
 std::printf("RESULT\t%llu\t%llu\t%d\n",(unsigned long long)calls,(unsigned long long)errors,bindingStable);
 return errors||!bindingStable?1:0;
}
