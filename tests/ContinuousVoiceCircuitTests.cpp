#include "experimental/ContinuousVoiceCircuit.h"

#include <bit>
#include <cstdio>
#include <limits>
#include <vector>

using cs80::experimental::ContinuousVoiceCircuit;
using Circuit = ContinuousVoiceCircuit;
namespace
{
int checks = 0, failures = 0;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}
bool same(float a, float b)
{ return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }
bool same(cs80::Svf::State a, cs80::Svf::State b)
{ return same(a.ic1, b.ic1) && same(a.ic2, b.ic2); }
bool same(const Circuit::State& a, const Circuit::State& b)
{
    if (a.prepared != b.prepared || !same(a.sampleRate, b.sampleRate)
        || !same(a.heldKeyVolts, b.heldKeyVolts) || a.samples != b.samples) return false;
    for (int rank = 0; rank < 2; ++rank)
        if (!same(a.ranks[rank].phase, b.ranks[rank].phase)
            || !same(a.ranks[rank].hpf, b.ranks[rank].hpf)
            || !same(a.ranks[rank].lpf, b.ranks[rank].lpf)) return false;
    return true;
}
Circuit::Controls controls()
{
    Circuit::Controls c;
    for (auto& r : c.ranks)
    {
        r.pulseEnabled = true; r.pulseWidth = .37f;
        r.hpfEffectiveCv = 3.0f; r.lpfEffectiveCv = 6.0f;
        r.hpfQ = 1.2f; r.lpfQ = 2.4f;
        r.filterLevel = .7f; r.sineLevel = .3f; r.outputGain = .2f;
    }
    c.ranks[1].vcoMultiplier = 1.5f;
    return c;
}
void truthTable(double rate)
{
    for (bool tr : {false, true}) for (bool sus : {false, true})
    {
        Circuit c;
        check(c.prepare(rate, .5f, {.13f, .71f}), "explicit cold setup accepted");
        auto input = controls(); input.trHigh = tr; input.susHigh = sus;
        input.keyInputVolts = 1.0f;
        const auto out = c.processSample(input);
        const float expected = tr && sus ? .5f : 1.0f;
        check(out.accepted && out.heldKeyVolts == expected,
              "electrical NAND LL/LH/HL tracks and HH holds");
        check(out.ranks[0].oscillatorHz == expected * input.ranks[0].hzPerVolt,
              "held key drives first-rank frequency");
        check(out.ranks[1].oscillatorHz == expected * input.ranks[1].hzPerVolt * 1.5f,
              "both ranks share key but retain independent transposition");
        check(out.ranks[0].hpfHz == out.ranks[1].hpfHz,
              "VCO transposition does not move common filter key voltage");
    }
    Circuit c;
    check(c.prepare(rate, .25f, {.13f, .71f}), "hold fixture prepared");
    auto input = controls(); input.trHigh = input.susHigh = true;
    for (int sample = 0; sample < 1000; ++sample)
    {
        input.keyInputVolts = sample % 2 ? 4.0f : .125f;
        check(c.processSample(input).heldKeyVolts == .25f,
              "ideal hold retains explicit cold K despite moving disconnected KI");
    }
    input.susHigh = false;
    check(c.processSample(input).heldKeyVolts == input.keyInputVolts,
          "reopening tracking acquires current KI, not a global last-note variable");
}

