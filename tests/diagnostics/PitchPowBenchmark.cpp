// Arguments: policy (0 Legacy, 1 Circuit, 2 PURE), voices, SUB-VCA depth, pitch LFO depth (20 Hz).
// Nine render-only timings and output witnesses at 48kHz/256 frames.
#include "core/Engine.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    const int policy = argc > 1 ? std::atoi(argv[1]) : 1;
    const int notes = argc > 2 ? std::atoi(argv[2]) : 8;
    cs80::Engine engine;
    engine.prepare(48000.0);
    cs80::ParamValues params;
    params[cs80::P_sourcePerformance] = policy ? 1.0f : 0.0f;
    params[cs80::P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    params[cs80::P_oversampling] = 1.0f;
    params[cs80::P_subVca] = argc > 3 ? std::atof(argv[3]) : 0.0f;
    params[cs80::P_lfo1Depth] = argc > 4 ? std::atof(argv[4]) : 0.0f;
    params[cs80::P_lfo1Rate] = 20.0f;
    params[cs80::P_lfo1Dest] = 1.0f; // pitch, not destination 0 (off)
    engine.setParams(params);
    for (int n = 0; n < notes; ++n) engine.noteOn(48 + n * 3, .8f);
    std::array<float, 512> output {};
    for (int n = 0; n < 32; ++n) engine.render(output.data(), 256);
    for (int round = 0; round < 9; ++round)
    {
        const auto start = std::chrono::steady_clock::now();
        double sum = 0.0;
        for (int block = 0; block < 256; ++block)
        {
            engine.render(output.data(), 256);
            sum += output[0];
        }
        const auto stop = std::chrono::steady_clock::now();
        std::printf("%.9f %.17g\n", std::chrono::duration<double>(stop - start).count(), sum);
    }
}
