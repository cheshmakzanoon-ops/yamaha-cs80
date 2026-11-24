// A21: native complete Pads recipes. Only MIDI events vary; no patch overrides.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
using namespace cs80;

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::filesystem::create_directories(argv[1]);
    struct Scene { const char* name; int note; float velocity, pressure; int hold, tail; bool chord; };
    const std::vector<Scene> choirScenes {
        {"low", 36, .85f, 0.f, 3, 3, false},
        {"middle", 48, .85f, 0.f, 3, 3, false},
        {"high", 72, .85f, 0.f, 3, 3, false},
        {"soft", 36, .25f, 0.f, 3, 3, false},
        {"pressure", 36, .85f, .8f, 3, 3, false},
        {"stack", 36, 1.f, 1.f, 4, 8, true}
    };
    const std::vector<Scene> syntheticScenes {
        {"slow", 60, .85f, 0.f, 6, 8, false},
        {"stack", 36, 1.f, 1.f, 6, 8, true}
    };
    constexpr int rate = 48000;
    const auto& bank = programLibrary();
    for (const char* name : {"Choir Pad", "Dark Choir", "Sweep Pad"})
    {
        const auto p = std::find_if(bank.begin(), bank.end(), [&](const auto& v) { return v.name == name; });
        if (p == bank.end()) return 3;
        const auto& scenes = std::string(name) == "Sweep Pad" ? syntheticScenes : choirScenes;
        for (const auto& scene : scenes)
        {
            auto engine = std::make_unique<Engine>();
            engine->prepare(rate);
            engine->setParams(p->values);
            std::vector<int> notes = scene.chord ? std::vector<int>{36, 43, 48, 52, 55, 60, 64, 67}
                                               : std::vector<int>{scene.note};
            for (int note : notes) engine->noteOn(note, scene.velocity, 1);
            const int held = scene.hold * rate, frames = (scene.hold + scene.tail) * rate;
            const int pressureAt = rate * 3 / 2;
            std::vector<float> audio(static_cast<size_t>(frames) * 2);
            for (int at = 0; at < frames;)
            {
                if (at == pressureAt && scene.pressure > 0.f) engine->channelPressure(1, scene.pressure);
                if (at == held) for (int note : notes) engine->noteOff(note, 1);
                int boundary = frames;
                for (int event : {pressureAt, held, frames}) if (event > at) { boundary = event; break; }
                const int count = std::min(113, boundary - at);
                engine->render(audio.data() + at * 2, count);
                at += count;
            }
            std::string slug(name); std::replace(slug.begin(), slug.end(), ' ', '_');
            const auto path = std::filesystem::path(argv[1]) / (slug + "_" + scene.name + ".f32");
            auto* file = std::fopen(path.c_str(), "wb");
            if (!file) return 4;
            const auto written = std::fwrite(audio.data(), sizeof(float), audio.size(), file);
            const int closed = std::fclose(file);
            if (written != audio.size() || closed != 0) return 5;
            std::printf("%s,%s,%d,%.9g,%.9g,%d,%d,%d,%d\n", name, scene.name, scene.note,
                        scene.velocity, scene.pressure, scene.hold, scene.tail,
                        engine->getOversamplingFactor(), engine->activeVoices());
            std::fflush(stdout);
        }
    }
}
