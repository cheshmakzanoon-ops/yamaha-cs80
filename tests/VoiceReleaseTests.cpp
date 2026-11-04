// Voice-mode edits must not change ownership of already allocated circuits.
// This is a MIDI/software lifecycle contract, not a CS-80 hardware claim.
#include "core/Engine.h"

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
int countHeld(const Engine& engine, int note = -1, int channel = -1, bool pedal = false)
{
    int count = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto voice = engine.getVoiceInfo(i);
        if (voice.active && (pedal ? voice.pedalHeld : voice.held)
            && (note < 0 || voice.note == note) && (channel < 0 || voice.channel == channel))
            ++count;
    }
    return count;
}
ParamValues patch(int mode, bool circuit)
{
    ParamValues p;
    p[P_sourcePerformance] = circuit ? 1.0f : 0.0f;
    p[P_voiceMode] = static_cast<float>(mode);
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_masterSustain] = p[P_keyRelease] = 0.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_vA)] = p[chParam(rank, CP_fA)] = 0.001f;
        p[chParam(rank, CP_vS)] = 1.0f;
        p[chParam(rank, CP_vR)] = p[chParam(rank, CP_fR)] = 0.01f;
    }
    return p;
}
void render(Engine& engine, int frames)
{
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    engine.render(out.data(), frames);
    bool finite = true;
    for (float sample : out) finite = finite && std::isfinite(sample);
    check(finite, "release render is finite");
}
int voicesFor(int mode) { return mode == 2 ? 2 : mode == 3 ? 4 : mode == 4 ? 8 : 1; }

void testModeTransitions()
{
    for (bool circuit : {false, true})
        for (int before = 0; before <= 4; ++before)
            for (int after = 0; after <= 4; ++after)
                for (bool pedal : {false, true})
                {
                    auto p = patch(before, circuit);
                    Engine engine; engine.prepare(48000.0); engine.setParams(p);
                    engine.noteOn(60, 0.8f, 2);
                    const int count = voicesFor(before);
                    const std::string row = "mode " + std::to_string(before) + " -> " + std::to_string(after)
                        + (circuit ? " circuit" : " legacy") + (pedal ? " pedal" : " key");
                    check(countHeld(engine) == count, row + " allocation");
                    p[P_voiceMode] = static_cast<float>(after); engine.setParams(p);
                    check(countHeld(engine) == count, row + " edit preserves held notes");
                    if (pedal) engine.controlChange(64, 1.0f, 2);
                    engine.noteOff(60, 2);
                    check(countHeld(engine) == 0, row + " releases every allocated circuit");
                    check(countHeld(engine, -1, -1, true) == (pedal ? count : 0), row + " pedal ownership");
                    if (pedal) engine.controlChange(64, 0.0f, 2);
                    check(countHeld(engine, -1, -1, true) == 0, row + " no pedal-held circuit after pedal-up");
                    for (int i = 0; i < count; ++i)
                        check(engine.getRankEnvelopeState(i, 0).amplitude.stage == Adsr::Stage::Release,
                              row + " amplitude release stage");
                }
}

void testOverlappingOnsets()
{
    for (bool circuit : {false, true})
        for (bool pedal : {false, true})
            for (int oldMode : {0, 2, 3})
                for (int newMode : {0, 2, 3})
                {
                    const int oldCount = voicesFor(oldMode), newCount = voicesFor(newMode);
                    if (oldCount + newCount > 8) continue;
                    auto p = patch(oldMode, circuit);
                    Engine engine; engine.prepare(48000.0); engine.setParams(p);
                    engine.noteOn(60, 0.3f, 2);
                    p[P_voiceMode] = static_cast<float>(newMode); engine.setParams(p);
                    engine.noteOn(60, 0.8f, 2);
                    if (pedal) engine.controlChange(64, 1.0f, 2);
                    engine.noteOff(60, 3);
                    check(countHeld(engine, 60, 2) == oldCount + newCount, "wrong-channel release is inert");
                    engine.noteOff(60, 2);
                    check(countHeld(engine, 60, 2) == oldCount, "one note-off releases only newest onset's circuits");
                    bool olderStillHeld = true;
                    for (int i = 0; i < 8; ++i)
                    {
                        const auto voice = engine.getVoiceInfo(i);
                        if (voice.held) olderStillHeld = olderStillHeld && voice.velocity == 0.3f;
                        if (voice.pedalHeld) olderStillHeld = olderStillHeld && voice.velocity == 0.8f;
                    }
                    check(olderStillHeld, "equal-sized groups preserve older onset identity");
                    check(countHeld(engine, 60, 2, true) == (pedal ? newCount : 0), "only newest onset enters pedal hold");
                    // A further edit cannot change the older onset's allocation.
                    p[P_voiceMode] = 0.0f; engine.setParams(p);
                    engine.noteOff(60, 2);
                    check(countHeld(engine) == 0, "second note-off releases older onset's circuits");
                    check(countHeld(engine, 60, 2, true) == (pedal ? oldCount + newCount : 0),
                          "pedal holds both released onsets");
                    engine.controlChange(64, 0.0f, 2);
                    check(countHeld(engine, -1, -1, true) == 0, "pedal-up releases both onset groups");
                }
}

void testPartialUnisonAndReuse()
{
    for (bool circuit : {false, true})
    {
        auto p = patch(0, circuit);
        Engine engine; engine.prepare(48000.0); engine.setParams(p);
        for (int n = 0; n < 5; ++n) engine.noteOn(40 + n, 0.8f, 3);
        p[P_voiceMode] = 3.0f; engine.setParams(p); // only three of four circuits fit
        engine.noteOn(60, 0.8f, 2);
        check(countHeld(engine, 60, 2) == 3, "partial unison allocation fixture");
        render(engine, 64);
        p[P_voiceMode] = 0.0f; engine.setParams(p);
        engine.noteOff(60, 2);
        check(countHeld(engine, 60, 2) == 0 && countHeld(engine, -1, 3) == 5,
              "partial unison releases without touching another channel");
        // Reused circuits get a new owner instead of retaining the unison group.
        engine.noteOn(60, 0.8f, 2); engine.noteOn(60, 0.7f, 2);
        p[P_voiceMode] = 2.0f; engine.setParams(p);
        engine.noteOff(60, 2);
        check(countHeld(engine, 60, 2) == 1, "reuse does not inherit prior unison ownership");
        engine.noteOff(60, 2); engine.allNotesOff(true, 3);
        render(engine, 24000);
        check(engine.activeVoices() == 0, "released groups finish their audio tails");
        engine.reset(); engine.noteOn(64, 0.8f, 2); engine.noteOff(64, 2);
        check(countHeld(engine) == 0, "reset leaves new onset release ownership clean");
    }
}
}
int main()
{
    testModeTransitions();
    testOverlappingOnsets();
    testPartialUnisonAndReuse();
    std::printf("Voice release: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
