// Whole-preset performance regressions. These checks use rendered audio, not
// the authored parameter values, and do not claim physical-instrument matches.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
constexpr double sampleRate = 44100.0;
int checks = 0, failures = 0;

void check(bool success, const std::string& label)
{
    ++checks;
    if (!success) { ++failures; std::printf("FAIL %s\n", label.c_str()); }
}

double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-12)); }

struct Audio
{
    std::vector<float> samples;
    double rms(double begin, double end) const
    {
        const int first = static_cast<int>(std::llround(begin * sampleRate));
        const int last = static_cast<int>(std::llround(end * sampleRate));
        double energy = 0.0;
        for (int i = first; i < last; ++i)
        {
            const double left = samples[static_cast<size_t>(2 * i)];
            const double right = samples[static_cast<size_t>(2 * i + 1)];
            energy += 0.5 * (left * left + right * right);
        }
        return std::sqrt(energy / (last - first));
    }
};

const FactoryPreset& preset(const char* name)
{
    for (const auto& p : curatedLibrary()) if (p.name == name) return p;
    throw std::runtime_error(std::string("Missing program: ") + name);
}

Audio render(const FactoryPreset& p, int note, float velocity, bool pressure = false)
{
    constexpr double hold = 2.4, duration = 2.8;
    const int noteOff = static_cast<int>(std::llround(hold * sampleRate));
    const int pressed = static_cast<int>(std::llround(0.6 * sampleRate));
    const int eased = static_cast<int>(std::llround(1.4 * sampleRate));
    Audio audio;
    const int frames = static_cast<int>(std::llround(duration * sampleRate));
    audio.samples.resize(static_cast<size_t>(2 * frames));
    Engine engine;
    engine.prepare(sampleRate);
    engine.setParams(p.values);
    engine.noteOn(note, velocity, 1);
    for (int done = 0; done < frames;)
    {
        if (done == noteOff) engine.noteOff(note, 1);
        if (pressure && done == pressed) engine.channelPressure(1, 1.0f);
        if (pressure && done == eased) engine.channelPressure(1, 0.0f);
        int next = frames;
        if (done < noteOff) next = std::min(next, noteOff);
        if (pressure && done < pressed) next = std::min(next, pressed);
        if (pressure && done < eased) next = std::min(next, eased);
        const int count = std::min(256, next - done);
        engine.render(audio.samples.data() + 2 * done, count);
        done += count;
    }
    bool finite = true;
    double peak = 0.0;
    for (float value : audio.samples)
    {
        finite = finite && std::isfinite(value);
        peak = std::max(peak, std::abs(static_cast<double>(value)));
    }
    check(finite, p.name + ": finite render");
    check(peak < 1.0, p.name + ": single-note headroom");
    return audio;
}

void response(const FactoryPreset& p, int note)
{
    const auto soft = render(p, note, 0.20f);
    const auto firm = render(p, note, 0.90f);
    const bool detached = p.name == "Pizzicato Strings" || p.name == "Spiccato Strings";
    const double begin = detached ? 0.015 : 1.8;
    const double end = detached ? 0.18 : 2.3;
    const double gentle = soft.rms(begin, end), strong = firm.rms(begin, end);
    const double gain = db(strong / std::max(gentle, 1.e-12));
    const auto label = p.name + " note " + std::to_string(note);
    check(gentle > 1.e-4, label + ": quiet gesture retains an audible body");
    check(strong > 3.e-4, label + ": firm gesture retains an audible body");
    if (p.name == "String Machine")
    {
        check(std::abs(gain) < 0.25, label + ": constant-level string-machine registration");
        const double closing = firm.rms(2.52, 2.55) / std::max(strong, 1.e-12);
        check(closing < 0.20, label + ": old chord falls at least 14 dB within 150 ms of release");
        std::printf("GATE,String Machine,%.6f\n", db(closing));
    }
    else if (p.name != "Swell Strings")
        check(gain > (detached ? 7.0 : 5.5), label + ": useful soft-to-firm level range");
    if (detached)
        check(firm.rms(1.8, 2.3) < 0.01 * strong, label + ": held detached bow/pluck dies away");
    if (p.category == "Winds")
        check(firm.rms(2.65, 2.78) < 0.003 * strong, label + ": breath stops after key release");
    std::printf("RESPONSE,%s,%d,%.6f,%.6f,%.6f,%.6f\n", p.name.c_str(), note,
                gain, db(gentle), db(strong), db(firm.rms(2.44, 2.49) / std::max(strong, 1.e-12)));
}

void familyCoverage()
{
    int winds = 0, strings = 0;
    for (const auto& p : curatedLibrary())
    {
        if (p.category != "Winds" && p.category != "Strings") continue;
        // This cohort owns Winds.inc and Strings.inc. The three separately
        // authored Modern.inc string programs have their own effect voicings.
        if (std::find(p.tags.begin(), p.tags.end(), "modern") != p.tags.end()) continue;
        p.category == "Winds" ? ++winds : ++strings;
        response(p, 60);
    }
    check(winds == 14 && strings == 28, "all existing wind and string recipes remain covered");
    // The larger/upper registrations and detached playing need more than a
    // middle-C witness. Keep sounding footage and all factory effects intact.
    for (const char* name : { "Flute", "Clarinet", "Bass Clarinet", "Oboe", "English Horn",
                              "Bassoon", "Piccolo", "Solo Violin", "Solo Cello",
                              "Pizzicato Strings", "Spiccato Strings", "Tutti Strings" })
        for (int note : { 48, 72 }) response(preset(name), note);
}

void independentPressure()
{
    const auto& p = preset("Swell Strings");
    const auto phrase = render(p, 60, 0.55f, true);
    const double opening = phrase.rms(0.35, 0.55);
    const double pressed = phrase.rms(0.95, 1.25);
    const double eased = phrase.rms(1.90, 2.20);
    check(opening > 1.e-4, "Swell Strings: a quiet bow remains before pressure");
    check(pressed > 4.0 * opening, "Swell Strings: held pressure creates a substantial swell");
    check(eased < 0.45 * pressed, "Swell Strings: easing pressure returns to the quiet bow");
    std::printf("PRESSURE,Swell Strings,%.6f,%.6f\n", db(pressed / opening), db(eased / pressed));
}
} // namespace

int main()
{
    try { familyCoverage(); independentPressure(); }
    catch (const std::exception& error) { check(false, error.what()); }
    std::printf("Wind/string refinement: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
