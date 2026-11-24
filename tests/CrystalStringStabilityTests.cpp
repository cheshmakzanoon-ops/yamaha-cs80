// A coherent bowed foundation must not lose most of a repeated note's level
// against its free-running circuit companion. Keep all factory modulation on.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cs80;
namespace {
const FactoryPreset& preset(const char* name) {
    for (const auto& p : programLibrary()) if (p.name == name) return p;
    throw std::runtime_error(std::string("missing program: ") + name);
}
void render(Engine& engine, std::vector<float>& audio, int frames) {
    audio.resize(static_cast<size_t>(2 * frames));
    for (int at = 0; at < frames;) {
        const int n = std::min(128, frames - at);
        engine.render(audio.data() + 2 * at, n); at += n;
    }
}
}
int main() {
    int failures = 0;
    try {
        for (const char* name : {"Crystal Strings", "Glass Strings", "Frozen Strings 29"})
            for (int note : {48, 60, 72}) {
                auto engine = std::make_unique<Engine>();
                engine->prepare(48000); engine->setParams(preset(name).values);
                std::vector<float> audio;
                double minimum = 1.0, maximum = 0.0, total = 0.0, peak = 0.0;
                bool finite = true;
                for (int repeat = 0; repeat < 5; ++repeat) {
                    engine->noteOn(note, .8f, 1);
                    // Ordinary channel pressure targets active voices.
                    engine->channelPressure(1, .3f);
                    render(*engine, audio, 57600);
                    double energy = 0.0;
                    for (size_t i = 0; i < audio.size(); ++i) {
                        finite = finite && std::isfinite(audio[i]);
                        peak = std::max(peak, std::abs(static_cast<double>(audio[i])));
                        if (i >= 57600) energy += double(audio[i]) * audio[i];
                    }
                    const double power = energy / 57600.0;
                    minimum = std::min(minimum, power); maximum = std::max(maximum, power); total += power;
                    engine->noteOff(note, 1);
                    render(*engine, audio, 96000 + static_cast<int>(repeat * .071 * 48000));
                }
                const double span = 10 * std::log10(maximum / std::max(minimum, 1.e-24));
                // Ordinary ensemble motion remains; old body/circuit overlap
                // produced approximately 4.6--7 dB at this same gesture.
                const bool pass = finite && peak < 1 && total / 5 > 1.e-5 && span < 4.1;
                failures += !pass;
                std::printf("%s,%s,%d,span=%.6f,mean=%.6f,peak=%.6f\n", pass ? "PASS" : "FAIL", name, note,
                            span, 10 * std::log10(total / 5), 20 * std::log10(std::max(peak, 1.e-12)));
            }
    } catch (const std::exception& e) { std::printf("FAIL %s\n", e.what()); ++failures; }
    return failures ? 1 : 0;
}
