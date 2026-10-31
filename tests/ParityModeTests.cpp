// W0 feature-isolation contract only. No comparison to an original instrument.
#include "core/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& name)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", name.c_str()); }
}
void prepare(Engine& e, const ParamValues& p)
{
    e.setParams(p);
    e.prepare(48000.0);
}
std::vector<float> render(Engine& e, int frames = 4096)
{
    std::vector<float> x((size_t) frames * 2);
    e.render(x.data(), frames);
    return x;
}
std::vector<float> phrase(const ParamValues& p, bool controllers = false)
{
    Engine e;
    prepare(e, p);
    e.noteOn(60, 0.8f, 2);
    if (controllers)
    {
        e.pitchBend(1, 0.8f);
        e.pitchBend(2, -0.9f);
        for (int cc : { 1, 11, 64, 74 }) e.controlChange(cc, 0.9f, 2);
    }
    auto a = render(e);
    e.noteOff(60, 2);
    auto b = render(e);
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
}
int main()
{
    ParamValues p;
    check(P_parityMode == 137 && P_numParams >= 139, "core append-only index");
    check(paramId(P_parityMode) == "parity_mode" && p[P_parityMode] == 0.0f,
          "stable ID and neutral default");
    auto explicitOff = p;
    explicitOff[P_parityMode] = 0.0f;
    check(phrase(p) == phrase(explicitOff), "default equals explicit off");
    p[P_parityMode] = 1.0f;
    const auto reference = phrase(p);
    bool audible = false;
    for (float x : reference) audible = audible || std::abs(x) > 1.0e-5f;
    check(audible, "isolation fixture is audible");

    // Isolation must mean neutral, not merely any invariant replacement value.
    auto neutral = p;
    neutral[P_parityMode] = 0.0f;
    neutral[P_keyRelease] = 0.0f;
    neutral[P_voiceDispersion] = neutral[P_drift] = neutral[P_analogNoise] = 0.0f;
    Engine isolated, neutralModern;
    prepare(isolated, p); prepare(neutralModern, neutral);
    isolated.noteOn(60, 0.8f); neutralModern.noteOn(60, 0.8f);
    (void) render(isolated); (void) render(neutralModern);
    for (int rank = 0; rank < 2; ++rank)
    {
        const auto a = isolated.getRankInfo(0, rank), b = neutralModern.getRankInfo(0, rank);
        check(a.hpfEnabled && a.lpfEnabled, "both filters forced on");
        check(a.oscillatorHz == b.oscillatorHz, "neutral tuning and macro width");
        // Different precision backends need their own neutral-time oracle;
        // equality across modes would falsely demand legacy rounding in reconstruction.
        for (bool reconstruction : {false, true})
        {
            auto value = [&](int id) { return p[chParam(rank, id)]; };
            Adsr amp; FilterEnv filter;
            amp.noteOn(value(CP_vA), value(CP_vD), value(CP_vS), value(CP_vR));
            filter.noteOn(filterInitialOffset(value(CP_fIl)), filterAttackOffset(value(CP_fAl)),
                          value(CP_fA), value(CP_fD), value(CP_fR), 0.0f);
            for (int sample = 0; sample < 8192; ++sample)
            {
                amp.tick(96000.0f, true, reconstruction);
                filter.tick(96000.0f, true, reconstruction);
            }
            const auto actual = reconstruction ? a : b;
            check(actual.ampEnvelope == amp.getValue() && actual.filterEnvelope == filter.getValue(),
                  "neutral macro envelope times match mode-specific neutral primitive");
        }
    }

    // Remove only the envelope contribution for this cross-mode brightness
    // assertion, so distinct state rounding cannot masquerade as macro leakage.
    auto flatIsolated = p, flatModern = neutral;
    for (int rank = 0; rank < 2; ++rank)
        for (int control : {CP_fIl, CP_fAl})
            flatIsolated[chParam(rank, control)] = flatModern[chParam(rank, control)] = 0.0f;
    prepare(isolated, flatIsolated); prepare(neutralModern, flatModern);
    isolated.noteOn(60, 0.8f); neutralModern.noteOn(60, 0.8f);
    (void) render(isolated); (void) render(neutralModern);
    for (int rank = 0; rank < 2; ++rank)
    {
        const auto a = isolated.getRankInfo(0, rank), b = neutralModern.getRankInfo(0, rank);
        check(a.filterEnvelope == 0.0f && b.filterEnvelope == 0.0f,
              "brightness fixture has no envelope contribution");
        check(a.lpfHz == b.lpfHz && a.hpfHz == b.hpfHz, "neutral macro brightness");
    }

    // Each independent extension must be inert, including its maximum value.
    const int inert[] = {
        P_sourcePerformance, P_keyRelease, P_subOriginalMode, P_subWave, P_subSpeed,
        P_outputMode, P_voiceDispersion, P_drift, P_analogNoise,
        P_mpeOn, P_pitchBendRange, P_mpeBendRange, P_tuningRef,
        P_modWheelSub, P_modWheelVcf,
        P_lfo1Rate, P_lfo1Depth, P_lfo1Dest,
        P_lfo2Rate, P_lfo2Depth, P_lfo2Dest,
        P_lfo3Rate, P_lfo3Depth, P_lfo3Dest,
        P_fx1Type, P_fx1Mix, P_fx2Type, P_fx2Mix, P_fx3Type, P_fx3Mix,
        P_arpOn, P_arpMode, P_arpRate, P_arpOctaves,
        P_macro1, P_macro2, P_macro3, P_macro4,
        chParam(0, CP_hpfOn), chParam(0, CP_lpfOn), chParam(0, CP_pan),
        chParam(1, CP_hpfOn), chParam(1, CP_lpfOn), chParam(1, CP_pan)
    };
    for (int id : inert)
    {
        for (float value : {paramDef(id).min, paramDef(id).max})
        {
            auto changed = p;
            changed[id] = value;
            check(phrase(changed) == reference, "inert " + paramId(id) + " " + std::to_string(value));
        }
    }
    // Depth and destination/type together catch dormant-route false positives.
    auto busy = p;
    for (int id : inert) busy[id] = paramDef(id).max;
    busy[P_lfo1Dest] = 1; busy[P_lfo2Dest] = 2; busy[P_lfo3Dest] = 3;
    for (int type = 1; type <= 5; ++type)
    {
        busy[P_fx1Type] = busy[P_fx2Type] = busy[P_fx3Type] = (float) type;
        check(phrase(busy, true) == reference, "combined extensions and controllers type " + std::to_string(type));
    }
    check(phrase(p, true) == reference, "CC1/11/64/74 and pitch wheel ignored");
    auto ring = p;
    ring[P_ringAmount] = 1.0f; ring[P_ringSpeed] = 200.0f;
    const auto ringReference = phrase(ring);
    ring[P_ringSpeed] = 500.0f;
    check(phrase(ring) == ringReference, "ring base speed capped at 200 Hz");
    ring[P_ringSpeed] = 100.0f;
    check(phrase(ring) != ringReference, "original ring speed still active");

    Engine e;
    prepare(e, busy);
    check(e.getParams().v == busy.v, "effective mask never rewrites stored parameters");
    check(e.isParityMode(), "mode diagnostic");
    check(e.getLatencySamples() == 63 && e.getInternalSampleRate() == 96000.0,
          "explicit retained numerical backend and latency");
    e.noteOn(60, 0.8f, 2);
    e.controlChange(64, 1.0f, 2);
    e.noteOff(60, 2);
    check(!e.getVoiceInfo(0).pedalHeld && !e.getVoiceInfo(0).held, "no MIDI pedal hold");
    e.noteOn(64, 0.8f, 2);
    e.controlChange(120, 0.0f, 2);
    check(e.activeVoices() == 0, "MIDI panic remains available");

    // Switching modes has a defined hard reset, not stale modern envelopes/tails.
    auto modern = busy; modern[P_parityMode] = 0.0f; modern[P_arpOn] = 0.0f;
    prepare(e, modern);
    e.noteOn(60, 0.8f, 2); e.controlChange(64, 1.0f, 2);
    e.controlChange(11, 0.0f, 2); e.pitchBend(2, 1.0f);
    (void) render(e);
    e.setParams(p);
    check(e.activeVoices() == 0, "engagement clears pre-existing voices");
    auto silence = render(e);
    check(std::all_of(silence.begin(), silence.end(), [](float x) {return x == 0.0f;}),
          "engagement clears modern tails and decimator history");
    e.noteOn(60, 0.8f, 2); e.noteOff(60, 2);
    check(!e.getVoiceInfo(0).pedalHeld, "engagement clears MIDI pedal state");
    e.setParams(modern);
    check(e.activeVoices() == 0, "disengagement resets notes too");

    auto tape = neutral;
    tape[P_fx1Type] = 3.0f; tape[P_fx1Mix] = 1.0f;
    prepare(e, tape); e.noteOn(60, 0.8f);
    (void) render(e, 24000); // Longer than the tape delay: excite damping states.
    e.setParams(p);
    auto entry = render(e, 1024);
    check(std::all_of(entry.begin(), entry.end(), [](float x) { return x == 0.0f; }),
          "tape-delay entry clears buffers");
    e.setParams(tape);
    auto exit = render(e, 24000);
    check(std::all_of(exit.begin(), exit.end(), [](float x) { return x == 0.0f; }),
          "tape-delay exit cannot resurrect damping history");

    // Direct original-controller proxies survive; only optional MIDI mappings go.
    // PRA dry X is circuit-conditioned and changes with pedal resistance, so
    // cross-pedal output is no longer a pure gain ratio. Keep the pedal identical
    // and scale the source after the nonlinear voice filters instead: this still
    // proves the isolated downstream path has no output soft clip. The separate
    // PraWahIntegrationTests full-pipeline oracle proves downstream expression gain.
    auto quieter = p;
    for (int rank = 0; rank < 2; ++rank)
        quieter[chParam(rank, CP_vLevel)] *= 0.25f;
    Engine full, pedal;
    prepare(full, p); prepare(pedal, quieter);
    full.noteOn(60, 0.8f); pedal.noteOn(60, 0.8f);
    full.setExpression(0.25f); pedal.setExpression(0.25f);
    auto a = render(full), b = render(pedal);
    double error = 0;
    for (size_t i = 0; i < a.size(); ++i) error = std::max(error, std::abs((double)b[i] - a[i] * 0.25));
    check(error < 1.0e-8, "same-pedal source scaling remains linear without output soft clip");
    const double targetLog = std::log(1000000.0) + 0.25 * (std::log(2000.0)-std::log(1000000.0));
    check(std::abs(std::log(pedal.getPraWahState(0).targetOhms)-targetLog) < 1.0e-12,
          "direct physical expression remains active despite isolated MIDI CC11 mask");
    prepare(e, p); e.noteOn(60, 0.8f); e.polyPressure(60, 0.7f);
    check(e.getVoiceInfo(0).pressure == 0.7f, "direct poly pressure retained");
    e.setRibbon(0.5f); (void) render(e);
    check(e.getVoiceInfo(0).ribbonRatio > 1.0f, "ribbon retained");

    // EXT remains an explicit transport exception: the original EXT function
    // can be exercised, without asserting a calibrated voltage conversion.
    auto ext = p; ext[P_subOriginalFunction] = 5; ext[P_subExtLevel] = 1.0f; ext[P_subVco] = 1.0f;
    Engine disconnected, connected;
    prepare(disconnected, ext); prepare(connected, ext);
    disconnected.noteOn(60, 0.8f); connected.noteOn(60, 0.8f);
    std::vector<float> input(4096, 0.5f), output(8192);
    connected.render(output.data(), 4096, input.data());
    check(output != render(disconnected), "physical EXT function retained via digital transport exception");

    std::printf("Parity mode: %d checks, %d failures (feature isolation, not calibrated parity)\n", checks, failures);
    return failures ? 1 : 0;
}
