// A20 evidence only. Actual shipped presets, 48 kHz Standard, float PCM.
// No normalization; rank balance, effects, and all authored values retained.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace cs80;
constexpr int rate = 48000;

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const bool current = std::string(argv[2]) == "current";
    std::filesystem::create_directories(argv[1]);
    std::printf("name,scenario,frames,note_off_frame,velocity,pressure\n");
    for (const char* name : {"Flute Organ 8+4", "Percussive Organ", "Pipe Organ",
                            "Combo Organ", "Tremolo LFO Organ"})
    {
        const auto& bank = programLibrary();
        auto p = std::find_if(bank.begin(), bank.end(), [name](const auto& x){return x.name == name;});
        if (p == bank.end() || p->values[P_oversampling] != 1) return 3;
        std::vector<std::string> scenarios {"single", "chord8"};
        if (current)
        {
            scenarios.push_back("soft");
            if (std::string(name) == "Tremolo LFO Organ" || std::string(name) == "Flute Organ 8+4")
                scenarios.push_back("pressure");
            if (std::string(name) == "Percussive Organ") scenarios.push_back("restrike");
        }
        for (const auto& scenario : scenarios)
        {
            const bool repeat = scenario == "restrike";
            const bool chord = scenario == "chord8";
            const float velocity = scenario == "soft" ? 0.25f : 0.85f;
            const float pressure = chord || scenario == "pressure" ? 0.8f : 0.0f;
            const int off = repeat ? 72000 : 96000;
            const int frames = repeat ? 177600 : 115200;
            std::vector<int> notes = chord ? std::vector<int>{48,55,60,64,67,72,76,79}
                                          : std::vector<int>{60};
            auto e = std::make_unique<Engine>();
            e->prepare(rate);
            e->setParams(p->values);
            for (int note : notes) e->noteOn(note, velocity, 1);
            e->channelPressure(1, pressure);
            std::vector<float> audio(2 * frames);
            // Every scheduled event is divisible by 96; no event quantization.
            for (int at = 0; at < frames; at += 96)
            {
                if (at == off || (repeat && at == 158400))
                    for (int note : notes) e->noteOff(note, 1);
                if (repeat && at == 86400) e->noteOn(60, velocity, 1);
                e->render(audio.data() + 2 * at, 96);
            }
            std::string stem = name;
            for (char& c : stem) if (c == ' ' || c == '+') c = '_';
            const auto file = std::filesystem::path(argv[1]) / (stem + "-" + scenario + ".f32");
            FILE* f = std::fopen(file.c_str(), "wb");
            if (!f) return 4;
            const auto wrote = std::fwrite(audio.data(), sizeof(float), audio.size(), f);
            const int closed = std::fclose(f);
            if (wrote != audio.size() || closed) return 5;
            std::printf("%s,%s,%d,%d,%.9g,%.9g\n", name, scenario.c_str(), frames, off, velocity, pressure);
            std::fflush(stdout);
        }
    }
}
