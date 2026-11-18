#include <immintrin.h>
#include <cfenv>
#include <cerrno>
#include <cstdio>
#include "core/WscImmediateScalar.h"
static void snap(const char* s){unsigned csr;unsigned short cw,sw;asm volatile("stmxcsr %0;fnstcw %1;fnstsw %2":"=m"(csr),"=m"(cw),"=m"(sw)::"memory");std::printf("%s mxcsr=0x%04x x87=0x%04x status=0x%04x errno=%d eligible=%d\n",s,csr,cw,sw,errno,int(cs80::wscImmediateScalarEligible()));}
int main(){std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0U);errno=19;snap("initial");volatile double a=1.,b=10.;volatile double q=a/b;snap("after-inexact");return q>0.?0:2;}
