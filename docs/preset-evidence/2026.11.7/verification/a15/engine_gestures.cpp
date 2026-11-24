// Actual, unchanged curated programs through Engine; no isolated-body oracle.
// c++ -O2 -std=c++20 -I<MATCHING_SOURCE>/src engine_gestures.cpp <MATCHING_BUILD>/libcs80_core.a -o probe
// probe /tmp/output-prefix ; stereo float32 PCM at 48 kHz remains temporary.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
int main(int argc,char**argv) {
    if(argc!=2)return 2;
    const std::array<const char*,5> names{"Guitar II","Pressure Pluck","Harp","Koto","Banjo"};
    for(int p=0;p<5;++p) {
        const auto& bank=cs80::curatedLibrary();
        const auto it=std::find_if(bank.begin(),bank.end(),[&](const auto& x){return x.name==names[p];});
        if(it==bank.end())throw std::runtime_error(names[p]);
        for(int note:{48,60,72})for(int mode=0;mode<4;++mode) {
            auto e=std::make_unique<cs80::Engine>();e->prepare(48000);e->setParams(it->values);
            e->noteOn(note,.80f,1);
            if(mode==3)e->controlChange(64,1,1);
            std::vector<float> audio(105600*2);
            // 48-frame blocks place every event exactly on its stated time.
            for(int at=0;at<105600;at+=48) {
                if(mode==1) {
                    if(at==4800)e->pitchBend(1,1);
                    if(at==16800)e->pitchBend(1,-1);
                    if(at==31200)e->pitchBend(1,0);
                }
                if(mode==2) {
                    if(at==16800)e->channelPressure(1,.8f);
                    if(at==33600)e->channelPressure(1,0);
                }
                if(at==48000)e->noteOff(note,1);
                if(mode==3&&at==81600)e->controlChange(64,0,1);
                e->render(audio.data()+2*at,48);
            }
            const std::string path=std::string(argv[1])+"/p"+std::to_string(p)+"_n"+std::to_string(note)+"_m"+std::to_string(mode)+".f32";
            FILE*f=std::fopen(path.c_str(),"wb");if(!f)return 3;
            std::fwrite(audio.data(),sizeof(float),audio.size(),f);std::fclose(f);
            std::printf("%s,%d,%d,%s\n",names[p],note,mode,path.c_str());std::fflush(stdout);
        }
    }
}
