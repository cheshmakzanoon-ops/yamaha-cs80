// MIDI channel/note identity belongs to the controller interface, including
// ordinary non-MPE and PURE CS-80 input. No hardware-fidelity claim is made.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace cs80;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}

ParamValues controls(int policy, bool mpe, bool curved)
{
    ParamValues p;
    p[P_sourcePerformance] = policy == 0 ? 0.0f : 1.0f;
    p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    p[P_mpeOn] = mpe ? 1.0f : 0.0f;
    p[P_pressureCurve] = curved ? -0.5f : 0.0f;
    p[P_pressureThreshold] = curved ? 0.2f : 0.0f;
    p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_afterLevel)] = 0.8f;
        p[chParam(rank, CP_afterBrill)] = 0.6f;
    }
    return p;
}

float mapped(float pressure, bool curved)
{
    // Independent oracle for curve -0.5 and threshold 0.2: square the
    // normalized remainder. Default is identity; both clamp before mapping.
    pressure = std::clamp(pressure, 0.0f, 1.0f);
    if (!curved) return pressure;
    const float scaled = std::max(0.0f, pressure - 0.2f) / 0.8f;
    return scaled * scaled;
}

void checkVoices(const Engine& e, std::array<float, 4> expected, const char* label)
{
    for (int voice = 0; voice < 4; ++voice)
    {
        const auto info = e.getVoiceInfo(voice);
        check(info.active && std::abs(info.pressure - expected[(size_t) voice]) < 1.0e-6f, label);
    }
}

void testIdentity(int policy, bool mpe, bool curved)
{
    std::printf("pressure routing: policy %d, MPE %d, curve %d\n", policy, mpe, curved);
    Engine e;
    e.prepare(48000.0);
    auto p = controls(policy, mpe, curved);
    e.setParams(p);
    e.noteOn(60, 0.8f, 2);
    e.noteOn(60, 0.8f, 3);
    e.noteOn(64, 0.8f, 2);
    e.noteOn(60, 0.8f, 2); // Same channel/note: every matching onset responds.
    std::array<float, 4> expected {};
    for (const float pressure : { 0.8f, 0.0f, 2.0f, -1.0f, 0.5f })
    {
        e.polyPressure(60, pressure, 2);
        expected[0] = expected[3] = mapped(pressure, curved);
        checkVoices(e, expected, "poly pressure reaches only the addressed channel and note");
    }
    e.polyPressure(60, 0.7f, 3);
    expected[1] = mapped(0.7f, curved);
    checkVoices(e, expected, "the other channel can hold an independent pressure");
    e.polyPressure(60, 1.0f, 9);
    e.polyPressure(72, 1.0f, 2);
    e.polyPressure(60, std::numeric_limits<float>::quiet_NaN(), 2);
    e.polyPressure(60, std::numeric_limits<float>::infinity(), 2);
    checkVoices(e, expected, "unmatched or malformed pressure cannot alter voices");
    e.polyPressure(60, 0.6f); // The explicit application broadcast stays available.
    expected[0] = expected[1] = expected[3] = mapped(0.6f, curved);
    checkVoices(e, expected, "channel-zero broadcast reaches matching notes on every channel");

    p[P_mpeOn] = mpe ? 0.0f : 1.0f;
    e.setParams(p);
    e.polyPressure(60, 0.1f, 2);
    expected[0] = expected[3] = mapped(0.1f, curved);
    checkVoices(e, expected, "changing MPE mode does not change MIDI note identity");
}

void testUnison(int policy, bool mpe)
{
    Engine e;
    e.prepare(48000.0);
    auto p = controls(policy, mpe, false);
    p[P_voiceMode] = 2.0f; // Two circuits per note outside PURE CS-80.
    e.setParams(p);
    e.noteOn(60, 0.8f, 2);
    e.noteOn(60, 0.8f, 3);
    e.polyPressure(60, 0.9f, 2);
    int targetCount = 0, otherCount = 0;
    for (int voice = 0; voice < 8; ++voice)
    {
        const auto info = e.getVoiceInfo(voice);
        if (!info.active) continue;
        if (info.channel == 2)
        {
            ++targetCount;
            check(info.pressure == 0.9f, "all addressed unison circuits receive pressure");
        }
        else if (info.channel == 3)
        {
            ++otherCount;
            check(info.pressure == 0.0f, "the other channel's unison circuits remain untouched");
        }
    }
    check(targetCount == 2 && otherCount == 2, "unison fixture allocated both circuits per channel");
}

void testAudioRouting(int policy, bool mpe, bool curved)
{
    Engine actual, reference;
    const auto p = controls(policy, mpe, curved);
    for (Engine* e : { &actual, &reference })
    {
        e->prepare(48000.0);
        e->setParams(p);
        e->noteOn(60, 0.8f, 2);
        e->noteOn(60, 0.8f, 3);
    }
    std::array<float, 256> a {}, b {};
    bool hasSound = false;
    // With exactly one note per member channel and no channel-pressure matrix
    // routing, its existing channel-pressure path is an independent oracle.
    // Exercise onset, later smoothing, pressure release, and release tails.
    for (const float pressure : { 0.6f, 0.9f, 0.0f, 0.4f })
    {
        actual.polyPressure(60, pressure, 2);
        reference.channelPressure(2, pressure);
        for (int block = 0; block < 12; ++block)
        {
            actual.render(a.data(), 128);
            reference.render(b.data(), 128);
            check(a == b, "poly pressure audio matches the channel-scoped reference exactly");
            check(std::all_of(a.begin(), a.end(), [](float value) { return std::isfinite(value); }),
                  "pressure routing produces finite audio");
            hasSound = hasSound || std::any_of(a.begin(), a.end(), [](float value) { return value != 0.0f; });
        }
        actual.noteOff(60, 2);
        reference.noteOff(60, 2);
    }
    check(hasSound, "audio comparison covers sounding voices, not silence");
}
} // namespace

int main()
{
    for (int policy = 0; policy < 3; ++policy)
        for (bool mpe : { false, true })
            for (bool curved : { false, true })
            {
                testIdentity(policy, mpe, curved);
                testAudioRouting(policy, mpe, curved);
            }
    for (int policy = 0; policy < 2; ++policy)
        for (bool mpe : { false, true })
            testUnison(policy, mpe);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
