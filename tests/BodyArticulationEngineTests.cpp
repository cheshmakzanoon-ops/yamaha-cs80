// Rendered contracts for note-captured instrument-body articulation. These
// qualify response/lifetime/isolation, not resemblance to a recorded instrument.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool condition, const std::string& message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}

ParamValues fixture(int model)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = p[P_kasScanTiming] = p[P_analogNoise] = 0.0f;
    p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
    p[P_kbdBrillianceLow] = p[P_kbdBrillianceHigh] = 0.0f;
    p[P_kbdLevelLow] = p[P_kbdLevelHigh] = 0.0f;
    p[P_mix] = p[P_detune] = p[P_portamento] = p[P_initBend] = 0.0f;
    p[P_subVco] = p[P_subAtVco] = p[P_subVcf] = p[P_subAtVcf] = p[P_subVca] = 0.0f;
    p[P_volume] = 0.2f;
    p[P_masterSustain] = p[P_keyRelease] = 0.0f;
    p[P_body1Model] = static_cast<float>(model);
    p[P_body1Mix] = 1.0f;
    p[P_body1Brightness] = 0.55f;
    p[P_body1Decay] = 1.0f;
    p[chParam(0, CP_hpfOn)] = p[chParam(0, CP_lpfOn)] = 0.0f;
    p[chParam(0, CP_feet)] = 1.0f;
    p[chParam(0, CP_vA)] = 0.005f;
    p[chParam(0, CP_vD)] = 1.0f;
    p[chParam(0, CP_vS)] = 0.0f;
    p[chParam(0, CP_vR)] = 0.35f;
    p[chParam(0, CP_fIl)] = p[chParam(0, CP_fAl)] = 0.0f;
    p[chParam(0, CP_initBrill)] = p[chParam(0, CP_afterBrill)] = 0.0f;
    p[chParam(0, CP_initLevel)] = p[chParam(0, CP_afterLevel)] = 0.0f;
    // Isolate the optional body; circuit-source differences must not be
    // mistaken for unequal envelope timing.
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_saw)] = p[chParam(rank, CP_pulse)] = 0.0f;
        p[chParam(rank, CP_sine)] = p[chParam(rank, CP_noise)] = 0.0f;
    }
    return p;
}

std::unique_ptr<Engine> engine(const ParamValues& p, double rate = 48000.0)
{
    auto result = std::make_unique<Engine>();
    result->setParams(p);
    result->prepare(rate);
    return result;
}

std::vector<float> render(Engine& e, int frames, int block = 113)
{
    std::vector<float> result(static_cast<std::size_t>(2 * frames));
    for (int at = 0; at < frames; at += block)
        e.render(result.data() + 2 * at, std::min(block, frames - at));
    bool finite = true;
    double peak = 0.0;
    for (float sample : result)
    {
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    check(finite && peak < 1.0, "articulation renders stay finite and within fixture headroom");
    return result;
}

int voiceIndex(const Engine& e, int note)
{
    for (int index = 0; index < 8; ++index)
        if (e.getVoiceInfo(index).active && e.getVoiceInfo(index).note == note) return index;
    check(false, "expected sounding voice remains present");
    return 0;
}
Adsr::State state(const Engine& e, int note)
{
    return e.getRankEnvelopeState(voiceIndex(e, note), 0).amplitude;
}
double relativeError(const std::vector<float>& a, const std::vector<float>& b)
{
    double error = 0.0, power = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const double delta = static_cast<double>(a[i]) - b[i];
        error += delta * delta;
        power += static_cast<double>(b[i]) * b[i];
    }
    return std::sqrt(error / std::max(power, 1.0e-30));
}
double rms(const std::vector<float>& audio)
{
    double sum = 0.0;
    for (float value : audio) sum += static_cast<double>(value) * value;
    return std::sqrt(sum / std::max<std::size_t>(1, audio.size()));
}

void struckRegister()
{
    for (int model : {body::piano, body::tine, body::reed, body::nylon, body::marimba})
    {
        auto p = fixture(model);
        auto bass = engine(p), treble = engine(p);
        bass->noteOn(36, 0.75f); treble->noteOn(84, 0.75f);
        (void) render(*bass, 9600); (void) render(*treble, 9600);
        const double decayRatio = state(*bass, 36).value / state(*treble, 84).value;
        check(decayRatio > 1.35, std::string(body::name(model)) + ": bass amplitude decay retains more aftersound than treble");

        p[chParam(0, CP_vS)] = 1.0f;
        bass = engine(p); treble = engine(p);
        bass->noteOn(36, 0.75f); treble->noteOn(84, 0.75f);
        (void) render(*bass, 4800); (void) render(*treble, 4800);
        bass->noteOff(36); treble->noteOff(84);
        (void) render(*bass, 3840); (void) render(*treble, 3840);
        const double releaseRatio = state(*bass, 36).value / state(*treble, 84).value;
        check(releaseRatio > 1.20, std::string(body::name(model)) + ": bass key damping is less abrupt than treble");
        std::printf("REGISTER %s bass/treble envelope decay=%.5f release=%.5f\n",
                    body::name(model), decayRatio, releaseRatio);
    }
}

