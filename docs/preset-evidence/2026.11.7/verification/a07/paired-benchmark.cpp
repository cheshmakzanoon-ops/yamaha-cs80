#include "core/InstrumentBody.h"
#include "paired-baseline/InstrumentBody.h"
#include <chrono>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
double receipt=0;
template<class Body> double bench(int note){
 Body body;body.prepare(96000,48000);body.noteOn(15,note,.85f,1,1,0x237165e,1,3);
 float f=440*std::exp2((note-69.)/12);double sum=0;
 auto t=std::chrono::steady_clock::now();
 for(int i=0;i<384000;++i)sum+=body.tick(f,1);
 double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-t).count()/384000;
 receipt+=sum;return ns;
}
int main(){for(int note:{36,48,60}){
 std::array<double,7>a{},b{};
 for(int i=0;i<7;++i){if(i%2){b[i]=bench<cs80::body::InstrumentBody>(note);a[i]=bench<cs80old::body::InstrumentBody>(note);}else{a[i]=bench<cs80old::body::InstrumentBody>(note);b[i]=bench<cs80::body::InstrumentBody>(note);}}
 std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
 std::printf("note=%d old_median_ns=%.3f current_median_ns=%.3f ratio=%.3f\n",note,a[3],b[3],b[3]/a[3]);
 }std::printf("receipt=%.9g\n",receipt);}
