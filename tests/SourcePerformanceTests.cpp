// Source-performance policy v1: software transport contracts, not hardware parity.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
void near(double actual, double expected, double tolerance, const char* message)
{ check(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message); }
ControlFrame fixture()
{
    ControlFrame f;
    f.effectiveValues[P_sourcePerformance] = 1.0f;
    f.hardwareSources = {1, 3};
    auto& p = f.effectiveValues;
    p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
    p[P_subVco] = p[P_subVcf] = p[P_subVca] = 0.0f;
    p[P_keyRelease] = p[P_masterSustain] = p[P_ringAmount] = 0.0f;
    p[P_subAtVco] = p[P_subAtVcf] = p[P_subAtSpeed] = 0.0f;
    p[P_detune] = 0.0f;
    return f;
}
void init(Engine& e, double rate, const ControlFrame& f)
{
    e.prepare(rate);
    check(e.setControlFrame(f), "valid source-performance frame accepted");
    check(e.getRenderPolicy() == RenderPolicy::sourcePerformance && !e.isParityMode(),
          "source-performance is not isolation");
    check(e.getLatencySamples() == 63 && e.getInternalSampleRate() == rate * 2.0,
          "source-performance retains numerical backend and host latency");
}
std::vector<float> render(Engine& e, int frames = 256)
{
    std::vector<float> audio((size_t) frames * 2);
    e.render(audio.data(), frames);
    check(std::all_of(audio.begin(), audio.end(), [](float x) { return std::isfinite(x); }),
          "source-performance render finite");
    return audio;
}
bool same(Engine::RankEnvelopeState a, Engine::RankEnvelopeState b)
{
    return a.amplitude.stage == b.amplitude.stage && a.amplitude.value == b.amplitude.value
        && a.amplitude.elapsed == b.amplitude.elapsed && a.filter.stage == b.filter.stage
        && a.filter.value == b.filter.value && a.filter.elapsed == b.filter.elapsed;
}
void controllerTransports(double rate)
{
    auto f = fixture(); f.effectiveValues[P_mpeOn] = 1.0f;
    Engine e; init(e, rate, f);
    e.noteOn(60, .7f, 2); e.noteOn(60, .7f, 3); render(e);
    const auto frequency = e.getRankInfo(0, 0).oscillatorHz;
    e.pitchBend(2, .5f); render(e, 1);
    near(e.getRankInfo(0, 0).oscillatorHz / frequency, 4.0, 1e-5,
         "MPE member bend uses configured48-semitone range");
    near(e.getRankInfo(1, 0).oscillatorHz, frequency, 1e-5,
         "MPE member bend does not bend another member");
    e.pitchBend(1, .5f); render(e, 1);
    near(e.getRankInfo(1, 0).oscillatorHz / frequency, std::exp2(1.0 / 12.0), 1e-6,
         "master bend is the existing global transport");
    e.polyPressure(60, .8f, 2); e.channelPressure(3, .3f); render(e, 1);
    near(e.getVoiceInfo(0).pressure, .8f, 1e-7, "poly pressure addresses member identity");
    near(e.getVoiceInfo(1).pressure, .3f, 1e-7, "channel pressure addresses its member");
    e.controlChange(64, 1, 1); e.controlChange(64, 1, 2);
    e.noteOff(60, 2); e.noteOff(60, 3);
    check(e.getVoiceInfo(0).pedalHeld && e.getVoiceInfo(1).pedalHeld,
          "MPE master sustain holds both members");
    e.controlChange(64, 0, 1);
    check(e.getVoiceInfo(0).pedalHeld && !e.getVoiceInfo(1).pedalHeld,
          "master sustain release respects the remaining member pedal");
    e.controlChange(64, 0, 2);
    check(!e.getVoiceInfo(0).pedalHeld, "member sustain releases its voice");
    e.noteOn(64, .8f, 2); e.noteOn(67, .8f, 3);
    e.controlChange(120, 0, 2);
    bool member3 = false;
    for (int i = 0; i < 8; ++i)
    {
        const auto voice = e.getVoiceInfo(i);
        check(!voice.active || voice.channel != 2, "channel panic clears only addressed member");
        member3 = member3 || (voice.active && voice.channel == 3);
    }
    check(member3, "channel panic preserves other member");

    // Compare same-time loaded-node readouts, not merely non-identical audio.
    Engine baseline, changed;
    f.effectiveValues[P_modWheelSub] = f.effectiveValues[P_modWheelVcf] = 1;
    init(baseline, rate, f); init(changed, rate, f);
    for (auto* engine : {&baseline, &changed})
    { engine->noteOn(60, .7f, 2); engine->noteOn(64, .5f, 3); render(*engine); }
    changed.controlChange(74, .5f, 2);
    render(baseline, 1); render(changed, 1);
    near(changed.getRankInfo(0, 0).lpfHz / baseline.getRankInfo(0, 0).lpfHz,
         std::exp2(.4), 2e-6, "CC74 adds exactly its per-member0.4-octave transport");
    near(changed.getRankInfo(1, 0).lpfHz, baseline.getRankInfo(1, 0).lpfHz, 1e-4,
         "CC74 does not directly offset another member cutoff");
    near(changed.getSourceCutoffReadout(0).effectiveCv[0][1],
         baseline.getSourceCutoffReadout(0).effectiveCv[0][1], 1e-12,
         "CC74 does not double-modulate the source target network");
    changed.controlChange(74, 0, 2); changed.controlChange(1, .5f, 3);
    render(baseline, 1); render(changed, 1);
    near(changed.getRankInfo(0, 0).lpfHz / baseline.getRankInfo(0, 0).lpfHz,
         std::exp2(.75), 2e-6, "CC1 adds existing global filter transport exactly once");
    near(changed.getRankInfo(0, 0).oscillatorHz / baseline.getRankInfo(0, 0).oscillatorHz,
         std::exp2(baseline.getSubOscillatorValue() / 12.0), 2e-6,
         "CC1 adds existing global SUB pitch transport exactly once");

    Engine midiPedal, directPedal;
    f.effectiveValues[P_footMode] = 1;
    init(midiPedal, rate, f); init(directPedal, rate, f);
    midiPedal.controlChange(11, .35f, 2); directPedal.setExpression(.35f);
    midiPedal.noteOn(60, .7f, 2); directPedal.noteOn(60, .7f, 2);
    check(render(midiPedal) == render(directPedal), "CC11 is the same global PRA/expression proxy as direct pedal");
    near(midiPedal.getPraWahState(0).targetOhms, directPedal.getPraWahState(0).targetOhms,
         1e-8, "CC11 drives the source PRA pedal, not a disconnected legacy SVF");
}
void macroTransports(double rate)
{
    for (int macro : {P_macro1, P_macro2, P_macro3, P_macro4})
    {
        auto a = fixture(), b = a;
        a.effectiveValues[P_detune] = b.effectiveValues[P_detune] = 20;
        b.effectiveValues[macro] = macro == P_macro3 ? 0.0f : 1.0f;
        Engine base, moved; init(base, rate, a); init(moved, rate, b);
        base.noteOn(60, .8f, 2); moved.noteOn(60, .8f, 2);
        const auto normal = render(base), altered = render(moved);
        const auto baseRank = base.getRankInfo(0, 0), movedRank = moved.getRankInfo(0, 0);
        check(normal != altered, "each configured macro has an audible non-inert route");
        if (macro == P_macro1)
        {
            near(movedRank.lpfHz / baseRank.lpfHz, std::exp2(.75), 2e-6,
                 "brightness macro is exactly one post-network octave offset");
            near(moved.getSourceCutoffReadout(0).effectiveCv[0][1],
                 base.getSourceCutoffReadout(0).effectiveCv[0][1], 1e-12,
                 "brightness macro leaves loaded source targets untouched");
        }
        if (macro == P_macro2)
        {
            source_factory::ToneControls tone;
            check(source_factory::compileTone(0, a.hardwareSources[0], tone), "macro envelope oracle compiles tone");
            const float scale = std::pow(2.0f, -1.1f);
            Adsr oracle;
            oracle.noteOn(std::max(.001f, (float) tone.amplitudeEnvelope.attackSeconds * scale),
                          std::max(.001f, (float) tone.amplitudeEnvelope.decaySeconds * scale),
                          (float) tone.amplitudeEnvelope.sustainRatio,
                          std::max(.001f, (float) tone.amplitudeEnvelope.releaseSeconds * scale));
            for (int sample = 0; sample < 512; ++sample) oracle.tick((float) rate * 2, true, true);
            check(movedRank.ampEnvelope == oracle.getValue(),
                  "motion macro scales compiled amplitude times on the precise backend");
            const auto before = moved.getRankEnvelopeState(0, 0);
            b.effectiveValues[P_macro2] = 0;
            check(moved.setControlFrame(b) && same(before, moved.getRankEnvelopeState(0, 0)),
                  "live motion change preserves envelope value/stage/elapsed time");
            render(moved);
            check(moved.getRankEnvelopeState(0, 0).amplitude.elapsed > before.amplitude.elapsed,
                  "live timing change does not restart the envelope clock");
        }
        if (macro == P_macro3)
        {
            near(moved.getRankInfo(0, 1).oscillatorHz / base.getRankInfo(0, 1).oscillatorHz,
                 std::exp2(-12.0 / 1200.0), 2e-6, "width macro subtracts exactly12cents without negative dispersion");
            check(movedRank.oscillatorHz == baseRank.oscillatorHz,
                  "width macro detune transport belongs to rankII");
        }
        if (macro == P_macro4)
        {
            near(movedRank.lpfHz / baseRank.lpfHz,
                 std::exp2(base.getSubOscillatorValue() * .20 * 1.5), 2e-6,
                 "space macro adds SUB filter depth once after source targets");
            near(moved.getSourceSharedReadout().levelGain[0][0],
                 base.getSourceSharedReadout().levelGain[0][0], 1e-12,
                 "space macro amplitude overlay does not rewrite loaded level targets");
        }
    }
}
void routingAndExtensions(double rate)
{
    auto f = fixture();
    f.effectiveValues[chParam(0, CP_hpfOn)] = f.effectiveValues[chParam(0, CP_lpfOn)] = 0;
    f.effectiveValues[chParam(0, CP_pan)] = -1;
    f.effectiveValues[P_mix] = 0;
    Engine e; init(e, rate, f); e.noteOn(60, .9f); const auto audio = render(e, 1024);
    check(!e.getRankInfo(0, 0).hpfEnabled && !e.getRankInfo(0, 0).lpfEnabled,
          "source-performance retains explicit software filter bypass controls");
    double left = 0, right = 0;
    for (size_t i = 0; i < audio.size(); i += 2) { left += std::abs(audio[i]); right += std::abs(audio[i+1]); }
    check(left > .01 && right == 0, "source-performance pan remains an explicit audio overlay");
    const auto toneInfo = e.getSourceToneInfo(0);
    check(source_factory::isSourceDriven(toneInfo.route) && !toneInfo.provenance.hardwareMeasured,
          "source identity and unmeasured provenance survive performance overlays");

    auto old = e.getRankEnvelopeState(0, 0);
    auto invalid = f; invalid.effectiveValues[P_parityMode] = 1;
    invalid.effectiveValues[P_sourcePerformance] = std::numeric_limits<float>::quiet_NaN();
    check(!e.setControlFrame(invalid) && e.getRenderPolicy() == RenderPolicy::sourcePerformance
              && same(old, e.getRankEnvelopeState(0, 0)),
          "rejected policy frame cannot partially change model or voice state");
    for (int type : {1, 2, 3, 4, 5})
    {
        auto dry = fixture(), wet = dry;
        wet.effectiveValues[P_fx1Type] = (float) type; wet.effectiveValues[P_fx1Mix] = 1;
        Engine a, b; init(a, rate, dry); init(b, rate, wet);
        a.noteOn(60, .8f); b.noteOn(60, .8f);
        check(render(a, (int) rate / 2) != render(b, (int) rate / 2),
              "configured modern FX slot remains audible after source PRA and before output circuit");
        b.noteOff(60); render(b, (int) rate);
    }
    auto arp = fixture(); arp.effectiveValues[P_arpOn] = 1;
    Engine sequence; init(sequence, rate, arp);
    sequence.noteOn(60, .7f, 2); sequence.noteOn(67, .9f, 3);
    render(sequence, (int) rate);
    check(sequence.activeVoices() > 0 && source_factory::isSourceDriven(sequence.getSourceToneInfo(0).route),
          "arp generates voices through the source model without changing source ownership");
}
}
int main()
{
    check(P_parityMode == 137 && P_sourcePerformance == 138 && P_numParams >= 139,
          "core parameter indexes are append-only");
    check(paramId(P_sourcePerformance) == "source_performance" && ParamValues{}[P_sourcePerformance] == 0,
          "source-performance has stable ID and legacy default");
    for (double rate : {44100.0, 48000.0, 96000.0})
    { controllerTransports(rate); macroTransports(rate); routingAndExtensions(rate); }
    std::printf("Source performance: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
