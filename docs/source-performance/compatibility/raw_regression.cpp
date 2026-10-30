#include "core/Engine.h"
#include <cstdio>
#include <vector>
using namespace cs80;
int main(int argc,char**argv){
    if(argc!=2)return 2;FILE*f=std::fopen(argv[1],"wb");if(!f)return 2;
    for(double rate:{44100.,48000.,96000.})for(bool parity:{false,true})for(int selector=0;selector<14;++selector){
        ControlFrame frame;frame.hardwareSources={selector,(selector+5)%14};auto&p=frame.effectiveValues;
        p[P_parityMode]=parity?1.f:0.f;p[P_mpeOn]=1;p[P_subVco]=.23f;p[P_subVcf]=.27f;p[P_subVca]=.19f;
        p[P_ringAmount]=.35f;p[P_ringDepth]=.15f;p[P_macro1]=.71f;p[P_macro2]=.32f;p[P_macro3]=.65f;p[P_macro4]=.54f;
        p[P_lfo1Depth]=.32f;p[P_lfo1Dest]=2;p[P_fx1Type]=2;p[P_fx1Mix]=.4f;p[P_footMode]=1;
        Engine e;e.prepare(rate);if(!e.setControlFrame(frame))return 3;
        e.setExpression(.56f);e.noteOn(48,.73f,2);e.noteOn(67,.42f,3);e.noteOn(84,.91f,4);
        for(int stage=0;stage<7;++stage){
            if(stage==1){e.controlChange(1,.37f);e.polyPressure(48,.42f,2);e.pitchBend(3,.3f);e.controlChange(74,.25f,4);}
            if(stage==2){e.controlChange(64,1,2);e.noteOff(48,2);e.noteOff(67,3);e.setRibbon(-.12f);}
            if(stage==3){frame.hardwareSources[0]=(selector+1)%14;p[P_volume]=.32f;p[chParam(0,CP_fA)]=.23f;p[chParam(0,CP_vS)]=.4f;p[chParam(0,CP_hpfCutoff)]=1700;p[chParam(0,CP_lpfCutoff)]=6400;if(!e.setControlFrame(frame))return 3;}
            if(stage==4){e.controlChange(64,0,2);e.allNotesOff(true);}
            if(stage==5){p[P_parityMode]=parity?0.f:1.f;if(!e.setControlFrame(frame))return 3;e.noteOn(63,.82f,2);}
            if(stage==6){frame.hardwareSources={11,12};if(!e.setControlFrame(frame))return 3;e.noteOff(63,2);}
            std::vector<float>out(static_cast<size_t>(257+stage*17)*2);e.render(out.data(),static_cast<int>(out.size()/2));
            if(std::fwrite(out.data(),sizeof(float),out.size(),f)!=out.size())return 4;
        }
    }
    return std::fclose(f)!=0;
}
