// Factory performance regressions for existing bass and plucked-string voices.
// Audio criteria distinguish useful touch, string aftersound, key damping and
// intentionally steady pedal registration. They do not certify acoustic parity.
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
void check(bool pass, const std::string& name)
{
    ++checks;
    if (!pass) { ++failures; std::printf("FAIL: %s\n", name.c_str()); }
}
double db(double amplitude) { return 20.0 * std::log10(std::max(amplitude, 1.e-15)); }
const FactoryPreset& preset(const char* name)
{
    const auto& bank = curatedLibrary();
    const auto it = std::find_if(bank.begin(), bank.end(), [name](const auto& p) { return p.name == name; });
    if (it == bank.end()) throw std::runtime_error(std::string("Missing preset: ") + name);
    return *it;
}
struct Audio
{
    std::vector<float> samples;
    double rms(double start, double end) const
    {
        const int first = (int) std::llround(start * rate), last = (int) std::llround(end * rate);
        double sum = 0;
        for (int n = 2 * first; n < 2 * last; ++n) sum += (double) samples[(size_t) n] * samples[(size_t) n];
        return std::sqrt(sum / (2.0 * (last - first)));
    }
};
Audio render(const FactoryPreset& p, int note, float velocity, double held, double tail = 0.0)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate);
    auto dryP = p.values; dryP[P_fxMasterBypass] = 1.0f; engine->setParams(dryP);
    engine->noteOn(note, velocity, 1);
    const int releaseAt = (int) std::llround(held * rate);
    const int frames = (int) std::llround((held + tail) * rate);
    Audio a; a.samples.resize((size_t) (frames * 2));
    for (int at = 0; at < frames;)
    {
        if (at == releaseAt) engine->noteOff(note, 1);
        const int count = std::min(128, (at < releaseAt ? releaseAt : frames) - at);
        engine->render(a.samples.data() + 2 * at, count);
        at += count;
    }
    bool finite = true; double peak = 0.0;
    for (float value : a.samples) { finite = finite && std::isfinite(value); peak = std::max(peak, std::abs((double) value)); }
    check(finite && peak > .00001 && peak < 1.0, p.name + ": finite, audible render with headroom");
    return a;
}
void touchResponse(const char* name, int note, double minimum)
{
    const auto& p = preset(name);
    const auto soft = render(p, note, .25f, .5), firm = render(p, note, .85f, .5);
    const double contrast = db(firm.rms(0.0, .5) / soft.rms(0.0, .5));
    check(contrast > minimum && contrast < 18.0, p.name + ": useful touch range without collapsing soft notes");
    std::printf("TOUCH %-18s %.3f dB\n", name, contrast);
}
}
int main()
{
    try
    {
        // These are the existing programs in the two authored source families;
        // modern effects variants are independently voiced in Modern.inc.
        for (const char* name : { "Analog Bass", "Resonance Bass", "Sine Bass", "Sub Bass", "Pluck Bass",
                "Fretless Glide", "Funk Bass", "Brass Bass", "Octave Bass", "Pulse Bass", "Round Bass",
                "Velocity Bass", "Rubber Bass", "Dark Bass", "Bright Bass", "Picked Bass", "Upright Pluck",
                "Fifth Bass", "Squelch Bass", "Ring Bass", "Sub Wobble Bass", "Pressure Bass", "Contour Bass" })
            touchResponse(name, 48, 7.2);
        for (const char* name : { "Guitar I", "Guitar II", "Funky Pluck I", "Funky Pluck II", "Harp", "Koto",
                "Banjo", "Mandolin", "Drone Pluck", "Nylon Pluck", "Steel Pluck", "Muted Pluck", "Kalimba",
                "Zither", "Dulcimer Pluck", "Pressure Pluck" })
            touchResponse(name, 60, 8.0);

        // Full string bodies ring independently of the short pick. Compare an
        // intentionally choked outer envelope, at the same calibrated gain.
        for (const char* name : { "Guitar I", "Guitar II", "Nylon Pluck", "Steel Pluck", "Dulcimer Pluck" })
        {
            const auto& p = preset(name);
            const auto body = render(p, 60, .85f, 3.0, .7);
            auto choked = p; choked.values[chParam(0, CP_vS)] = 0.0f;
            const auto control = render(choked, 60, .85f, 3.0, .7);
            const double tail = db(body.rms(2.5, 3.0) / body.rms(0.0, .2));
            check(tail > -38.0 && tail < -18.0, p.name + ": decaying aftersound remains audible after the pick");
            check(body.rms(2.5, 3.0) > 1.6 * control.rms(2.5, 3.0), p.name + ": aftersound distinguishes the choked-envelope control");
            if (std::string(name) != "Dulcimer Pluck")
                check(body.rms(3.4, 3.7) < .02 * body.rms(2.5, 3.0), p.name + ": key release damps the string independently");
            std::printf("BODY  %-18s %.3f dB aftersound\n", name, tail);
        }
        const auto& pedal = preset("Pedal Bass");
        const auto soft = render(pedal, 48, .25f, 1.0, .3), firm = render(pedal, 48, .85f, 1.0, .3);
        check(std::abs(db(firm.rms(.2, .8) / soft.rms(.2, .8))) < .1,
              "Pedal Bass: organ-style registration stays velocity-neutral");
        const double ending = db(firm.rms(1.08, 1.12) / firm.rms(.8, .95));
        check(ending < -48.0, "Pedal Bass: coordinated short key release clears low pedal notes");
        std::printf("PEDAL key damping %.3f dB\n", ending);

        const auto muted = render(preset("Muted Pluck"), 60, .85f, 1.2);
        check(db(muted.rms(.8, 1.1) / muted.rms(.01, .08)) < -55.0,
              "Muted Pluck: the palm-muted body stays compact on a held key");
        // The body extension must not spill into the deliberately synthetic,
        // fully damped Rubber Bass, whose detailed spectral tests live elsewhere.
        const auto rubber = render(preset("Rubber Bass"), 48, .85f, 4.0);
        check(db(rubber.rms(3.5, 3.9) / rubber.rms(.02, .08)) < -80.0,
              "Rubber Bass: elastic pitch attack still ends in silence while held");
    }
    catch (const std::exception& e) { check(false, e.what()); }
    std::printf("BassPluckedRefinementTests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
