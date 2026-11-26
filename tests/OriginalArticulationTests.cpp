// Rendered behavior of the original-bank finishes and arpeggio articulation.
// These are musical-role regressions, not claims of recorded-instrument parity.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace cs80;
constexpr double sampleRate = 48000.0;
int checks = 0, failures = 0;

void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label.c_str()); }
}

const FactoryPreset& preset(const char* name)
{
    const auto& bank = programLibrary();
    const auto found = std::find_if(bank.begin(), bank.end(),
        [name](const auto& p) { return p.name == name; });
    if (found == bank.end()) throw std::runtime_error(std::string("Missing preset: ") + name);
    return *found;
}

struct Audio
{
    std::vector<float> stereo;
    int frames() const { return static_cast<int>(stereo.size() / 2); }
    double rms(double from, double to) const
    {
        const int first = std::clamp(static_cast<int>(std::llround(from * sampleRate)), 0, frames());
        const int last = std::clamp(static_cast<int>(std::llround(to * sampleRate)), 0, frames());
        if (last <= first) throw std::runtime_error("Empty audio window");
        double energy = 0.0;
        for (int i = first * 2; i < last * 2; ++i)
            energy += static_cast<double>(stereo[static_cast<size_t>(i)]) * stereo[static_cast<size_t>(i)];
        return std::sqrt(energy / (2.0 * (last - first)));
    }
};

Audio render(ParamValues p, double held, double tail, const std::string& label)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(sampleRate);
    auto dryP = p;
    for (int ext : { (int) P_fx1ExtModel, (int) P_fx2ExtModel, (int) P_fx3ExtModel, (int) P_fx4ExtModel })
        if (dryP[ext] > 0.0f) dryP[ext] = 0.0f;
    engine->setParams(dryP);
    engine->noteOn(60, 0.70f);
    const int releaseFrame = static_cast<int>(std::llround(held * sampleRate));
    Audio audio;
    audio.stereo.resize(static_cast<size_t>(2 * std::llround((held + tail) * sampleRate)));
    for (int at = 0; at < audio.frames();)
    {
        if (at == releaseFrame) engine->noteOff(60);
        int count = std::min(128, audio.frames() - at);
        if (at < releaseFrame) count = std::min(count, releaseFrame - at);
        engine->render(audio.stereo.data() + 2 * at, count);
        at += count;
    }
    bool finite = true;
    double peak = 0.0;
    for (float sample : audio.stereo)
    {
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    check(finite && peak < 1.0, label + ": finite audio below full scale");
    check(peak > 0.001, label + ": audible output");
    return audio;
}

void sineFinishes()
{
    struct Case { const char* name; const char* source; };
    for (const auto& item : { Case {"Frozen Leads 121", "Whistle Lead"},
                             Case {"Soft Leads 128", "Sine Lead"} })
    {
        const auto& finished = preset(item.name).values;
        const auto& source = preset(item.source).values;
        const auto audio = render(finished, 0.6, 0.45, item.name);
        const auto control = render(source, 0.6, 0.45, item.source);
        const double onset = audio.rms(0.005, 0.025) / audio.rms(0.3, 0.5);
        const double controlOnset = control.rms(0.005, 0.025) / control.rms(0.3, 0.5);
        const double release = audio.rms(0.64, 0.69) / audio.rms(0.5, 0.58);
        const double controlRelease = control.rms(0.64, 0.69) / control.rms(0.5, 0.58);
        std::printf("%s: relative onset %.6f, source %.6f; release %.6f, source %.6f\n",
                    item.name, onset, controlOnset, release, controlRelease);
        check(onset < 0.85 * controlOnset, std::string(item.name) + ": audibly cushioned onset");
        check(release > 1.12 * controlRelease, std::string(item.name) + ": more connected release");
        // This control represents the old failure: moving an inactive LP.
        auto deadFilter = finished;
        deadFilter[chParam(0, CP_lpfCutoff)] = 200.0f;
        deadFilter[chParam(0, CP_fAl)] = 0.9f;
        deadFilter[P_kbdBrillianceHigh] = 0.8f;
        const auto inactive = render(deadFilter, 0.6, 0.45, std::string(item.name) + " inactive-filter control");
        double difference = 0.0, energy = 0.0;
        for (size_t i = 0; i < audio.stereo.size(); ++i)
        {
            const double delta = audio.stereo[i] - inactive.stereo[i];
            difference += delta * delta;
            energy += static_cast<double>(audio.stereo[i]) * audio.stereo[i];
        }
        check(difference < 1.0e-12 * energy,
              std::string(item.name) + ": filter controls cannot fake a bypass-sine finish");
        for (const int parameter : { CP_saw, CP_pulse, CP_sine, CP_noise, CP_vcfLevel, CP_feet })
            check(finished[chParam(0, parameter)] == source[chParam(0, parameter)],
                  std::string(item.name) + ": source oscillator topology and register preserved");
    }
}

