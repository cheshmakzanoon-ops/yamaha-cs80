// Rendered integration contracts for the optional instrument bodies. These
// tests establish signal continuity, routing and tuning, not acoustic identity.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool value, const std::string& message)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
ParamValues fixture(int model = body::piano)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = p[P_kasScanTiming] = p[P_analogNoise] = 0.0f;
    p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
    p[P_kbdBrillianceLow] = p[P_kbdBrillianceHigh] = 0.0f;
    p[P_kbdLevelLow] = p[P_kbdLevelHigh] = 0.0f;
    p[P_mix] = 0.0f;
    p[P_body1Model] = static_cast<float>(model);
    p[P_body1Mix] = 1.0f;
    p[P_body1Brightness] = 0.55f;
    p[P_body1Decay] = 1.0f;
    p[P_detune] = p[P_portamento] = p[P_initBend] = 0.0f;
    p[P_subVco] = p[P_subAtVco] = p[P_subVcf] = p[P_subAtVcf] = 0.0f;
    p[chParam(0, CP_vA)] = 0.003f;
    p[chParam(0, CP_vD)] = 4.0f;
    p[chParam(0, CP_vS)] = 0.7f;
    p[chParam(0, CP_vR)] = 1.0f;
    p[chParam(0, CP_fIl)] = p[chParam(0, CP_fAl)] = 0.0f;
    p[chParam(0, CP_initBrill)] = p[chParam(0, CP_afterBrill)] = 0.0f;
    return p;
}
void prepare(Engine& engine, const ParamValues& p, double rate = 48000.0)
{
    engine.setParams(p);
    engine.prepare(rate);
}
std::vector<float> render(Engine& engine, int frames, int block = 0)
{
    std::vector<float> result(static_cast<std::size_t>(2 * frames));
    if (block <= 0) block = frames;
    for (int i = 0; i < frames; i += block)
        engine.render(result.data() + 2 * i, std::min(block, frames - i));
    return result;
}
double relativeError(const std::vector<float>& a, const std::vector<float>& b)
{
    double error = 0.0, power = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const double delta = static_cast<double>(a[i]) - b[i];
        error += delta * delta;
        power += static_cast<double>(b[i]) * b[i];
    }
    return std::sqrt(error / std::max(power, 1.0e-30));
}
void partitionAndBatch()
{
    for (int model = 1; model <= body::maxModel; ++model)
    {
        auto p = fixture(model);
        p[P_mix] = 0.4f;
        p[P_body2Model] = static_cast<float>(model);
        p[P_body2Mix] = 0.8f;
        p[P_body2Brightness] = 0.65f;
        Engine scalar, batched;
        prepare(scalar, p); prepare(batched, p);
        scalar.setFilterBatchingEnabled(false);
        batched.setFilterBatchingEnabled(true);
        if (model == 1)
            std::printf("BATCH comparison backend: %s\n", batched.hasFilterBatchingKernel()
                ? "runtime-dispatched exact batch" : "scalar fallback on this platform");
        for (int note = 48; note < 80; note += 4)
        {
            scalar.noteOn(note, 0.7f);
            batched.noteOn(note, 0.7f);
        }
        check(render(scalar, 2400) == render(batched, 2400, 113),
              std::string(body::name(model)) + ": scalar/batched and host partition are identical");
        scalar.noteOff(48); batched.noteOff(48);
        check(render(scalar, 37) == render(batched, 37, 7), "release partition is identical");
        scalar.noteOn(85, 0.9f); batched.noteOn(85, 0.9f);
        check(render(scalar, 768) == render(batched, 768, 11),
              std::string(body::name(model)) + ": body handoff is partition and batch invariant");
    }
}
void soundingRegister()
{
    for (int model : {body::piano, body::bowed, body::oboe, body::bassoon})
    {
        auto lower = fixture(model), upper = lower;
        // This contract isolates excitation register. The panel filter's key
        // tracking deliberately follows the played key, before VCO footage.
        lower[chParam(0, CP_hpfOn)] = lower[chParam(0, CP_lpfOn)] = 0.0f;
        upper[chParam(0, CP_hpfOn)] = upper[chParam(0, CP_lpfOn)] = 0.0f;
        lower[chParam(0, CP_feet)] = 1.0f; // 8-foot C3
        upper[chParam(0, CP_feet)] = 0.0f; // 16-foot C4, also sounding C3
        Engine a, b;
        prepare(a, lower); prepare(b, upper);
        a.noteOn(48, 0.8f); b.noteOn(60, 0.8f);
        const double error = relativeError(render(a, 12000), render(b, 12000));
        check(error < 0.0001, std::string(body::name(model)) + ": same sounding register gives same body");
        std::printf("REGISTER %s relative waveform error %.9g\n", body::name(model), error);
    }
    // A4 reference participates in nominal body register, not only tick pitch.
    auto aParams = fixture(body::piano), bParams = aParams;
    aParams[chParam(0, CP_hpfOn)] = aParams[chParam(0, CP_lpfOn)] = 0.0f;
    bParams[chParam(0, CP_hpfOn)] = bParams[chParam(0, CP_lpfOn)] = 0.0f;
    aParams[P_masterTune] = static_cast<float>(12.0 * std::log2(450.0 / 440.0));
    bParams[P_tuningRef] = 450.0f;
    Engine a, b;
    prepare(a, aParams); prepare(b, bParams);
    a.noteOn(48, 0.8f); b.noteOn(48, 0.8f);
    check(relativeError(render(a, 12000), render(b, 12000)) < 0.0001,
          "master tuning and A4 reference yield the same nominal body register");
}
void keyboardBrilliance()
{
    // At a full body mix the analog filter output is absent. Each KBC
    // brilliance contour must therefore reach the body's effective filters,
    // independently of the shared keyboard LEVEL/VCA contour.
    for (bool high : {false, true})
    {
        const int note = high ? 84 : 36;
        const int lever = high ? P_kbdBrillianceHigh : P_kbdBrillianceLow;
        const std::string label = high ? "HIGH" : "LOW";
        auto p = fixture(body::oboe);
        p[P_body1Brightness] = 0.45f;
        p[chParam(0, CP_initLevel)] = p[chParam(0, CP_afterLevel)] = 0.0f;
        Engine centered, partitioned;
        prepare(centered, p); prepare(partitioned, p);
        centered.noteOn(note, 0.8f); partitioned.noteOn(note, 0.8f);
        const auto reference = render(centered, 12000);
        check(reference == render(partitioned, 12000, 113),
              label + " centered brilliance is deterministic across render partitions");
        for (float position : {-1.0f, 1.0f})
        {
            auto moved = p;
            moved[lever] = position;
            Engine changed;
            prepare(changed, moved);
            changed.noteOn(note, 0.8f);
            const double error = relativeError(render(changed, 12000), reference);
            check(error > 0.01, label + " brilliance contour changes the pure-body waveform at "
                  + std::to_string(position));
            std::printf("KBC body %s lever %+.0f relative waveform change %.9g\n",
                        label.c_str(), position, error);
        }
    }
}

