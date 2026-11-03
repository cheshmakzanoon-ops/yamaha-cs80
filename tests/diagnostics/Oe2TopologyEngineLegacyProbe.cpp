#include "core/Engine.h"
#include <array>
#include <cmath>
#include <cstdio>
int main(int argc,char** argv){
    if(argc!=2)return 2;FILE* f=std::fopen(argv[1],"wb");if(!f)return 2;
    int nonfinite=0;unsigned long long frames=0;
    for(float rate:{44100.f,48000.f,96000.f}) for(int policy:{0,1,2}) for(int mode=0;mode<4;++mode){
        cs80::Engine e;e.prepare(rate);cs80::ParamValues p;
        p[cs80::P_sourcePerformance]=policy==1;p[cs80::P_parityMode]=policy==2;
        p[cs80::P_chorusOn]=(mode&1)!=0;p[cs80::P_chorusMode]=(mode&2)!=0;
        p[cs80::P_chorusSpeed]=3.7f;p[cs80::P_chorusDepth]=.83f;e.setParams(p);
        std::array<float,256> audio{};
        for(int b=0;b<48;++b){
            if(b==2)e.noteOn(60,.7f);if(b==12)e.noteOn(67,.5f);if(b==24)e.noteOff(60);if(b==32)e.noteOff(67);
            if(b==16){p[cs80::P_chorusSpeed]=8.f;p[cs80::P_chorusDepth]=1;e.setParams(p);}
            if(b==28){p[cs80::P_chorusOn]=0;e.setParams(p);}if(b==36){p[cs80::P_chorusOn]=1;e.setParams(p);}
            e.render(audio.data(),128);for(float x:audio)if(!std::isfinite(x))++nonfinite;
            if(std::fwrite(audio.data(),sizeof(float),audio.size(),f)!=audio.size())return 2;frames+=128;
        }
    }
    std::fclose(f);std::printf("Engine frames=%llu, nonfinite=%d\n",frames,nonfinite);return nonfinite?1:0;
}
