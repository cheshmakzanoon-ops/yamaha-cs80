// Complete owned pad/poly recipe response, measured through the actual Engine.
// These contracts establish musical behavior, not acoustic-recording parity.
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

using namespace cs80;
namespace
{
constexpr double rate = 44100.0;
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label.c_str()); }
}
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-12)); }
const FactoryPreset& preset(const char* name)
{
    for (const auto& p : curatedLibrary()) if (p.name == name) return p;
    throw std::runtime_error(std::string("missing preset ") + name);
}
struct Audio
{
    std::vector<float> samples;
    double rms(double begin, double end) const
    {
        const int first = (int) std::llround(begin * rate), last = (int) std::llround(end * rate);
        double sum = 0.0;
        for (int i = 2 * first; i < 2 * last; ++i) sum += (double) samples[(size_t) i] * samples[(size_t) i];
        return std::sqrt(sum / (2 * (last - first)));
    }
};
Audio render(const FactoryPreset& p, int note, float velocity, float pressure = 0.0f,
             double duration = 2.85, double hold = 2.4)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate);
    engine->setParams(p.values);
    engine->noteOn(note, velocity, 1);
    engine->channelPressure(1, pressure);
    Audio a;
    const int frames = (int) std::llround(duration * rate), off = (int) std::llround(hold * rate);
    a.samples.resize((size_t) (2 * frames));
    for (int at = 0; at < frames;)
    {
        if (at == off) engine->noteOff(note, 1);
        const int count = std::min({ 256, frames - at, (at < off ? off : frames) - at });
        engine->render(a.samples.data() + 2 * at, count);
        at += count;
    }
    bool finite = true; double peak = 0.0;
    for (float value : a.samples) { finite = finite && std::isfinite(value); peak = std::max(peak, std::abs((double) value)); }
    check(finite && peak < 1.0, p.name + ": finite single-note render with headroom");
    return a;
}
constexpr const char* padNames[] {
    "Slow Pad", "Ring Drift Pad", "Choir Pad", "Dark Choir", "Sweep Pad", "Breathing Pad",
    "Glass Pad", "Warm Pad", "Wide Pad", "Octave Pad", "Fifth Pad", "Evolving Pad",
    "Noise Pad", "Pressure Pad", "Sine Pad", "Hollow Pad", "Resonant Pad", "Pulse Pad",
    "Tremolo Pad", "Neo-Noir Pad", "Rain Pad", "Cathedral Pad", "Shimmer Pad", "Low Drone",
    "Ether Pad", "Velvet Pad", "Analog Haze", "Bowed Glass", "Night Pad", "Arctic Pad"
};
constexpr const char* polyNames[] {
    "Poly Stab", "Poly Brass Stab", "PWM Poly", "Resonant Poly", "Detuned Poly", "Poly Sweep",
    "Velocity Poly", "Poly Pluck", "Octave Poly", "Fifth Poly", "Bright Poly", "Dark Poly",
    "Hollow Poly", "Wide Poly", "Poly Swell", "Saw Pulse Poly", "Synthwave Chords", "Square Poly",
    "Sine Poly", "Contour Poly", "Ring Poly", "Glide Poly", "Tremolo Poly", "Poly Sub Pulse"
};
void response(const char* name, int note, bool pad)
{
    const auto& p = preset(name);
    const auto soft = render(p, note, .20f), firm = render(p, note, .90f);
    const bool pressureLed = p.name == "Pressure Pad" || p.name == "Poly Swell";
    const bool struck = p.name == "Poly Stab" || p.name == "Poly Brass Stab" || p.name == "Poly Pluck";
    const double first = struck ? .02 : 1.5, last = struck ? .20 : 2.25;
    const double quiet = soft.rms(first, last), loud = firm.rms(first, last);
    const double span = db(loud / std::max(quiet, 1.e-12));
    check(quiet > 5.e-5 && loud > 2.e-4, p.name + ": both gestures retain a usable sounding body");
    if (!pressureLed)
        check(span > (pad ? 6.5 : 8.0) && span < 18.0,
              p.name + ": fixed level does not flatten soft-to-firm expression at note " + std::to_string(note));
    if (struck)
    {
        const double falling = firm.rms(1.8, 2.3) / firm.rms(.02, .20);
        check(falling < .03, p.name + ": held chord hit continues to decay");
        std::printf("DECAY,%s,%.6f\n", name, db(falling));
    }
    std::printf("RESPONSE,%s,%d,%.6f,%.6f,%.6f\n", name, note, span, db(quiet), db(loud));
}
void pressureLed()
{
    for (const char* name : { "Pressure Pad", "Poly Swell" })
    {
        const auto& p = preset(name);
        const auto quiet = render(p, 60, .55f, 0.0f), pressed = render(p, 60, .55f, 1.0f);
        const double range = db(pressed.rms(1.5, 2.25) / quiet.rms(1.5, 2.25));
        check(range > 17.0, p.name + ": pressure can substantially grow the quiet chord");
        std::printf("PRESSURE,%s,%.6f\n", name, range);
    }
}
void longHeldPadHeadroom()
{
    // Slow footage layers can reach their largest combined peak after the
    // one-second calibration stress window. Exercise the complete held chord.
    constexpr int stressRate = 48000, frames = 12 * stressRate;
    constexpr std::array<int, 8> notes {60, 62, 64, 65, 67, 69, 71, 72};
    for (const char* name : { "Cathedral Pad", "Golden Pads 66" })
    {
        const auto& bank = programLibrary();
        const auto it = std::find_if(bank.begin(), bank.end(),
                                    [name](const auto& p) { return p.name == name; });
        check(it != bank.end(), std::string(name) + ": existing long-bloom voice is present");
        if (it == bank.end()) continue;
        auto engine = std::make_unique<Engine>();
        engine->prepare(stressRate);
        engine->setParams(it->values);
        for (int note : notes) engine->noteOn(note, 1.0f, 1);
        engine->channelPressure(1, 1.0f);
        std::array<float, 512> block {};
        bool finite = true;
        double peak = 0.0;
        for (int at = 0; at < frames; at += 256)
        {
            engine->render(block.data(), 256);
            for (float value : block)
            {
                finite = finite && std::isfinite(value);
                peak = std::max(peak, std::abs(static_cast<double>(value)));
            }
        }
        check(finite && db(peak) < -1.5,
              std::string(name) + ": complete twelve-second dense chord retains measured headroom");
        std::printf("LONG_CHORD,%s,%.6f dBFS\n", name, db(peak));
    }
}
void compingRelease()
{
    // Compare the released signal with the same independently rendered held
    // chord at the same time. Spectral drift cannot masquerade as damping.
    for (const char* name : { "PWM Poly", "Bright Poly", "Square Poly", "Saw Pulse Poly", "Contour Poly" })
    {
        const auto& p = preset(name);
        const auto released = render(p, 60, .85f), held = render(p, 60, .85f, 0.0f, 2.85, 3.0);
        const double closing = db(released.rms(2.52, 2.57) / held.rms(2.52, 2.57));
        check(closing < -22.0, p.name + ": previous chord clears promptly during comping");
        std::printf("RELEASE,%s,%.6f\n", name, closing);
    }
}
} // namespace
int main()
{
    try
    {
        for (const char* name : padNames) response(name, 60, true);
        for (const char* name : polyNames) response(name, 60, false);
        check(std::size(padNames) == 30 && std::size(polyNames) == 24, "all existing owned pad/poly recipes are covered");
        for (int note : {48, 72})
        {
            for (const char* name : { "Choir Pad", "Dark Choir", "Sine Pad", "Low Drone", "Arctic Pad" }) response(name, note, true);
            for (const char* name : { "Velocity Poly", "Square Poly", "Sine Poly", "Octave Poly" }) response(name, note, false);
        }
        pressureLed();
        compingRelease();
        longHeldPadHeadroom();
    }
    catch (const std::exception& e) { check(false, e.what()); }
    std::printf("Pad/poly refinement: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
