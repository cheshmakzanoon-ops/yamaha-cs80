// Played keyboard response: broad audible dynamics, ringing held aftersound,
// and separate key damping. These are synthesizer behavior contracts, not a
// claim of measured equivalence to any sampled or acoustic instrument.
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
const FactoryPreset& preset(const char* name)
{
    const auto& bank = curatedLibrary();
    const auto found = std::find_if(bank.begin(), bank.end(),
        [name](const FactoryPreset& p) { return p.name == name; });
    if (found == bank.end()) throw std::runtime_error(std::string("missing ") + name);
    return *found;
}
struct Audio
{
    std::vector<float> data;
    double rms(double begin, double end) const
    {
        const int first = (int) std::llround(begin * rate), last = (int) std::llround(end * rate);
        double sum = 0.0;
        for (int i = 2 * first; i < 2 * last; ++i) sum += (double) data[(size_t) i] * data[(size_t) i];
        return std::sqrt(sum / (2 * (last - first)));
    }
};
Audio render(const FactoryPreset& p, int note, float velocity, bool play = true)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate);
    engine->setParams(p.values);
    if (play) engine->noteOn(note, velocity, 1);
    Audio a; a.data.resize((size_t) (8 * rate));
    const int held = (int) (3 * rate), frames = (int) (4 * rate);
    for (int at = 0; at < frames;)
    {
        if (play && at == held) engine->noteOff(note, 1);
        const int count = std::min(128, (at < held ? held : frames) - at);
        engine->render(a.data.data() + 2 * at, count);
        at += count;
    }
    bool finite = true; double peak = 0.0;
    for (float v : a.data) { finite = finite && std::isfinite(v); peak = std::max(peak, std::abs((double) v)); }
    check(finite && peak < 1.0 && (!play || peak > 0.00001), p.name + ": finite keyboard capture with headroom");
    return a;
}
}
int main()
{
    try
    {
        // 25% and 85% gestures are far from MIDI endpoints. A fixed VCA floor
        // must not collapse their contrast, especially on piano/reed voices.
        for (const char* name : { "Stage Keys", "Soft Keys", "Honky Keys", "Velocity Keys",
                                  "Electric Piano I", "Electric Piano II", "Tine Piano", "Reed Piano" })
        {
            const auto& p = preset(name);
            // The modeled chorus has a stationary BBD floor even when no
            // note is playing. Measure that separately so key damping is a
            // signal-decay contract, not a demand to mute the effect circuit.
            const auto idle = render(p, 60, 0.0f, false);
            const double idleFloor = idle.rms(3.5, 4.0);
            for (int note : { 36, 60, 84 })
            {
                const auto soft = render(p, note, .25f), firm = render(p, note, .85f);
                const double contrast = db(firm.rms(0.0, 3.0) / soft.rms(0.0, 3.0));
                check(contrast > 8.5 && contrast < 18.0, p.name + ": broad but usable soft-to-firm range at note " + std::to_string(note));
                const double dampedSignal = .015 * firm.rms(2.5, 3.0);
                check(firm.rms(3.5, 4.0) < std::hypot(dampedSignal, 1.12 * idleFloor),
                      p.name + ": key release separately damps the held body to the measured effect floor");
                if (note == 60)
                    std::printf("%s dynamics %.3f dB; held aftersound %.3f dB relative to onset\n", name,
                        contrast, db(firm.rms(2.5, 3.0) / firm.rms(0.0, .2)));
            }
        }
        // The slow course/body survives the attack. Negative controls with
        // the preceding zero-sustain contour lose this independently audible
        // aftersound; these gates do not depend on calibrated master volume.
        for (const char* name : { "Stage Keys", "Soft Keys", "Velocity Keys", "Reed Piano" })
        {
            const auto& p = preset(name);
            const auto a = render(p, 60, .85f);
            const double aftersound = db(a.rms(2.5, 3.0) / a.rms(0.0, .2));
            check(aftersound > -34.0 && aftersound < -15.0, p.name + ": audible decaying held body, without an organ-like plateau");
            auto choked = p;
            choked.values[chParam(0, CP_vS)] = 0.0f;
            const auto control = render(choked, 60, .85f);
            check(a.rms(2.5, 3.0) > 1.65 * control.rms(2.5, 3.0), p.name + ": aftersound gate distinguishes the choked-envelope control");
        }
        const auto& quill = preset("Harpsichord I");
        const auto softQuill = render(quill, 60, .25f), firmQuill = render(quill, 60, .85f);
        check(std::abs(db(firmQuill.rms(0.0, 3.0) / softQuill.rms(0.0, 3.0))) < .3,
              "quill registration retains nearly level-invariant touch");
        check(db(firmQuill.rms(2.5, 3.0) / firmQuill.rms(0.0, .2)) > -34.0,
              "quill string retains a decaying held aftersound");
    }
    catch (const std::exception& e) { check(false, e.what()); }
    std::printf("KeyboardResponseTests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
