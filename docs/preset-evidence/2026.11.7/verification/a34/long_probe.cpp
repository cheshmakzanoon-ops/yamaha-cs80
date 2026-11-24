// A34 small supplemental render: complete recipes with their authored gain/effects.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace cs80;
struct Case { const char* name; bool chord; int hold, tail; };

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    constexpr int rate = 48000, block = 96;
    const std::array<int, 8> chord {36, 43, 48, 55, 60, 64, 67, 72};
    std::filesystem::create_directories(argv[1]);
    std::puts("index,name,scenario,rate,hold,tail,velocity,pressure,frames,active_voices_at_end,oversampling,latency_host_samples,file");
    for (const auto test : {Case{"Slow Brass 10",false,6,6}, Case{"Evolving Pad",false,6,8},
                           Case{"Golden FX 156",false,10,6}, Case{"Cathedral Pad",true,12,8},
                           Case{"Golden Pads 66",true,12,8}})
    {
        const auto& bank = programLibrary();
        auto p = std::find_if(bank.begin(), bank.end(), [&](const auto& x) { return x.name == test.name; });
        if (p == bank.end()) return 3;
        auto e = std::make_unique<Engine>();
        e->prepare(rate); e->setParams(p->values);
        const float velocity = test.chord ? 1.f : .8f, pressure = test.chord ? 1.f : 0.f;
        const auto notes = test.chord ? std::vector<int>(chord.begin(),chord.end()) : std::vector<int>{60};
        for (int note : notes) e->noteOn(note, velocity, 1);
        e->channelPressure(1, pressure);
        const int off = rate * test.hold, frames = rate * (test.hold + test.tail);
        std::vector<float> audio(2 * frames);
        for (int at = 0; at < frames; at += block)
        {
            if (at == off) for (int note : notes) e->noteOff(note, 1);
            e->render(audio.data() + 2 * at, std::min(block, frames - at));
        }
        std::string stem = test.name;
        for (char& c : stem) if (c == ' ') c = '_';
        const std::string file = stem + (test.chord ? "-stack.f32" : "-single.f32");
        const auto path = std::filesystem::path(argv[1]) / file;
        FILE* f = std::fopen(path.c_str(),"wb"); if (!f) return 4;
        const auto wrote = std::fwrite(audio.data(),sizeof(float),audio.size(),f);
        const int closed = std::fclose(f); if (wrote != audio.size() || closed) return 5;
        int active = 0; for (int voice=0;voice<8;++voice) active += e->getVoiceInfo(voice).active;
        std::printf("%zu,\"%s\",%s,%d,%d,%d,%.9g,%.9g,%d,%d,%d,%d,%s\n",size_t(p-bank.begin()),
            test.name,test.chord ? "eight-note" : "single-midi60",rate,test.hold,test.tail,velocity,pressure,frames,
            active,e->getOversamplingFactor(),e->getLatencySamples(),file.c_str());
        std::fflush(stdout);
    }
}
