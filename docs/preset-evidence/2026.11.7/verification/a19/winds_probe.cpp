// A19: authored Winds vectors; quality is the only parameter override.
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
struct Patch { const char* name; std::array<int, 3> notes; };
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const bool baseline = std::string(argv[2]) == "baseline";
    std::filesystem::create_directories(argv[1]);
    const auto& bank = programLibrary();
    const std::array<Patch, 5> patches {{
        {"Flute", {60, 72, 84}}, {"Clarinet", {48, 60, 72}},
        {"Oboe", {60, 72, 84}}, {"Bassoon", {48, 60, 72}},
        {"Shakuhachi", {48, 60, 72}}
    }};
    std::puts("name,note,mode,gesture,factor,velocity,pressure_at_1p2s,frames");
    for (const auto& patch : patches) {
        const auto it = std::find_if(bank.begin(), bank.end(), [&](const auto& p) { return p.name == patch.name; });
        if (it == bank.end()) return 3;
        for (int note : patch.notes) for (int quality = 0; quality < 3; ++quality)
        for (int gesture = 0; gesture < 3; ++gesture) {
            const bool middle = note == patch.notes[1];
            if (!middle && (gesture != 0 || quality == 2)) continue;
            if (quality == 2 && gesture != 0) continue;
            if (baseline && (!middle || quality == 2 || gesture != 0)) continue;
            auto p = it->values;
            p[P_oversampling] = quality == 1 ? 2.f : 1.f;
            auto e = std::make_unique<Engine>();
            e->setParams(p); e->setNonRealtime(quality == 2); e->prepare(48000);
            const float velocity = gesture == 1 ? .30f : .85f;
            e->noteOn(note, velocity, 1);
            constexpr int rate = 48000, pressureAt = 6 * rate / 5, held = 3 * rate, total = 4 * rate;
            std::vector<float> audio(total * 2);
            for (int at = 0; at < total;) {
                if (at == pressureAt && gesture == 2) e->channelPressure(1, .8f);
                if (at == held) e->noteOff(note, 1);
                const int boundary = at < pressureAt ? pressureAt : at < held ? held : total;
                const int count = std::min(113, boundary - at);
                e->render(audio.data() + 2 * at, count); at += count;
            }
            std::string slug = patch.name; std::replace(slug.begin(), slug.end(), ' ', '_');
            const char* mode = quality == 0 ? "rt2x" : quality == 1 ? "hq8x" : "offline8x";
            const char* action = gesture == 0 ? "held" : gesture == 1 ? "soft" : "pressure";
            const auto path = std::filesystem::path(argv[1]) / (slug + "_" + std::to_string(note) + "_" + mode + "_" + action + ".f32");
            auto* file = std::fopen(path.c_str(), "wb"); if (!file) return 4;
            const auto written = std::fwrite(audio.data(), sizeof(float), audio.size(), file); std::fclose(file);
            if (written != audio.size()) return 5;
            std::printf("%s,%d,%s,%s,%d,%.2f,%.2f,%d\n", patch.name, note, mode, action,
                        e->getOversamplingFactor(), velocity, gesture == 2 ? .8 : 0., total);
            std::fflush(stdout);
        }
    }
}
