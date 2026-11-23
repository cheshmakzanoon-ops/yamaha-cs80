#define cs80 cs80_baseline
#include "/workspace/scratch/3152723d2891/yamaha-cs80/src/core/InstrumentBody.h"
#undef cs80
#include "/workspace/scratch/a9592b052b11/cs80-work/src/core/InstrumentBody.h"
#include <ctime>
#include <cstdio>
#include <vector>
#include <algorithm>
template<class Body> double run(int model,int note,double &sum) {
 Body a;a.prepare(96000,48000);a.noteOn(model,note,.9f,.6f,1.f,23456);
 const float f=440.0*std::exp2((note-69.)/12.);
 const std::clock_t start=std::clock();
 for(int i=0;i<192000;i++)sum+=a.tick(f,.6f);
 return 1.e9*(std::clock()-start)/CLOCKS_PER_SEC/192000;
}
int main(){ double sum=0;for(int note:{36,60,84})for(int model:{1,2,9,15}){
 std::vector<double>b,c;for(int i=0;i<7;i++) { if(i%2){c.push_back(run<cs80::body::InstrumentBody>(model,note,sum));b.push_back(run<cs80_baseline::body::InstrumentBody>(model,note,sum));}else{b.push_back(run<cs80_baseline::body::InstrumentBody>(model,note,sum));c.push_back(run<cs80::body::InstrumentBody>(model,note,sum));}}
 std::sort(b.begin(),b.end());std::sort(c.begin(),c.end());std::printf("note %d %s baseline %.3f current %.3f ratio %.3f\n",note,cs80::body::name(model),b[3],c[3],c[3]/b[3]);}
 std::printf("checksum %.12g\n",sum);
}
