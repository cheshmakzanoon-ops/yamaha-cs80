#include "core/Engine.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
int main(int argc,char** argv) {
    if(argc!=3) return 2;
    cs80::ParamValues values;
    std::ifstream in(argv[1]); std::string id; float value=0;
    int loaded=0;
    while(in>>id>>value) for(int p=0;p<cs80::P_numParams;++p) {
        const std::string key=cs80::paramId(p);
        if(id==key) { values[p]=value; ++loaded; break; }
    }
    std::cout<<"loaded core parameters="<<loaded<<" of "<<cs80::P_numParams<<'\n';
    if(loaded!=cs80::P_numParams) return 3;
    std::vector<float> previous;
    for(int trial=0;trial<3;++trial) {
        auto p=values; if(trial==1)p[cs80::P_chorusOn]=0;
        cs80::Engine e; e.setNonRealtime(true); e.setParams(p); e.prepare(48000);
        constexpr int frames=96000+64;
        std::vector<float> wave(frames*2);
        for(int offset=0;offset<frames;offset+=256)
            e.render(wave.data()+2*offset,std::min(256,frames-offset));
        double peak=0,sum=0; for(float x:wave) {peak=std::max(peak,std::abs((double)x));sum+=(double)x*x;}
        std::cout<<"trial="<<trial<<" chorus="<<p[cs80::P_chorusOn]<<" latency="<<e.getLatencySamples()
                 <<" peak="<<peak<<" rms="<<std::sqrt(sum/wave.size())<<'\n';
        std::ofstream raw(std::string(argv[2])+"/core-no-midi-"+std::to_string(trial)+".f32",std::ios::binary);
        raw.write(reinterpret_cast<const char*>(wave.data()),wave.size()*sizeof(float));
        if(trial==0) {if(!(peak>1e-6))return 4; previous=wave;}
        if(trial==1&&peak>1e-6)return 5;
        if(trial==2&&wave!=previous)return 6;
    }
}