// A common sine carrier exposes the authored rank-II amplitude contour without
// confusing a change of filter spectrum with a change of note articulation.
// The arpeggiator, rate, gate, envelope and complete circuit VCA stay active.
ParamValues accentCarrier(ParamValues p)
{
    p[P_mix] = 1.0f;
    p[P_fx1Mix] = p[P_fx2Mix] = p[P_fx3Mix] = p[P_chorusOn] = 0.0f;
    p[P_arpOctaves] = 1.0f;
    p[P_volume] = 0.18f;
    p[bodyParam(1, 0)] = p[bodyParam(1, 1)] = 0.0f;
    p[chParam(1, CP_saw)] = p[chParam(1, CP_pulse)] = p[chParam(1, CP_noise)] = 0.0f;
    p[chParam(1, CP_sine)] = 1.0f;
    p[chParam(1, CP_vcfLevel)] = 0.0f;
    p[chParam(1, CP_feet)] = 2.0f;
    return p;
}

void sequencePacing()
{
    struct Case { const char* name; const char* source; bool longer; };
    for (const auto& item : { Case {"Amber Sequences 157", "Arp Poly", true},
                             Case {"Distant Sequences 159", "Arp Bass", true},
                             Case {"Neon Sequences 160", "Random Arp", false},
                             Case {"Distant Sequences 173", "Random Arp", true},
                             Case {"Neon Sequences 174", "Arp Poly", false} })
    {
        auto p = accentCarrier(preset(item.name).values);
        auto control = p;
        const auto& source = preset(item.source).values;
        // A deliberately unpaced control: changing the note rate while copying
        // the original fixed accent envelope was the previous behavior.
        for (const int parameter : {CP_vA, CP_vD, CP_vS, CP_vR})
            control[chParam(1, parameter)] = source[chParam(1, parameter)];
        const double period = 1.0 / p[P_arpRate];
        const double gate = period * (p[P_arpMode] == 3.0f ? 0.45 : 0.58);
        const auto audio = render(p, 2.0 * period, period, item.name);
        const auto unpaced = render(control, 2.0 * period, period, std::string(item.name) + " unpaced control");
        const double late = audio.rms(0.65 * gate, 0.95 * gate) / audio.rms(0.15 * gate, 0.35 * gate);
        const double controlLate = unpaced.rms(0.65 * gate, 0.95 * gate) / unpaced.rms(0.15 * gate, 0.35 * gate);
        std::printf("%s: accent late/early %.6f, fixed-time control %.6f; gate %.4f s\n",
                    item.name, late, controlLate, gate);
        check(item.longer ? late > 1.50 * controlLate : late < 0.90 * controlLate,
              std::string(item.name) + ": accent contour follows the note spacing");
        check(audio.rms(2.6 * period, 2.95 * period) < 0.05 * audio.rms(0.0, gate),
              std::string(item.name) + ": dry accent clears after key release");
    }
    // Full-patch checks include the deliberately long bell resonance and its
    // unretimed echo; neither should become a short generic synth pluck.
    const auto& bell = preset("Slow Sequences 166").values;
    const auto audio = render(bell, 0.5, 0.6, "Slow Sequences 166 full patch");
    check(audio.rms(0.66, 0.90) > 0.0001, "Slow Sequences 166: bell/echo tail retained");
}
}

int main()
{
    try { sineFinishes(); sequencePacing(); }
    catch (const std::exception& exception) { check(false, exception.what()); }
    std::printf("Original articulation: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
