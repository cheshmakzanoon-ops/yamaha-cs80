#include "core/InstrumentBody.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
int main(){for(int model:{cs80::body::flute,cs80::body::piano,cs80::body::organ})for(double rate:{96000.,384000.}){std::array<double,7> timing{};double sink=0;for(auto& elapsed:timing){cs80::body::InstrumentBody body;body.prepare(rate,48000);body.noteOn(model,60,.8f,.5f,1,42);const auto start=std::chrono::steady_clock::now();for(int i=0;i<1000000;++i)sink+=body.tick(261.625565f,.5f);elapsed=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/1000000.;}std::sort(timing.begin(),timing.end());std::printf("model=%d rate=%.0f median_ns_per_sample=%.6f sum=%.9g\n",model,rate,timing[3],sink);}}