std::vector<float> renderBody(const ParamValues& p, int note = 60, int frames = 19200)
{
    // Keep large processor state off the Windows stack, including callers
    // that compare several independently prepared captures.
    auto engine = std::make_unique<Engine>();
    prepare(*engine, p);
    engine->noteOn(note, 0.8f);
    return render(*engine, frames, 113);
}

double toneAmplitude(const std::vector<float>& audio, double frequency)
{
    const int first = 4800, frames = static_cast<int>(audio.size() / 2) - first;
    double real = 0.0, imaginary = 0.0, weights = 0.0;
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < frames; ++i)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * i / (frames - 1));
        const double angle = 2.0 * pi * frequency * (first + i) / 48000.0;
        const double sample = audio[static_cast<std::size_t>(2 * (first + i))];
        real += window * sample * std::cos(angle);
        imaginary += window * sample * std::sin(angle);
        weights += window;
    }
    return 2.0 * std::hypot(real, imaginary) / weights;
}

void bodyFilterRouting()
{
    constexpr double fundamental = 261.6255653005986;
    auto open = fixture(body::organ);
    open[P_volume] = 0.20f;
    open[P_resonance] = 0.0f;
    open[chParam(0, CP_vS)] = 1.0f;
    open[chParam(0, CP_initLevel)] = open[chParam(0, CP_afterLevel)] = 0.0f;
    open[chParam(0, CP_hpfRes)] = open[chParam(0, CP_lpfRes)] = 0.0f;
    open[chParam(0, CP_hpfCutoff)] = 30.0f;
    open[chParam(0, CP_lpfCutoff)] = 12000.0f;
    open[chParam(0, CP_hpfOn)] = 1.0f;
    open[chParam(0, CP_lpfOn)] = 0.0f;
    const auto openAudio = renderBody(open);

    auto high = open;
    high[chParam(0, CP_hpfCutoff)] = 1200.0f;
    const auto highAudio = renderBody(high);
    const double highPassDb = 20.0 * std::log10(std::max(1.0e-12,
        toneAmplitude(highAudio, fundamental) / toneAmplitude(openAudio, fundamental)));
    check(highPassDb < -18.0, "full body mix respects high-pass fundamental rejection");
    std::printf("BODY FILTER high-pass fundamental change %.3f dB\n", highPassDb);

    auto hpOff = high;
    hpOff[chParam(0, CP_hpfOn)] = 0.0f;
    auto openOff = open;
    openOff[chParam(0, CP_hpfOn)] = 0.0f;
    check(renderBody(hpOff) == renderBody(openOff),
          "disabled high-pass does not color the instrument body");

    auto hpResonant = open;
    auto hpDamped = open;
    hpResonant[chParam(0, CP_hpfCutoff)] = hpDamped[chParam(0, CP_hpfCutoff)]
        = static_cast<float>(fundamental);
    hpResonant[chParam(0, CP_hpfRes)] = 0.90f;
    const double highResonanceDb = 20.0 * std::log10(std::max(1.0e-12,
        toneAmplitude(renderBody(hpResonant), fundamental)
        / toneAmplitude(renderBody(hpDamped), fundamental)));
    check(highResonanceDb > 4.0, "high-pass resonance shapes a full-body rank");
    std::printf("BODY FILTER high-pass resonance at cutoff change %.3f dB\n", highResonanceDb);

    auto low = openOff;
    low[chParam(0, CP_lpfOn)] = 1.0f;
    low[chParam(0, CP_lpfCutoff)] = 480.0f;
    const auto lowAudio = renderBody(low);
    const auto bypassAudio = renderBody(openOff);
    const double upperChangeDb = 20.0 * std::log10(std::max(1.0e-12,
        toneAmplitude(lowAudio, 4.0 * fundamental) / toneAmplitude(bypassAudio, 4.0 * fundamental)));
    check(upperChangeDb < -12.0, "full body mix respects low-pass upper-partial rejection");
    std::printf("BODY FILTER low-pass fourth-partial change %.3f dB\n", upperChangeDb);

    auto lpOff = low;
    lpOff[chParam(0, CP_lpfOn)] = 0.0f;
    check(renderBody(lpOff) == bypassAudio, "disabled low-pass does not color the instrument body");

    auto resonant = low;
    low[chParam(0, CP_lpfCutoff)] = resonant[chParam(0, CP_lpfCutoff)] = static_cast<float>(2.0 * fundamental);
    resonant[chParam(0, CP_lpfRes)] = 0.90f;
    const double resonanceDb = 20.0 * std::log10(std::max(1.0e-12,
        toneAmplitude(renderBody(resonant), 2.0 * fundamental)
        / toneAmplitude(renderBody(low), 2.0 * fundamental)));
    check(resonanceDb > 4.0, "low-pass resonance shapes a full-body rank");
    std::printf("BODY FILTER resonance at cutoff change %.3f dB\n", resonanceDb);

    // Exercise live bypass transitions and strongest authored controls through
    // the same continuous state: every intermediate sample must remain bounded.
    auto moving = fixture(body::bowed);
    moving[P_volume] = 0.15f;
    auto engine = std::make_unique<Engine>();
    prepare(*engine, moving);
    engine->noteOn(60, 0.8f);
    (void) render(*engine, 2400);
    bool finite = true;
    double peak = 0.0;
    for (int step = 0; step < 8; ++step)
    {
        moving[chParam(0, CP_hpfOn)] = step % 2 ? 0.0f : 1.0f;
        moving[chParam(0, CP_lpfOn)] = step % 3 ? 1.0f : 0.0f;
        moving[chParam(0, CP_hpfCutoff)] = step % 2 ? 30.0f : 6000.0f;
        moving[chParam(0, CP_lpfCutoff)] = step % 2 ? 7000.0f : 90.0f;
        moving[chParam(0, CP_hpfRes)] = moving[chParam(0, CP_lpfRes)] = 1.0f;
        engine->setParams(moving);
        for (float sample : render(*engine, 1024, 29))
        {
            finite = finite && std::isfinite(sample);
            peak = std::max(peak, std::abs(static_cast<double>(sample)));
        }
    }
    check(finite && peak < 1.0, "body filter live cutoffs/resonance/bypass remain finite and bounded");
}
void nextNoteSelection()
{
    for (int replacement : {body::off, body::brass})
    {
        auto p = fixture(body::tine);
        Engine changed, unchanged;
        prepare(changed, p); prepare(unchanged, p);
        changed.noteOn(60, 0.8f); unchanged.noteOn(60, 0.8f);
        check(render(changed, 1000) == render(unchanged, 1000), "selection fixture starts identically");
        p[P_body1Model] = static_cast<float>(replacement);
        changed.setParams(p);
        check(render(changed, 2400) == render(unchanged, 2400),
              "model selection, including Off, leaves already sounding body intact");
        changed.allNotesOff(false); unchanged.allNotesOff(false);
        changed.noteOn(65, 0.8f); unchanged.noteOn(65, 0.8f);
        check(relativeError(render(changed, 2400), render(unchanged, 2400)) > 0.05,
              "new model selection takes effect on the next accepted note");
    }
    // Same-sample note traffic has not made an audible tail. It must not
    // create a phantom outgoing body when the next note selects Off.
    auto bodyParams = fixture(body::piano), offParams = fixture(body::off);
    Engine bodyTraffic, offTraffic;
    prepare(bodyTraffic, bodyParams); prepare(offTraffic, offParams);
    for (int note = 48; note < 56; ++note)
    {
        bodyTraffic.noteOn(note, 0.8f); offTraffic.noteOn(note, 0.8f);
        bodyTraffic.noteOff(note); offTraffic.noteOff(note);
    }
    bodyTraffic.setParams(offParams);
    bodyTraffic.noteOn(60, 0.8f); offTraffic.noteOn(60, 0.8f);
    check(render(bodyTraffic, 1024) == render(offTraffic, 1024),
          "unrendered note assignments do not produce phantom body handoffs");
}
void isolation()
{
    for (int policy : {0, 1, 2})
    {
        auto on = fixture(body::piano), off = on;
        on[P_sourcePerformance] = off[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
        on[P_parityMode] = off[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
        off[P_body1Model] = 0.0f;
        Engine a, b;
        prepare(a, on); prepare(b, off);
        a.noteOn(60, 0.8f); b.noteOn(60, 0.8f);
        const auto x = render(a, 2048), y = render(b, 2048);
        check(policy == 1 ? x != y : x == y, "bodies affect only source-performance policy");
    }
    for (int selector : {0, 5, 10, 11, 12, 13})
    {
        auto p = fixture(body::piano);
        Engine a, b;
        prepare(a, p); prepare(b, p);
        ControlFrame frame;
        frame.effectiveValues = p;
        frame.hardwareSources = {selector, selector};
        check(a.setControlFrame(frame), "valid body/source control frame accepted");
        frame.effectiveValues[P_body1Model] = 0.0f;
        check(b.setControlFrame(frame), "valid no-body/source control frame accepted");
        a.noteOn(60, 0.8f); b.noteOn(60, 0.8f);
        const auto x = render(a, 2048), y = render(b, 2048);
        check(selector == 13 ? x != y : x == y, "hardware tabs and memories ignore optional bodies");
    }
    auto aParams = fixture(body::off), bParams = aParams;
    aParams[P_body1Mix] = aParams[P_body2Mix] = 0.0f;
    bParams[P_body1Mix] = bParams[P_body2Mix] = 1.0f;
    bParams[P_body1Brightness] = 0.0f; bParams[P_body2Brightness] = 1.0f;
    bParams[P_body1Decay] = 4.0f; bParams[P_body2Decay] = 0.25f;
    Engine a, b;
    prepare(a, aParams); prepare(b, bParams);
    a.noteOn(60, 0.8f); b.noteOn(60, 0.8f);
    check(render(a, 2048) == render(b, 2048), "disabled bodies retain exact original circuit samples");

    auto zeroMix = fixture(body::piano), noBody = zeroMix;
    zeroMix[P_body1Mix] = noBody[P_body1Mix] = 0.0f;
    noBody[P_body1Model] = 0.0f;
    check(renderBody(zeroMix) == renderBody(noBody),
          "enabled body at zero mix preserves exact no-body circuit samples");
}
void tailHandoff()
{
    const auto& presets = curatedLibrary();
    const auto found = std::find_if(presets.begin(), presets.end(),
        [](const FactoryPreset& p) { return p.name == "Electric Piano I"; });
    if (found == presets.end()) throw std::runtime_error("missing Electric Piano I");
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        double worstRatio = 0.0, worstStep = 0.0;
        for (int phase = 0; phase < 12; ++phase)
        {
            auto p = found->values;
            p[P_kasScanTiming] = p[P_chorusOn] = p[P_analogNoise] = 0.0f;
            p[P_voiceDispersion] = p[P_drift] = p[P_parityMode] = p[P_mix] = 0.0f;
            p[P_sourcePerformance] = 1.0f;
            p[P_volume] = 0.3f; // independent of later library level calibration
            p[chParam(0, CP_vR)] = 3.0f;
            Engine engine;
            prepare(engine, p, rate);
            for (int n = 0; n < 8; ++n)
            {
                engine.noteOn(48 + n, 0.8f);
                (void) render(engine, static_cast<int>(rate * 0.02));
                engine.noteOff(48 + n);
            }
            const auto before = render(engine, static_cast<int>(rate * (512.0 + phase * 7) / 48000.0));
            double previousStep = 0.0;
            for (std::size_t i = before.size() - 256; i < before.size(); ++i)
                previousStep = std::max(previousStep, std::abs(static_cast<double>(before[i]) - before[i - 2]));
            engine.noteOn(60, 0.8f);
            const auto after = render(engine, static_cast<int>(rate * 0.004));
            double onsetStep = 0.0;
            for (std::size_t i = 0; i < after.size(); ++i)
            {
                const float previous = i < 2 ? before[before.size() - 2 + i] : after[i - 2];
                onsetStep = std::max(onsetStep, std::abs(static_cast<double>(after[i]) - previous));
                check(std::isfinite(after[i]), "tail handoff produces finite samples");
            }
            worstRatio = std::max(worstRatio, onsetStep / std::max(previousStep, 1.0e-12));
            worstStep = std::max(worstStep, onsetStep);
        }
        std::printf("TAIL rate %.0f maximum onset/pre derivative %.5f, sample step %.8f FS\n", rate, worstRatio, worstStep);
        check(worstRatio < 3.0, "busy EP tail reassignment avoids an abrupt body reset");
    }
}
}
int main()
{
    try
    {
        partitionAndBatch(); soundingRegister(); keyboardBrilliance(); bodyFilterRouting(); nextNoteSelection(); isolation(); tailHandoff();
        std::printf("Instrument body Engine integration: %d checks, %d failures\n", checks, failures);
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Instrument body Engine integration error: %s\n", error.what());
        return 2;
    }
}
