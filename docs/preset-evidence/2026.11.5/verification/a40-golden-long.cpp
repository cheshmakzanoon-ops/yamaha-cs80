// A40 inherited original host109 Golden Pads 66 exact 32-second performance.
// No normalization, parameter overrides, all-notes-off reset or mute is applied.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr int rate = 48000;
constexpr int duration = 32;
constexpr int hold = 20;
constexpr std::array<int, 8> notes {36, 43, 48, 55, 60, 64, 67, 72};
constexpr std::array<const char*, 1> names { "Golden Pads 66" };
struct Window {
    double sum = 0.0, peak = 0.0;
    long long samples = 0, nonfinite = 0, clipped = 0;
    int active = 0;
};
double db(double x) { return 20.0 * std::log10(std::max(x, 1.e-15)); }
const cs80::FactoryPreset& preset(const char* name) {
    for (const auto& p : cs80::programLibrary()) if (p.name == name) return p;
    throw std::runtime_error(std::string("missing preset: ") + name);
}
float pressureAt(int second) {
    return second < 4 || (second >= 8 && second < 12) ? 0.0f : 1.0f;
}
}

int main() {
    int checks = 0, failures = 0;
    std::puts("program,second,pressure,rms_dbfs,peak_dbfs,active_at_window_end,samples,nonfinite,clipped");
    try {
        for (const auto* name : names) {
            const auto& p = preset(name);
            auto engine = std::make_unique<cs80::Engine>();
            engine->prepare(rate);
            engine->setParams(p.values);
            for (int n : notes) engine->noteOn(n, 1.0f, 1);
            std::array<float, 512> block {};
            std::array<Window, duration> windows {};
            for (int second = 0; second < duration; ++second) {
                engine->channelPressure(1, pressureAt(second));
                if (second == hold) for (int n : notes) engine->noteOff(n, 1);
                auto& w = windows[static_cast<std::size_t>(second)];
                for (int at = 0; at < rate;) {
                    const int count = std::min(256, rate - at);
                    engine->render(block.data(), count);
                    for (int i = 0; i < 2 * count; ++i) {
                        const double x = block[static_cast<std::size_t>(i)];
                        ++w.samples;
                        if (!std::isfinite(x)) { ++w.nonfinite; continue; }
                        if (std::abs(x) >= 1.0) ++w.clipped;
                        w.sum += x * x;
                        w.peak = std::max(w.peak, std::abs(x));
                    }
                    at += count;
                }
                w.active = engine->activeVoices();
                std::printf("%s,%d,%.1f,%.9f,%.9f,%d,%lld,%lld,%lld\n", name, second,
                    pressureAt(second), db(std::sqrt(w.sum / w.samples)), db(w.peak),
                    w.active, w.samples, w.nonfinite, w.clipped);
                std::fflush(stdout);
            }
            for (const auto& w : windows) {
                ++checks; failures += w.samples != 2LL * rate;
                ++checks; failures += w.nonfinite != 0;
                ++checks; failures += w.clipped != 0;
            }
            ++checks; failures += windows.back().active != 0;
            // A held pad must remain audible before the ordinary note-off.
            ++checks; failures += db(std::sqrt(windows[19].sum / windows[19].samples)) < -60.0;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "EXCEPTION: %s\n", e.what());
        ++checks; ++failures;
    }
    std::fprintf(stderr, "A40 Golden Pads 66 long chord: %d checks, %d failures; 1 program x 32 seconds x 48000 stereo frames\n", checks, failures);
    return failures ? 1 : 0;
}