void expressiveAttack()
{
    for (int model : {body::bowed, body::flute, body::clarinet, body::oboe, body::bassoon, body::brass})
    {
        auto p = fixture(model);
        p[chParam(0, CP_vA)] = 0.12f;
        p[chParam(0, CP_vS)] = 1.0f;
        auto soft = engine(p), firm = engine(p);
        soft->noteOn(60, 0.2f); firm->noteOn(60, 0.95f);
        const auto softEarly = render(*soft, 2880), firmEarly = render(*firm, 2880);
        const double onsetRatio = state(*firm, 60).value / state(*soft, 60).value;
        (void) render(*soft, 9600); (void) render(*firm, 9600);
        const double softRelative = rms(softEarly) / rms(render(*soft, 2400));
        const double firmRelative = rms(firmEarly) / rms(render(*firm, 2400));
        check(onsetRatio > 1.10, std::string(body::name(model)) + ": a firm strike advances the amplitude attack");
        check(firmRelative > softRelative * 1.08,
              std::string(body::name(model)) + ": level-normalized audio also has a firmer early onset");
        std::printf("ATTACK %s envelope firm/soft=%.5f audio early/held soft=%.5f firm=%.5f\n",
                    body::name(model), onsetRatio, softRelative, firmRelative);
    }
}

void registerFootageAndRates()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int model : {body::piano, body::tine, body::marimba, body::bowed})
        {
            auto aParams = fixture(model), bParams = aParams;
            bParams[chParam(0, CP_feet)] = 0.0f;
            auto a = engine(aParams, rate), b = engine(bParams, rate);
            a->noteOn(48, 0.65f); b->noteOn(60, 0.65f);
            const int frames = static_cast<int>(0.12 * rate);
            check(relativeError(render(*a, frames), render(*b, frames, 37)) < 0.0001,
                  "equal sounding registers retain equal body/envelope audio across footage and rate");
            a->noteOff(48); b->noteOff(60);
            const auto aTail = render(*a, frames), bTail = render(*b, frames, 29);
            const double error = relativeError(aTail, bTail);
            std::printf("FOOTAGE %s rate=%.0f release relative error=%.9g envelope %.9g / %.9g pitch %.9g / %.9g\n",
                        body::name(model), rate, error, state(*a, 48).value, state(*b, 60).value,
                        a->getRankInfo(voiceIndex(*a, 48), 0).oscillatorHz,
                        b->getRankInfo(voiceIndex(*b, 60), 0).oscillatorHz);
            check(state(*a, 48).value == state(*b, 60).value,
                  "footage-equivalent bodies retain exactly matching envelope damping");
            check(std::abs(rms(aTail) / rms(bTail) - 1.0) < 0.001,
                  "footage-equivalent bodies retain matching audible release levels");
        }
}

void capturedLifetimeAndLiveTiming()
{
    for (int replacement : {body::off, body::organ, body::brass})
    {
        auto p = fixture(body::piano);
        auto changed = engine(p), reference = engine(p);
        changed->noteOn(36, 0.7f); reference->noteOn(36, 0.7f);
        check(render(*changed, 2400) == render(*reference, 2400), "lifetime fixture starts identically");
        const auto before = state(*changed, 36);
        p[chParam(0, CP_vD)] = 1.7f;
        reference->setParams(p);
        p[P_body1Model] = static_cast<float>(replacement);
        changed->setParams(p);
        const auto after = state(*changed, 36);
        check(before.value == after.value && before.stage == after.stage && before.elapsed == after.elapsed,
              "live timing updates preserve the current envelope value, stage and elapsed time");
        check(render(*changed, 4800) == render(*reference, 4800),
              "model replacement does not retime an existing body during live decay edits");
        changed->noteOff(36); reference->noteOff(36);
        check(render(*changed, 4800) == render(*reference, 4800),
              "release retains the captured body profile after model replacement");
    }

    // The neutral organ provides a same-ADSR control, independent of body
    // spectra. Inspect the realized envelope after rendering actual audio.
    auto p = fixture(body::piano), q = fixture(body::organ);
    auto body = engine(p), neutral = engine(q);
    body->noteOn(36, 0.7f); neutral->noteOn(36, 0.7f);
    (void) render(*body, 2400); (void) render(*neutral, 2400);
    p[chParam(0, CP_vD)] = q[chParam(0, CP_vD)] = 1.7f;
    body->setParams(p); neutral->setParams(q);
    const double bodyBefore = state(*body, 36).value, neutralBefore = state(*neutral, 36).value;
    (void) render(*body, 4800); (void) render(*neutral, 4800);
    const double bodySlope = -std::log(state(*body, 36).value / bodyBefore);
    const double neutralSlope = -std::log(state(*neutral, 36).value / neutralBefore);
    check(bodySlope < neutralSlope * 0.85,
          "live envelope timing refresh preserves the captured bass decay adjustment");
}

