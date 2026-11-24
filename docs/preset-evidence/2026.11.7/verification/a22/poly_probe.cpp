// A22 evidence probe: untouched factory programs; no gain normalization.
// This deliberately extends the library audit's one-second stress chord.
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
constexpr int rate = 48000, block = 96, off = 6 * rate, frames = 10 * rate;
constexpr std::array<int, 8> notes {48, 55, 60, 64, 67, 72, 76, 79};

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const bool current = std::string(argv[2]) == "current";
    std::filesystem::create_directories(argv[1]);
    std::printf("name,scenario,frames,note_off_frame,velocity,pressure\n");
    for (const char* name : {"Poly Brass Stab", "PWM Poly", "Poly Pluck", "Ring Poly",
                            "Poly Swell", "Synthwave Chords", "Phased Polysynth"})
    {
        const auto& bank = programLibrary();
        const auto p = std::find_if(bank.begin(), bank.end(), [name](const auto& x) { return x.name == name; });
        if (p == bank.end() || p->category != "Polysynth" || p->values[P_oversampling] != 1) return 3;
        std::vector<float> pressures {1.0f};
        if (current && (std::string(name) == "Poly Brass Stab" || std::string(name) == "PWM Poly"
                        || std::string(name) == "Poly Swell" || std::string(name) == "Phased Polysynth"))
            pressures.push_back(0.0f);
        for (const float pressure : pressures)
        {
            auto engine = std::make_unique<Engine>();
            engine->prepare(rate);
            engine->setParams(p->values);
            for (int note : notes) engine->noteOn(note, 1.0f, 1);
            engine->channelPressure(1, pressure);
            std::vector<float> audio(2 * frames);
            for (int at = 0; at < frames; at += block)
            {
                if (at == off) for (int note : notes) engine->noteOff(note, 1);
                engine->render(audio.data() + 2 * at, block);
            }
            std::string stem = name;
            for (char& c : stem) if (c == ' ') c = '_';
            const char* scenario = pressure > 0 ? "pressed8" : "unpressed8";
            const auto path = std::filesystem::path(argv[1]) / (stem + "-" + scenario + ".f32");
            FILE* f = std::fopen(path.c_str(), "wb");
            if (!f) return 4;
            const auto wrote = std::fwrite(audio.data(), sizeof(float), audio.size(), f);
            const int closed = std::fclose(f);
            if (wrote != audio.size() || closed) return 5;
            std::printf("%s,%s,%d,%d,1,%.0f\n", name, scenario, frames, off, pressure);
            std::fflush(stdout);
        }
    }
}
