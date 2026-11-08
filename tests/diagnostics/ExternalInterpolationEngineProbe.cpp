// Offline public-Engine witness. Compile against a baseline and this revision,
// then compare the binary outputs; see docs/EXTERNAL_INTERPOLATOR_SPARSE.md.
#include "core/Engine.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::FILE* file = std::fopen(argv[1], "wb");
    if (file == nullptr) return 2;
    std::size_t frames = 0;
    bool finite = true, written = true;
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int policy : { 0, 1, 2 })
            for (float quality : { 0.0f, 1.0f, 2.0f })
            {
                cs80::Engine engine;
                engine.prepare(rate);
                cs80::ParamValues params;
                params[cs80::P_sourcePerformance] = policy == 0 ? 0.0f : 1.0f;
                params[cs80::P_parityMode] = policy == 2 ? 1.0f : 0.0f;
                params[cs80::P_oversampling] = quality;
                params[cs80::P_subOriginalMode] = 1.0f;
                params[cs80::P_subOriginalFunction] = 5.0f; // EXT
                params[cs80::P_subExtLevel] = 1.0f;
                params[cs80::P_subVco] = params[cs80::P_subVcf] = params[cs80::P_subVca] = 0.25f;
                params[cs80::P_analogNoise] = 0.0f;
                engine.setParams(params);
                engine.noteOn(60, 0.8f);
                engine.noteOn(67, 0.6f);
                std::array<float, 127> input {};
                std::array<float, 254> output {};
                for (int block = 0; block < 64; ++block)
                {
                    const int count = 1 + ((block * 37) % 127);
                    for (int n = 0; n < count; ++n)
                    {
                        input[(size_t) n] = (float) std::sin(0.017 * (block * 127 + n));
                        if (block == 8) input[(size_t) n] *= 5.0f;
                        if (block == 9 && n == 0) input[0] = std::numeric_limits<float>::quiet_NaN();
                    }
                    if (block == 20) engine.noteOff(60);
                    if (block == 30) { params[cs80::P_oversampling] = 2.0f; engine.setParams(params); }
                    if (block == 40) { params[cs80::P_oversampling] = 0.0f; engine.setParams(params); }
                    if (block == 50) engine.noteOff(67);
                    engine.render(output.data(), count, block == 12 || block == 13 ? nullptr : input.data());
                    for (int n = 0; n < count * 2; ++n) finite = std::isfinite(output[(size_t) n]) && finite;
                    written = std::fwrite(output.data(), sizeof(float), (size_t) count * 2, file)
                        == (size_t) count * 2 && written;
                    frames += (size_t) count;
                }
            }
    const bool closed = std::fclose(file) == 0;
    std::printf("%zu stereo frames, finite=%d, written=%d\n", frames, (int) finite, (int) (written && closed));
    return finite && written && closed ? 0 : 1;
}
