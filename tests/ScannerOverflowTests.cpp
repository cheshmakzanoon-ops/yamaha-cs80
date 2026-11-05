// Bounded KAS queue overload is a MIDI adapter policy, not a hardware claim.
#include "core/Engine.h"
#include "core/KasScanner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}
void render(Engine& engine, int frames)
{
    std::array<float, 256> audio {};
    while (frames > 0)
    {
        const int count = std::min(frames, 128);
        engine.render(audio.data(), count);
        check(std::all_of(audio.begin(), audio.begin() + count * 2,
                         [](float value) { return std::isfinite(value); }), "finite overflow audio");
        frames -= count;
    }
}
int held(const Engine& engine, int note = -1, int channel = -1, float velocity = -1.0f)
{
    int count = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto voice = engine.getVoiceInfo(i);
        count += voice.held && (note < 0 || voice.note == note)
            && (channel < 0 || voice.channel == channel)
            && (velocity < 0.0f || voice.velocity == velocity);
    }
    return count;
}
ParamValues patch(int policy, bool scan, int quality)
{
    ParamValues p;
    p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
    p[P_kasScanTiming] = scan ? 1.0f : 0.0f;
    p[P_oversampling] = static_cast<float>(quality);
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    return p;
}
void fillRemainder(Engine& engine, int count = kas::Scanner::kCapacity - 1)
{
    // Harmless releases fill the bounded event queue without using voices.
    for (int i = 0; i < count; ++i) engine.noteOff(127, 16);
}
void testBoundedFlush()
{
    kas::Scanner scanner;
    scanner.prepare(96000.0);
    for (int i = 0; i < 31; ++i) scanner.tick([](const auto&) {});
    std::array<kas::Scanner::Event, kas::Scanner::kCapacity> expected {}, actual {};
    for (int i = 0; i < kas::Scanner::kCapacity; ++i)
    {
        const int note = 48 + (i * 7) % 12, channel = 1 + i % 16;
        const bool on = i % 3 != 0;
        expected[static_cast<std::size_t>(i)] = {note, channel, 0.5f, on};
        check(scanner.push(note, channel, 0.5f, on), "all capacity slots are usable");
    }
    check(!scanner.push(60, 2, 0.75f, true) && scanner.pending() == kas::Scanner::kCapacity,
          "overflow is refused without changing queued events");
    std::stable_sort(expected.begin(), expected.end(), [](const auto& a, const auto& b) {
        return a.note < b.note;
    });
    int received = 0;
    scanner.flush([&](const auto& event) {
        if (received < kas::Scanner::kCapacity) actual[static_cast<std::size_t>(received)] = event;
        ++received;
    });
    bool matches = received == kas::Scanner::kCapacity;
    for (std::size_t i = 0; i < actual.size(); ++i)
        matches &= actual[i].note == expected[i].note && actual[i].channel == expected[i].channel
            && actual[i].velocity == expected[i].velocity && actual[i].on == expected[i].on;
    check(matches && scanner.pending() == 0,
          "bounded flush uses low-key priority and same-pitch arrival order exactly once");
    scanner.flush([&](const auto&) { ++received; });
    check(received == kas::Scanner::kCapacity, "empty repeated flush is inert");
    check(scanner.push(60, 2, 0.75f, true), "flushed queue accepts another event");
    int tickReceived = 0;
    bool timing = true;
    for (int i = 1; i <= 65; ++i)
        timing &= scanner.tick([&](const auto&) { ++tickReceived; }) == (i == 65);
    check(timing && tickReceived == 1, "early flush preserves the running scan clock");
}
void testOverflowLifecycle()
{
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        for (int policy = 0; policy < 3; ++policy)
            for (bool scan : {false, true})
                for (int quality : {0, 1, 2})
                {
                    Engine engine;
                    engine.prepare(rate);
                    engine.setParams(patch(policy, scan, quality));
                    const int scanFrames = static_cast<int>(std::ceil(rate * kas::kNominalScanSeconds)) + 2;
                    const auto context = " rate=" + std::to_string(static_cast<int>(rate))
                        + " policy=" + std::to_string(policy) + " scan=" + std::to_string(scan)
                        + " quality=" + std::to_string(quality);

                    engine.noteOn(60, 0.75f, 2);
                    fillRemainder(engine);
                    engine.noteOff(60, 2); // first event beyond capacity
                    render(engine, scanFrames);
                    check(held(engine) == 0, "overflow release cannot precede its queued strike" + context);

                    engine.reset();
                    engine.noteOn(60, 0.25f, 2);
                    engine.noteOn(60, 0.5f, 3);
                    render(engine, scanFrames);
                    engine.noteOff(60, 2);
                    fillRemainder(engine);
                    engine.noteOn(60, 0.75f, 2); // overflow retrigger must follow old release
                    render(engine, scanFrames);
                    check(held(engine, 60, 2, 0.75f) == 1 && held(engine, 60, 2) == 1,
                          "overflow retrigger keeps new onset, not the stale key" + context);
                    check(held(engine, 60, 3, 0.5f) == 1,
                          "overflow preserves equal pitch on another MIDI channel" + context);
                    engine.noteOff(60, 2); engine.noteOff(60, 3);
                    render(engine, scanFrames);
                    check(held(engine) == 0, "overflow notes release normally afterward" + context);

                    engine.reset();
                    for (int i = 0; i < 8; ++i) engine.noteOn(48 + i, 0.5f, 1);
                    render(engine, scanFrames);
                    engine.noteOn(90, 0.75f, 2); // ninth key will have to wait
                    fillRemainder(engine);
                    engine.noteOff(90, 2);
                    render(engine, scanFrames);
                    engine.noteOff(48, 1);
                    render(engine, scanFrames);
                    check(held(engine, 90, 2) == 0 && held(engine) == 7,
                          "overflow release cannot leave a waiting key to revive later" + context);

                    engine.reset();
                    // Repeated overload in the same scan interval stays bounded
                    // and preserves short-note order, not only the first flush.
                    for (int i = 0; i < kas::Scanner::kCapacity * 3; ++i)
                    {
                        engine.noteOn(60, 0.75f, 2);
                        engine.noteOff(60, 2);
                    }
                    render(engine, scanFrames);
                    check(held(engine) == 0, "repeated overflows leave no held keys" + context);
                }
}
} // namespace

int main()
{
    testBoundedFlush();
    testOverflowLifecycle();
    std::printf("KAS queue overflow: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
