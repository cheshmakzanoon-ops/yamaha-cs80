// All 22 source columns (seven complete, fifteen explicitly inferred) are a
// nominal reconstruction, not
// measured CS-80 parity. The independent circuit oracle is SourceFactoryToneTests.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace cs80;
namespace sf = cs80::source_factory;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { if (failures < 40) std::cerr << message << '\n'; ++failures; }
}
void near(double a, double b, double tolerance, const char* message)
{ check(std::isfinite(a) && std::isfinite(b) && std::abs(a - b) <= tolerance, message); }
template<class State> bool same(State a, State b)
{ return a.stage == b.stage && a.value == b.value && a.elapsed == b.elapsed; }
bool same(Engine::RankEnvelopeState a, Engine::RankEnvelopeState b)
{ return same(a.amplitude, b.amplitude) && same(a.filter, b.filter); }
bool same(Engine::RankAudioState a, Engine::RankAudioState b)
{
    return a.phase == b.phase && a.hpf.ic1 == b.hpf.ic1 && a.hpf.ic2 == b.hpf.ic2
        && a.lpf.ic1 == b.lpf.ic1 && a.lpf.ic2 == b.lpf.ic2;
}
ControlFrame frame(bool isolated = true)
{
    ControlFrame f;
    auto& p = f.effectiveValues;
    p[P_parityMode] = isolated ? 1.f : 0.f;
    // Oversampling quality (ws2-engine, plan 1.6): the circuit engine now defaults to
    // Standard (4x at 44.1/48 kHz). This fixture's independent oracle runs at the historical 2x
    // internal rate, so pin Eco; per-factor behavior is covered by OversamplingTests.
    p[P_oversampling] = 0.f;
    p[P_masterSustain] = p[P_keyRelease] = p[P_brilliance] = p[P_subVcf] = p[P_subVca] = 0;
    p[P_subAtVcf] = p[P_subAtVco] = p[P_subAtSpeed] = p[P_ringAmount] = p[P_chorusOn] = 0;
    p[P_detune] = 0;
    return f;
}
std::vector<float> render(Engine& e, int count, int block = 0, const ControlFrame* repeat = nullptr)
{
    std::vector<float> out(static_cast<std::size_t>(count) * 2);
    if (block <= 0) block = std::max(count, 1);
    for (int n = 0; n < count; n += block)
    {
        if (repeat) check(e.setControlFrame(*repeat), "repeated valid frame accepted");
        e.render(out.data() + n * 2, std::min(count - n, block));
    }
    return out;
}
void routingAndAtomicity()
{
    Engine e;
    int complete = 0, inferred = 0, inferredCells = 0;
    for (int rank = 0; rank < 2; ++rank)
        for (int selector = 0; selector < 14; ++selector)
            for (bool isolated : {false, true})
            {
                auto f = frame(isolated); f.hardwareSources[rank] = selector;
                check(e.setControlFrame(f), "all valid source selections accepted");
                const auto info = e.getSourceToneInfo(rank);
                check(info.selector == selector && info.route == sf::route(rank, selector, isolated), "exact source route");
                const auto count = e.getSourceCompileCount();
                check(e.setControlFrame(f) && e.getSourceCompileCount() == count, "unchanged selector does not compile in callback");
                if (isolated && selector < 11)
                {
                    if (info.route == sf::Route::source_complete)
                    {
                        ++complete;
                        check(info.provenance.version == sf::modelVersion && info.provenance.knownCells == sf::allOutputsMask
                              && info.provenance.inferredCells == 0 && info.provenance.unknownCells == 0
                              && !info.provenance.hardwareMeasured, "active source provenance honest and complete");
                    }
                    else
                    {
                        ++inferred;
                        const auto unknown = sf::unknownMask(*sf::address(rank, selector));
                        inferredCells += std::popcount(unknown);
                        check(info.route == sf::Route::source_inferred && sf::isSourceDriven(info.route),
                              "originally incomplete columns use an explicitly inferred source route");
                        check(unknown != 0 && info.provenance.unknownCells == unknown
                              && info.provenance.inferredCells == unknown
                              && info.provenance.knownCells == (sf::allOutputsMask & ~unknown),
                              "inferred source retains original unknown mask, disjoint known mask, and inference overlap");
                        check(info.provenance.version == sf::modelVersion
                              && info.provenance.hypothesisVersion == factory_hypotheses::version
                              && info.provenance.hypothesisHash == factory_hypotheses::fingerprint
                              && info.provenance.hypothesisOverrideMask == 0 && !info.provenance.hardwareMeasured,
                              "runtime inference is the frozen nominal manifest, not measurement or a sensitivity override");
                    }
                }
            }
    check(complete == 7 && inferred == 15 && inferredCells == 25,
          "all22 source routes distinguish seven complete from fifteen inferred with exactly25 original exclusions");
    auto good = frame(); good.hardwareSources = {1, 3};
    check(e.setControlFrame(good), "source pair accepted");
    e.prepare(48000); // resource initialization must retain accepted identity
    check(e.getSourceToneInfo(0).route == sf::Route::source_complete && e.getSourceToneInfo(1).selector == 3,
          "prepare retains control frame identity");
    // Reject atomically from an inferred active frame too, retaining its masks.
    good.hardwareSources = {0, 1};
    check(e.setControlFrame(good), "inferred source pair accepted");
    e.noteOn(60, .8f); render(e, 128);
    for (int invalid = 0; invalid < 6; ++invalid)
    {
        auto bad = good; bad.effectiveValues[P_volume] = .1f;
        if (invalid == 0) bad.hardwareSources[0] = -1;
        if (invalid == 1) bad.hardwareSources[1] = 14;
        if (invalid == 2) ++bad.modelVersion;
        if (invalid == 3) bad.effectiveValues[P_brilliance] = std::numeric_limits<float>::quiet_NaN();
        if (invalid == 4) bad.effectiveValues[chParam(1, CP_vA)] = std::numeric_limits<float>::infinity();
        if (invalid == 5) bad.modelVersion = 1; // prior seven-tone model is not a hidden opt-out
        const auto params = e.getParams().v;
        const auto audio = e.getRankAudioState(0, 0);
        const auto envelope = e.getRankEnvelopeState(0, 0);
        const auto count = e.getSourceCompileCount();
        check(!e.setControlFrame(bad), "malformed frame rejected");
        check(e.getControlFrameStatus() != Engine::ControlFrameStatus::accepted, "rejection diagnostic is visible");
        check(e.getParams().v == params && e.getSourceToneInfo(0).selector == 0 && e.getSourceToneInfo(1).selector == 1
              && e.getSourceToneInfo(0).route == sf::Route::source_inferred
              && e.getSourceToneInfo(0).provenance.inferredCells == sf::unknownMask(*sf::address(0, 0))
              && e.getSourceCompileCount() == count, "rejected whole frame leaves all parameters and compiled sources unchanged");
        check(same(audio, e.getRankAudioState(0, 0)) && same(envelope, e.getRankEnvelopeState(0, 0)),
              "rejected frame preserves exact audio and envelope state");
    }
    auto clamp = good; clamp.effectiveValues[P_volume] = 100;
    check(e.setControlFrame(clamp) && e.getParams()[P_volume] == paramDef(P_volume).max,
          "finite core values retain established boundary clamping");
    auto legacy = good.effectiveValues; legacy[P_volume] = std::numeric_limits<float>::quiet_NaN();
    e.setParams(legacy);
    check(e.getParams()[P_volume] == paramDef(P_volume).def && e.getSourceToneInfo(0).selector == 13,
          "one-argument API retains sanitization and explicitly clears source identity");
}
void receiverRouting(double rate)
{
    for (int rank = 0; rank < 2; ++rank) for (int selector = 0; selector < 11; ++selector)
    {
        check(sf::isSourceDriven(sf::route(rank, selector, true)), "every valid isolated factory selector is source driven");
        auto f = frame(); f.hardwareSources[rank] = selector;
        f.effectiveValues[P_resonance] = .37f;
        Engine e; e.prepare(rate); check(e.setControlFrame(f), "receiver fixture accepted");
        for (int voice = 0; voice < 8; ++voice)
        {
            e.noteOn(36 + voice * 7, .1f * (voice + 1));
            e.polyPressure(36 + voice * 7, .09f * voice);
        }
        render(e, 53);
        sf::ToneControls tone; check(sf::compileTone(rank, selector, tone), "independent compiled receiver fixture");
        FilterEnv expectedFilter;
        Adsr expectedAmplitude;
        expectedFilter.noteOn(static_cast<float>(sf::envelopeStateFromVolts(tone.filterEnvelope.initialVolts)),
                              static_cast<float>(sf::envelopeStateFromVolts(tone.filterEnvelope.attackVolts)),
                              static_cast<float>(tone.filterEnvelope.attackSeconds),
                              static_cast<float>(tone.filterEnvelope.decaySeconds),
                              static_cast<float>(tone.filterEnvelope.releaseSeconds), 0);
        expectedAmplitude.noteOn(static_cast<float>(tone.amplitudeEnvelope.attackSeconds),
                                 static_cast<float>(tone.amplitudeEnvelope.decaySeconds),
                                 static_cast<float>(tone.amplitudeEnvelope.sustainRatio),
                                 static_cast<float>(tone.amplitudeEnvelope.releaseSeconds));
        for (int sample = 0; sample < 106; ++sample)
        { expectedFilter.tick(static_cast<float>(rate * 2), true, true); expectedAmplitude.tick(static_cast<float>(rate * 2), true, true); }
        check(same(expectedFilter.getState(), e.getRankEnvelopeState(0, rank).filter)
              && same(expectedAmplitude.getState(), e.getRankEnvelopeState(0, rank).amplitude),
              "all source envelope targets and times use typed primitives exactly once");
        sf::BothRankDrives drives {};
        std::array<sf::RankBoundary, 2> boundaries;
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto& p = f.effectiveValues;
            if (ch == rank) boundaries[ch] = tone.boundary;
            else check(sf::bridgeBoundary(p[chParam(ch, CP_hpfRes)], p[chParam(ch, CP_lpfRes)],
                       p[chParam(ch, CP_vLevel)], p[chParam(ch, CP_initLevel)], p[chParam(ch, CP_afterLevel)], boundaries[ch]),
                       "mixed rank represented by explicit bridge boundary");
            for (int voice = 0; voice < 8; ++voice)
            {
                auto vi = e.getVoiceInfo(voice);
                drives[ch][voice] = {e.getRankEnvelopeState(voice, ch).filter.value,
                                    vi.active ? vi.velocity : 0, vi.active ? vi.pressure : 0};
            }
        }
        sf::CutoffReadout expectedCutoff;
        sf::SharedReadout expectedShared;
        check(sf::evaluateCutoff(tone, drives[rank], expectedCutoff), "cutoff oracle evaluated");
        check(sf::evaluateShared(boundaries, f.effectiveValues[P_resonance], drives, expectedShared), "shared receiver oracle evaluated");
        const auto actualCutoff = e.getSourceCutoffReadout(rank);
        const auto actualShared = e.getSourceSharedReadout();
        for (int voice = 0; voice < 8; ++voice)
        {
            const auto info = e.getRankInfo(voice, rank);
            for (int side = 0; side < 2; ++side)
            {
                near(actualCutoff.pinVolts[voice][side], expectedCutoff.pinVolts[voice][side], 1e-12,
                     "all eight cutoff receivers use same-sample envelope and local touch");
                near(actualShared.q[rank][voice][side], expectedShared.q[rank][voice][side], 1e-12,
                     "shared two-rank Q readout applied exactly once");
            }
            near(info.hpfHz, nominalFilterCutoff(info.keyVolts, static_cast<float>(expectedCutoff.effectiveCv[voice][0])), 1e-3,
                 "source HP does not add old envelope or old touch gain twice");
            near(info.lpfHz, nominalFilterCutoff(info.keyVolts, static_cast<float>(expectedCutoff.effectiveCv[voice][1])), 1e-3,
                 "source LP does not add old envelope or old touch gain twice");
            near(info.hpfQ, expectedShared.q[rank][voice][0], 1e-6, "source HP bypasses old resonance mixer");
            near(info.lpfQ, expectedShared.q[rank][voice][1], 1e-6, "source LP bypasses old resonance mixer");
            near(actualShared.levelGain[rank][voice], expectedShared.levelGain[rank][voice], 1e-12,
                 "LC receiver uses voltage-domain local initial and after-level once");
        }
        std::array<Engine::RankAudioState, 8> dormantAudio;
        for (int voice = 0; voice < 8; ++voice) dormantAudio[voice] = e.getRankAudioState(voice, rank);
        e.allNotesOff(false); render(e, 1);
        expectedFilter.noteOff();
        for (int sample = 0; sample < 2; ++sample) expectedFilter.tick(static_cast<float>(rate * 2), true, true);
        for (int voice = 0; voice < 8; ++voice)
        {
            check(same(expectedFilter.getState(), e.getRankEnvelopeState(voice, rank).filter),
                  "source filter control state continues independently after VCA retirement");
            check(same(dormantAudio[voice], e.getRankAudioState(voice, rank)),
                  "inactive source control evolution does not clock oscillator or audio filters");
            drives[rank][voice] = {e.getRankEnvelopeState(voice, rank).filter.value, 0, 0};
        }
        sf::evaluateCutoff(tone, drives[rank], expectedCutoff);
        const auto idleCutoff = e.getSourceCutoffReadout(rank);
        for (int voice = 0; voice < 8; ++voice) for (int side = 0; side < 2; ++side)
            near(idleCutoff.pinVolts[voice][side], expectedCutoff.pinVolts[voice][side], 1e-12,
                 "dormant circuit keeps all eight passive loads with explicitly zero idle touch");
    }
}
void ownershipAndClearing(double rate)
{
    for (int rank = 0; rank < 2; ++rank) for (int selector = 0; selector < 11; ++selector)
    {
        check(sf::isSourceDriven(sf::route(rank, selector, true)), "every valid isolated factory selector is source driven");
        auto a = frame(); a.hardwareSources[rank] = selector;
        auto b = a;
        for (int control = 0; control < CP_count; ++control)
        {
            if (control == CP_feet || control == CP_pan || control == CP_hpfOn || control == CP_lpfOn) continue;
            const auto& def = kChannelDefs[control];
            b.effectiveValues[chParam(rank, control)] = def.def == def.min ? def.max : def.min;
        }
        Engine first, second;
        for (auto* e : {&first, &second}) e->prepare(rate);
        first.setControlFrame(a); second.setControlFrame(b);
        first.noteOn(60, .8f); second.noteOn(60, .8f);
        check(render(first, 1024) == render(second, 1024), "all 26 stored programmer controls are owned by active source");
        check(first.getParams().v != second.getParams().v, "ownership does not overwrite stored effective input snapshot");
        auto mono = a; mono.effectiveValues[P_outputMode] = 0;
        Engine monoEngine; monoEngine.prepare(rate); monoEngine.setControlFrame(mono); monoEngine.noteOn(60, .8f);
        Engine stereoEngine; stereoEngine.prepare(rate); stereoEngine.setControlFrame(a); stereoEngine.noteOn(60, .8f);
        check(render(monoEngine, 1024) == render(stereoEngine, 1024), "modern mono selector remains inert in isolated source path");
        for (int destination : {13, 11, 12})
        {
            // The plugin resolves PANEL/memory/legacy tables before this seam.
            // Setting a valid non-source selection must never reuse old voltage controls.
            auto next = b; next.hardwareSources[rank] = destination;
            applyHardwareTone(next.effectiveValues, rank, 0);
            first.setControlFrame(next);
            check(!sf::isSourceDriven(first.getSourceToneInfo(rank).route),
                  "PANEL/memory immediately clears complete or inferred active source route");
            Engine fresh, framed;
            fresh.prepare(rate); framed.prepare(rate);
            fresh.setParams(next.effectiveValues); framed.setControlFrame(next);
            fresh.noteOn(62, .7f); framed.noteOn(62, .7f);
            check(render(fresh, 257) == render(framed, 257), "valid non-source routes use exact legacy audio adapter");
            first.setControlFrame(a);
        }
    }
}
void transitions(double rate)
{
    for (int phase = 0; phase < 4; ++phase)
    {
        auto base = frame();
        for (int rank = 0; rank < 2; ++rank)
        {
            auto& p = base.effectiveValues;
            p[chParam(rank, CP_fA)] = p[chParam(rank, CP_vA)] = phase == 0 ? 1.f : .001f;
            p[chParam(rank, CP_fD)] = p[chParam(rank, CP_vD)] = phase == 2 ? .001f : 2.f;
            p[chParam(rank, CP_fR)] = p[chParam(rank, CP_vR)] = 2.f;
            p[chParam(rank, CP_vS)] = .4f;
        }
        Engine e, split; e.prepare(rate); split.prepare(rate);
        for (auto* target : {&e, &split})
        {
            target->setControlFrame(base); target->noteOn(60, .7f); render(*target, static_cast<int>(rate * .08));
            target->polyPressure(60, .6f); target->setRibbon(.1f); target->setExpression(.6f);
            if (phase == 3) { target->noteOff(60); render(*target, static_cast<int>(rate * .01)); }
        }
        for (int rank = 0; rank < 2; ++rank)
            check(static_cast<int>(e.getRankEnvelopeState(0, rank).amplitude.stage) == phase + 1,
                  "transition stage fixture reached");
        for (int selector = 0; selector < 14; ++selector)
        {
            auto selected = base; selected.hardwareSources = {selector, selector};
            std::array<Engine::RankEnvelopeState, 2> envelopes;
            std::array<Engine::RankAudioState, 2> audio;
            for (int rank = 0; rank < 2; ++rank)
            { envelopes[rank] = e.getRankEnvelopeState(0, rank); audio[rank] = e.getRankAudioState(0, rank); }
            const auto voice = e.getVoiceInfo(0);
            e.setControlFrame(selected); split.setControlFrame(selected);
            for (int rank = 0; rank < 2; ++rank)
            {
                check(same(envelopes[rank], e.getRankEnvelopeState(0, rank)), "source changes preserve exact stored z/amplitude/stage/time");
                check(same(audio[rank], e.getRankAudioState(0, rank)), "source changes do not reset oscillator or filter");
            }
            const auto after = e.getVoiceInfo(0);
            check(voice.note == after.note && voice.pressure == after.pressure && voice.held == after.held
                  && voice.ribbonRatio == after.ribbonRatio, "source changes preserve voice and performance controllers");
            const auto whole = render(e, 257), parts = render(split, 257, 17, &selected);
            check(whole == parts, "source automation is sample-identical across callback partitions and repeated transactions");
            for (float x : whole) check(std::isfinite(x), "live source transitions stay finite");
        }
        e.allNotesOff(false); e.setControlFrame(base); render(e, 64);
        check(e.activeVoices() == 0, "source transactions never resurrect idle voices");
    }
}
void audioStressAndTailReuse(double rate)
{
    constexpr std::array<int, 8> notes {0, 21, 36, 60, 84, 108, 120, 127};
    for (int rank = 0; rank < 2; ++rank) for (int selector = 0; selector < 11; ++selector)
    {
        check(sf::isSourceDriven(sf::route(rank, selector, true)), "every valid isolated factory selector is source driven");
        auto f = frame(); f.hardwareSources[rank] = selector;
        f.effectiveValues[P_mix] = static_cast<float>(rank); // dry single-rank audition, not saved patch rewrite
        Engine e; e.prepare(rate); e.setControlFrame(f);
        for (int voice = 0; voice < 8; ++voice) e.noteOn(notes[voice], .125f * (voice + 1));
        auto audio = render(e, static_cast<int>(rate * .2));
        bool finite = true; float peak = 0;
        for (float x : audio) { finite = finite && std::isfinite(x); peak = std::max(peak, std::abs(x)); }
        check(finite && peak > 1e-6f, "all22 complete or inferred sources are audible and finite across keyboard extremes and eight voices");
        // Do not require peak<=1 or normalize the capture: nominal linear LI can
        // exceed digital full scale, an explicitly reported headroom limitation.
        e.allNotesOff();
        audio = render(e, static_cast<int>(rate * 1.5));
        check(e.activeVoices() == 0
              && std::all_of(audio.begin(), audio.end(), [](float x) { return std::isfinite(x); }),
              "each original source release settles without relying on a later tab change");
        e.reset(); e.setControlFrame(f);
        for (int voice = 0; voice < 8; ++voice) e.noteOn(notes[voice], .125f * (voice + 1));
        render(e, static_cast<int>(rate * .1));
        e.noteOff(notes[0]); render(e, 1);
        const auto oldAudio = e.getRankAudioState(0, rank);
        const auto oldVoice = e.getVoiceInfo(0);
        check(oldVoice.active && !oldVoice.held, "active-tail reuse fixture");
        auto next = f; next.hardwareSources[rank] = rank == 0 ? 3 : 10;
        e.setControlFrame(next);
        check(same(oldAudio, e.getRankAudioState(0, rank)), "source event preserves released circuit before cyclic reuse");
        e.noteOn(61, .6f);
        check(e.getVoiceInfo(0).note == 61 && same(oldAudio, e.getRankAudioState(0, rank)),
              "source-selected active tail reuse preserves oscillator and both filter memories");
        e.allNotesOff();
        audio = render(e, static_cast<int>(rate * 1.5));
        finite = std::all_of(audio.begin(), audio.end(), [](float x) { return std::isfinite(x); });
        check(finite && e.activeVoices() == 0, "all source and bridge releases settle without nonfinite output");
    }
}
void inactiveControlRelaxation(double rate)
{
    auto f = frame(); f.hardwareSources[0] = 9; // Funky1: slow filter tail, fast VCA release
    f.effectiveValues[chParam(1, CP_vR)] = .001f;
    Engine e; e.prepare(rate); e.setControlFrame(f);
    render(e, 31);
    for (int voice = 0; voice < 8; ++voice)
    {
        const auto state = e.getRankEnvelopeState(voice, 0).filter;
        check(state.stage == FilterEnv::Stage::Idle && state.value == 0, "cold untriggered source control remains idle at zero");
    }
    e.noteOn(60, .8f); render(e, static_cast<int>(rate * .1)); e.noteOff(60);
    int waited = 0;
    while (e.activeVoices() && waited++ < static_cast<int>(rate)) render(e, 1);
    check(e.activeVoices() == 0, "short VCA retires before slow source filter control");
    const auto atRetirement = e.getRankEnvelopeState(0, 0).filter;
    check(atRetirement.stage == FilterEnv::Stage::Release && atRetirement.value > -3,
          "retired receiver still has unfinished filter release");
    const auto audio = e.getRankAudioState(0, 0);
    render(e, static_cast<int>(rate));
    const auto relaxed = e.getRankEnvelopeState(0, 0).filter;
    check(relaxed.stage == FilterEnv::Stage::Idle && relaxed.value == -3,
          "inactive source filter relaxes fully rather than permanently biasing common FH FL");
    check(same(audio, e.getRankAudioState(0, 0)), "inactive relaxation does not evolve oscillator or audio filter memories");
    sf::ToneControls tone; sf::compileTone(0, 9, tone);
    sf::RankDrives drives {}; drives[0].filterEnvelopeZ = -3;
    sf::CutoffReadout oracle; sf::evaluateCutoff(tone, drives, oracle);
    for (int side = 0; side < 2; ++side)
        near(e.getSourceCutoffReadout(0).loadedBusVolts[side], oracle.loadedBusVolts[side], 1e-12,
             "shared bus converges to relaxed inactive receiver oracle");
    auto changed = f; changed.hardwareSources[0] = 3;
    e.setControlFrame(changed);
    check(same(relaxed, e.getRankEnvelopeState(0, 0).filter), "inactive source target change preserves zero-time state exactly");
    sf::compileTone(0, 3, tone);
    const float target = static_cast<float>(sf::envelopeStateFromVolts(tone.filterEnvelope.initialVolts));
    render(e, static_cast<int>(rate));
    const auto retargeted = e.getRankEnvelopeState(0, 0).filter;
    check(retargeted.stage == FilterEnv::Stage::Idle && retargeted.value == target,
          "completed inactive release follows new source IL using refreshed time without retrigger");
    check(e.activeVoices() == 0 && same(audio, e.getRankAudioState(0, 0)),
          "inactive source retargeting never resurrects audio circuits");
    // Also enter from a deliberately different PANEL release target AFTER its
    // VCA has retired. This checks the bridge-to-source handoff separately from
    // the source-to-source Funky1 -> Flute IL/time retarget above.
    auto panel = frame();
    for (int rank = 0; rank < 2; ++rank)
    {
        panel.effectiveValues[chParam(rank, CP_vR)] = .001f;
        panel.effectiveValues[chParam(rank, CP_fR)] = 2;
        panel.effectiveValues[chParam(rank, CP_fIl)] = .2f;
    }
    Engine fromPanel; fromPanel.prepare(rate); fromPanel.setControlFrame(panel);
    fromPanel.noteOn(60, .8f); render(fromPanel, static_cast<int>(rate * .1)); fromPanel.noteOff(60);
    render(fromPanel, static_cast<int>(rate * .02));
    check(fromPanel.activeVoices() == 0, "PANEL-to-source inactive target fixture retired");
    const auto beforeSource = fromPanel.getRankEnvelopeState(0, 0).filter;
    panel.hardwareSources[0] = 9;
    fromPanel.setControlFrame(panel);
    check(same(beforeSource, fromPanel.getRankEnvelopeState(0, 0).filter), "new inactive source target is not an immediate IL jump");
    render(fromPanel, static_cast<int>(rate));
    check(fromPanel.getRankEnvelopeState(0, 0).filter.value == -3,
          "source-selected inactive EG receives new IL and release time rather than stale PANEL target");
}
void panicControlGates(double rate)
{
    auto f = frame(); f.hardwareSources = {9, 3};
    Engine e; e.prepare(rate); e.setControlFrame(f);
    e.noteOn(60, .7f, 1); e.noteOn(64, .8f, 2); render(e, 64);
    const auto before = e.getRankEnvelopeState(1, 0).filter;
    const auto audio = e.getRankAudioState(1, 0);
    e.controlChange(120, 0, 2);
    const auto gated = e.getRankEnvelopeState(1, 0).filter;
    check(gated.stage == FilterEnv::Stage::Release && gated.value == before.value && gated.elapsed == 0,
          "source channel panic gates filter control release without value reset");
    check(e.getVoiceInfo(0).active && !e.getVoiceInfo(1).active, "channel panic keeps other channel voice and hard-mutes selected audio");
    render(e, 31);
    const auto progressing = e.getRankEnvelopeState(1, 0).filter;
    e.controlChange(120, 0, 2); e.setControlFrame(f); e.setControlFrame(f);
    check(same(progressing, e.getRankEnvelopeState(1, 0).filter), "repeated panic and source frames never restart control release");
    render(e, static_cast<int>(rate));
    check(e.getRankEnvelopeState(1, 0).filter.value == -3 && same(audio, e.getRankAudioState(1, 0)),
          "panicked source control relaxes to IL without clocking inactive audio");

    auto panel = frame();
    for (int rank = 0; rank < 2; ++rank) panel.effectiveValues[chParam(rank, CP_fA)] = 1;
    Engine bridge; bridge.prepare(rate); bridge.setControlFrame(panel); bridge.noteOn(60, .8f); render(bridge, 64);
    bridge.allNotesOff(false);
    const auto abandoned = bridge.getRankEnvelopeState(0, 0).filter;
    check(abandoned.stage == FilterEnv::Stage::Attack, "legacy hard-panic EG semantics remain unchanged");
    panel.hardwareSources = {9, 3}; bridge.setControlFrame(panel);
    check(same(abandoned, bridge.getRankEnvelopeState(0, 0).filter), "bridge-panic to source preserves exact zero-frame envelope state");
    render(bridge, 1);
    check(bridge.getRankEnvelopeState(0, 0).filter.stage == FilterEnv::Stage::Release,
          "inactive unheld source gate reconciles at next control sample only");
    const auto firstRelease = bridge.getRankEnvelopeState(0, 0).filter;
    bridge.setControlFrame(panel); render(bridge, 1);
    check(bridge.getRankEnvelopeState(0, 0).filter.elapsed > firstRelease.elapsed,
          "repeated frames cannot restart bridge-panic release clock");
    render(bridge, static_cast<int>(rate));
    check(bridge.activeVoices() == 0 && bridge.getRankEnvelopeState(0, 0).filter.value == -3,
          "bridge-panic source control converges without audio resurrection");
}
void defaultOff()
{
    const auto& presets = factoryPresets();
    check(presets.size() == 222, "legacy program inventory unchanged");
    for (double rate : {44100., 48000., 96000.}) for (const auto& preset : presets)
    {
        Engine legacy, framed; legacy.prepare(rate); framed.prepare(rate);
        auto p = preset.values; p[P_parityMode] = 0;
        ControlFrame f {p, {1, 3}};
        legacy.setParams(p); framed.setControlFrame(f);
        legacy.noteOn(48, .3f); framed.noteOn(48, .3f);
        legacy.noteOn(67, .9f, 2); framed.noteOn(67, .9f, 2);
        check(render(legacy, 257) == render(framed, 257), "all 222 off programs retain raw float path through source-aware API");
        for (int selector : {9, 11, 0, 13, 3})
        {
            if (selector < 11) { applyHardwareTone(p, 0, selector); applyHardwareTone(p, 1, selector); }
            f.effectiveValues = p; f.hardwareSources = {selector, selector};
            legacy.setParams(p); framed.setControlFrame(f);
            check(render(legacy, 31) == render(framed, 31), "default-off dynamic hardware tabs retain raw float arithmetic");
        }
    }
}
} // namespace
int main()
{
    routingAndAtomicity();
    for (double rate : {44100., 48000., 96000., 192000.})
    { receiverRouting(rate); ownershipAndClearing(rate); transitions(rate); audioStressAndTailReuse(rate); inactiveControlRelaxation(rate); panicControlGates(rate); }
    defaultOff();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
