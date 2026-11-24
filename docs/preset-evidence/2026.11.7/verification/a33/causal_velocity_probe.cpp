// A33 evidence only. Current .7 Engine versus the same Engine and patch with
// only its two published .4 VCA baseline values restored. This is a diagnostic
// counterfactual, not a reconstruction of the full published .4 instrument.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cs80;

double rms(const std::vector<float>& audio, int first, int last)
{
    double sum = 0;
    for (int i = first * 2; i < last * 2; ++i)
        sum += double(audio[size_t(i)]) * audio[size_t(i)];
    return std::sqrt(sum / (2.0 * (last - first)));
}

double db(double value) { return 20 * std::log10(std::max(value, 1e-12)); }

int main(int argc, char** argv)
{
    if (argc != 6) return 2;
    const std::filesystem::path out(argv[1]);
    const std::string name(argv[2]);
    const int note = std::stoi(argv[3]);
    const float publishedBaselines[2] = {std::stof(argv[4]), std::stof(argv[5])};
    const FactoryPreset* found = nullptr;
    for (const auto& preset : programLibrary())
        if (preset.name == name) found = &preset;
    if (!found) throw std::runtime_error("Preset not found: " + name);
    std::filesystem::create_directories(out);
    std::cout << "name,mode,note,velocity,vca1,vca2,peak_dbfs,early_rms_dbfs,held_rms_dbfs,late_held_rms_dbfs,tail_rms_dbfs,nonfinite,samples_over_full_scale,pcm_file\n"
              << std::setprecision(12);
    for (int restore = 0; restore <= 1; ++restore)
        for (float velocity : {.25f, .85f})
        {
            auto values = found->values;
            if (restore)
                for (int rank = 0; rank < 2; ++rank)
                    values[chParam(rank, CP_vLevel)] = publishedBaselines[rank];
            auto engine = std::make_unique<Engine>();
            constexpr int rate = 48000, held = rate, frames = rate * 3 / 2;
            engine->prepare(rate);
            engine->setParams(values);
            engine->noteOn(note, velocity, 1);
            engine->channelPressure(1, 0);
            std::vector<float> audio(size_t(frames) * 2);
            for (int at = 0; at < frames;)
            {
                if (at == held) engine->noteOff(note, 1);
                const int count = std::min({128, frames - at, at < held ? held - at : frames - at});
                if (count <= 0) throw std::runtime_error("Nonadvancing capture");
                engine->render(audio.data() + 2 * at, count);
                at += count;
            }
            double peak = 0;
            int nonfinite = 0, over = 0;
            for (const float x : audio)
            {
                if (!std::isfinite(x)) ++nonfinite;
                else peak = std::max(peak, std::abs(double(x)));
                if (std::abs(x) > 1) ++over;
            }
            const std::string mode = restore ? "restored_published_vca_baselines" : "current_7";
            const std::string filename = mode + (velocity < .5f ? "_v025.f32" : "_v085.f32");
            std::ofstream pcm(out / filename, std::ios::binary);
            pcm.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
            if (!pcm) throw std::runtime_error("Could not write diagnostic PCM");
            std::cout << name << ',' << mode << ',' << note << ',' << velocity << ','
                      << values[chParam(0, CP_vLevel)] << ',' << values[chParam(1, CP_vLevel)] << ','
                      << db(peak) << ',' << db(rms(audio, 0, rate / 5)) << ','
                      << db(rms(audio, 0, held)) << ',' << db(rms(audio, held / 2, held)) << ','
                      << db(rms(audio, held, frames)) << ',' << nonfinite << ',' << over << ',' << filename << '\n';
        }
}
