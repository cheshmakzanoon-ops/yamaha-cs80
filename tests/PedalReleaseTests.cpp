// Circuit CC64 tails obey the panel sustain policy; Legacy remains compatible.
// This is a MIDI/software interaction, not a measured hardware behavior.
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
void check(bool condition, const std::string& message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
ParamValues patch(bool circuit, int mode, bool gate)
{
    ParamValues p;
    p[P_sourcePerformance] = circuit ? 1.0f : 0.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_sustainMode] = static_cast<float>(mode);
    p[P_sustainAssign] = p[P_footswitchPlugged] = 1.0f;
    p[P_footswitchPressed] = gate ? 1.0f : 0.0f;
    p[P_masterSustain] = p[P_keyRelease] = 0.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_vA)] = p[chParam(rank, CP_fA)] = 0.001f;
        p[chParam(rank, CP_vS)] = 1.0f;
        p[chParam(rank, CP_vR)] = p[chParam(rank, CP_fR)] = 2.0f;
    }
    return p;
}
void init(Engine& engine, double rate, const ParamValues& p)
{
    engine.prepare(rate);
    engine.setParams(p);
}
std::vector<float> render(Engine& engine, int frames, int block = 0)
{
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    if (block <= 0) block = frames;
    for (int offset = 0; offset < frames; offset += block)
        engine.render(out.data() + offset * 2, std::min(block, frames - offset));
    check(std::all_of(out.begin(), out.end(), [](float value) { return std::isfinite(value); }),
          "pedal trace renders finite audio");
    return out;
}
int activeNote(const Engine& engine, int note, int channel)
{
    int count = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto voice = engine.getVoiceInfo(i);
        if (voice.active && voice.note == note && voice.channel == channel) ++count;
    }
    return count;
}
bool same(Engine::RankEnvelopeState a, Engine::RankEnvelopeState b)
{
    return a.amplitude.stage == b.amplitude.stage && a.amplitude.value == b.amplitude.value
        && a.amplitude.elapsed == b.amplitude.elapsed && a.filter.stage == b.filter.stage
        && a.filter.value == b.filter.value && a.filter.elapsed == b.filter.elapsed;
}
void testDeferredRelease()
{
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        for (bool circuit : {false, true})
            for (int mode : {0, 1})
                for (bool gate : {false, true})
                    for (bool notesOff : {false, true})
                    {
                        Engine engine; init(engine, rate, patch(circuit, mode, gate));
                        const auto row = std::to_string(static_cast<int>(rate))
                            + (circuit ? " circuit" : " legacy") + " mode " + std::to_string(mode)
                            + (gate ? " open" : " closed") + (notesOff ? " CC123" : " note-off");
                        engine.noteOn(60, 0.8f, 2);
                        render(engine, 512);
                        engine.controlChange(64, 1.0f, 2);
                        if (notesOff) engine.controlChange(123, 0.0f, 2);
                        else engine.noteOff(60, 2);
                        check(engine.getVoiceInfo(0).pedalHeld && !engine.getVoiceInfo(0).held,
                              row + " key release defers to pedal");
                        engine.noteOn(64, 0.7f, 3);
                        check(engine.getVoiceInfo(0).pedalHeld && activeNote(engine, 60, 2) == 1,
                              row + " new strike cannot cut a pedal-held voice");
                        engine.controlChange(64, 1.0f, 3);
                        engine.controlChange(64, 0.0f, 3);
                        check(engine.getVoiceInfo(0).pedalHeld, row + " unrelated pedal-up is inert");
                        render(engine, 256);
                        const auto held = engine.getRankEnvelopeState(1, 0);
                        const auto before = engine.getRankEnvelopeState(0, 0);
                        check(before.amplitude.value > 0.01f, row + " sounding pedal-held fixture");
                        engine.controlChange(64, 0.0f, 2);
                        const auto after = engine.getRankEnvelopeState(0, 0);
                        check(!engine.getVoiceInfo(0).pedalHeld && !engine.getVoiceInfo(0).held,
                              row + " pedal-up clears hold");
                        check(after.amplitude.stage == Adsr::Stage::Release
                                  && after.filter.stage == FilterEnv::Stage::Release,
                              row + " both envelopes enter release");
                        check(after.amplitude.value == before.amplitude.value
                                  && after.filter.value == before.filter.value,
                              row + " pedal-up is value-continuous");
                        check(same(held, engine.getRankEnvelopeState(1, 0)) && engine.getVoiceInfo(1).held,
                              row + " held key is unchanged");
                        render(engine, 128);
                        const auto tail = engine.getRankEnvelopeState(0, 0);
                        engine.controlChange(64, 0.0f, 2);
                        check(same(tail, engine.getRankEnvelopeState(0, 0)),
                              row + " repeated pedal-up cannot restart release");
                        engine.noteOn(67, 0.6f, 4);
                        check(activeNote(engine, 60, 2) == ((circuit && mode == 1 && gate) ? 0 : 1),
                              row + " deferred tail obeys its render-policy contract");
                        check(engine.getVoiceInfo(1).held && activeNote(engine, 64, 3) == 1,
                              row + " tail termination preserves held notes");
                    }
}
void testEditsWhileHeld()
{
    for (bool circuit : {false, true})
        for (int initialMode : {0, 1})
            for (int finalMode : {0, 1})
                for (bool initialGate : {false, true})
                    for (bool finalGate : {false, true})
                    {
                        auto p = patch(circuit, initialMode, initialGate);
                        Engine engine; init(engine, 48000.0, p);
                        engine.noteOn(60, 0.8f, 2); render(engine, 512);
                        engine.controlChange(64, 1.0f, 2); engine.noteOff(60, 2);
                        p[P_sustainMode] = static_cast<float>(finalMode);
                        p[P_footswitchPressed] = finalGate ? 1.0f : 0.0f;
                        engine.setParams(p);
                        check(engine.getVoiceInfo(0).pedalHeld, "panel edit preserves MIDI hold");
                        engine.controlChange(64, 0.0f, 2);
                        engine.noteOn(67, 0.8f, 3);
                        check(activeNote(engine, 60, 2) == ((circuit && finalMode == 1 && finalGate) ? 0 : 1),
                              "Circuit deferred release uses current mode/gate; Legacy keeps ordinary tails");
                    }
}
void testGateCancellationAndCompletion()
{
    for (bool circuit : {false, true})
    {
        auto p = patch(circuit, 1, true);
        Engine engine; init(engine, 48000.0, p);
        engine.noteOn(60, 0.8f, 2); render(engine, 512);
        engine.controlChange(64, 1.0f, 2); engine.noteOff(60, 2);
        engine.controlChange(64, 0.0f, 2);
        p[P_footswitchPressed] = 0.0f; engine.setParams(p);
        p[P_footswitchPressed] = 1.0f; engine.setParams(p);
        engine.noteOn(67, 0.8f, 3);
        check(activeNote(engine, 60, 2) == 1, "gate cancellation removes deferred tail privilege permanently");
        engine.allNotesOff(false);
        p = patch(circuit, 1, true);
        for (int rank = 0; rank < 2; ++rank)
            p[chParam(rank, CP_vR)] = p[chParam(rank, CP_fR)] = 0.01f;
        engine.setParams(p);
        engine.noteOn(60, 0.8f, 2); render(engine, 512);
        engine.controlChange(64, 1.0f, 2); engine.noteOff(60, 2);
        engine.controlChange(64, 0.0f, 2); render(engine, 24000);
        check(engine.activeVoices() == 0, "deferred Sustain II release completes without another strike");
        engine.controlChange(64, 1.0f, 2); engine.controlChange(64, 0.0f, 2);
        check(engine.activeVoices() == 0, "later pedal gestures cannot resurrect completed tails");
    }
}
void testChordAndIndependentPedal()
{
    for (bool circuit : {false, true})
    {
        auto p = patch(circuit, 1, true);
        p[P_sustainAssign] = p[P_footswitchPlugged] = p[P_footswitchPressed] = 0.0f;
        Engine engine; init(engine, 48000.0, p);
        for (int note : {60, 62, 64}) engine.noteOn(note, 0.8f, 2);
        engine.noteOn(70, 0.8f, 3); render(engine, 1024);
        engine.controlChange(64, 1.0f, 2); engine.controlChange(64, 1.0f, 3);
        for (int note : {60, 62, 64}) engine.noteOff(note, 2);
        engine.noteOff(70, 3);
        std::array<Engine::RankEnvelopeState, 6> before;
        for (int voice = 0; voice < 3; ++voice)
            for (int rank = 0; rank < 2; ++rank)
                before[voice * 2 + rank] = engine.getRankEnvelopeState(voice, rank);
        engine.controlChange(64, 0.0f, 2);
        for (int voice = 0; voice < 3; ++voice)
            for (int rank = 0; rank < 2; ++rank)
            {
                const auto after = engine.getRankEnvelopeState(voice, rank);
                const auto prior = before[voice * 2 + rank];
                check(after.amplitude.value == prior.amplitude.value && after.filter.value == prior.filter.value,
                      "all chord ranks release continuously");
                check(after.amplitude.stage == Adsr::Stage::Release && after.filter.stage == FilterEnv::Stage::Release,
                      "all chord ranks enter release");
            }
        engine.noteOn(75, 0.8f, 4);
        for (int note : {60, 62, 64})
            check(activeNote(engine, note, 2) == (circuit ? 0 : 1),
                  "default no-plug gate handles every pedal chord tail");
        check(activeNote(engine, 70, 3) == 1 && engine.getVoiceInfo(3).pedalHeld,
              "another channel pedal-held voice survives tail termination");
    }
}
void testBlockPartitionAndIsolation()
{
    for (bool circuit : {false, true})
    {
        Engine a, b;
        for (auto* engine : {&a, &b}) init(*engine, 48000.0, patch(circuit, 1, true));
        for (auto* engine : {&a, &b}) engine->noteOn(60, 0.8f, 2);
        check(render(a, 513) == render(b, 513, 17), "initial render is partition independent");
        for (auto* engine : {&a, &b})
        { engine->controlChange(64, 1.0f, 2); engine->noteOff(60, 2); }
        check(render(a, 257) == render(b, 257, 19), "pedal hold is partition independent");
        for (auto* engine : {&a, &b}) engine->controlChange(64, 0.0f, 2);
        check(render(a, 129) == render(b, 129, 7), "pedal release is partition independent");
        for (auto* engine : {&a, &b}) engine->noteOn(67, 0.8f, 3);
        check(render(a, 513) == render(b, 513, 23), "tail termination is partition independent");
    }
    auto p = patch(true, 1, true); p[P_parityMode] = 1.0f;
    Engine engine; init(engine, 48000.0, p);
    engine.noteOn(60, 0.8f, 2); render(engine, 512);
    engine.controlChange(64, 1.0f, 2); engine.noteOff(60, 2);
    check(!engine.getVoiceInfo(0).pedalHeld, "isolation still ignores CC64");
    engine.noteOn(67, 0.8f, 3);
    check(activeNote(engine, 60, 2) == 0, "isolation retains ordinary Sustain II release");
}
}
int main()
{
    testDeferredRelease();
    testEditsWhileHeld();
    testGateCancellationAndCompletion();
    testChordAndIndependentPedal();
    testBlockPartitionAndIsolation();
    std::printf("Pedal release: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
