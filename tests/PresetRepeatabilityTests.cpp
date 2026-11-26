// Identical acoustic-style keying must not turn arbitrary oscillator phase into level.
// The bound permits the named free-running tremulant and stereo modulation.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string& message) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
const FactoryPreset& preset(const char* name) {
    const auto& bank = curatedLibrary();
    auto at = std::find_if(bank.begin(), bank.end(), [name](const auto& p) { return p.name == name; });
    if (at == bank.end()) throw std::runtime_error(std::string("Missing preset: ") + name);
    return *at;
}
void repeatedGates(const FactoryPreset& p, int note) {
    auto e = std::make_unique<Engine>();
    e->prepare(48000.0);
    auto dryP = p.values; dryP[P_fxMasterBypass] = 1.0f; e->setParams(dryP);
    std::array<float, 256> block {};
    double minimum = 1.0, maximum = 0.0, peak = 0.0;
    bool finite = true;
    for (int gate = 0; gate < 5; ++gate) {
        e->noteOn(note, .8f, 1);
        double energy = 0.0;
        int samples = 0;
        for (int at = 0; at < 19200;) {
            const int count = std::min(128, 19200 - at);
            e->render(block.data(), count);
            for (int i = 0; i < count * 2; ++i) {
                const double x = block[(size_t) i];
                finite = finite && std::isfinite(x);
                peak = std::max(peak, std::abs(x));
                if (at >= 9600) { energy += x * x; ++samples; }
            }
            at += count;
        }
        const double rms = std::sqrt(energy / samples);
        minimum = std::min(minimum, rms);
        maximum = std::max(maximum, rms);
        e->noteOff(note, 1);
        for (int at = 0; at < 28800;) {
            const int count = std::min(128, 28800 - at);
            e->render(block.data(), count);
            at += count;
        }
    }
    const auto label = p.name + " note " + std::to_string(note);
    const double range = 20.0 * std::log10(std::max(maximum, 1.e-20) / std::max(minimum, 1.e-20));
    check(finite && peak < 1.0 && minimum > .0001, label + ": five audible finite gates retain headroom");
    check(range < 1.5, label + ": equal sustained notes retain level within intentional modulation");
    std::printf("PRESET REPEATS %-22s note %d range %.6f dB\n", p.name.c_str(), note, range);
}
}
int main() {
    try {
        for (const char* name : {"Flute Organ 8+4", "Flute Organ 16+8", "Reed Organ", "Chapel Organ",
              "Theatre Organ", "Tremolo Organ", "Mellow Organ", "Bright Organ", "Fifth Organ", "Organ Vibrato"})
            repeatedGates(preset(name), 60);
        for (const char* name : {"Reed Organ", "Tremolo Organ"})
            for (int note : {48, 72}) repeatedGates(preset(name), note);
        for (int note : {48, 60, 72}) repeatedGates(preset("Ocarina"), note);
    } catch (const std::exception& e) { check(false, e.what()); }
    std::printf("PresetRepeatabilityTests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
