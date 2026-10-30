#include "BenchmarkBridge.h"
#include <immintrin.h>
#include <cstdio>
#include <vector>
int main(){const auto before=_mm_getcsr();std::printf("process entry MXCSR=0x%08x FTZ=%d DAZ=%d\n",before,(before>>15)&1,(before>>6)&1);for(int arm=0;arm<2;++arm){auto a=arm?candidateApi():baselineApi();auto p=a.create({48000,128,2,0,0,true});auto prepared=_mm_getcsr();std::vector<float> x(256);a.callback(p,0,x.data(),nullptr);auto after=_mm_getcsr();std::printf("%s after setup=0x%08x after callback=0x%08x FTZ=%d DAZ=%d\n",arm?"candidate":"baseline",prepared,after,(after>>15)&1,(after>>6)&1);a.destroy(p);}}
