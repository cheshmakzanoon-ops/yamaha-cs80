// Bounded MPE accepted-release target retention, not full MIDI/MPE conformance.
// A voice's accepted engine gate (including ARP gates) owns this boundary.
// Channel-pressure matrix and optional CC74 ribbon effects remain global.
#include "core/Engine.h"
#include "core/ControllerCurves.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
std::string context;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s: %s\n", context.c_str(), label); }
}
bool exact(float a, float b)
{
    return std::isfinite(a) && std::isfinite(b)
        && std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}
struct Expression { float bend, pressure, timbre; };
constexpr Expression initial {.125f, .3f, .2f};
constexpr Expression later {-.25f, .8f, .9f};
constexpr Expression other {.0625f, .4f, .6f};
ParamValues patch(int policy = 1, int sustainMode = 0, float quality = 0.0f)
{
    ParamValues p;
    p[P_sourcePerformance] = policy == 0 ? 0.0f : 1.0f;
    p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    p[P_mpeOn] = 1.0f;
    p[P_oversampling] = quality;
    p[P_kasScanTiming] = p[P_arpOn] = 0.0f;
    p[P_analogNoise] = p[P_voiceDispersion] = p[P_drift] = p[P_detune] = 0.0f;
    p[P_chorusOn] = p[P_initBend] = 0.0f;
    p[P_sustainMode] = static_cast<float>(sustainMode);
    p[P_sustainAssign] = p[P_footswitchPlugged] = 1.0f;
    p[P_footswitchPressed] = 0.0f;
    p[P_masterSustain] = 0.0f;
    p[P_keyRelease] = 5.0f;
    p[P_pitchBendRange] = 2.0f;
    p[P_mpeBendRange] = 48.0f;
    p[P_pressureCurve] = p[P_pressureThreshold] = 0.0f;
    p[P_ribbonCcEnable] = 0.0f;
    p[P_mod1Amount] = 1.0f;
    // Only per-voice timbre is connected in the default matrix. No global
    // channel-pressure, shared-pressure or ribbon route can mask isolation.
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_vA)] = p[chParam(rank, CP_fA)] = .001f;
        p[chParam(rank, CP_vS)] = 1.0f;
        p[chParam(rank, CP_vR)] = p[chParam(rank, CP_fR)] = 2.0f;
    }
    return p;
}
void configure(Engine& e, ParamValues p, double rate = 48000.0,
               modern::ModSource source = modern::ModSource::timbre)
{
    e.prepare(rate); e.setParams(p); e.setFilterBatchingEnabled(false);
    modern::ModernConfig config;
    config.slots[0] = {source, modern::ModSource::none,
                      modern::ModDestination::pulseWidth, modern::ModTarget::both};
    e.setModernConfig(config);
}
void rpn(Engine& e, int channel, int number, int value, int cents = -1)
{
    e.controlChange(101, 0.0f, channel);
    e.controlChange(100, number / 127.0f, channel);
    e.controlChange(6, value / 127.0f, channel);
    if (cents >= 0) e.controlChange(38, cents / 127.0f, channel);
}
void layout(Engine& e, int lower = 2, int upper = 2)
{
    rpn(e, 1, 6, 0); rpn(e, 16, 6, 0);
    if (lower) rpn(e, 1, 6, lower);
    if (upper) rpn(e, 16, 6, upper);
}
void sendField(Engine& e, int channel, Expression x, int field)
{
    if (field == 0) e.pitchBend(channel, x.bend);
    if (field == 1) e.channelPressure(channel, x.pressure);
    if (field == 2) e.controlChange(74, x.timbre, channel);
}
void send(Engine& e, int channel, Expression x)
{
    for (int field = 0; field < 3; ++field) sendField(e, channel, x, field);
}
Expression replace(Expression a, Expression b, int field)
{
    if (field == 0) a.bend = b.bend;
    if (field == 1) a.pressure = b.pressure;
    if (field == 2) a.timbre = b.timbre;
    return a;
}
std::vector<float> render(Engine& e, int frames, int block = 128)
{
    std::vector<float> out(static_cast<std::size_t>(2 * frames), std::numeric_limits<float>::quiet_NaN());
    for (int i = 0; i < frames; i += block)
        e.render(out.data() + 2 * i, std::min(block, frames - i));
    check(std::all_of(out.begin(), out.end(), [](float x) { return std::isfinite(x); }),
          "all rendered samples are written and finite");
    return out;
}
bool sameAudio(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), exact);
}
void sounding(const std::vector<float>& a)
{
    check(std::any_of(a.begin(), a.end(), [](float x) { return std::abs(x) > 1.0e-8f; }),
          "measured audio is non-silent");
}
int find(const Engine& e, int note, int channel, bool held)
{
    for (int v = 0; v < 8; ++v)
    {
        const auto info = e.getVoiceInfo(v);
        if (info.active && info.note == note && info.channel == channel && info.held == held) return v;
    }
    return -1;
}
bool witness(const Engine& e, int v, int note, int channel, bool held, bool pedal = false)
{
    const auto info = e.getVoiceInfo(v);
    const bool ok = v >= 0 && info.active && info.note == note && info.channel == channel
        && info.held == held && info.pedalHeld == pedal;
    check(ok, "active/held/pedal/channel/note recipient witness");
    return ok;
}
void targets(const Engine& e, int voice, Expression x, float range = 48.0f,
             float curve = 0.0f, float threshold = 0.0f)
{
    if (voice < 0) { check(false, "target recipient exists"); return; }
    const auto info = e.getVoiceInfo(voice);
    check(exact(info.bend, x.bend * range), "exact event-set bend target");
    const float pressure = curve == 0.0f && threshold <= 0.0f ? x.pressure
        : applyPressureCurve(x.pressure, curve, threshold);
    check(exact(info.pressure, pressure), "exact event-set pressure target");
}
void timbre(const Engine& e, int voice, float value)
{
    for (int rank = 0; rank < 2; ++rank)
        check(exact(e.getModernRankOffsets(voice, rank).pulseWidth, value * modern::kPulseWidthRange),
              "both rank timbre readouts match expected trajectory by bits");
}
// Independent first-interval one-pole oracle. This is only a running readout;
// bend/pressure telemetry above exposes targets, not instantaneous smoothers.
float firstInterval(const Engine& e, float from, float to, int frames, int policy)
{
    if (policy == 0) return to;
    double value = from;
    const double k = -std::expm1(-1.0 / (.005 * e.getInternalSampleRate()));
    for (int i = 0; i < frames * e.getOversamplingFactor(); ++i)
    {
        value += k * (static_cast<double>(to) - value);
        if (std::abs(to - value) <= 1.0e-9 * std::max(1.0, std::abs(static_cast<double>(to)))) value = to;
    }
    return static_cast<float>(value);
}
void release(Engine& e, int note, int channel, int state)
{
    if (state == 2) e.controlChange(64, 1.0f, channel);
    if (state != 0) e.noteOff(note, channel);
}
void memberRecipientMatrix()
{
    struct Zone { int lower, upper, member, unrelated; };
    constexpr std::array<Zone, 4> zones {{{2, 2, 2, 15}, {2, 2, 15, 2},
                                           {15, 0, 16, 2}, {0, 15, 1, 15}}};
    for (int policy : {0, 1}) for (const auto z : zones) for (int sustain : {0, 1})
        for (int state : {0, 1, 2}) for (int field = 0; field < 3; ++field)
        {
            context = "recipients policy=" + std::to_string(policy) + " channel=" + std::to_string(z.member)
                + " sustain=" + std::to_string(sustain) + " state=" + std::to_string(state)
                + " field=" + std::to_string(field);
            Engine e; configure(e, patch(policy, sustain)); layout(e, z.lower, z.upper);
            const auto role = e.getMpeZones().role(z.member);
            check(role == MpeZoneLayout::Role::lowerMember || role == MpeZoneLayout::Role::upperMember,
                  "fixture channel is an actual member including full-zone endpoints");
            send(e, z.member, initial); send(e, z.unrelated, other);
            e.noteOn(60, .8f, z.member); e.noteOn(64, .8f, z.member); e.noteOn(67, .8f, z.unrelated);
            render(e, 128);
            const int v = find(e, 60, z.member, true), sibling = find(e, 64, z.member, true);
            const int untouched = find(e, 67, z.unrelated, true);
            witness(e, v, 60, z.member, true); witness(e, sibling, 64, z.member, true);
            release(e, 60, z.member, state);
            witness(e, v, 60, z.member, state == 0, state == 2);
            // Repeated events must neither thaw a tail nor lose latest raw state.
            sendField(e, z.member, other, field);
            sendField(e, z.member, later, field); sendField(e, z.member, later, field);
            const auto updated = replace(initial, later, field);
            targets(e, v, state == 0 ? updated : initial); targets(e, sibling, updated);
            targets(e, untouched, other);
            render(e, 1);
            timbre(e, v, firstInterval(e, initial.timbre, state == 0 ? updated.timbre : initial.timbre, 1, policy));
            timbre(e, sibling, firstInterval(e, initial.timbre, updated.timbre, 1, policy));
            timbre(e, untouched, other.timbre);
            e.noteOn(72, .8f, z.member); render(e, 1);
            const int fresh = find(e, 72, z.member, true);
            witness(e, fresh, 72, z.member, true); targets(e, fresh, updated); timbre(e, fresh, updated.timbre);
            witness(e, v, 60, z.member, state == 0, state == 2);
            targets(e, v, state == 0 ? updated : initial);
        }
}
void repeatedPitchAndUnison()
{
    for (int policy : {0, 1}) for (int mode : {0, 2, 3, 4}) for (int channel : {2, 15})
    {
        context = "unison/repeat policy=" + std::to_string(policy) + " mode=" + std::to_string(mode)
            + " channel=" + std::to_string(channel);
        Engine e; auto p = patch(policy); p[P_voiceMode] = static_cast<float>(mode);
        configure(e, p); layout(e); send(e, channel, initial);
        e.noteOn(60, .8f, channel); render(e, 128);
        const int count = mode == 0 ? 1 : 1 << (mode - 1);
        for (int v = 0; v < count; ++v) witness(e, v, 60, channel, true);
        e.noteOff(60, channel); send(e, channel, later); render(e, 1);
        for (int v = 0; v < count; ++v)
        {
            witness(e, v, 60, channel, false); targets(e, v, initial); timbre(e, v, initial.timbre);
        }
        e.noteOn(60, .8f, channel); render(e, 1);
        int held = 0;
        for (int v = 0; v < 8; ++v) if (e.getVoiceInfo(v).active && e.getVoiceInfo(v).held)
        {
            ++held; witness(e, v, 60, channel, true); targets(e, v, later); timbre(e, v, later.timbre);
        }
        check(held == count, "same-pitch re-onset allocates the expected unison group using cache");
    }
}
void smoothingContinuation()
{
    for (int policy : {0, 1}) for (int channel : {2, 15}) for (int state : {1, 2})
        for (double rate : {44100.0, 96000.0}) for (float quality : {0.0f, 2.0f})
        {
            context = "tail continuation policy=" + std::to_string(policy) + " channel=" + std::to_string(channel)
                + " state=" + std::to_string(state) + " rate=" + std::to_string(rate)
                + " quality=" + std::to_string(quality);
            Engine actual, reference;
            for (Engine* e : {&actual, &reference})
            {
                configure(*e, patch(policy, 0, quality), rate); layout(*e); send(*e, channel, initial);
                e->noteOn(60, .8f, channel); render(*e, 192);
                send(*e, channel, later); render(*e, 7); // In flight, well before the 5 ms time constant.
                release(*e, 60, channel, state);
                witness(*e, 0, 60, channel, false, state == 2); targets(*e, 0, later);
            }
            const float before = reference.getModernRankOffsets(0, 0).pulseWidth;
            if (policy == 1)
                check(before > initial.timbre * modern::kPulseWidthRange
                      && before < later.timbre * modern::kPulseWidthRange, "release catches in-flight smoothing, not a settled fixture");
            for (int field = 0; field < 3; ++field) sendField(actual, channel, other, field);
            for (int segment = 0; segment < 3; ++segment)
            {
                const auto a = render(actual, 193, 17), b = render(reference, 193, 31);
                sounding(a); sounding(b);
                check(sameAudio(a, b), "tail audio matches independent no-later-event engine by finite bits");
                for (int rank = 0; rank < 2; ++rank)
                    check(exact(actual.getModernRankOffsets(0, rank).pulseWidth,
                                reference.getModernRankOffsets(0, rank).pulseWidth), "retained timbre trajectory matches independent reference");
                targets(actual, 0, later); witness(actual, 0, 60, channel, false, state == 2);
            }
            if (policy == 1)
                check(reference.getModernRankOffsets(0, 0).pulseWidth > before,
                      "running smoother continues toward retained event target after release");
            // No arbitrary reset/RNG assumptions: both independent engines were
            // freshly constructed and received identical histories until the event.
        }
}
void cacheWithChangedLaw()
{
    for (int policy : {0, 1}) for (int channel : {2, 15}) for (bool negotiated : {false, true})
    {
        context = "changed cache law policy=" + std::to_string(policy) + " channel=" + std::to_string(channel)
            + " negotiated=" + std::to_string(negotiated);
        Engine e; auto p = patch(policy); configure(e, p); layout(e); send(e, channel, initial);
        e.noteOn(60, .8f, channel); render(e, 128); e.noteOff(60, channel);
        send(e, channel, later); // Only a released recipient exists: raw cache must still advance.
        p[P_mpeBendRange] = 7.5f; p[P_pressureCurve] = -.6f; p[P_pressureThreshold] = .15f;
        e.setParams(p);
        if (negotiated) rpn(e, channel, 0, 12, 50);
        targets(e, 0, initial); witness(e, 0, 60, channel, false);
        e.noteOn(64, .8f, channel); render(e, 1);
        const int v = find(e, 64, channel, true);
        witness(e, v, 64, channel, true);
        // layout() received MCMs: parameter edits cannot replace their 48-semitone default.
        targets(e, v, later, negotiated ? 12.5f : 48.0f, -.6f, .15f); timbre(e, v, later.timbre);
        targets(e, 0, initial); timbre(e, 0, initial.timbre);
    }
}
void resetException()
{
    for (int policy : {0, 1}) for (int channel : {2, 15}) for (int sustain : {0, 1})
        for (int state : {0, 1, 2})
        {
            context = "CC121 policy=" + std::to_string(policy) + " channel=" + std::to_string(channel)
                + " sustain=" + std::to_string(sustain) + " state=" + std::to_string(state);
            Engine e; configure(e, patch(policy, sustain)); layout(e); send(e, channel, later);
            e.noteOn(60, .8f, channel); render(e, 128); release(e, 60, channel, state);
            witness(e, 0, 60, channel, state == 0, state == 2); targets(e, 0, later);
            e.controlChange(121, 0.0f, channel);
            witness(e, 0, 60, channel, state == 0, false); targets(e, 0, {0, 0, later.timbre});
            render(e, 1); timbre(e, 0, later.timbre);
            e.controlChange(121, 1.0f, channel); targets(e, 0, {0, 0, later.timbre});
            e.noteOn(64, .8f, channel); render(e, 1);
            const int fresh = find(e, 64, channel, true);
            witness(e, fresh, 64, channel, true); targets(e, fresh, {0, 0, later.timbre}); timbre(e, fresh, later.timbre);
        }
}
void managerRoutes(bool pressureCombination)
{
    for (int policy : {0, 1}) for (int manager : {1, 16}) for (int state : {1, 2})
    {
        context = "manager policy=" + std::to_string(policy) + " channel=" + std::to_string(manager)
            + " state=" + std::to_string(state);
        Engine e, reference; const int member = manager == 1 ? 2 : 15, unrelated = manager == 1 ? 15 : 2;
        for (Engine* x : {&e, &reference})
        {
            configure(*x, patch(policy)); layout(*x); send(*x, member, initial); send(*x, unrelated, other);
            x->noteOn(60, .8f, member); x->noteOn(64, .8f, unrelated); x->noteOn(67, .8f, 8);
            render(*x, 128); release(*x, 60, member, state); release(*x, 64, unrelated, state);
        }
        e.pitchBend(manager, .5f);
        render(e, 64); render(reference, 64);
        witness(e, 0, 60, member, false, state == 2);
        const double delta = 12.0 * std::log2(e.getRankInfo(0, 0).oscillatorHz / reference.getRankInfo(0, 0).oscillatorHz);
        const double expected = policy == 0 ? 1.0 : 1.0 - std::exp(-64.0 / (.005 * 48000.0));
        check(std::abs(delta - expected) < 2.0e-5, "manager bend still reaches its released member via unchanged smoothing");
        check(exact(e.getRankInfo(1, 0).oscillatorHz, reference.getRankInfo(1, 0).oscillatorHz),
              "manager bend leaves the other zone unchanged");
        check(exact(e.getRankInfo(2, 0).oscillatorHz, reference.getRankInfo(2, 0).oscillatorHz),
              "manager bend leaves a conventional gap unchanged");
        e.channelPressure(manager, later.pressure); e.controlChange(74, later.timbre, manager);
        targets(e, 0, {initial.bend, later.pressure, later.timbre}); targets(e, 1, other);
        targets(e, 2, {0, 0, 0}); render(e, 1);
        timbre(e, 0, firstInterval(e, initial.timbre, later.timbre, 1, policy));
        timbre(e, 1, other.timbre); timbre(e, 2, 0);
        if (pressureCombination)
        {
            // A falling manager reveals the frozen note-local component, not the
            // last late member event, and cannot affect the other zone or gap.
            e.channelPressure(member, .95f);
            e.channelPressure(manager, .1f);
            targets(e, 0, {initial.bend, initial.pressure, later.timbre});
            targets(e, 1, other); targets(e, 2, {0, 0, 0});
            witness(e, 0, 60, member, false, state == 2);
            e.channelPressure(manager, .6f);
            targets(e, 0, {initial.bend, .6f, later.timbre});
            e.channelPressure(manager, 0.0f);
            targets(e, 0, {initial.bend, initial.pressure, later.timbre});
        }
    }
}
void unchangedRoutes()
{
    for (int policy : {0, 1, 2}) for (bool mpe : {false, true})
    {
        context = "unchanged route policy=" + std::to_string(policy) + " mpe=" + std::to_string(mpe);
        Engine e, reference; auto p = patch(policy); p[P_mpeOn] = mpe ? 1.0f : 0.0f;
        const int channel = mpe && policy != 2 ? 8 : 2;
        for (Engine* x : {&e, &reference})
        {
            configure(*x, p); if (mpe && policy != 2) layout(*x);
            x->noteOn(60, .8f, channel); send(*x, channel, initial); render(*x, 192);
            x->noteOff(60, channel); witness(*x, 0, 60, channel, false);
        }
        send(e, channel, later);
        // PURE keeps direct channel-pressure telemetry but suppresses MIDI bend,
        // CC74 and the modern matrix; conventional bend is global, not mpeBend.
        targets(e, 0, {mpe && policy != 2 ? later.bend : 0.0f, later.pressure, later.timbre});
        render(e, 64); render(reference, 64);
        if (policy == 2) timbre(e, 0, 0.0f);
        else
        {
            check(e.getModernRankOffsets(0, 0).pulseWidth > reference.getModernRankOffsets(0, 0).pulseWidth,
                  "ordinary non-member CC74 still retargets a released voice");
            check(e.getRankInfo(0, 0).oscillatorHz < reference.getRankInfo(0, 0).oscillatorHz,
                  "ordinary non-member bend still moves released pitch");
        }
        witness(e, 0, 60, channel, false);
        e.polyPressure(60, .55f, channel);
        check(exact(e.getVoiceInfo(0).pressure, .55f), "direct poly-pressure still updates a released matching note");
        e.polyPressure(61, .1f, channel);
        check(exact(e.getVoiceInfo(0).pressure, .55f), "poly-pressure remains note-scoped");
    }
    for (int policy : {0, 1}) for (int channel : {2, 15}) for (int state : {1, 2})
    {
        context = "MPE direct poly-pressure policy=" + std::to_string(policy) + " channel=" + std::to_string(channel);
        Engine e; configure(e, patch(policy)); layout(e); send(e, channel, initial);
        e.noteOn(60, .8f, channel); render(e, 128); release(e, 60, channel, state);
        e.polyPressure(60, .65f, channel);
        witness(e, 0, 60, channel, false, state == 2);
        targets(e, 0, {initial.bend, .65f, initial.timbre});
        e.noteOn(64, .8f, channel); render(e, 1);
        const int fresh = find(e, 64, channel, true);
        witness(e, fresh, 64, channel, true); targets(e, fresh, initial); timbre(e, fresh, initial.timbre);
    }
}
void currentModeAndRole()
{
    for (int policy : {0, 1}) for (int channel : {2, 15})
    {
        context = "current effective mode policy=" + std::to_string(policy) + " channel=" + std::to_string(channel);
        Engine e; auto p = patch(policy); configure(e, p); layout(e); send(e, channel, initial);
        e.noteOn(60, .8f, channel); render(e, 128); e.noteOff(60, channel);
        p[P_mpeOn] = 0.0f; e.setParams(p); witness(e, 0, 60, channel, false);
        send(e, channel, later); targets(e, 0, {initial.bend, later.pressure, later.timbre});
        render(e, 1); timbre(e, 0, firstInterval(e, initial.timbre, later.timbre, 1, policy));
        p[P_mpeOn] = 1.0f; e.setParams(p); send(e, channel, other);
        targets(e, 0, {initial.bend, later.pressure, later.timbre});
        render(e, 1); timbre(e, 0, firstInterval(e, initial.timbre, later.timbre, 2, policy));
        e.noteOn(64, .8f, channel); render(e, 1);
        const int fresh = find(e, 64, channel, true);
        witness(e, fresh, 64, channel, true); targets(e, fresh, other); timbre(e, fresh, other.timbre);
    }
    for (int policy : {0, 1}) for (int transition = 0; transition < 5; ++transition)
    {
        context = "current role policy=" + std::to_string(policy) + " transition=" + std::to_string(transition);
        Engine e; configure(e, patch(policy));
        const int channel = transition == 2 ? 1 : transition == 3 ? 16 : transition == 1 ? 15 : 2;
        if (transition == 2) layout(e, 0, 15);
        else if (transition == 3) layout(e, 15, 0);
        else if (transition == 4) layout(e, 0, 0);
        else layout(e);
        e.noteOn(60, .8f, channel); send(e, channel, initial); render(e, 192); e.noteOff(60, channel);
        // Both membership and later role-transfer policy stop old notes.
        // Direct transitions avoid conflating the two cancellation paths.
        if (transition <= 1) layout(e, 0, 0);       // member -> gap
        if (transition == 2) rpn(e, 1, 6, 2);      // upper member ch1 -> lower manager
        if (transition == 3) rpn(e, 16, 6, 2);      // lower member ch16 -> upper manager
        if (transition == 4) layout(e, 2, 2);      // gap -> member
        const auto role = e.getMpeZones().role(channel);
        check(transition == 4 ? role == MpeZoneLayout::Role::lowerMember
              : role == (transition == 2 ? MpeZoneLayout::Role::lowerMaster
                       : transition == 3 ? MpeZoneLayout::Role::upperMaster : MpeZoneLayout::Role::none),
              "current role transition fixture");
        send(e, channel, later);
        check(!e.getVoiceInfo(0).active, "MCM membership or role crossing stops the historical release tail");
        render(e, 64);
        e.noteOn(64, .8f, channel); render(e, 1);
        const int fresh = find(e, 64, channel, true);
        witness(e, fresh, 64, channel, true);
        // Newly active managers inherit their own retained pressure and CC74 floors.
        const float managerPressure = transition == 2 || transition == 3 ? later.pressure : 0.0f;
        const float managerTimbre = transition == 2 || transition == 3 ? later.timbre : 0.0f;
        targets(e, fresh, transition == 4 ? later : Expression{0, managerPressure, managerTimbre});
        timbre(e, fresh, transition == 4 ? later.timbre : managerTimbre);
    }
}
void malformedInputs(bool clampLate)
{
    for (int policy : {0, 1}) for (int channel : {2, 15})
    {
        context = "malformed policy=" + std::to_string(policy) + " channel=" + std::to_string(channel)
            + " clamp=" + std::to_string(clampLate);
        Engine e; configure(e, patch(policy)); layout(e); send(e, channel, initial);
        e.noteOn(60, .8f, channel); e.noteOn(64, .8f, channel); render(e, 128); e.noteOff(60, channel);
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()}) send(e, channel, {invalid, invalid, invalid});
        for (int invalidChannel : {-1, 0, 17, 128}) send(e, invalidChannel, later);
        targets(e, 0, initial); targets(e, 1, initial);
        render(e, 1); timbre(e, 0, initial.timbre); timbre(e, 1, initial.timbre);
        const Expression cached = clampLate ? Expression{-1.0f, 1.0f, 0.0f} : initial;
        if (clampLate)
        {
            send(e, channel, {-5.0f, 5.0f, -3.0f});
            targets(e, 0, initial); targets(e, 1, cached);
        }
        e.noteOn(67, .8f, channel); render(e, 1);
        const int fresh = find(e, 67, channel, true);
        witness(e, fresh, 67, channel, true); targets(e, fresh, cached); timbre(e, fresh, cached.timbre);
        witness(e, 0, 60, channel, false); targets(e, 0, initial); timbre(e, 0, initial.timbre);
    }
}
void globalExceptions()
{
    for (int policy : {0, 1}) for (int channel : {2, 15}) for (bool ribbon : {false, true})
    {
        context = "global exception policy=" + std::to_string(policy) + " channel=" + std::to_string(channel)
            + " ribbon=" + std::to_string(ribbon);
        Engine actual, reference; auto p = patch(policy);
        if (ribbon) { p[P_ribbonCcEnable] = 1.0f; p[P_ribbonCc] = 74.0f; }
        const Expression old = ribbon ? Expression{.125f, .3f, 64.0f / 127.0f} : initial;
        for (Engine* e : {&actual, &reference})
        {
            configure(*e, p, 48000.0, ribbon ? modern::ModSource::ribbon : modern::ModSource::channelPressure);
            layout(*e); send(*e, channel, old); e->noteOn(60, .8f, channel); render(*e, 192); e->noteOff(60, channel);
        }
        if (ribbon) actual.controlChange(74, 1.0f, channel);
        else actual.channelPressure(channel, later.pressure);
        targets(actual, 0, old); witness(actual, 0, 60, channel, false);
        render(actual, 1); render(reference, 1);
        const float from = ribbon ? 0.0f : old.pressure, to = ribbon ? 1.0f : later.pressure;
        timbre(actual, 0, firstInterval(actual, from, to, 1, policy));
        check(actual.getModernRankOffsets(0, 0).pulseWidth > reference.getModernRankOffsets(0, 0).pulseWidth,
              "global matrix channel-pressure/ribbon source still updates on late member events");
        // Matrix readout proves the exception. No complete audio-isolation
        // assertion is made with these intentionally connected global routes.
    }
}
void sustainGateTails()
{
    for (int policy : {0, 1}) for (int sustain : {0, 1}) for (int channel : {2, 15})
    {
        context = "panel sustain tail policy=" + std::to_string(policy) + " sustain=" + std::to_string(sustain)
            + " channel=" + std::to_string(channel);
        Engine e; auto p = patch(policy, sustain); p[P_footswitchPressed] = 1.0f; p[P_masterSustain] = 2.0f;
        configure(e, p); layout(e); send(e, channel, initial); e.noteOn(60, .8f, channel); render(e, 192);
        e.noteOff(60, channel); witness(e, 0, 60, channel, false);
        send(e, channel, later); targets(e, 0, initial);
        const auto audio = render(e, 193); sounding(audio);
        witness(e, 0, 60, channel, false); timbre(e, 0, initial.timbre);
    }
}
void deferredBoundaries()
{
    for (int channel : {2, 15})
    {
        context = "KAS boundary channel=" + std::to_string(channel);
        Engine e; auto p = patch(); p[P_kasScanTiming] = 1.0f;
        configure(e, p); layout(e); send(e, channel, initial); e.noteOn(60, .8f, channel);
        check(find(e, 60, channel, true) < 0, "KAS onset is genuinely queued");
        render(e, 128); const int v = find(e, 60, channel, true); witness(e, v, 60, channel, true);
        e.noteOff(60, channel); witness(e, v, 60, channel, true);
        send(e, channel, later); targets(e, v, later);
        render(e, 48); witness(e, v, 60, channel, false);
        timbre(e, v, firstInterval(e, initial.timbre, later.timbre, 48, 1));
        send(e, channel, other); targets(e, v, later); render(e, 1);
        timbre(e, v, firstInterval(e, initial.timbre, later.timbre, 49, 1));
        e.noteOn(64, .8f, channel); render(e, 128);
        const int fresh = find(e, 64, channel, true);
        witness(e, fresh, 64, channel, true); targets(e, fresh, other); timbre(e, fresh, other.timbre);
    }
    for (int policy : {0, 1}) for (int channel : {2, 15})
    {
        context = "ARP boundary policy=" + std::to_string(policy) + " channel=" + std::to_string(channel);
        Engine e; auto p = patch(policy); p[P_arpOn] = 1.0f; p[P_arpRate] = 16.0f;
        configure(e, p); layout(e); send(e, channel, initial); e.noteOn(60, .8f, channel);
        check(find(e, 60, channel, true) < 0, "ARP source key is not yet a generated gate");
        render(e, 1); const int v = find(e, 60, channel, true);
        witness(e, v, 60, channel, true); targets(e, v, initial); timbre(e, v, initial.timbre);
        send(e, channel, later); targets(e, v, later);
        render(e, 1999); witness(e, v, 60, channel, false);
        // Physical source remains down: only the generated gate has closed.
        send(e, channel, other); targets(e, v, later); render(e, 1);
        timbre(e, v, firstInterval(e, initial.timbre, later.timbre, 2000, policy));
        render(e, 1200); const int fresh = find(e, 60, channel, true);
        witness(e, fresh, 60, channel, true); targets(e, fresh, other); timbre(e, fresh, other.timbre);
        check(fresh != v, "future step has an independent active circuit while prior tail is active");
        witness(e, v, 60, channel, false); targets(e, v, later);
    }
}
} // namespace
int main(int argc, char** argv)
{
    const bool unchangedOnly = argc == 2 && std::string(argv[1]) == "--unchanged";
    if (argc > 1 && !unchangedOnly) { std::fprintf(stderr, "usage: %s [--unchanged]\n", argv[0]); return 2; }
    // This subset must also pass with matching baseline headers and library.
    resetException(); managerRoutes(!unchangedOnly); unchangedRoutes(); malformedInputs(false);
    if (!unchangedOnly)
    {
        memberRecipientMatrix(); repeatedPitchAndUnison(); smoothingContinuation(); cacheWithChangedLaw();
        currentModeAndRole(); malformedInputs(true); globalExceptions(); sustainGateTails(); deferredBoundaries();
    }
    std::printf("MPE released targets%s: %d checks, %d failures\n", unchangedOnly ? " unchanged paths" : "", checks, failures);
    return failures ? 1 : 0;
}
