// Bounded immediate two-instrument release is an adapter/software contract.
// Deferred/ARP paths intentionally retain the old broadcast behavior. These
// tests are not a hardware, complete MIDI, or complete 16-voice timing claim.
#include "core/Engine.h"
#include "core/KasScanner.h"
#include "core/VoiceModes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

using namespace cs80;
namespace
{
using Kind = Engine::ReleaseCandidate::Kind;
int checks = 0, failures = 0;
void check(bool ok, const std::string& message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
ParamValues patch(bool circuit, int mode = 1)
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
struct Pair
{
    // The source must precede (and outlive) both instruments. Every oracle
    // constructs independent engines and source: never copy a linked pair.
    ReleaseOrderClock clock;
    Engine first, second;
    ParamValues params;
    explicit Pair(bool circuit, bool linked = true, std::uint64_t initial = 0)
        : clock(initial), params(patch(circuit))
    {
        if (linked)
            check(first.bindReleaseOrderClock(clock) && second.bindReleaseOrderClock(clock), "cold pair binds");
        first.prepare(48000.0); second.prepare(48000.0);
        apply();
    }
    Pair(const Pair&) = delete;
    Pair& operator=(const Pair&) = delete;
    void apply() { first.setParams(params); second.setParams(params); }
    void on(int note, float velocity = 0.8f, int channel = 2, bool ui = false)
    {
        auto& target = SixteenVoiceRouter::noteOnTarget(first, second);
        if (ui) target.noteOnFromKeyboard(note, velocity, channel);
        else target.noteOn(note, velocity, channel);
    }
    bool off(int note, int channel = 2, bool ui = false)
    { return SixteenVoiceRouter::noteOff(first, second, note, channel, ui); }
    void broadcast(int note, int channel = 2, bool ui = false)
    {
        if (ui) { first.noteOffFromKeyboard(note, channel); second.noteOffFromKeyboard(note, channel); }
        else { first.noteOff(note, channel); second.noteOff(note, channel); }
    }
};
int held(const Engine& e, int note = -1, int channel = -1, bool pedal = false)
{
    int count = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto v = e.getVoiceInfo(i);
        count += v.active && (pedal ? v.pedalHeld : v.held)
            && (note < 0 || v.note == note) && (channel < 0 || v.channel == channel);
    }
    return count;
}
auto candidate(const Engine& e, int note = 60, int channel = 2, bool ui = false)
{ return e.immediateReleaseCandidate(note, channel, ui); }
void fill(Engine& e, int base = 48, int count = 8, int channel = 2)
{ for (int i = 0; i < count; ++i) e.noteOn(base + i, 0.5f, channel); }
void skew(Engine& e)
{ for (int i = 0; i < 40; ++i) { e.noteOn(30, 0.2f, 3); e.noteOff(30, 3); } }
void sameState(const Engine& a, const Engine& b, const std::string& row)
{
    bool same = a.activeVoices() == b.activeVoices();
    for (int i = 0; i < 8; ++i)
    {
        const auto x = a.getVoiceInfo(i), y = b.getVoiceInfo(i);
        same &= x.active == y.active && x.note == y.note && x.channel == y.channel
            && x.velocity == y.velocity && x.pressure == y.pressure && x.bend == y.bend
            && x.ribbonRatio == y.ribbonRatio && x.held == y.held && x.pedalHeld == y.pedalHeld
            && x.touchA == y.touchA && x.touchB == y.touchB && x.touchC == y.touchC;
    }
    check(same, row + " exact voice telemetry");
}
void audio(Engine& actual, Engine& expected, const std::string& row, int frames = 256)
{
    std::array<float, 128> a {}, b {};
    bool finite = true, exact = true;
    while (frames > 0)
    {
        const int count = std::min(frames, 64);
        actual.render(a.data(), count); expected.render(b.data(), count);
        finite &= std::all_of(a.begin(), a.begin() + 2 * count, [](float v) { return std::isfinite(v); })
               && std::all_of(b.begin(), b.begin() + 2 * count, [](float v) { return std::isfinite(v); });
        exact &= std::memcmp(a.data(), b.data(), static_cast<std::size_t>(2 * count) * sizeof(float)) == 0;
        frames -= count;
    }
    check(finite, row + " finite audio");
    check(exact, row + " bit-exact audio");
    sameState(actual, expected, row);
}
void audio(Pair& actual, Pair& expected, const std::string& row, int frames = 256)
{
    audio(actual.first, expected.first, row + " primary", frames);
    audio(actual.second, expected.second, row + " extension", frames);
}
void fallback(Pair& actual, Pair& expected, int note, const std::string& row, bool ui = false)
{
    const bool handled = actual.off(note, 2, ui);
    check(!handled, row + " explicitly returns broadcast fallback");
    // Follow the actual caller contract, so an incorrectly handled event also
    // fails the state/audio oracle rather than accidentally being broadcast.
    if (!handled) actual.broadcast(note, 2, ui);
    expected.broadcast(note, 2, ui);
    audio(actual, expected, row);
}

void testCounterSkewAndReuse()
{
    for (bool circuit : {false, true})
        for (bool newerPrimary : {false, true})
        {
            Pair actual(circuit), reference(circuit);
            const std::string row = std::string(circuit ? "circuit" : "legacy")
                + (newerPrimary ? " newer primary" : " newer extension");
            for (auto* p : {&actual, &reference})
            {
                skew(newerPrimary ? p->second : p->first);
                if (newerPrimary)
                {
                    fill(p->first, 48);
                    p->on(60, 0.3f);
                    p->first.noteOff(48, 2);
                    p->on(60, 0.8f);
                }
                else
                {
                    fill(p->first, 60);
                    p->on(60, 0.8f);
                }
            }
            check(held(actual.first, 60, 2) == 1 && held(actual.second, 60, 2) == 1, row + " overlap fixture");
            audio(actual, reference, row + " before off");
            check(actual.off(60), row + " first off coordinated");
            (newerPrimary ? reference.first : reference.second).noteOff(60, 2);
            check(held(newerPrimary ? actual.first : actual.second, 60, 2) == 0
                && held(newerPrimary ? actual.second : actual.first, 60, 2) == 1, row + " first off releases only newest strike");
            audio(actual, reference, row + " first off");
            check(actual.off(60), row + " second off coordinated");
            (newerPrimary ? reference.second : reference.first).noteOff(60, 2);
            check(held(actual.first, 60) + held(actual.second, 60) == 0, row + " second off releases older strike");
            audio(actual, reference, row + " second off");
            check(actual.off(60), row + " extra off handled without another release");
            audio(actual, reference, row + " extra off");

            // A free primary can be newer than an extension whose local age
            // is much larger; repeatedly reusing it must refresh ownership.
            for (int repetition = 0; repetition < 3; ++repetition)
            {
                actual.second.noteOn(60, 0.4f, 2); reference.second.noteOn(60, 0.4f, 2);
                actual.on(60, 0.9f); reference.on(60, 0.9f);
                check(actual.off(60), row + " reused primary off coordinated");
                reference.first.noteOff(60, 2);
                check(held(actual.first, 60) == 0 && held(actual.second, 60) == 1, row + " reuse chooses fresh primary");
                audio(actual, reference, row + " reuse");
                check(actual.off(60), row + " reused extension off coordinated");
                reference.second.noteOff(60, 2);
            }
        }
}

void testChannelsPedalAndGroups()
{
    for (bool circuit : {false, true})
    {
        Pair a(circuit), r(circuit);
        for (auto* p : {&a, &r})
        {
            p->first.noteOn(60, 0.3f, 2); p->second.noteOn(60, 0.8f, 2);
            p->first.noteOn(60, 0.4f, 3); p->second.noteOn(60, 0.7f, 3);
            p->first.controlChange(64, 1.0f, 2); p->second.controlChange(64, 1.0f, 2);
        }
        check(a.off(60, 4), "wrong channel is a handled no-op");
        audio(a, r, "wrong channel");
        check(a.off(60), "pedal-held release coordinated"); r.second.noteOff(60, 2);
        check(held(a.first, 60, 2) == 1 && held(a.second, 60, 2) == 0
            && held(a.second, 60, 2, true) == 1 && held(a.first, 60, 3) == 1 && held(a.second, 60, 3) == 1,
            "CC64 applies only to selected onset and exact channel");
        audio(a, r, "selected pedal ownership");
        check(a.off(60), "second pedal release coordinated"); r.first.noteOff(60, 2);
        for (auto* p : {&a, &r})
        { p->first.controlChange(64, 0.0f, 2); p->second.controlChange(64, 0.0f, 2); }
        check(held(a.first, 60, 2, true) + held(a.second, 60, 2, true) == 0, "pedal-up releases both tails");
        audio(a, r, "pedal-up");

        for (int mode : {2, 3, 4})
            for (bool partial : {false, true})
            {
                Pair group(circuit), oracle(circuit);
                const int requested = mode == 2 ? 2 : mode == 3 ? 4 : 8;
                const int occupied = partial ? (requested == 2 ? 7 : 5) : 0;
                for (auto* p : {&group, &oracle})
                {
                    fill(p->first, 40, occupied, 3);
                    p->params[P_voiceMode] = static_cast<float>(mode); p->apply();
                    p->first.noteOn(60, 0.3f, 2);
                    p->params[P_voiceMode] = 1.0f; p->apply();
                    p->second.noteOn(60, 0.8f, 2);
                }
                const std::string row = "unison " + std::to_string(mode) + (partial ? " partial" : " full") + " to sixteen";
                check(held(group.first, 60, 2) == std::min(requested, 8 - occupied), row + " fixture");
                check(group.off(60), row + " newer extension handled"); oracle.second.noteOff(60, 2);
                audio(group, oracle, row + " newer extension");
                check(group.off(60), row + " old group handled"); oracle.first.noteOff(60, 2);
                check(held(group.first, 60, 2) == 0 && held(group.first, -1, 3) == occupied,
                    row + " whole original group releases without touching other channel");
                audio(group, oracle, row + " whole group");
            }
    }
}

void testWaitersAndPromotion()
{
    Pair a(true), r(true);
    for (auto* p : {&a, &r})
    {
        fill(p->first, 48); fill(p->second, 60);
        p->on(60, 0.9f); // seventeenth held key waits in primary
    }
    const auto waiter = candidate(a.first);
    check(waiter.kind == Kind::waiting && waiter.order > candidate(a.second).order, "seventeenth key has qualified waiter identity");
    check(a.off(60), "seventeenth-key release coordinated"); r.first.noteOff(60, 2);
    check(candidate(a.first).kind == Kind::none && held(a.second, 60) == 1, "matching waiter is canceled before sounding extension");
    audio(a, r, "seventeenth-key cancellation");
    check(a.off(60), "next off reaches sounding extension"); r.second.noteOff(60, 2);
    audio(a, r, "seventeenth-key next off");

    for (bool pedal : {false, true})
    {
        Pair promoted(true), oracle(true);
        for (auto* p : {&promoted, &oracle})
        {
            fill(p->first);
            p->first.noteOn(60, 0.9f, 2);
            p->second.noteOn(60, 0.7f, 2); // newer than the waiting key
            if (pedal) p->first.controlChange(64, 1.0f, 2);
        }
        const auto before = candidate(promoted.first).order;
        check(promoted.off(48), "freeing primary is coordinated"); oracle.first.noteOff(48, 2);
        if (pedal)
        {
            check(candidate(promoted.first).kind == Kind::waiting, "CC64 does not prematurely promote waiter");
            promoted.first.controlChange(64, 0.0f, 2); oracle.first.controlChange(64, 0.0f, 2);
        }
        const auto after = candidate(promoted.first);
        check(after.kind == Kind::held && after.order > before && after.order > candidate(promoted.second).order,
            "immediate promotion receives fresh globally newest order");
        bool zeroTouch = false;
        for (int i = 0; i < 8; ++i)
        {
            const auto v = promoted.first.getVoiceInfo(i);
            if (v.held && v.note == 60) zeroTouch = v.velocity == 0.0f && v.touchB == 0.0f && v.touchC == 0.0f;
        }
        check(zeroTouch, "promotion preserves zero initial touch rather than original waiting velocity");
        audio(promoted, oracle, "zero-touch promotion");
        check(promoted.off(60), "promoted onset release coordinated"); oracle.first.noteOff(60, 2);
        check(held(promoted.first, 60) == 0 && held(promoted.second, 60) == 1, "promoted onset is newer than intervening extension strike");
        audio(promoted, oracle, "promotion release");
    }

    Pair twoWaiters(true), twoReference(true);
    for (auto* p : {&twoWaiters, &twoReference})
    {
        fill(p->first); fill(p->second, 60);
        p->first.noteOn(60, 0.4f, 2); p->second.noteOn(60, 0.8f, 2);
    }
    for (bool primary : {false, true})
    {
        check(twoWaiters.off(60), "two-waiter release coordinated");
        (primary ? twoReference.first : twoReference.second).noteOff(60, 2);
        check(held(twoWaiters.second, 60) == 1, "both waiter cancellations preserve sounding onset");
        check(candidate(primary ? twoWaiters.first : twoWaiters.second).kind == (primary ? Kind::none : Kind::held),
            "matching waiters cancel by shared order, newest first");
        audio(twoWaiters, twoReference, "two matching waiters");
    }

    Pair olderWaiter(true), olderReference(true);
    for (auto* p : {&olderWaiter, &olderReference})
    { fill(p->first); p->first.noteOn(60, 0.4f, 2); p->second.noteOn(60, 0.8f, 2); }
    check(candidate(olderWaiter.first).order < candidate(olderWaiter.second).order, "older waiter fixture");
    check(olderWaiter.off(60), "older waiter cancellation coordinated"); olderReference.first.noteOff(60, 2);
    check(held(olderWaiter.second, 60) == 1, "waiter priority overrides newer held order across engines");
    audio(olderWaiter, olderReference, "global waiter-first ordering");
}

void testAdmissionDoesNotInventOrder()
{
    for (bool circuit : {false, true})
    {
        Pair a(circuit), r(circuit);
        for (auto* p : {&a, &r})
        {
            fill(p->first);
            if (circuit)
                for (int note = 70; note < 102; ++note) p->first.noteOn(note, 0.7f, 2);
        }
        const auto previous = candidate(a.first, circuit ? 101 : 55).order;
        for (auto* p : {&a, &r})
        {
            p->first.noteOn(60, 0.9f, 2); // refused circuit or full waiter bank
            p->second.noteOn(60, 0.8f, 2);
        }
        check(candidate(a.first).kind == Kind::none && candidate(a.second).order == previous + 1,
            "refused note does not consume order or invent a waiter");
        check(a.off(60), "refused note leaves extension release qualified"); r.second.noteOff(60, 2);
        audio(a, r, circuit ? "full waiter bank refusal" : "legacy full allocator refusal");
    }
}

void testKeyboardOwnership()
{
    for (bool newerPrimary : {false, true})
        for (bool circuit : {false, true})
        {
            Pair a(circuit), r(circuit);
            for (auto* p : {&a, &r})
            {
                auto& older = newerPrimary ? p->second : p->first;
                auto& newer = newerPrimary ? p->first : p->second;
                older.noteOnFromKeyboard(60, 0.3f, 2); newer.noteOn(60, 0.8f, 2);
            }
            check(a.off(60, 2, true), "UI alias is handled when newest onset is host-owned");
            check(held(a.first, 60) + held(a.second, 60) == 2, "newest host group blocks UI fall-through across engines");
            audio(a, r, "blocked UI alias");
            check(a.off(60), "host release selects newest host group");
            (newerPrimary ? r.first : r.second).noteOff(60, 2);
            check(a.off(60, 2, true), "older UI onset can release after host group");
            (newerPrimary ? r.second : r.first).noteOffFromKeyboard(60, 2);
            audio(a, r, "host then UI alias release");
        }
    for (bool firstUi : {false, true})
    {
        Pair a(true), r(true);
        for (auto* p : {&a, &r})
        {
            fill(p->first);
            p->second.noteOnFromKeyboard(60, 0.8f, 2);
            if (firstUi) p->first.noteOnFromKeyboard(60, 0.3f, 2);
            else p->first.noteOn(60, 0.3f, 2);
        }
        const auto before = candidate(a.first);
        for (auto* p : {&a, &r})
        {
            if (firstUi) p->first.noteOn(60, 0.9f, 2);
            else p->first.noteOnFromKeyboard(60, 0.9f, 2);
        }
        const auto after = candidate(a.first);
        check(after.kind == Kind::waiting && after.order == before.order && after.keyboardOrigin == firstUi,
            "duplicate waiter retains first source and allocation order");
        check(a.off(60, 2, true), "source-filtered UI waiter release handled");
        if (firstUi) r.first.noteOffFromKeyboard(60, 2);
        else r.second.noteOffFromKeyboard(60, 2);
        check(held(a.second, 60) == (firstUi ? 1 : 0), "UI waiter filtering is distinct from newest-host held blocking");
        check(candidate(a.first).kind == (firstUi ? Kind::none : Kind::waiting), "UI cannot cancel retained host waiter");
        audio(a, r, "source-retaining duplicate waiter");
        check(a.off(60), "ordinary host release remains source agnostic");
        if (firstUi) r.second.noteOff(60, 2); else r.first.noteOff(60, 2);
        audio(a, r, "source-agnostic host release");
    }
    Pair hidden(true), hiddenReference(true);
    for (auto* p : {&hidden, &hiddenReference})
    {
        fill(p->first);
        auto scan = p->params; scan[P_kasScanTiming] = 1.0f; p->first.setParams(scan);
        p->first.noteOn(60, 0.9f, 2); // host-owned deferred waiter
    }
    audio(hidden, hiddenReference, "deferred host waiter", 128);
    for (auto* p : {&hidden, &hiddenReference})
    { p->first.setParams(p->params); p->second.noteOnFromKeyboard(60, 0.7f, 2); }
    check(candidate(hidden.first).kind == Kind::waiting && candidate(hidden.first).order == 0
        && candidate(hidden.first, 60, 2, true).kind == Kind::none,
        "UI filtering excludes even unqualified host waiters");
    check(hidden.off(60, 2, true), "filtered deferred host waiter does not force UI fallback");
    hiddenReference.second.noteOffFromKeyboard(60, 2);
    check(candidate(hidden.first).kind == Kind::waiting && held(hidden.second, 60) == 0,
        "UI release leaves deferred host waiter untouched");
    audio(hidden, hiddenReference, "filtered deferred host waiter");
    fallback(hidden, hiddenReference, 60, "ordinary host sees deferred waiter");

    Pair local(true), localReference(true);
    for (auto* p : {&local, &localReference})
    {
        p->first.noteOnFromKeyboard(60, 0.2f, 2);
        p->second.noteOnFromKeyboard(60, 0.4f, 2);
        p->first.noteOnFromKeyboard(60, 0.7f, 2);
    }
    for (bool primary : {true, false, true})
    {
        check(local.off(60, 2, true), "repeated UI alias release coordinated");
        (primary ? localReference.first : localReference.second).noteOffFromKeyboard(60, 2);
        audio(local, localReference, "preserve local newest-age UI candidate");
    }
}

void testInvalidBindingResetAndExhaustion()
{
    for (bool circuit : {false, true})
    {
        Pair a(circuit), r(circuit);
        for (auto* p : {&a, &r}) { p->first.noteOn(60, 0.4f, 2); p->second.noteOn(60, 0.8f, 2); }
        const auto before = candidate(a.second).order;
        for (int note : {-1, 128, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        {
            a.first.noteOn(note, 0.8f, 2); a.second.noteOnFromKeyboard(note, 0.8f, 2);
            check(a.off(note), "invalid pitch release is a handled no-op");
        }
        for (float value : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity()})
        { a.first.noteOn(60, value, 2); a.second.noteOnFromKeyboard(60, value, 2); }
        for (int channel : {0, -1, 17}) check(a.off(60, channel), "unmatched channel release cannot redirect");
        check(candidate(a.second).order == before, "malformed note-ons leave release identity unchanged");
        audio(a, r, "malformed event no-op");
        a.first.noteOn(62, 0.0f, 2); r.first.noteOn(62, 0.0f, 2);
        check(candidate(a.first, 62).order == before + 1, "rejected input consumes no shared order; core zero velocity remains an onset");
        audio(a, r, "unchanged core zero-velocity API");

        const auto oldOrder = candidate(a.second).order;
        a.first.reset(); r.first.reset();
        a.first.noteOn(60, 0.9f, 2); r.first.noteOn(60, 0.9f, 2);
        check(candidate(a.first).order > oldOrder, "one-engine reset preserves shared chronology");
        check(a.off(60), "post-reset newest primary handled"); r.first.noteOff(60, 2);
        audio(a, r, "reset does not reset shared source");
        for (auto* p : {&a, &r})
        { p->first.allNotesOff(false); p->second.allNotesOff(false); }
        check(a.off(60), "panic removes all ownership");
        a.on(60); r.on(60);
        check(a.off(60), "post-panic allocation remains qualified"); r.first.noteOff(60, 2);
        audio(a, r, "post-panic release");
    }
    ReleaseOrderClock shared, other;
    Engine cold, warm, second;
    check(cold.bindReleaseOrderClock(shared) && cold.bindReleaseOrderClock(shared), "binding is idempotent only for same source");
    check(!cold.bindReleaseOrderClock(other), "source rebinding is denied");
    cold.prepare(48000.0); cold.reset();
    check(!cold.bindReleaseOrderClock(other), "reset cannot rebind a linked engine");
    check(second.bindReleaseOrderClock(shared), "second cold engine binds same source");
    check(cold.canCoordinateImmediateReleaseWith(second), "prepare and reset retain binding");
    warm.prepare(48000.0); warm.noteOn(60, 0.8f, 2); warm.noteOff(60, 2);
    check(!warm.bindReleaseOrderClock(shared), "used engine cannot be retroactively linked");
    check(!SixteenVoiceRouter::noteOff(cold, warm, 60, 2), "unlinked source returns fallback");
    warm.reset(); check(!warm.bindReleaseOrderClock(other), "reset does not permit late binding after prior allocation");
    Engine foreign;
    check(foreign.bindReleaseOrderClock(other), "different cold engine can bind a distinct source");
    check(!SixteenVoiceRouter::noteOff(cold, foreign, 60, 2), "different sources return fallback even for no candidates");
    cold.noteOn(60, 0.8f, 2);
    check(!SixteenVoiceRouter::noteOff(cold, cold, 60, 2) && held(cold, 60) == 1,
        "aliased candidate identity fails closed without releasing");
    {
        Engine pending;
        pending.prepare(48000.0); auto p = patch(true); p[P_kasScanTiming] = 1.0f; pending.setParams(p);
        pending.noteOnFromKeyboard(60, 0.8f, 2);
        check(!pending.bindReleaseOrderClock(shared), "pending scanner prevents late binding");
    }
    {
        Engine arp;
        arp.prepare(48000.0); auto p = patch(true); p[P_arpOn] = 1.0f; arp.setParams(p); arp.noteOn(60, 0.8f, 2);
        check(!arp.bindReleaseOrderClock(shared), "held arpeggiator key prevents late binding");
    }
    for (int linkage = 0; linkage < 3; ++linkage)
    {
        ReleaseOrderClock foreignClock;
        Pair a(true, false), oracle(true, false);
        if (linkage > 0) check(a.first.bindReleaseOrderClock(a.clock), "partial linkage fixture");
        if (linkage > 1) check(a.second.bindReleaseOrderClock(foreignClock), "mismatched linkage fixture");
        for (auto* p : {&a, &oracle})
        { p->first.noteOn(60, 0.3f, 2); p->second.noteOn(60, 0.8f, 2); }
        fallback(a, oracle, 60, "source linkage " + std::to_string(linkage));
        check(held(a.first, 60) + held(a.second, 60) == 0, "ineligible source retains two-engine broadcast release");
    }
    const auto max = std::numeric_limits<std::uint64_t>::max();
    Pair exhausted(true, true, max - 2), reference(true, true, max - 2);
    for (auto* p : {&exhausted, &reference}) { p->first.noteOn(60, 0.4f, 2); p->second.noteOn(60, 0.8f, 2); }
    check(candidate(exhausted.first).order == max - 1 && candidate(exhausted.second).order == max && exhausted.clock.valid(),
        "last two nonzero serials are ordered without signed comparison");
    check(exhausted.off(60), "last qualified serial releases normally"); reference.second.noteOff(60, 2);
    audio(exhausted, reference, "last qualified serial");
    exhausted.second.noteOn(62, 0.7f, 2); reference.second.noteOn(62, 0.7f, 2);
    check(!exhausted.clock.valid() && candidate(exhausted.second, 62).order == 0, "exhaustion invalidates qualification without wrapping");
    fallback(exhausted, reference, 60, "exhausted source");
    check(exhausted.clock.next() == 0 && exhausted.clock.next() == 0, "exhaustion remains sticky");
    exhausted.first.reset(); exhausted.second.reset();
    check(!exhausted.off(60), "engine resets cannot revive exhausted source");
}

void testDeferredAndArpFallback()
{
    for (int scenario = 0; scenario < 6; ++scenario)
        for (bool ui : {false, true})
        {
            Pair a(true), r(true, false);
            const std::string row = "fallback scenario " + std::to_string(scenario) + (ui ? " UI" : " host");
            for (auto* p : {&a, &r})
            {
                p->params[P_kasScanTiming] = scenario < 4 ? 1.0f : 0.0f;
                p->params[P_arpOn] = scenario == 4 ? 1.0f : 0.0f;
                p->apply();
                if (ui) { p->first.noteOnFromKeyboard(60, 0.3f, 2); p->second.noteOnFromKeyboard(60, 0.8f, 2); }
                else { p->first.noteOn(60, 0.3f, 2); p->second.noteOn(60, 0.8f, 2); }
            }
            if (scenario == 2 || scenario == 4) audio(a, r, row + " dispatch", 128);
            if (scenario == 3)
            {
                for (auto* p : {&a, &r})
                {
                    for (int i = 0; i < kas::Scanner::kCapacity - 1; ++i) p->broadcast(127, 16);
                    // The overflow event is immediate; all flushed earlier
                    // candidates are deferred and must remain unqualified.
                    p->first.noteOn(62, 0.5f, 2); p->second.noteOn(62, 0.5f, 2);
                }
                check(candidate(a.first).order == 0 && candidate(a.first, 62).order != 0,
                    "overflow distinguishes deferred queue from immediate rejected event");
            }
            if (scenario == 1 || scenario == 2 || scenario == 3)
                for (auto* p : {&a, &r}) { p->params[P_kasScanTiming] = 0.0f; p->apply(); }
            if (scenario == 5)
            {
                // A pending unrelated event in just one engine disables the
                // coordinated path, even though both matching owners qualify.
                for (auto* p : {&a, &r})
                {
                    auto scan = p->params; scan[P_kasScanTiming] = 1.0f; p->first.setParams(scan);
                    p->first.noteOff(127, 16); p->first.setParams(p->params);
                }
            }
            if (scenario == 2 || scenario == 4)
                check(candidate(a.first).kind == Kind::held && candidate(a.first).order == 0,
                    row + " executed deferred/ARP candidate is unqualified");
            fallback(a, r, 60, row, ui);
            // Disabled scan with queued ordinary host onsets intentionally
            // retains its old ordering, including an onset after its off.
            if (scenario == 1 && !ui)
                check(held(a.first, 60) + held(a.second, 60) == 2, "pending-after-disable host fallback preserves old broadcast behavior");
            if (scenario == 4)
            {
                for (auto* p : {&a, &r}) { p->params[P_arpOn] = 0.0f; p->apply(); }
                audio(a, r, row + " after arp disable");
            }
        }
    Pair promoted(true), reference(true, false);
    for (auto* p : {&promoted, &reference})
    {
        fill(p->first); p->first.noteOn(60, 0.9f, 2); p->second.noteOn(60, 0.4f, 2);
        p->params[P_kasScanTiming] = 1.0f; p->apply(); p->first.noteOff(48, 2);
    }
    audio(promoted, reference, "deferred callback promotion", 128);
    check(candidate(promoted.first).kind == Kind::held && candidate(promoted.first).order == 0,
        "promotion executed by scanner remains unqualified");
    for (auto* p : {&promoted, &reference}) { p->params[P_kasScanTiming] = 0.0f; p->apply(); }
    fallback(promoted, reference, 60, "deferred promotion after scan disable");

    // The waiter's origin alone does not decide its eventual ownership: an
    // immediate release after scan disable promotes it as a fresh allocation.
    Pair immediate(true), immediateReference(true, false);
    for (auto* p : {&immediate, &immediateReference})
    {
        fill(p->first);
        p->params[P_kasScanTiming] = 1.0f; p->apply(); p->first.noteOn(60, 0.9f, 2);
    }
    audio(immediate, immediateReference, "deferred waiter dispatch", 128);
    check(candidate(immediate.first).kind == Kind::waiting && candidate(immediate.first).order == 0,
        "deferred waiter remains unknown before promotion");
    for (auto* p : {&immediate, &immediateReference})
    {
        p->params[P_kasScanTiming] = 0.0f; p->apply(); p->second.noteOn(60, 0.4f, 2);
    }
    check(immediate.off(48), "unrelated qualified off can promote formerly deferred waiter");
    immediateReference.first.noteOff(48, 2);
    check(candidate(immediate.first).kind == Kind::held
        && candidate(immediate.first).order > candidate(immediate.second).order,
        "immediate promotion qualifies deferred waiter with fresh newest order");
    audio(immediate, immediateReference, "immediate promotion of deferred waiter");
    check(immediate.off(60), "formerly deferred promoted note releases as immediate");
    immediateReference.first.noteOff(60, 2);
    check(held(immediate.first, 60) == 0 && held(immediate.second, 60) == 1,
        "immediate promotion releases only promoted onset");
    audio(immediate, immediateReference, "release formerly deferred promoted note");

    // One unqualified matching candidate vetoes the whole pair, even when an
    // eligible immediate waiter would otherwise outrank it.
    Pair mixed(true), mixedReference(true, false);
    for (auto* p : {&mixed, &mixedReference})
    {
        p->params[P_kasScanTiming] = 1.0f; p->apply(); p->second.noteOn(60, 0.8f, 2);
    }
    audio(mixed, mixedReference, "mixed deferred dispatch", 128);
    for (auto* p : {&mixed, &mixedReference})
    {
        p->params[P_kasScanTiming] = 0.0f; p->apply(); fill(p->first); p->first.noteOn(60, 0.7f, 2);
    }
    check(candidate(mixed.first).kind == Kind::waiting && candidate(mixed.first).order != 0
        && candidate(mixed.second).order == 0, "qualified waiter versus deferred held fixture");
    fallback(mixed, mixedReference, 60, "mixed waiter and deferred held");
}

void testStandaloneSourceIsInert()
{
    for (bool circuit : {false, true})
        for (int mode : {0, 2, 3, 4})
        {
            Pair linked(circuit), baseline(circuit, false);
            linked.params[P_voiceMode] = baseline.params[P_voiceMode] = static_cast<float>(mode);
            linked.apply(); baseline.apply();
            const std::string row = "standalone mode " + std::to_string(mode) + (circuit ? " circuit" : " legacy");
            for (int step = 0; step < 12; ++step)
            {
                // Calling the eight-voice Engine API remains unchanged even
                // with a source bound: no implicit global release routing.
                for (auto* p : {&linked, &baseline})
                {
                    p->first.noteOn(60 + step % 3, 0.2f + 0.05f * step, 2);
                    p->first.noteOn(60 + step % 3, 0.9f, 2);
                    p->first.noteOff(60 + step % 3, 2);
                }
                audio(linked.first, baseline.first, row + " step " + std::to_string(step), 64);
                for (auto* p : {&linked, &baseline}) p->first.allNotesOff(step % 2 == 0);
            }
            audio(linked.first, baseline.first, row + " final tails");
        }
}
} // namespace
int main()
{
    testCounterSkewAndReuse();
    testChannelsPedalAndGroups();
    testWaitersAndPromotion();
    testAdmissionDoesNotInventOrder();
    testKeyboardOwnership();
    testInvalidBindingResetAndExhaustion();
    testDeferredAndArpFallback();
    testStandaloneSourceIsInert();
    std::printf("Immediate sixteen-voice release: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
