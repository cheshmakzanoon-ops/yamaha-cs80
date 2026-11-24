// A59: isolate old/new VCA baseline controls on the SAME current Engine.
// Input contains all current controls, with only volume and VCA baselines
// differing from the published .4 inventory. Common master removes trim.
#include "core/Engine.h"
#include "core/Params.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::ifstream input(argv[1]);
    if (!input) return 2;
    std::cout << "name,note,version,velocity,pressure,held_rms_dbfs,peak,early_rms_dbfs,late_rms_dbfs,source1,source2\n";
    std::cout << std::setprecision(12);
    std::string name;
    int note = 0;
    while (input >> std::quoted(name) >> note)
    {
        std::array<cs80::ParamValues, 2> controls;
        for (auto& p : controls)
            for (int i = 0; i < cs80::P_numParams; ++i)
                if (!(input >> p[i])) return 3;
        for (int version = 0; version < 2; ++version)
        {
            const auto original = controls[version];
            controls[version][cs80::P_volume] = .5f;
            for (int gesture = 0; gesture < 4; ++gesture)
            {
                const float velocity = gesture == 0 ? .25f : gesture == 1 ? .85f : .5f;
                const float pressure = gesture == 3 ? .8f : 0.f;
                auto engine = std::make_unique<cs80::Engine>();
                engine->prepare(48000);
                engine->setParams(controls[version]);
                if (engine->getSourceToneInfo(0).selector != 13
                    || engine->getSourceToneInfo(1).selector != 13) return 4;
                engine->noteOn(note, velocity, 1);
                engine->channelPressure(1, pressure);
                constexpr int framesTotal = 96000;
                std::array<float, 256> block {};
                double sum = 0, early = 0, late = 0, peak = 0;
                int count = 0, earlyCount = 0, lateCount = 0;
                for (int at = 0; at < framesTotal; at += 128)
                {
                    const int frames = std::min(128, framesTotal - at);
                    engine->render(block.data(), frames);
                    for (int i = 0; i < 2 * frames; ++i)
                    {
                        const double sample = block[static_cast<size_t>(i)];
                        if (!std::isfinite(sample) || std::abs(sample) > 1) return 5;
                        peak = std::max(peak, std::abs(sample));
                        sum += sample * sample;
                        ++count;
                        if (at + i/2 < 9600) { early += sample * sample; ++earlyCount; }
                        if (at + i/2 >= 72000) { late += sample * sample; ++lateCount; }
                    }
                }
                const auto db = [](double energy, int n) {
                    return 20 * std::log10(std::max(1e-30, std::sqrt(energy / n)));
                };
                std::cout << std::quoted(name) << ',' << note << ',' << (version ? "current" : "published_controls")
                    << ',' << velocity << ',' << pressure << ',' << db(sum, count) << ',' << peak
                    << ',' << db(early, earlyCount) << ',' << db(late, lateCount)
                    << ',' << engine->getSourceToneInfo(0).selector << ',' << engine->getSourceToneInfo(1).selector << '\n';
            }
        }
    }
    return input.eof() ? 0 : 6;
}