void performanceReleaseAndPedal()
{
    for (bool useMaster : {false, true})
    {
        auto p = fixture(body::piano);
        p[chParam(0, CP_vS)] = 1.0f;
        p[chParam(0, CP_vR)] = 0.12f;
        p[useMaster ? P_masterSustain : P_keyRelease] = useMaster ? 0.08f : 0.8f;
        auto bass = engine(p), treble = engine(p);
        bass->noteOn(36, 0.7f); treble->noteOn(84, 0.7f);
        (void) render(*bass, 2400); (void) render(*treble, 2400);
        bass->noteOff(36); treble->noteOff(84);
        (void) render(*bass, 4800); (void) render(*treble, 4800);
        check(state(*bass, 36).value == state(*treble, 84).value,
              "explicit key-release and master sustain overrides retain their full unscaled timing");
    }

    for (int note : {36, 84})
    {
        auto p = fixture(body::piano);
        p[chParam(0, CP_vS)] = 0.8f;
        auto pedal = engine(p), held = engine(p);
        pedal->noteOn(note, 0.7f); held->noteOn(note, 0.7f);
        check(render(*pedal, 2400) == render(*held, 2400), "pedal fixture starts identically");
        pedal->controlChange(64, 1.0f);
        pedal->noteOff(note);
        check(render(*pedal, 2400) == render(*held, 2400), "pedal hold does not begin body damping early");
        check(pedal->getVoiceInfo(voiceIndex(*pedal, note)).pedalHeld, "released key is held by CC64");
        pedal->controlChange(64, 0.0f);
        held->noteOff(note);
        check(render(*pedal, 4800) == render(*held, 4800),
              "pedal-up applies the captured key damping once, identically to direct release");
    }

    auto p = fixture(body::piano);
    p[chParam(0, CP_vS)] = 1.0f;
    p[P_masterSustain] = 0.2f;
    p[P_footswitchPlugged] = p[P_footswitchPressed] = p[P_sustainAssign] = 1.0f;
    auto bass = engine(p), treble = engine(p);
    bass->noteOn(36, 0.7f); treble->noteOn(84, 0.7f);
    (void) render(*bass, 2400); (void) render(*treble, 2400);
    bass->noteOff(36); treble->noteOff(84);
    (void) render(*bass, 2400); (void) render(*treble, 2400);
    const auto before = state(*bass, 36);
    p[P_footswitchPressed] = 0.0f;
    bass->setParams(p); treble->setParams(p);
    const auto after = state(*bass, 36);
    check(before.value == after.value && before.stage == after.stage && before.elapsed == after.elapsed,
          "cancelling a sustain tail keeps the envelope continuous");
    (void) render(*bass, 2400); (void) render(*treble, 2400);
    check(state(*bass, 36).value > state(*treble, 84).value * 1.2,
          "cancelling master sustain restores the captured register-specific release");
}

void isolationAndPartition()
{
    for (int note : {36, 84})
        for (int model : {body::piano, body::bowed})
        {
            auto zero = fixture(model), off = zero;
            zero[P_body1Mix] = off[P_body1Mix] = 0.0f;
            off[P_body1Model] = 0.0f;
            zero[chParam(0, CP_saw)] = off[chParam(0, CP_saw)] = 1.0f;
            auto a = engine(zero), b = engine(off);
            a->noteOn(note, 0.2f); b->noteOn(note, 0.2f);
            check(render(*a, 3000) == render(*b, 3000), "zero body mix retains exact circuit attack/decay");
            zero[chParam(0, CP_vD)] = off[chParam(0, CP_vD)] = 1.4f;
            a->setParams(zero); b->setParams(off);
            a->noteOff(note); b->noteOff(note);
            check(render(*a, 3000) == render(*b, 3000), "zero body mix retains exact circuit live timing/release");
        }
    for (int model : {body::piano, body::bowed})
    {
        const auto p = fixture(model);
        auto whole = engine(p), split = engine(p);
        whole->setFilterBatchingEnabled(false); split->setFilterBatchingEnabled(true);
        for (int note : {36, 48, 72, 84}) { whole->noteOn(note, 0.3f); split->noteOn(note, 0.3f); }
        check(render(*whole, 3200, 3200) == render(*split, 3200, 37),
              "captured profiles are independent of batching and host block partition");
        for (int note : {36, 48, 72, 84}) { whole->noteOff(note); split->noteOff(note); }
        check(render(*whole, 3200, 3200) == render(*split, 3200, 17),
              "release profiles are independent of batching and host block partition");
    }
    auto p = fixture(body::organ);
    auto soft = engine(p), firm = engine(p);
    soft->noteOn(36, 0.2f); firm->noteOn(36, 0.95f);
    check(render(*soft, 3000) == render(*firm, 3000), "organ keeps its velocity-independent registration and timing");
}
}

int main()
{
    struckRegister();
    expressiveAttack();
    registerFootageAndRates();
    capturedLifetimeAndLiveTiming();
    performanceReleaseAndPedal();
    isolationAndPartition();
    std::printf("Body articulation integration: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
