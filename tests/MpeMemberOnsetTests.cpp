// Optional MPE member onset state, not a full MIDI/MPE compliance test.
#include "core/Engine.h"
#include "core/ControllerCurves.h"
#include "core/VoiceModes.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label); }
}
bool exact(float a, float b)
{
    return std::isfinite(a) && std::isfinite(b)
        && std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}
ParamValues controls(int policy = 1, float quality = 0.0f)
{
    ParamValues p;
    p[P_sourcePerformance] = policy == 0 ? 0.0f : 1.0f;
    p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    p[P_mpeOn] = 1.0f;
    p[P_oversampling] = quality;
    p[P_analogNoise] = p[P_voiceDispersion] = p[P_drift] = p[P_detune] = 0.0f;
    p[P_initBend] = p[P_chorusOn] = p[P_masterSustain] = 0.0f;
    p[P_keyRelease] = 0.5f;
    p[P_pitchBendRange] = 2.0f;
    p[P_mpeBendRange] = 48.0f;
    p[P_mod1Amount] = 1.0f;
    return p;
}
void configure(Engine& e, ParamValues p, double rate = 48000.0)
{
    e.prepare(rate); e.setParams(p);
    modern::ModernConfig config;
    config.slots[0] = {modern::ModSource::timbre, modern::ModSource::none,
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
void layout(Engine& e, int lower, int upper)
{
    rpn(e, 1, 6, 0); rpn(e, 16, 6, 0);
    if (lower) rpn(e, 1, 6, lower);
    if (upper) rpn(e, 16, 6, upper);
}
struct Expression { float bend = .25f, pressure = .7f, timbre = .8f; };
void send(Engine& e, int channel, Expression x)
{
    e.pitchBend(channel, x.bend);
    e.channelPressure(channel, x.pressure);
    e.controlChange(74, x.timbre, channel);
}
std::vector<float> render(Engine& e, int frames, int block = 128)
{
    std::vector<float> result(static_cast<size_t>(2 * frames), std::numeric_limits<float>::quiet_NaN());
    for (int i = 0; i < frames; i += block)
        e.render(result.data() + 2 * i, std::min(block, frames - i));
    bool finite = true;
    for (float x : result) finite &= std::isfinite(x);
    check(finite, "every rendered output is written and finite");
    return result;
}
void sameAudio(const std::vector<float>& a, const std::vector<float>& b)
{
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); ++i) same &= exact(a[i], b[i]);
    check(same, "onset equals explicit same-boundary controller replay by finite audio bits");
}
int find(const Engine& e, int note, int channel)
{
    for (int v = 0; v < 8; ++v)
    {
        const auto info = e.getVoiceInfo(v);
        if (info.active && info.held && info.note == note && info.channel == channel) return v;
    }
    return -1;
}
void expected(const Engine& e, int voice, Expression x, float range = 48.0f,
              float curve = 0.0f, float threshold = 0.0f, bool modern = true)
{
    check(voice >= 0, "expected note owns an active held circuit");
    if (voice < 0) return;
    const auto info = e.getVoiceInfo(voice);
    if (!exact(info.bend, x.bend * range))
        std::printf("CONTEXT: note=%d channel=%d actual bend=%g expected=%g policy=%d factor=%d\n",
                    info.note, info.channel, info.bend, x.bend * range, (int)e.getRenderPolicy(), e.getOversamplingFactor());
    check(exact(info.bend, x.bend * range), "onset uses latest raw member bend and current range");
    const float pressure = curve == 0.0f && threshold <= 0.0f ? x.pressure
                          : applyPressureCurve(x.pressure, curve, threshold);
    check(exact(info.pressure, pressure), "onset applies current pressure curve exactly once");
    for (int rank = 0; rank < 2; ++rank)
        check(exact(e.getModernRankOffsets(voice, rank).pulseWidth,
                    modern ? x.timbre * modern::kPulseWidthRange : 0.0f),
              "both ranks see the retained member timbre through the existing matrix");
}
void pairOnset(Engine& a, Engine& b, int channel, Expression x, int note = 60,
               int frames = 192, float range = 48.0f, float curve = 0.0f, float threshold = 0.0f)
{
    send(a, channel, x);
    a.noteOn(note, .8f, channel);
    b.noteOn(note, .8f, channel);
    send(b, channel, x); // Independent ordinary-event behavioral reference.
    const auto actualAudio = render(a, frames, 1);
    sameAudio(actualAudio, render(b, frames, 23));
    if (frames >= 192)
        check(std::any_of(actualAudio.begin(), actualAudio.end(), [](float v) { return v != 0.0f; }),
              "onset equality includes an actually sounding measured segment");
    expected(a, find(a, note, channel), x, range, curve, threshold);
    expected(b, find(b, note, channel), x, range, curve, threshold);
}
void allMemberChannels()
{
    constexpr std::array<std::array<int, 2>, 4> zones {{{15, 0}, {0, 15}, {7, 7}, {2, 2}}};
    for (int policy : {0, 1})
        for (auto z : zones)
            for (int channel = 1; channel <= 16; ++channel)
            {
                Engine a, b; configure(a, controls(policy)); configure(b, controls(policy));
                layout(a, z[0], z[1]); layout(b, z[0], z[1]);
                const auto role = a.getMpeZones().role(channel);
                if (role != MpeZoneLayout::Role::lowerMember && role != MpeZoneLayout::Role::upperMember) continue;
                const Expression x {channel % 2 ? -.125f : .25f, channel / 17.0f, (17-channel) / 17.0f};
                pairOnset(a, b, channel, x);
                send(a, channel, {-.5f, .2f, .3f}); send(b, channel, {-.5f, .2f, .3f});
                sameAudio(render(a, 257, 17), render(b, 257, 64));
            }
}
void ratesCurvesAndRanges()
{
    for (int policy : {0, 1})
        for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
            for (float quality : {0.0f, 2.0f})
            {
                Engine a, b; auto p = controls(policy, quality);
                p[P_pressureCurve] = -.6f; p[P_pressureThreshold] = .2f;
                configure(a, p, rate); configure(b, p, rate);
                layout(a, 2, 2); layout(b, 2, 2);
                rpn(a, 15, 0, 7, 25); rpn(b, 15, 0, 7, 25);
                // A manager's existing retained pitch adds independently.
                a.pitchBend(16, -.5f); b.pitchBend(16, -.5f);
                pairOnset(a, b, 15, {.25f, .7f, .8f}, 60, 64, 7.25f, -.6f, .2f);
            }
    for (int policy : {0, 1})
    {
        Engine a, b; auto p = controls(policy); configure(a, p); configure(b, p);
        const Expression x {-.25f, .6f, .3f};
        send(a, 2, x);
        // Retain raw, not already-curved pressure or pre-multiplied bend.
        p[P_mpeBendRange] = 7.5f; p[P_pressureCurve] = .7f; p[P_pressureThreshold] = .15f;
        a.setParams(p); b.setParams(p);
        a.noteOn(60, .8f, 2); b.noteOn(60, .8f, 2); send(b, 2, x);
        sameAudio(render(a, 64), render(b, 64));
        expected(a, find(a, 60, 2), x, 7.5f, .7f, .15f);
        rpn(a, 2, 0, 12, 50); rpn(b, 2, 0, 12, 50);
        check(exact(a.getVoiceInfo(find(a, 60, 2)).bend, -.25f * 12.5f),
              "range negotiation rescales the held member from its retained raw bend");
        a.noteOn(61, .8f, 2); b.noteOn(61, .8f, 2); send(b, 2, x);
        // The held older note and new onset now share the updated range.
        render(a, 1); render(b, 1);
        expected(a, find(a, 61, 2), x, 12.5f, .7f, .15f);
        rpn(a, 1, 6, 15); // Same role retains raw expression; MCM reinstates 48 semitones.
        a.noteOn(62, .8f, 2); render(a, 1);
        expected(a, find(a, 62, 2), x, 48.0f, .7f, .15f);
    }
}
void isolationAndDefaults()
{
    for (int policy : {0, 1})
    {
        Engine e; configure(e, controls(policy)); layout(e, 2, 2);
        const Expression lower {-.25f, .3f, .4f}, upper {.125f, .8f, .6f};
        send(e, 2, lower); send(e, 15, upper);
        e.noteOn(60, .8f, 2); e.noteOn(61, .8f, 15); e.noteOn(62, .8f, 3);
        render(e, 1);
        expected(e, find(e, 60, 2), lower); expected(e, find(e, 61, 15), upper);
        expected(e, find(e, 62, 3), {0, 0, 0});
        e.polyPressure(64, .9f, 3); e.noteOn(64, .8f, 3); render(e, 1);
        expected(e, find(e, 64, 3), {0, 0, 0});
        // An active manager inherits its retained pressure and CC74 floors;
        // gap expression and both channels' bend onset defaults stay neutral.
        send(e, 1, {.25f, .9f, .9f}); send(e, 8, {.25f, .9f, .9f});
        e.noteOn(65, .8f, 1); e.noteOn(66, .8f, 8); render(e, 1);
        expected(e, find(e, 65, 1), {0, .9f, .9f}); expected(e, find(e, 66, 8), {0, 0, 0});
    }
    for (int policy : {0, 1, 2})
    {
        Engine e; auto p = controls(policy); p[P_mpeOn] = policy == 2 ? 1.0f : 0.0f;
        configure(e, p); send(e, 2, {.25f, .7f, .8f});
        e.noteOn(60, .8f, 2); render(e, 1);
        expected(e, find(e, 60, 2), {0, 0, 0}, 48.0f, 0.0f, 0.0f, policy != 2);
    }
}
void resetAndRoleLifecycle()
{
    for (int policy : {0, 1})
        for (int transition = 0; transition < 9; ++transition)
        {
            Engine e; auto p = controls(policy); configure(e, p); layout(e, 2, 2);
            send(e, 2, {.25f, .7f, .8f});
            if (transition == 0) e.reset();
            if (transition == 1) { p[P_mpeOn] = 0; e.setParams(p); send(e, 2, {.5f, .9f, .9f}); p[P_mpeOn] = 1; e.setParams(p); }
            if (transition == 2) { rpn(e, 1, 6, 0); rpn(e, 1, 6, 2); }
            if (transition == 3) { rpn(e, 16, 6, 15); rpn(e, 1, 6, 2); }
            if (transition == 4) { p[P_parityMode] = 1; e.setParams(p); send(e, 2, {.5f, .9f, .9f}); p[P_parityMode] = 0; e.setParams(p); }
            if (transition == 5) { p[P_sourcePerformance] = policy == 0 ? 1.0f : 0.0f; e.setParams(p); }
            if (transition == 6) e.prepare(96000.0);
            if (transition == 7) { p[P_oversampling] = 2; e.setParams(p); }
            if (transition == 8) e.setNonRealtime(true);
            e.noteOn(60, .8f, 2); render(e, 1);
            // Legacy ignores the offline override, but explicit HQ still
            // changes its factor and resets. All other transitions clear state.
            const bool retains = policy == 0 && transition == 8;
            expected(e, find(e, 60, 2), retains ? Expression{.25f, .7f, .8f} : Expression{0, 0, 0});
        }
    for (int policy : {0, 1})
    {
        Engine e; configure(e, controls(policy)); layout(e, 2, 2);
        send(e, 2, {.25f, .7f, .8f}); send(e, 3, {-.25f, .3f, .4f});
        e.controlChange(121, 0.0f, 2);
        e.noteOn(60, .8f, 2); e.noteOn(61, .8f, 3); render(e, 1);
        expected(e, find(e, 60, 2), {0, 0, .8f}); expected(e, find(e, 61, 3), {-.25f, .3f, .4f});
        // Manager reset does not rewrite independent member retention.
        e.controlChange(121, 0.0f, 1); e.noteOn(62, .8f, 3); render(e, 1);
        expected(e, find(e, 62, 3), {-.25f, .3f, .4f});
        // Channel-mode messages keep controllers; they are not Engine::reset.
        e.allNotesOff(false, 3); e.noteOn(63, .8f, 3); render(e, 1);
        expected(e, find(e, 63, 3), {-.25f, .3f, .4f});
    }
}
void malformedAndClamped()
{
    for (int policy : {0, 1})
    {
        Engine e; configure(e, controls(policy));
        send(e, 2, {.25f, .7f, .8f});
        for (float x : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) send(e, 2, {x, x, x});
        for (int channel : {-1, 0, 17}) send(e, channel, {-.5f, .1f, .1f});
        e.noteOn(60, .8f, 2); render(e, 1); expected(e, find(e, 60, 2), {.25f, .7f, .8f});
        send(e, 3, {-5.0f, 5.0f, -3.0f}); e.noteOn(61, .8f, 3); render(e, 1);
        expected(e, find(e, 61, 3), {-1.0f, 1.0f, 0.0f});
    }
}
void reuseAndDeferredAllocation()
{
    for (int policy : {0, 1})
        for (int mode : {0, 2, 3, 4})
        {
            Engine e; auto p = controls(policy); p[P_voiceMode] = float(mode); configure(e, p);
            const Expression x {-.125f, .4f, .6f}; send(e, 2, x);
            e.noteOn(60, .8f, 2); render(e, 1);
            const int count = mode == 0 ? 1 : 1 << (mode - 1);
            for (int i = 0; i < count; ++i) expected(e, i, x);
            e.noteOff(60, 2); send(e, 2, {.25f, .7f, .8f});
            e.noteOn(61, .8f, 2); render(e, 1);
            for (int i = 0; i < 8; ++i) if (e.getVoiceInfo(i).held) expected(e, i, {.25f, .7f, .8f});
        }
    for (int mechanism = 0; mechanism < 3; ++mechanism)
    {
        Engine e; auto p = controls();
        if (mechanism == 0) p[P_kasScanTiming] = 1;
        if (mechanism == 2) { p[P_arpOn] = 1; p[P_arpRate] = 20; }
        configure(e, p);
        if (mechanism == 1) for (int n = 40; n < 48; ++n) e.noteOn(n, .8f, 3);
        send(e, 2, {.25f, .7f, .8f}); e.noteOn(60, .8f, 2);
        check(find(e, 60, 2) < 0, "KAS/waiter/arp onset is actually deferred");
        send(e, 2, {-.125f, .4f, .6f});
        if (mechanism == 1) e.noteOff(40, 3);
        render(e, 128);
        expected(e, find(e, 60, 2), {-.125f, .4f, .6f});
    }
}
void activeTwoInstruments()
{
    for (int policy : {0, 1})
    {
        Engine first, second; auto p = controls(policy); p[P_voiceMode] = 1;
        configure(first, p); configure(second, p);
        for (int n = 48; n < 56; ++n) SixteenVoiceRouter::noteOnTarget(first, second).noteOn(n, .8f, 3);
        check(!first.hasFreeCircuit() && second.hasFreeCircuit(), "primary is full before extension routing");
        const Expression x {-.25f, .7f, .8f};
        // The processor forwards controllers to both ALREADY-ACTIVE engines.
        send(first, 2, x); send(second, 2, x);
        SixteenVoiceRouter::noteOnTarget(first, second).noteOn(60, .8f, 2);
        render(first, 1); render(second, 1);
        check(find(first, 60, 2) < 0, "ninth note belongs only to the extension");
        expected(second, find(second, 60, 2), x);
    }
}
}
int main()
{
    allMemberChannels(); ratesCurvesAndRanges(); isolationAndDefaults();
    resetAndRoleLifecycle(); malformedAndClamped(); reuseAndDeferredAllocation(); activeTwoInstruments();
    std::printf("MPE member onset: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
