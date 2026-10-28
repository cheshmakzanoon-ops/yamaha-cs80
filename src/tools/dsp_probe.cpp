// Deterministic offline numerical probes; not a hardware-equivalence test.
#include "core/Dsp.h"
#include "core/Engine.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void require(bool condition, const char* message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
std::ofstream csv(const fs::path& directory, const std::string& name) {
    std::ofstream file(directory / name);
    if (!file) throw std::runtime_error("Cannot create probe output " + name);
    file << std::setprecision(10);
    return file;
}
void oscillatorProbes(const fs::path& out) {
    constexpr int count = 65536;
    constexpr float rate = 48000.0f;
    for (int bin : {151, 601, 3001, 6001}) {
        const float frequency = rate * static_cast<float>(bin) / static_cast<float>(count);
        const float dt = frequency / rate;
        cs80::Oscillator oscillator;
        cs80::Rng rng(0x73f1a9u);
        oscillator.reset();
        auto file = csv(out, "oscillator-" + std::to_string(bin) + ".csv");
        file << "sample,phase,saw_ramp,saw_full,pulse,narrow,naive_ramp,legacy_wrong_sign\n";
        for (int i = 0; i < count; ++i) {
            const auto sample = oscillator.tick(frequency, rate, 0.5f, true, true, 0.0f, rng);
            const float phase = oscillator.getPhase();
            const float naive = 1.0f - 2.0f * phase;
            const float ramp = sample.saw - sample.narrow * 0.70f;
            require(std::isfinite(ramp) && std::isfinite(sample.pulse), "oscillator outputs finite");
            file << i << ',' << phase << ',' << ramp << ',' << sample.saw << ','
                 << sample.pulse << ',' << sample.narrow << ',' << naive << ','
                 << naive - cs80::polyBlep(phase, dt) << '\n';
        }
    }
}
void filterEnvelopeProbes(const fs::path& out) {
    for (int rate : {44100, 48000, 96000, 192000}) {
        cs80::FilterEnv envelope, unbent;
        constexpr float initial = -1.5f, attackLevel = 2.25f;
        envelope.noteOn(initial, attackLevel, 0.12f, 0.35f, 0.4f, 0.0f);
        unbent.noteOn(initial, attackLevel, 0.12f, 0.35f, 0.4f, 0.0f);
        auto file = csv(out, "filter-envelope-" + std::to_string(rate) + ".csv");
        file << "time_s,bent_offset_oct,unbent_offset_oct,gate\n";
        float peak = initial;
        const int releaseSample = rate * 12 / 10;
        for (int i = 0; i < rate * 3; ++i) {
            if (i == releaseSample) {
                require(std::abs(envelope.getValue()) < 0.001f, "held filter contour reaches zero");
                envelope.noteOff(); unbent.noteOff();
            }
            const float value = envelope.tick(static_cast<float>(rate));
            const float plain = unbent.tick(static_cast<float>(rate), false);
            peak = std::max(peak, value);
            require(std::isfinite(value) && std::isfinite(plain), "filter contour finite");
            if (i % (rate / 1000) == 0)
                file << static_cast<double>(i + 1) / rate << ',' << value << ',' << plain << ',' << (i < releaseSample ? 1 : 0) << '\n';
        }
        require(std::abs(peak - attackLevel) < 0.001f, "filter attack reaches AL");
        require(std::abs(envelope.getValue() - initial) < 0.001f, "filter release reaches IL");
        require(envelope.isIdle(), "released filter contour reaches idle");
    }
}
void ringProbes(const fs::path& out) {
    constexpr int rate = 48000, block = 48;
    for (float base : {6.6666667f, 40.0f, 200.0f}) {
        cs80::Engine engine;
        engine.prepare(rate);
        cs80::ParamValues params;
        params[cs80::P_ringSpeed] = base;
        params[cs80::P_ringDepth] = 1.0f;
        params[cs80::P_ringAttack] = 0.08f;
        params[cs80::P_ringDecay] = 0.45f;
        params[cs80::P_ringAmount] = 0.5f;
        engine.setParams(params);
        auto file = csv(out, "ring-" + std::to_string(static_cast<int>(std::round(base))) + ".csv");
        file << "time_s,envelope,frequency_hz,base_hz,held_keys\n";
        float audio[block * 2] {};
        int held = 0;
        for (int ms = 0; ms < 2200; ++ms) {
            if (ms == 0 || ms == 1400) { engine.noteOn(60, 0.8f); ++held; }
            if (ms == 300) { engine.noteOn(64, 0.8f); ++held; }
            if (ms == 550) { engine.noteOff(60); --held; }
            if (ms == 700) { engine.noteOff(64); --held; }
            if (ms == 1800) { engine.noteOff(60); --held; }
            engine.render(audio, block);
            const float actual = engine.getRingFrequency();
            const float expected = cs80::ringModulationFrequency(base, 1.0f, engine.getRingEnvelope());
            require(std::isfinite(actual), "ring frequency finite");
            require(std::abs(actual - expected) < 0.001f, "engine ring frequency follows declared law");
            file << (ms + 1) * 0.001 << ',' << engine.getRingEnvelope() << ',' << actual << ',' << base << ',' << held << '\n';
        }
    }
}
}
int main(int argc, char** argv) {
    try {
        const fs::path out = argc > 1 ? argv[1] : "artifacts/dsp-probe";
        fs::create_directories(out);
        oscillatorProbes(out);
        filterEnvelopeProbes(out);
        ringProbes(out);
        auto meta = csv(out, "probe.json");
        meta << "{\"schema\":1,\"sample_rate\":48000,\"sample_count\":65536,\"oscillator_bins\":[151,601,3001,6001],\"failures\":" << failures << "}\n";
        std::cout << "DSP probes: " << failures << " failures; output " << out << '\n';
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 2;
    }
}
