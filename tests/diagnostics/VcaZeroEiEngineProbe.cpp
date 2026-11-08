// Compile against baseline and candidate; compare binary output files.
// No plugin, GUI or host runtime required.
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    auto* file = std::fopen(argv[1], "wb");
    if (file == nullptr) return 2;
    cs80::Engine engine;
    if (engine.getInternalSampleRate() != 96000.) { std::fclose(file); return 3; }
    // Exercise pre-prepare setters without rendering unallocated buffers.
    engine.setNonRealtime(true); engine.setNonRealtime(false);
    std::size_t frames = 0;
    bool finite = true, written = true;
    for (double rate : { 8000., 44100.125, 48000., 96000.25, 192000., 0., -1.,
                         std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity() })
    {
        engine.prepare(rate);
        for (int policy : { 0, 1, 2 }) for (int quality : { 0, 1, 2 })
        {
            cs80::ParamValues params;
            params[cs80::P_sourcePerformance] = policy ? 1.f : 0.f;
            params[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
            params[cs80::P_oversampling] = static_cast<float>(quality);
            engine.setParams(params);
            for (int stage = 0; stage < 6; ++stage)
            {
                if (stage == 1) engine.setNonRealtime(true);
                if (stage == 2) engine.setNonRealtime(false);
                if (stage == 3) engine.reset();
                if (stage == 4) { params[cs80::P_oversampling] = 2.f; engine.setParams(params); }
                if (stage == 5) { params[cs80::P_oversampling] = 0.f; engine.setParams(params); }
                engine.noteOn(55, .8f); engine.noteOn(67, .4f);
                std::array<float, 254> output {};
                for (int block = 0; block < 4; ++block)
                {
                    if (block == 2) engine.noteOff(55);
                    engine.render(output.data(), 127);
                    for (float value : output) finite = finite && std::isfinite(value);
                    written = std::fwrite(output.data(), sizeof(float), output.size(), file) == output.size() && written;
                    frames += 127;
                }
                engine.noteOff(67);
            }
        }
    }
    written = std::fclose(file) == 0 && written;
    std::printf("frames=%zu finite=%d written=%d\n", frames, finite, written);
    return finite && written ? 0 : 1;
}
