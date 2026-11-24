// Full lead recipes rendered through the Engine. These contracts measure
// expression, articulation and controller identity, not acoustic parity.
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
    double pitch(double start, double duration, double expected) const
    {
        // Normalized autocorrelation of the emitted audio, with sub-sample peak
        // interpolation. Search one known fundamental octave, not its harmonics.
        const int first = (int) std::llround(start * rate), count = (int) std::llround(duration * rate);
        const int low = (int) std::floor(rate / (1.19 * expected));
        const int high = (int) std::ceil(rate / (.83 * expected));
        std::vector<double> correlation((size_t) (high + 2));
        for (int lag = low - 1; lag <= high + 1; ++lag)
        {
            double product = 0.0, aa = 0.0, bb = 0.0;
            for (int at = first; at < first + count; ++at)
            {
                const double a = .5 * (samples[(size_t) 2 * at] + samples[(size_t) 2 * at + 1]);
                const double b = .5 * (samples[(size_t) 2 * (at + lag)] + samples[(size_t) 2 * (at + lag) + 1]);
                product += a * b; aa += a * a; bb += b * b;
            }
            correlation[(size_t) lag] = product / std::sqrt(std::max(aa * bb, 1.e-24));
        }
        int best = low;
        for (int lag = low + 1; lag <= high; ++lag)
            if (correlation[(size_t) lag] > correlation[(size_t) best]) best = lag;
        const double left = correlation[(size_t) best - 1], mid = correlation[(size_t) best], right = correlation[(size_t) best + 1];
        const double denominator = left - 2.0 * mid + right;
        const double offset = std::abs(denominator) > 1.e-12 ? .5 * (left - right) / denominator : 0.0;
        return rate / (best + std::clamp(offset, -.5, .5));
    }
};
Audio render(ParamValues params, int note, float velocity, float pressure = 0.0f,
             double duration = 2.45, double held = 1.8, float ribbon = 0.0f,
             int nextNote = -1)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate); engine->setParams(params);
    engine->noteOn(note, velocity, 1); engine->channelPressure(1, pressure);
    Audio a;
    const int frames = (int) std::llround(duration * rate), off = (int) std::llround(held * rate);
    const int gesture = (int) std::llround(.4 * rate);
    a.samples.resize((size_t) (2 * frames));
    for (int at = 0; at < frames;)
    {
        if (at == off) engine->noteOff(nextNote >= 0 ? nextNote : note, 1);
        if (at == gesture)
        {
            if (ribbon != 0.0f) engine->setRibbon(ribbon);
            if (nextNote >= 0) { engine->noteOff(note, 1); engine->noteOn(nextNote, velocity, 1); }
        }
        int count = std::min(256, frames - at);
        if (at < off) count = std::min(count, off - at);
        if (at < gesture) count = std::min(count, gesture - at);
        engine->render(a.samples.data() + 2 * at, count); at += count;
    }
    bool finite = true; double peak = 0.0;
    for (float value : a.samples) { finite = finite && std::isfinite(value); peak = std::max(peak, std::abs((double) value)); }
    check(finite && peak < 1.0, "finite lead audio with single-note headroom");
    return a;
}
constexpr const char* names[] {
    "Touch Lead", "Aftertouch Vibrato Lead", "Ribbon Glide Lead", "Noir Lead", "Neo-Noir Brass Lead",
    "Glide Lead", "Sine Lead", "Whistle Lead", "Square Lead", "Narrow Pulse Lead", "Resonant Lead",
    "Screaming Lead", "Fifth Lead", "Octave Lead", "Soft Lead", "Init Bend Lead", "Pressure Lead",
    "Velocity Lead", "Detuned Lead", "Sub Vibrato Lead", "Trill Lead", "Glissando Lead", "Brass Lead",
    "Reed Lead", "Wah Lead", "Ring Lead", "Hollow Lead", "Ether Lead", "Solo Synth", "Contour Lead"
};
void response(const char* name, int note)
{
    const auto& p = preset(name);
    const bool pressureOnly = p.name == "Pressure Lead", pressureVibrato = p.name == "Aftertouch Vibrato Lead";
    const auto soft = render(p.values, note, .20f, pressureOnly ? .3f : 0.0f);
    const auto firm = render(p.values, note, .90f, pressureOnly ? 1.0f : 0.0f);
    const double quiet = soft.rms(1.1, 1.65), loud = firm.rms(1.1, 1.65);
    const double span = db(loud / std::max(quiet, 1.e-12));
    check(quiet > 3.e-5 && loud > 2.e-4, p.name + ": playable body at both efforts");
    if (!pressureOnly && !pressureVibrato)
        check(span > 8.0 && span < 22.0, p.name + ": useful soft-to-firm contrast at note " + std::to_string(note));
    if (pressureVibrato)
        check(span > 1.0 && span < 7.0, p.name + ": onset velocity remains subordinate to pressure");
    const double tail = db(firm.rms(2.15, 2.40) / std::max(loud, 1.e-12));
    check(tail < -16.0, p.name + ": released phrase clears its held body");
    std::printf("RESPONSE,%s,%d,%.6f,%.6f,%.6f,%.6f\n", name, note, span, db(quiet), db(loud), tail);
}
void pressureVoices()
{
    for (const char* name : { "Pressure Lead", "Aftertouch Vibrato Lead", "Ether Lead" })
    {
        const auto& p = preset(name);
        const auto quiet = render(p.values, 67, .55f), pressed = render(p.values, 67, .55f, 1.0f);
        const double range = db(pressed.rms(1.1, 1.65) / std::max(quiet.rms(1.1, 1.65), 1.e-12));
        check(range > 7.0, p.name + ": pressure has a substantial held-level range");
        if (p.name == "Pressure Lead")
        {
            check(quiet.rms(.15, 1.65) < 1.e-6, "Pressure Lead: no pressure remains silent");
            const auto softPressed = render(p.values, 67, .20f, 1.0f);
            check(std::abs(db(softPressed.rms(1.1, 1.65) / pressed.rms(1.1, 1.65))) < .1,
                  "Pressure Lead: identical pressure is independent of strike velocity");
        }
        if (p.name == "Aftertouch Vibrato Lead")
        {
            double dryLow = 1.e6, dryHigh = 0.0, pressedLow = 1.e6, pressedHigh = 0.0;
            for (double at = .65; at < 1.5; at += .035)
            {
                const double dryHz = quiet.pitch(at, .025, 392.0), pressedHz = pressed.pitch(at, .025, 392.0);
                dryLow = std::min(dryLow, dryHz); dryHigh = std::max(dryHigh, dryHz);
                pressedLow = std::min(pressedLow, pressedHz); pressedHigh = std::max(pressedHigh, pressedHz);
            }
            const double dryCents = 1200.0 * std::log2(dryHigh / dryLow);
            const double pressedCents = 1200.0 * std::log2(pressedHigh / pressedLow);
            check(dryCents < 4.0 && pressedCents > 25.0 && pressedCents < 65.0,
                  "Aftertouch Vibrato Lead: pressure introduces the stated restrained vibrato into stable audio");
            std::printf("VIBRATO,%s,%.6f,%.6f\n", name, dryCents, pressedCents);
        }
        std::printf("PRESSURE,%s,%.6f\n", name, range);
    }
}
void pureSineArticulation()
{
    for (const char* name : { "Sine Lead", "Whistle Lead", "Ether Lead" })
    {
        const auto& p = preset(name);
        const auto a = render(p.values, 60, .80f);
        const double onset = a.rms(.002, .012) / a.rms(.15, .35);
        check(onset < .82 && onset > .002, p.name + ": audible rounded VCA onset");
        check(a.rms(1.84, 1.90) > 1.e-7, p.name + ": release is smooth rather than an abrupt mute");
        std::printf("SINE,%s,%.6f,%.6f\n", name, onset, db(a.rms(1.84, 1.90) / a.rms(1.6, 1.75)));
    }
}
void intervalAndGestureIdentity()
{
    constexpr double c4 = 261.625565;
    for (const char* name : { "Fifth Lead", "Octave Lead" })
    {
        auto p = preset(name).values; p[P_mix] = 1.0f;
        const auto a = render(p, 60, .9f);
        const double expected = c4 * (std::string(name) == "Fifth Lead" ? 1.5 : 2.0);
        const double frequency = a.pitch(.7, .15, expected);
        check(std::abs(1200.0 * std::log2(frequency / expected)) < 8.0,
              std::string(name) + ": actual upper rank retains its named interval");
        std::printf("INTERVAL,%s,%.6f\n", name, frequency);
    }
    const auto trill = render(preset("Trill Lead").values, 60, .8f);
    double low = 10000.0, high = 0.0;
    for (double at = .55; at < 1.5; at += .027)
    {
        const double hz = trill.pitch(at, .025, c4); low = std::min(low, hz); high = std::max(high, hz);
    }
    const double trillWidth = 1200.0 * std::log2(high / low);
    check(trillWidth > 170.0 && trillWidth < 230.0, "Trill Lead: audio retains the intentional two-semitone alternation");
    const auto glide = render(preset("Glide Lead").values, 55, .85f, 0.0f, 1.65, 1.8, 0.0f, 67);
    const double early = glide.pitch(.65, .05, 315.0), late = glide.pitch(1.45, .08, 392.0);
    check(early < .9 * late && late > 380.0 && late < 400.0, "Glide Lead: audio connects notes smoothly before reaching the target");
    const auto gliss = render(preset("Glissando Lead").values, 55, .85f, 0.0f, 1.65, 1.8, 0.0f, 67);
    int settledSteps = 0;
    for (double at = .52; at < 1.05; at += .025)
    {
        const double expected = 196.0 * std::exp2((1.0 - std::exp(-(at - .4) / .19)));
        const double hz = gliss.pitch(at, .012, expected);
        const double semitones = 12.0 * std::log2(hz / 196.0);
        if (std::abs(semitones - std::round(semitones)) < .13) ++settledSteps;
    }
    check(settledSteps >= 10, "Glissando Lead: audio lands on discrete semitone steps");
    const auto ribbon = render(preset("Ribbon Glide Lead").values, 60, .85f, 0.0f, 1.4, 1.8, .35f);
    const double pre = ribbon.pitch(.25, .08, c4), post = ribbon.pitch(.9, .1, c4 * 1.35);
    check(std::abs(post / pre - 1.35) < .015, "Ribbon Glide Lead: ribbon moves the sounding pitch while held");
    std::printf("GESTURES,trill_cents,%.6f,glide_early_hz,%.6f,glide_late_hz,%.6f,gliss_settled_steps,%d,ribbon_ratio,%.6f\n",
                trillWidth, early, late, settledSteps, post / pre);
}
} // namespace
int main()
{
    try
    {
        check(std::size(names) == 30, "all existing lead recipes are covered");
        for (const char* name : names) response(name, 67);
        for (int note : {48, 79})
            for (const char* name : { "Sine Lead", "Whistle Lead", "Brass Lead", "Reed Lead", "Contour Lead" }) response(name, note);
        pressureVoices(); pureSineArticulation(); intervalAndGestureIdentity();
    }
    catch (const std::exception& e) { check(false, e.what()); }
    std::printf("Lead refinement: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
