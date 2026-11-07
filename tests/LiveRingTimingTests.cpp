// Engineering-inferred live ring AD timing, not a measured PRA transition law.
// The independent recurrence below pins the existing source-policy envelope;
// this test changes only which accepted timing controls reach that recurrence.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok)
    {
        if (failures < 24) std::fprintf(stderr, "FAIL: %s\n", label.c_str());
        ++failures;
    }
}
bool same(Adsr::State a, Adsr::State b)
{
    return a.stage == b.stage && a.value == b.value && a.elapsed == b.elapsed;
}
ParamValues patch(int policy, int quality = 0)
{
    ParamValues p;
    p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
    p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    p[P_oversampling] = static_cast<float>(quality);
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_ringAttack] = 0.5f; p[P_ringDecay] = 0.8f;
    p[P_ringSpeed] = 20.0f; p[P_ringDepth] = p[P_ringAmount] = 0.8f;
    return p;
}
void init(Engine& e, const ParamValues& p, double rate)
{
    e.setParams(p); e.prepare(rate); e.noteOn(60, 0.8f);
}
std::vector<float> render(Engine& e, int frames, int block = 257, const ParamValues* repeated = nullptr)
{
    std::vector<float> audio(static_cast<std::size_t>(frames) * 2);
    for (int n = 0; n < frames; n += block)
    {
        if (repeated) e.setParams(*repeated);
        e.render(audio.data() + n * 2, std::min(block, frames - n));
    }
    check(std::all_of(audio.begin(), audio.end(), [](float x) { return std::isfinite(x); }),
          "ring timing audio remains finite");
    return audio;
}
// Express the pre-existing reconstruction law independently of Adsr and its
// coefficient cache. The phrase starts at zero and attack targets 1.5.
struct Oracle
{
    Adsr::Stage stage = Adsr::Stage::Attack;
    double value = 0.0, elapsed = 0.0;
    float attack = 0.5f, decay = 0.8f;
    void advance(int samples, double rate)
    {
        for (int n = 0; n < samples; ++n)
        {
            elapsed += 1.0 / rate;
            if (stage == Adsr::Stage::Attack)
            {
                value = 1.5 + (value - 1.5) * std::exp(-std::log(3.0) / (attack * rate));
                if (value >= 1.0) { value = 1.0; elapsed = 0.0; stage = Adsr::Stage::Decay; }
            }
            else if (stage == Adsr::Stage::Decay)
            {
                value *= std::exp(-std::log(1000.0) / (decay * rate));
                if (std::abs(value) < 0.0005f) { value = 0.0; stage = Adsr::Stage::Sustain; }
            }
        }
    }
};
void testCurrentStage()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int policy : {0, 1, 2})
            for (int quality : {0, 2})
                for (bool attack : {false, true})
                    for (float newTime : {0.025f, 4.0f})
                    {
                        const auto label = std::to_string(static_cast<int>(rate)) + " policy="
                            + std::to_string(policy) + " quality=" + std::to_string(quality)
                            + (attack ? " attack=" : " decay=") + std::to_string(newTime);
                        auto p = patch(policy, quality);
                        if (!attack) p[P_ringAttack] = 0.001f;
                        Engine edited, control;
                        init(edited, p, rate); init(control, p, rate);
                        Oracle oracle; oracle.attack = p[P_ringAttack]; oracle.decay = p[P_ringDecay];
                        const int factor = oversamplingFactor(oversamplingMode(p[P_oversampling]), policy != 0, false, rate);
                        const int beforeFrames = static_cast<int>(rate * 0.02);
                        render(edited, beforeFrames); render(control, beforeFrames);
                        oracle.advance(beforeFrames * factor, rate * factor);
                        const auto before = edited.getRingEnvelopeState();
                        check(before.stage == (attack ? Adsr::Stage::Attack : Adsr::Stage::Decay), label + " stage fixture");
                        const auto rank = edited.getRankEnvelopeState(0, 0);
                        const auto voice = edited.getVoiceInfo(0);
                        const auto circuit = edited.getRankAudioState(0, 0);
                        const float frequency = edited.getRingFrequency();
                        p[attack ? P_ringAttack : P_ringDecay] = newTime;
                        edited.setParams(p);
                        check(same(before, edited.getRingEnvelopeState()), label + " setter preserves stage, value and elapsed time");
                        check(edited.getRingFrequency() == frequency, label + " setter does not clock carrier");
                        const auto afterVoice = edited.getVoiceInfo(0);
                        const auto afterCircuit = edited.getRankAudioState(0, 0);
                        check(voice.note == afterVoice.note && voice.channel == afterVoice.channel
                                  && voice.held == afterVoice.held && voice.active == afterVoice.active,
                              label + " setter preserves voice identity");
                        check(same(rank.amplitude, edited.getRankEnvelopeState(0, 0).amplitude)
                                  && circuit.phase == afterCircuit.phase && circuit.hpf.ic1 == afterCircuit.hpf.ic1
                                  && circuit.hpf.ic2 == afterCircuit.hpf.ic2 && circuit.lpf.ic1 == afterCircuit.lpf.ic1
                                  && circuit.lpf.ic2 == afterCircuit.lpf.ic2,
                              label + " setter preserves unrelated rank state");
                        if (policy != 0)
                        {
                            oracle.attack = p[P_ringAttack]; oracle.decay = p[P_ringDecay];
                        }
                        float frame[2];
                        bool followsLaw = true;
                        for (int n = 0; n < 31; ++n)
                        {
                            edited.render(frame, 1); oracle.advance(factor, rate * factor);
                            if (policy != 0)
                            {
                                const auto state = edited.getRingEnvelopeState();
                                followsLaw = followsLaw && state.stage == oracle.stage
                                    && std::abs(state.value - oracle.value) < 2.0e-6
                                    && std::abs(state.elapsed - oracle.elapsed) < 2.0e-6;
                            }
                        }
                        check(followsLaw, label + " next samples follow independently evaluated live law");
                        render(control, 31);
                        const auto changedAudio = render(edited, static_cast<int>(rate * 0.04));
                        const auto originalAudio = render(control, static_cast<int>(rate * 0.04));
                        if (policy == 0)
                            check(changedAudio == originalAudio && same(edited.getRingEnvelopeState(), control.getRingEnvelopeState()),
                                  label + " legacy latching stays bit-identical");
                        else
                        {
                            const float changed = edited.getRingEnvelope(), original = control.getRingEnvelope();
                            check(attack ? (newTime < 0.5f ? changed > original : changed < original)
                                         : (newTime < 0.8f ? changed < original : changed > original),
                                  label + " edit changes ongoing stage in expected direction");
                            check(changedAudio != originalAudio, label + " live timing reaches audio output");
                        }
                    }
}
void testPendingDecayAndPhrase()
{
    for (int policy : {0, 1, 2})
    {
        auto p = patch(policy); p[P_ringAttack] = 0.03f;
        Engine e; init(e, p, 48000.0);
        render(e, 240);
        p[P_ringDecay] = 0.02f; e.setParams(p);
        Oracle oracle; oracle.attack = 0.03f; oracle.decay = policy == 0 ? 0.8f : 0.02f;
        oracle.advance(3072, 96000.0);
        render(e, 1296);
        if (policy != 0)
            check(e.getRingEnvelopeState().stage == Adsr::Stage::Decay && oracle.value > 0.1
                      && std::abs(e.getRingEnvelope() - oracle.value) < 2.0e-6
                      && std::abs(e.getRingEnvelopeState().elapsed - oracle.elapsed) < 2.0e-6,
                  "decay selected during attack reaches nonzero later decay with correct elapsed time");
        const auto before = e.getRingEnvelopeState();
        e.noteOn(64, 0.7f);
        check(same(before, e.getRingEnvelopeState()), "legato note does not restart edited ring envelope");
        e.allNotesOff(false); e.noteOn(67, 0.8f);
        check(e.getRingEnvelopeState().value == 0.0f && e.getRingEnvelopeState().elapsed == 0.0f,
              "new phrase restarts from zero");
        Engine fresh; init(fresh, p, 48000.0);
        render(e, 1800); render(fresh, 1800);
        check(same(e.getRingEnvelopeState(), fresh.getRingEnvelopeState()), "next phrase uses current timing in every policy");
    }
}
void testEditedStageBoundaries()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int policy : {1, 2})
            for (int quality : {0, 2})
                for (bool attack : {false, true})
                {
                    auto p = patch(policy, quality);
                    p[P_ringAttack] = attack ? 0.03f : 0.001f;
                    p[P_ringDecay] = 0.02f;
                    Engine e; init(e, p, rate);
                    const int factor = oversamplingFactor(oversamplingMode(p[P_oversampling]), true, false, rate);
                    Oracle oracle; oracle.attack = p[P_ringAttack]; oracle.decay = p[P_ringDecay];
                    const int frames = static_cast<int>(rate * (attack ? 0.02 : 0.01));
                    render(e, frames); oracle.advance(frames * factor, rate * factor);
                    const auto oldStage = oracle.stage;
                    check(oldStage == (attack ? Adsr::Stage::Attack : Adsr::Stage::Decay), "boundary edit stage fixture");
                    p[attack ? P_ringAttack : P_ringDecay] = attack ? 0.01f : 0.001f;
                    e.setParams(p); oracle.attack = p[P_ringAttack]; oracle.decay = p[P_ringDecay];
                    bool sameHistory = true, crossed = false, followed = false;
                    float frame[2];
                    for (int n = 0; n < static_cast<int>(rate * 0.02); ++n)
                    {
                        e.render(frame, 1); oracle.advance(factor, rate * factor);
                        const auto state = e.getRingEnvelopeState();
                        sameHistory = sameHistory && state.stage == oracle.stage
                            && std::abs(state.value - oracle.value) < 2.0e-6
                            && std::abs(state.elapsed - oracle.elapsed) < 2.0e-6;
                        if (crossed) { followed = true; break; }
                        crossed = oracle.stage != oldStage;
                    }
                    check(sameHistory && crossed && followed, "edited recurrence matches stage boundary and following sample");
                }
}
void testReleaseIdleAndRepeatedSetters()
{
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        for (int policy : {0, 1, 2})
        {
            auto p = patch(policy); p[P_ringAttack] = 0.02f;
            Engine whole, split;
            init(whole, p, rate); init(split, p, rate);
            const int frames = static_cast<int>(rate * 0.04) + 17;
            check(render(whole, frames) == render(split, frames, 31, &p), "unchanged setters are audio-identical");
            p[P_ringAttack] = 10.0f; p[P_ringDecay] = 0.001f;
            whole.setParams(p); split.setParams(p);
            check(render(whole, frames) == render(split, frames, 17, &p), "edited timing is block-partition invariant");
            check(same(whole.getRingEnvelopeState(), split.getRingEnvelopeState()), "partitions retain exact ring state");

            // A held completed AD remains at zero Sustain, not a fresh phrase.
            auto fast = p; fast[P_ringAttack] = fast[P_ringDecay] = 0.001f;
            for (auto* e : {&whole, &split})
            {
                e->reset(); e->setParams(fast); e->noteOn(60, 0.8f); render(*e, frames);
            }
            const auto heldZero = whole.getRingEnvelopeState();
            check(heldZero.stage == Adsr::Stage::Sustain && heldZero.value == 0.0f, "held zero-sustain fixture");
            whole.setParams(p);
            check(same(heldZero, whole.getRingEnvelopeState()), "timing edits preserve held zero sustain at boundary");
            check(render(whole, 128) == render(split, 128) && whole.getRingEnvelopeState().stage == Adsr::Stage::Sustain,
                  "timing edits do not revive a held completed AD");
            split.setParams(p);

            // Start a fresh attack then release it: A/D edits must never replace
            // the fixed .35-second release, retrigger it or revive an idle EG.
            for (auto* e : {&whole, &split})
            {
                e->reset(); e->noteOn(60, 0.8f); render(*e, frames); e->noteOff(60);
            }
            const auto before = whole.getRingEnvelopeState();
            check(before.stage == Adsr::Stage::Release, "ring release fixture");
            auto edited = p; edited[P_ringAttack] = 0.001f; edited[P_ringDecay] = 10.0f;
            whole.setParams(edited);
            check(same(before, whole.getRingEnvelopeState()), "release edit preserves exact state");
            check(render(whole, frames) == render(split, frames), "A/D edits do not alter fixed release audio");
            render(whole, static_cast<int>(rate * 0.5)); render(split, static_cast<int>(rate * 0.5));
            check(whole.getRingEnvelopeState().stage == Adsr::Stage::Idle, "ring idle fixture");
            whole.setParams(p);
            check(render(whole, 128) == render(split, 128) && whole.getRingEnvelopeState().stage == Adsr::Stage::Idle,
                  "idle timing edits do not resurrect envelope or audio");
        }
}
}
int main()
{
    testCurrentStage(); testPendingDecayAndPhrase(); testEditedStageBoundaries(); testReleaseIdleAndRepeatedSetters();
    std::printf("Live ring timing: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
