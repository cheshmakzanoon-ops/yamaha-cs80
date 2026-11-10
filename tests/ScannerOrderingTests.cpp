// MIDI ordering at the opt-in KAS scan boundary. These are event-adapter
// regressions, not claims about the timing of a measured Yamaha keyboard.
#include "core/Engine.h"
#include "core/KasScanner.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}
void render(Engine& engine, int frames = 128)
{
    std::array<float, 256> audio {};
    while (frames > 0)
    {
        const int count = std::min(frames, 128);
        engine.render(audio.data(), count);
        frames -= count;
    }
}
int held(const Engine& engine, int note = -1, int channel = -1)
{
    int count = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto voice = engine.getVoiceInfo(i);
        count += voice.held && (note < 0 || voice.note == note)
                           && (channel < 0 || voice.channel == channel);
    }
    return count;
}

void testScannerOrder()
{
    // Every on/off sequence through eight events must retain its arrival
    // order for one pitch, including rapid repeats and unmatched releases.
    for (int length = 1; length <= 8; ++length)
        for (int bits = 0; bits < (1 << length); ++bits)
        {
            kas::Scanner scanner;
            scanner.prepare(96000.0);
            for (int i = 0; i < length; ++i)
                scanner.push(60, 1, 0.8f, (bits & (1 << i)) != 0);
            std::vector<kas::Scanner::Event> events;
            while (!scanner.tick([&](const auto& event) { events.push_back(event); })) {}
            bool ordered = static_cast<int>(events.size()) == length;
            for (int i = 0; i < static_cast<int>(events.size()); ++i)
                ordered &= events[static_cast<std::size_t>(i)].on == ((bits & (1 << i)) != 0);
            check(ordered, "same-pitch sequence keeps arrival order: "
                + std::to_string(length) + "/" + std::to_string(bits));
        }

    kas::Scanner scanner;
    scanner.prepare(96000.0);
    const std::array<kas::Scanner::Event, 8> input {{
        {72, 2, 0.7f, true}, {60, 1, 0.8f, true}, {48, 3, 0.9f, true},
        {60, 2, 0.6f, true}, {60, 1, 0.0f, false}, {48, 3, 0.0f, false},
        {60, 2, 0.0f, false}, {72, 2, 0.0f, false}
    }};
    for (const auto& event : input)
        scanner.push(event.note, event.channel, event.velocity, event.on);
    std::vector<kas::Scanner::Event> events;
    while (!scanner.tick([&](const auto& event) { events.push_back(event); })) {}
    auto expected = input;
    std::stable_sort(expected.begin(), expected.end(), [](const auto& a, const auto& b) {
        return a.note < b.note;
    });
    bool matches = events.size() == expected.size();
    for (std::size_t i = 0; i < events.size(); ++i)
        matches &= events[i].note == expected[i].note && events[i].channel == expected[i].channel
            && events[i].velocity == expected[i].velocity && events[i].on == expected[i].on;
    check(matches, "different pitches retain low-key priority without reordering a pitch's channels");
}

void testArpTransitionBoundary()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (bool source : {false, true}) for (bool scan : {false, true})
            for (bool keyboard : {false, true})
            {
                ParamValues p; p[P_sourcePerformance] = source ? 1.0f : 0.0f;
                p[P_kasScanTiming] = scan ? 1.0f : 0.0f;
                p[P_arpRate] = 16.0f;
                Engine e; e.prepare(rate); e.setParams(p);
                const auto on = [&]() { if (keyboard) e.noteOnFromKeyboard(60, .8f, 2); else e.noteOn(60, .8f, 2); };
                const auto off = [&]() { if (keyboard) e.noteOffFromKeyboard(60, 2); else e.noteOff(60, 2); };
                on();
                p[P_arpOn] = 1.0f; e.setParams(p); render(e, 256);
                check(held(e) == 0, "pre-toggle direct onset cannot seed ARP");
                on(); render(e, 256);
                check(held(e, 60, 2) == 1, "post-toggle fresh onset starts ARP");
                off(); render(e, 256);
                check(held(e) == 0, "fresh ARP key releases normally");

                e.reset(); p[P_arpOn] = 0.0f; e.setParams(p);
                on(); render(e, 256); off(); // release may still be queued
                p[P_arpOn] = 1.0f; e.setParams(p);
                on(); render(e, 256);
                check(held(e, 60, 2) == 1, "pre-toggle release cannot cancel fresh ARP key");

                e.reset(); p[P_arpOn] = 0.0f; e.setParams(p); on();
                p[P_arpOn] = 1.0f; e.setParams(p);
                p[P_arpOn] = 0.0f; e.setParams(p); render(e, 256);
                check(held(e) == 0, "rapid ARP on-off cannot resurrect discarded direct onset");
            }
}