// Independent assembly of the unchanged production primitive calls. The
// oscillator and nonlinear filter laws are not reimplemented or requalified.
void directPrimitiveOracle(double rate)
{
    Circuit c;
    c.prepare(rate, .5f, {.13f, .71f});
    std::array<cs80::Oscillator, 2> osc;
    std::array<cs80::Svf, 2> hp, lp;
    cs80::Rng unused(9123);
    osc[0].reset(.13f); osc[1].reset(.71f);
    auto input = controls();
    float key = .5f;
    for (int sample = 0; sample < 4096; ++sample)
    {
        input.trHigh = sample % 503 > 173;
        input.susHigh = sample % 911 > 79;
        input.keyInputVolts = sample < 701 ? .5f : sample < 2107 ? 2.0f : .125f;
        if (!(input.trHigh && input.susHigh)) key = input.keyInputVolts;
        for (int rank = 0; rank < 2; ++rank)
        {
            input.ranks[rank].hpfEnabled = sample % 317 < 211;
            input.ranks[rank].lpfEnabled = sample % 431 < 157;
            input.ranks[rank].noiseInput = static_cast<float>((sample + rank) % 17 - 8) * .01f;
        }
        const auto actual = c.processSample(input);
        check(actual.accepted, "valid dynamic primitive fixture accepted");
        for (int rank = 0; rank < 2; ++rank)
        {
            const auto& r = input.ranks[rank];
            const auto o = osc[rank].tick(key * r.hzPerVolt * r.vcoMultiplier,
                static_cast<float>(rate), r.pulseWidth, r.sawEnabled, r.pulseEnabled, 0, unused);
            const float x = (o.saw + o.pulse + r.noiseInput) * .9f;
            hp[rank].set(cs80::nominalFilterCutoff(key, r.hpfEffectiveCv), r.hpfQ, static_cast<float>(rate));
            const float high = hp[rank].process(x).hp;
            const float lowInput = r.hpfEnabled ? high : x;
            lp[rank].set(cs80::nominalFilterCutoff(key, r.lpfEffectiveCv), r.lpfQ, static_cast<float>(rate));
            const float low = lp[rank].process(lowInput).lp;
            const float filtered = r.lpfEnabled ? low : lowInput;
            const float expected = (filtered * r.filterLevel + o.sine * r.sineLevel) * r.outputGain;
            check(same(actual.ranks[rank].audio, expected), "dynamic output matches direct production primitive oracle");
            const auto state = c.getState();
            check(same(state.ranks[rank].phase, osc[rank].getPhase())
                  && same(state.ranks[rank].hpf, hp[rank].getState())
                  && same(state.ranks[rank].lpf, lp[rank].getState()),
                  "phase and both filter memories match continuously clocked oracle");
        }
    }
}
void silentGapAndEnvelopes(double rate)
{
    Circuit audible, muted;
    audible.prepare(rate, .5f, {.13f, .71f}); muted.prepare(rate, .5f, {.13f, .71f});
    auto input = controls();
    cs80::Adsr amp;
    cs80::FilterEnv filter;
    amp.noteOn(.001f, .01f, 1, .001f);
    filter.noteOn(-3, 3, .01f, .1f, 2, 0);
    float atMute = 0;
    bool changedInSilence = false;
    double reopenedAudio = 0;
    for (int sample = 0; sample < 8192; ++sample)
    {
        if (sample == 1024) { amp.noteOff(); filter.noteOff(); }
        // Caller owns exactly one tick per physical envelope/interval. Kernel
        // has no EG object and cannot double-tick source TP1.
        const float a = amp.tick(static_cast<float>(rate), true, true);
        const float f = filter.tick(static_cast<float>(rate), true, true);
        // The reference is an intentionally audible pre-VCA monitor; keeping
        // it nonzero makes the silent-gap/reopening comparison non-vacuous.
        // The separate amplitude EG below establishes the control-tail fixture.
        for (auto& r : input.ranks) { r.lpfEffectiveCv = 6 + f; r.outputGain = .2f; }
        if (sample == 2048) atMute = f;
        if (sample > 2048 && a == 0 && f != atMute) changedInSilence = true;
        const auto reference = audible.processSample(input);
        auto silent = input;
        if (sample >= 2048 && sample < 7000)
            for (auto& r : silent.ranks) r.outputGain = 0;
        const auto actual = muted.processSample(silent);
        check(same(audible.getState(), muted.getState()),
              "audible gain and long silent gap never alter phase/filter/key histories");
        if (sample >= 2048 && sample < 7000)
            check(actual.ranks[0].audio == 0 && actual.ranks[1].audio == 0,
                  "silent audible gate does not resurrect notes");
        else
            check(same(actual.ranks[0].audio, reference.ranks[0].audio)
                  && same(actual.ranks[1].audio, reference.ranks[1].audio),
                  "same output after reopening a continuously clocked path");
        if (sample >= 7000) reopenedAudio += std::abs(actual.ranks[0].audio) + std::abs(actual.ranks[1].audio);
    }
    check(changedInSilence, "caller-owned filter envelope relaxes after amplitude reaches idle");
    check(reopenedAudio > 1e-4, "reopened monitor is audibly nonzero, not a vacuous zero comparison");
    check(muted.getState().samples == 8192, "one circuit interval per caller interval");
}
void invalidTransactions()
{
    Circuit c;
    check(!c.processSample(controls()).accepted && !c.getState().prepared,
          "unprepared process is non-mutating rejection");
    c.prepare(96000, .5f, {.13f, .71f}); c.processSample(controls());
    const auto before = c.getState();
    for (double rate : {0., 15999., 1536001., std::numeric_limits<double>::infinity()})
        check(!c.prepare(rate, .5f, {0, 0}) && same(before, c.getState()), "invalid rate preserves all state");
    for (float key : {-1.f, 1025.f, std::numeric_limits<float>::quiet_NaN()})
    {
        check(!c.prepare(96000, key, {0, 0}) && same(before, c.getState()), "invalid cold K preserves state");
        auto input = controls(); input.keyInputVolts = key;
        check(!c.processSample(input).accepted && same(before, c.getState()), "invalid live KI preserves state");
    }
    for (int member = 0; member < 11; ++member)
    {
        auto input = controls(); input.keyInputVolts = 2;
        auto& r = input.ranks[1];
        float* fields[] = {&r.hzPerVolt, &r.vcoMultiplier, &r.pulseWidth, &r.noiseInput,
            &r.hpfEffectiveCv, &r.lpfEffectiveCv, &r.hpfQ, &r.lpfQ,
            &r.filterLevel, &r.sineLevel, &r.outputGain};
        *fields[member] = std::numeric_limits<float>::quiet_NaN();
        check(!c.processSample(input).accepted && same(before, c.getState()),
              "invalid second-rank field cannot partly advance key/first rank");
    }
}
}
int main()
{
    invalidTransactions();
    for (double hostRate : {44100., 48000., 96000., 192000.})
    {
        truthTable(hostRate * 2);
        directPrimitiveOracle(hostRate * 2);
        silentGapAndEnvelopes(hostRate * 2);
    }
    std::printf("Continuous voice circuit: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
