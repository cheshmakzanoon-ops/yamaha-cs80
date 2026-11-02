// Revision-free fixture shared verbatim with the pristine pre-change source.
// It intentionally uses only the historical default ControlFrame API.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>

using namespace cs80;
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ofstream out(argv[1],std::ios::binary);if(!out)return 2;
    std::uint32_t cases=0;
    const auto word=[&](std::uint32_t value) {
        const std::array<char,4> bytes{char(value),char(value>>8),char(value>>16),char(value>>24)};
        out.write(bytes.data(),bytes.size());
    };
    const auto run=[&](double rate,ControlFrame frame,int policy,int rank,int selector,int frames) {
        Engine engine;engine.prepare(rate);if(!engine.setControlFrame(frame))return false;engine.reset();
        engine.noteOn(policy==0?48:60,policy==0?.7f:.83f,1);
        word(static_cast<std::uint32_t>(rate));word(policy);word(rank);word(selector);word(frames);
        std::array<float,128> block{};
        for(int offset=0;offset<frames;offset+=64) {
            const int count=std::min(64,frames-offset);
            if(!engine.setControlFrame(frame))return false;engine.render(block.data(),count);
            for(int i=0;i<count*2;++i) {
                if(!std::isfinite(block[i]))return false;
                word(std::bit_cast<std::uint32_t>(block[i]));
            }
        }
        ++cases;return static_cast<bool>(out);
    };
    for(double rate:{44100.,48000.,96000.}) {
        for(int policy:{1,2})for(int rank=0;rank<2;++rank)for(int selector=0;selector<11;++selector) {
            ControlFrame frame;auto& p=frame.effectiveValues;
            p[P_parityMode]=policy==2?1.f:0.f;p[P_sourcePerformance]=policy==1?1.f:0.f;
            p[P_oversampling]=0;p[P_drift]=p[P_voiceDispersion]=p[P_analogNoise]=0;
            p[P_detune]=p[P_chorusOn]=p[P_ringAmount]=p[P_subVco]=p[P_subVcf]=p[P_subVca]=0;
            frame.hardwareSources[rank]=selector;
            if(!run(rate,frame,policy,rank,selector,256))return 3;
        }
        int index=0;
        for(const auto& preset:factoryPresets()) {
            auto p=preset.values;p[P_parityMode]=p[P_sourcePerformance]=0;
            if(!run(rate,ControlFrame{p,{0,1}},0,2,index++,64))return 3;
        }
    }
    out.flush();if(!out)return 2;out.close();if(!out)return 2;
    std::printf("Pristine default replay: %u cases; finite raw little-endian float bits\n",cases);
    return cases==798?0:4;
}
