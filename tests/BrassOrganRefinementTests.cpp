// Played brass effort and organ keying contracts. These audio checks cover
// instrument behavior; they do not assert measured acoustic equivalence.
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
namespace
{
constexpr double rate = 48000.0;
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label.c_str()); }
}
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-15)); }
struct Audio
{
    std::vector<float> data;
    double rms(double first, double last) const
    {
        const int a = (int) std::llround(first * rate), b = (int) std::llround(last * rate);
        double energy = 0.0;
        for (int i = 2*a; i < 2*b; ++i) energy += (double) data[(size_t) i] * data[(size_t) i];
        return std::sqrt(energy / (2 * (b-a)));
    }
};
Audio render(const FactoryPreset& p, float velocity, float pressure = 0.0f, int note = 60)
{
    auto e = std::make_unique<Engine>();
    e->prepare(rate);
    e->setParams(p.values);
    e->noteOn(note, velocity, 1);
    e->channelPressure(1, pressure);
    const int held = (int) (2.0 * rate), total = (int) (2.4 * rate);
    Audio audio; audio.data.resize((size_t) (2 * total));
    for (int at = 0; at < total;)
    {
        if (at == held) e->noteOff(note, 1);
        const int n = std::min(128, (at < held ? held : total) - at);
        e->render(audio.data.data() + 2*at, n);
        at += n;
    }
    bool finite = true; double peak = 0.0;
    for (float s : audio.data) { finite = finite && std::isfinite(s); peak = std::max(peak, std::abs((double) s)); }
    check(finite && peak < 1.0 && peak > .00001, p.name + ": finite audible complete-patch render with headroom");
    return audio;
}
const FactoryPreset& preset(const char* name)
{
    const auto& bank = curatedLibrary();
    auto p = std::find_if(bank.begin(), bank.end(), [name](const auto& entry) { return entry.name == name; });
    if (p == bank.end()) throw std::runtime_error(std::string("missing preset: ") + name);
    return *p;
}
bool naturalAccent(const std::string& name)
{
    for (const char* named : { "Sforzando Brass", "Brass Stab", "Horn Section", "Covered Horn", "Muted Trumpet",
                               "Trombone Glide", "Fanfare Brass", "Low Brass", "Tuba", "Brass Bite" })
        if (name == named) return true;
    return false;
}
}
int main()
{
    try
    {
        int brassCount = 0, organCount = 0;
        for (const auto& p : curatedLibrary())
        {
            if (p.category == "Brass" && p.name != "Delay Brass")
            {
                ++brassCount;
                const auto soft = render(p, .25f), firm = render(p, .85f);
                const double contrast = db(firm.rms(0.0, 1.9) / soft.rms(0.0, 1.9));
                // Pressure Swell intentionally preserves a nearly level-flat
                // initial gesture: its expressive axis is continuous pressure.
                if (p.name != "Pressure Swell Brass")
                    check(contrast > (naturalAccent(p.name) ? 7.0 : 5.0) && contrast < 22.0,
                          p.name + ": played effort is not flattened by fixed VCA level");
                else
                {
                    const auto pressed = render(p, .25f, .8f);
                    const double pressureRange = db(pressed.rms(.7, 1.9) / soft.rms(.7, 1.9));
                    check(pressureRange > 8.0, "Pressure Swell Brass: pressure supplies a wide held dynamic range");
                    std::printf("PRESSURE range %.3f dB\n", pressureRange);
                }
                if (p.name == "Brass Stab")
                    check(firm.rms(1.5, 1.9) < .03 * firm.rms(0.0, .2), "Brass Stab: accent dies on a held key");
                if (p.name == "Sforzando Brass")
                {
                    const double relaxed = firm.rms(.8, 1.2) / firm.rms(0.0, .15);
                    check(relaxed > .05 && relaxed < .7, "Sforzando Brass: accent relaxes into an audible sustained body");
                }
                std::printf("BRASS %-23s soft/firm %.3f dB\n", p.name.c_str(), contrast);
            }
            if (p.category == "Organ" && p.name != "Tremolo LFO Organ")
            {
                ++organCount;
                const auto soft = render(p, .20f), firm = render(p, .90f);
                const double contrast = db(firm.rms(0.0, 1.9) / soft.rms(0.0, 1.9));
                const double release = db(firm.rms(2.075, 2.105) / firm.rms(1.7, 1.9));
                check(std::abs(contrast) < .25, p.name + ": registration remains independent of strike velocity");
                check(release < -26.0, p.name + ": dry key damping clears repeated chord changes");
                check(firm.rms(1.7, 1.9) > .55 * firm.rms(.3, .5), p.name + ": steady stop survives the held note");
                std::printf("ORGAN %-23s dynamics %.3f dB; 75-105 ms key damping %.3f dB\n", p.name.c_str(), contrast, release);
            }
        }
        check(brassCount == 32 && organCount == 14, "all existing authored Brass and Organ recipes exercised");
        // Cover two further octaves without imposing an identical tone on
        // bass, horn, mute and section registrations.
        for (const char* name : { "Covered Horn", "Muted Trumpet", "Tuba", "Flute Organ 16+8", "Pipe Organ" })
            for (int note : {48, 72})
            {
                const auto& p = preset(name);
                const auto soft = render(p, .25f, 0.0f, note), firm = render(p, .85f, 0.0f, note);
                const double contrast = db(firm.rms(0.0, 1.9) / soft.rms(0.0, 1.9));
                check(p.category == "Brass" ? contrast > 7.0 : std::abs(contrast) < .25,
                      p.name + ": gesture retains its instrument role across registers");
            }
    }
    catch (const std::exception& e) { check(false, e.what()); }
    std::printf("BrassOrganRefinementTests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
