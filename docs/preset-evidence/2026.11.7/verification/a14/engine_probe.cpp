// A14: full Engine, untouched shipped recipes; raw float PCM is temporary.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
using namespace cs80;
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::filesystem::create_directories(argv[1]);
    constexpr int rate=48000, frames=rate*3, block=96, releaseAt=110400;
    for (const char* name : {"Analog Bass", "Pluck Bass", "Fretless Glide", "Upright Pluck"}) {
        const auto& bank=programLibrary();
        auto it=std::find_if(bank.begin(),bank.end(),[&](const auto& p){return p.name==name;});
        if (it==bank.end()) return 3;
        for (const char* scenario : {"steady", "up_return", "down_return", "pressure"}) {
            auto e=std::make_unique<Engine>(); e->prepare(rate); e->setParams(it->values);
            e->noteOn(36,.85f,1);
            std::vector<float> pcm(2*frames);
            for(int at=0;at<frames;at+=block) {
                const double t=double(at)/rate;
                const double ramp=std::clamp((t-.35)/.15,0.,1.)*(1-std::clamp((t-1.05)/.15,0.,1.));
                const std::string s=scenario;
                e->setRibbon(float(ramp*(s=="up_return"?1.:s=="down_return"?-.5:0.)));
                if(s=="pressure") e->polyPressure(36,t>=.35?.75f:0.f,1);
                if(at==releaseAt) e->noteOff(36,1);
                e->render(pcm.data()+2*at,block);
            }
            std::string stem=name; std::replace(stem.begin(),stem.end(),' ','_');
            const auto path=std::filesystem::path(argv[1])/(stem+"_"+scenario+".f32");
            FILE* f=std::fopen(path.c_str(),"wb"); if(!f)return 4;
            std::fwrite(pcm.data(),sizeof(float),pcm.size(),f);std::fclose(f);
            double peak=0;size_t bad=0;for(float x:pcm){peak=std::max(peak,std::abs(double(x)));bad+=!std::isfinite(x);}
            std::printf("%s,%s,peak=%.9g,nonfinite=%zu\n",name,scenario,peak,bad);
        }
    }
}