void testEngineOrdering()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int policy = 0; policy < 3; ++policy)
            for (bool scan : {false, true})
            {
                ParamValues params;
                params[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
                params[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
                params[P_kasScanTiming] = scan ? 1.0f : 0.0f;
                Engine engine;
                engine.prepare(rate);
                engine.setParams(params);
                const auto context = " rate=" + std::to_string(rate)
                    + " policy=" + std::to_string(policy) + " scan=" + std::to_string(scan);

                engine.noteOn(60, 0.8f, 1);
                engine.noteOff(60, 1);
                render(engine);
                check(held(engine) == 0, "short note cannot leave a held gate" + context);

                engine.reset();
                engine.noteOn(60, 0.8f, 1);
                engine.noteOff(60, 1);
                engine.noteOn(60, 0.7f, 1);
                render(engine);
                check(held(engine, 60, 1) == 1, "on/off/on retains only its last onset" + context);
                engine.noteOff(60, 1);
                render(engine);
                check(held(engine) == 0, "one release finishes the surviving onset" + context);

                engine.reset();
                engine.noteOn(60, 0.8f, 1);
                render(engine);
                engine.noteOff(60, 1);
                engine.noteOn(60, 0.7f, 1);
                engine.noteOff(60, 1);
                render(engine);
                check(held(engine) == 0, "held key's short retrigger also releases" + context);

                engine.reset();
                engine.noteOn(60, 0.8f, 1);
                render(engine);
                engine.noteOn(60, 0.7f, 1);
                engine.noteOff(60, 1);
                render(engine);
                bool originalTouch = true;
                for (int i = 0; i < 8; ++i)
                {
                    const auto voice = engine.getVoiceInfo(i);
                    if (voice.held && voice.note == 60 && voice.channel == 1)
                        originalTouch &= voice.velocity == 0.8f;
                }
                check(held(engine, 60, 1) == 1 && originalTouch,
                    "overlapping ON/OFF releases newest onset and retains the original" + context);

                engine.reset();
                engine.noteOn(60, 0.8f, 1);
                engine.noteOn(60, 0.7f, 2);
                engine.noteOff(60, 1);
                render(engine);
                check(held(engine, 60, 1) == 0 && held(engine, 60, 2) == 1,
                    "same-pitch channel release preserves the other channel" + context);
                engine.noteOff(60, 2);
                render(engine);
                check(held(engine) == 0, "second channel releases normally" + context);

                engine.reset();
                for (int i = 0; i < 8; ++i) engine.noteOn(48 + i, 0.8f, 1);
                render(engine);
                engine.noteOn(90, 0.8f, 2);
                engine.noteOff(90, 2);
                render(engine);
                engine.noteOff(48, 1);
                render(engine);
                check(held(engine, 90, 2) == 0 && held(engine) == 7,
                    "short ninth key is not revived when a circuit frees" + context);

                // Saturated cross-channel handoff is also order-sensitive:
                // the new key arrives before a circuit frees, so it is a
                // reconsidered held key, not a fresh strike at that instant.
                engine.reset();
                for (int i = 0; i < 7; ++i) engine.noteOn(48 + i, 0.8f, 1);
                engine.noteOn(60, 0.8f, 1);
                render(engine);
                engine.noteOn(60, 0.7f, 2);
                engine.noteOff(60, 1);
                render(engine);
                if (policy == 0)
                    check(held(engine, 60, 2) == 0 && held(engine) == 7,
                        "Legacy keeps discarding a key that arrived while full" + context);
                else
                {
                    bool zeroTouch = true;
                    for (int i = 0; i < 8; ++i)
                    {
                        const auto voice = engine.getVoiceInfo(i);
                        if (voice.held && voice.note == 60 && voice.channel == 2)
                            zeroTouch &= voice.velocity == 0.0f;
                    }
                    check(held(engine, 60, 2) == 1 && held(engine) == 8 && zeroTouch,
                        "same-pitch handoff keeps chronological ninth-key zero touch" + context);
                }

                engine.reset();
                engine.noteOn(60, 0.8f, 1);
                engine.noteOff(60, 1);
                params[P_kasScanTiming] = 0.0f;
                engine.setParams(params);
                render(engine, 1);
                check(held(engine) == 0, "disabling scan flushes a short note in arrival order" + context);
            }
}
}
int main()
{
    testScannerOrder();
    testEngineOrdering();
    testArpTransitionBoundary();
    std::printf("Scanner ordering: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
