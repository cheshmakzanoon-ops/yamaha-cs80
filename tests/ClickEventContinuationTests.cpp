// Exact exposed-state continuation at note events. Acceptance was registered
// separately in 98c8b73 and amended in d06648c before this implementation.
// EngineEventContinuityProbe has no production implementation or audio code.
//
// Original cs80_engine_audit --suite phrase metric/target (<= 1) remains a
// separate required gate. These assertions cover exact zero-elapsed-time
// continuation, not perceptual click or a measured hardware law.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "ExactContinuationComparator.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Deliberately limited read-only allocator inspection plus the same private
// wake that Engine::startVoice invokes before assigning an inactive circuit.
// This is a *separate reference engine*: it never receives the test noteOn.
// Its state is compared with the ordinary event engine only at the same time.
namespace cs80 {
struct EngineEventContinuityProbe
{
    static int nextAvailable(const Engine& engine) noexcept
    {
        for (int offset = 0; offset < Engine::kNumVoices; ++offset)
        {
            const int index = (engine.nextVoice + offset) % Engine::kNumVoices;
            const auto& voice = engine.voices[static_cast<std::size_t>(index)];
            if (!voice.held && !voice.pedalHeld)
                return index;
        }
        return -1;
    }

    static std::array<int, 2> pending(const Engine& engine, int voice) noexcept
    {
        const auto& selected = engine.voices[static_cast<std::size_t>(voice)];
        return { selected.idleClock, selected.idleSteps };
    }

    static void completeReferenceIdleTime(Engine& reference, int selected) noexcept
    {
        reference.wakeIdleVoice(reference.voices[static_cast<std::size_t>(selected)]);
    }
};
} // namespace cs80

namespace {
using namespace cs80;
namespace exact = click_continuity;
constexpr int kBlock = 64;
struct Results
{
    unsigned long long checks = 0, failures = 0;
    void require(bool truth, const char* label)
    {
        ++checks;
        if (!truth)
        {
            ++failures;
            std::fprintf(stderr, "CLICK-CONTINUITY FAIL: %s\n", label);
        }
    }
};

void requireRegistered(const std::string& path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error("missing committed click-continuity contract");
    const std::string document((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    if (document.find("\"status\": \"REGISTERED\"") == std::string::npos)
        throw std::runtime_error("click-continuity contract must first be committed as REGISTERED");
}

void compareExactly(Results& out, const ws1::Tolerances& tol,
                    const exact::Continuation& pre, const exact::Continuation& post,
                    const char* label)
{
    const auto diff = exact::compare(pre, post);
    if (diff.total() != 0)
        std::fprintf(stderr, "  %s: WSC %d, nodal %d, EG %d, VCA %d, phase %d\n",
                     label, diff.oscillator, diff.nodal, diff.envelopes, diff.vca, diff.phase);
    out.require(diff.oscillator <= tol["wsc_full_exposed_state_bit_mismatches_max"], label);
    out.require(diff.nodal <= tol["nodal_stage_history_bit_mismatches_max"], label);
    out.require(diff.envelopes <= tol["envelope_charge_bit_mismatches_max"], label);
    out.require(diff.vca <= tol["vca_li_coupling_bit_mismatches_max"], label);
    out.require(diff.phase <= tol["wsc_full_exposed_state_bit_mismatches_max"], label);
}

ControlFrame bassPhysicalPanel(bool pure)
{
    ParamValues params;
    bool found = false;
    for (const auto& preset : factoryPresets())
        if (preset.name == "Bass Stack")
        {
            params = preset.values;
            found = true;
            break;
        }
    if (!found) throw std::runtime_error("Bass Stack factory preset missing");
    params[P_sourcePerformance] = 1.0f;
    params[P_parityMode] = pure ? 1.0f : 0.0f;
    // Intentionally unannotated physical PANEL: no electrical programmer.
    ControlFrame frame { params };
    if (frame.hasProgrammerPositions[0] || frame.hasProgrammerPositions[1])
        throw std::runtime_error("Bass fixture unexpectedly selected electrical programmer");
    return frame;
}

ControlFrame funkyPair(int quality, bool pure)
{
    int selector = -1;
    for (int index = 0; index < 11; ++index)
        if (std::string(hardwareToneName(index, 0)) == "Funky 1" &&
            std::string(hardwareToneName(index, 1)) == "Funky 2")
            selector = index;
    if (selector < 0) throw std::runtime_error("rank-local Funky 1 + Funky 2 tab pair missing");

    ParamValues params;
    params[P_sourcePerformance] = 1.0f;
    params[P_parityMode] = pure ? 1.0f : 0.0f;
    params[P_subOriginalMode] = 1.0f;
    params[P_oversampling] = static_cast<float>(quality);
    for (int rank = 0; rank < 2; ++rank)
    {
        ParamValues fromTone;
        applyHardwareTone(fromTone, rank, selector); // DIFFERENT rank-local tone
        for (int local = 0; local < CP_count; ++local)
            if (local != CP_feet && local != CP_pan &&
                local != CP_hpfOn && local != CP_lpfOn)
                params[chParam(rank, local)] = fromTone[chParam(rank, local)];
        params[chParam(rank, CP_feet)] = 1.0f;
        params[chParam(rank, CP_pan)] = 0.0f;
    }
    return ControlFrame::programmer(params, { selector, selector });
}

std::unique_ptr<Engine> initialized(double rate, const ControlFrame& frame)
{
    auto result = std::make_unique<Engine>();
    result->prepare(rate);
    if (!result->setControlFrame(frame))
        throw std::runtime_error("Engine rejected phrase fixture");
    result->reset();
    return result;
}

void requireFunkyPair(Results& result, const Engine& engine)
{
    // The same selector number names different physical cards in the two
    // ranks. Verify what Engine actually compiled against independently
    // addressed source cards, not just two copies of a display string.
    std::array<source_factory::ToneControls, 2> expected;
    for (int rank : { 0, 1 })
    {
        const auto* address = source_factory::addressByName(rank, rank == 0 ? "Funky 1" : "Funky 2");
        result.require(address != nullptr, "named rank-local Funky source address exists");
        if (!address) continue;
        result.require(source_factory::compileTone(rank, address->selector, expected[rank]),
                       "independent named Funky source compiles");
        const auto info = engine.getSourceToneInfo(rank);
        result.require(info.selector == address->selector && source_factory::isSourceDriven(info.route),
                       "Funky fixture uses its named rank-local source route");
        const auto buses = engine.getSourceBusReadout(rank);
        bool exactBuses = true;
        for (std::size_t bus = 0; bus < buses.size(); ++bus)
            exactBuses = exactBuses && exact::sameBits(buses[bus], expected[rank].busVolts[bus]);
        result.require(exactBuses, rank == 0 ? "Funky1 compiled source identity"
                                            : "Funky2 compiled source identity");
    }
    result.require(expected[0].busVolts != expected[1].busVolts,
                   "Funky1 and Funky2 independent source states are distinguishable");
}

std::uint64_t renderSame(Engine& a, Engine& b, const ControlFrame& frame, long frames)
{
    std::array<float, 2 * kBlock> aa {}, bb {};
    std::uint64_t different = 0;
    for (long at = 0; at < frames; at += kBlock)
    {
        const int n = static_cast<int>(std::min<long>(kBlock, frames - at));
        if (!a.setControlFrame(frame) || !b.setControlFrame(frame))
            throw std::runtime_error("frame reapplication failed");
        a.render(aa.data(), n);
        b.render(bb.data(), n);
        for (int i = 0; i < n * 2; ++i)
            different += !exact::sameBits(aa[static_cast<std::size_t>(i)],
                                          bb[static_cast<std::size_t>(i)]);
    }
    return different;
}

std::uint64_t renderSame(Engine& a, Engine& b, Engine& c,
                         const ControlFrame& frame, long frames)
{
    std::array<float, 2 * kBlock> aa {}, bb {}, cc {};
    std::uint64_t different = 0;
    for (long at = 0; at < frames; at += kBlock)
    {
        const int n = static_cast<int>(std::min<long>(kBlock, frames - at));
        for (auto* e : { &a, &b, &c })
            if (!e->setControlFrame(frame))
                throw std::runtime_error("frame reapplication failed");
        a.render(aa.data(), n);
        b.render(bb.data(), n);
        c.render(cc.data(), n);
        for (int i = 0; i < n * 2; ++i)
        {
            const auto index = static_cast<std::size_t>(i);
            different += !exact::sameBits(aa[index], bb[index]);
            different += !exact::sameBits(aa[index], cc[index]);
        }
    }
    return different;
}

int nonzeroResiduals(const ReconstructedOscillator::State& state)
{
    int nonzero = 0;
    for (const auto& row : state.residual)
        for (double sample : row)
            nonzero += sample != 0.0;
    return nonzero;
}

// Cache-independent software reference: the same complete physical/residual
// state restored into a separately prepared oscillator necessarily starts
// with its derived next-branch cache INVALID, while the live oscillator has
// an already cached future crossing. This deliberately compares cache use
// against fresh analytic branch search at an exact control-change boundary.
// It cannot establish hardware sound or a new pitch CV law.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
WscOutputVolts tickShared(ReconstructedOscillator& oscillator,
                          double hz, const WscControls& controls) noexcept
{
    return oscillator.tick(hz, controls);
}

void wscCachedBranchEvent(Results& result, const ws1::Tolerances& tol, int internalRate)
{
    for (int controlCase : { 0, 1, 2 })
    {
        ReconstructedOscillator cached, refreshed;
        if (!cached.prepare(internalRate, {}) || !refreshed.prepare(internalRate, {}))
            throw std::runtime_error("WSC independent pair preparation failed");
        cached.reset(.31, 261.6);
        WscControls oldControls;
        oldControls.pulseThresholdVolts = .15;
        bool hasCachedFutureAndResidual = false;
        for (int sample = 0; sample < 8192; ++sample)
        {
            const auto work = cached.getDiagnostics().branchSearchCount;
            (void)tickShared(cached, 261.6, oldControls);
            const bool usedCache = cached.getDiagnostics().branchSearchCount == work;
            if (usedCache && cached.getDiagnostics().eventCount == 0
                && cached.getState().resetRemainingSeconds == 0.0
                && nonzeroResiduals(cached.getState()) > 0)
            {
                hasCachedFutureAndResidual = true;
                break;
            }
        }
        result.require(hasCachedFutureAndResidual,
                       "WSC fixture actually has a cached future branch and nonzero residual");
        const auto before = cached.getState();
        refreshed.restoreState(before); // DERIVED CACHE INVALIDATED, PHYSICAL STATE EXACT
        exact::Continuation lhs {}, rhs {};
        lhs.oscillator = cached.getState();
        rhs.oscillator = refreshed.getState();
        compareExactly(result, tol, lhs, rhs, "WSC cache-refresh oracle begins with identical full state");

        const double newHz = controlCase == 1 ? 261.6 : 1046.4;
        auto controls = oldControls;
        if (controlCase != 0) controls.pulseThresholdVolts = -.45;
        // Cases 0/1/2 = frequency change / threshold change / both, without
        // resetting the charge, RC baselines, branch residuals or phase.
        for (int sample = 0; sample < 128; ++sample)
        {
            const auto left = tickShared(cached, newHz, controls);
            const auto right = tickShared(refreshed, newHz, controls);
            const int outputDifferences = !exact::sameBits(left.sawPin5, right.sawPin5)
                + !exact::sameBits(left.pulsePin7, right.pulsePin7)
                + !exact::sameBits(left.sinePin2, right.sinePin2);
            result.require(outputDifferences <= tol["wsc_cached_restarted_output_bit_mismatches_max"],
                           "WSC control-edge cached/uncached output voltage bits identical");
            lhs.oscillator = cached.getState();
            rhs.oscillator = refreshed.getState();
            compareExactly(result, tol, lhs, rhs,
                           "WSC control-edge cached/uncached full residual state bits identical");
            result.require(!cached.getDiagnostics().eventCapacityExceeded
                           && !refreshed.getDiagnostics().eventCapacityExceeded,
                           "WSC control edge preserves bounded analytic event solver");
        }
        std::printf("WSC cache-invalidation oracle: internal_rate=%d control_case=%d\n",
                    internalRate, controlCase);
    }
}

long frameFor(double seconds, int rate)
{
    return static_cast<long>(std::ceil(seconds * rate / kBlock)) * kBlock;
}

void funkyNoteOff(Results& result, const ws1::Tolerances& tol,
                  int rate, int quality, double noteOffSeconds, bool pure)
{
    const auto frame = funkyPair(quality, pure);
    auto event = initialized(rate, frame);
    auto omitted = initialized(rate, frame);
    requireFunkyPair(result, *event);
    requireFunkyPair(result, *omitted);

    // Fixture A: the UNCHANGED phrase singleOn=.20 and singleOff=1.40.
    // Fixture A2: the ADDITIVE charge-mutation fixture noteOff=.22, with
    // no other tone/velocity/MIDI/rank/rounding difference. Both exact.
    std::uint64_t prefix = renderSame(*event, *omitted, frame, frameFor(.20, rate));
    event->noteOn(60, .8f, 1);
    omitted->noteOn(60, .8f, 1);
    prefix += renderSame(*event, *omitted, frame,
                         frameFor(noteOffSeconds, rate) - frameFor(.20, rate));
    result.require(prefix <= tol["no_event_control_pre_event_pcm_bit_mismatches_max"],
                   "Funky1+2 identical true/omitted PCM before note-off");
    result.require(event->getVoiceInfo(0).held && omitted->getVoiceInfo(0).held,
                   "Funky1+2 single key held before note-off");

    for (int rank : { 0, 1 })
        compareExactly(result, tol, exact::observe(*event, 0, rank),
                       exact::observe(*omitted, 0, rank),
                       "Funky1+2 identical rank state before note-off");
    const auto preI = exact::observe(*event, 0, 0);
    const auto preII = exact::observe(*event, 0, 1);
    if (noteOffSeconds == .22)
        result.require(preI.envelopes.filter.value != 0.0f
                       && preII.envelopes.filter.value != 0.0f
                       && preI.envelopes.amplitude.value != 0.0f
                       && preII.envelopes.amplitude.value != 0.0f,
                       "Funky1+2 early0.22s all four EG charges nonzero before mutation gate");
    result.require(preII.vca.coupling != 0.0,
                   "Funky2 output coupling node is nonzero before mutation gate");
    event->noteOff(60, 1); // OMIT in the independent control only
    result.require(!event->getVoiceInfo(0).held && omitted->getVoiceInfo(0).held,
                   "Funky1+2 gate changes exactly once");
    result.require(event->getRankEnvelopeState(0, 0).amplitude.stage == Adsr::Stage::Release,
                   "Funky1 amplitude gate entered Release");
    result.require(event->getRankEnvelopeState(0, 1).amplitude.stage == Adsr::Stage::Release,
                   "Funky2 amplitude gate entered Release");
    result.require(event->getRankEnvelopeState(0, 0).filter.stage == FilterEnv::Stage::Release,
                   "Funky1 filter gate entered Release");
    result.require(event->getRankEnvelopeState(0, 1).filter.stage == FilterEnv::Stage::Release,
                   "Funky2 filter gate entered Release");
    compareExactly(result, tol, preI, exact::observe(*event, 0, 0),
                   "Funky1 full zero-time release continuity");
    compareExactly(result, tol, preII, exact::observe(*event, 0, 1),
                   "Funky2 full zero-time release continuity");

    // Release intentionally changes subsequent sound. Render the same 8 ms
    // as the original audit, but never assert that event and omission match.
    std::array<float, 2 * kBlock> changed {}, held {};
    long remaining = static_cast<long>(std::llround(.008 * rate));
    while (remaining > 0)
    {
        const int n = static_cast<int>(std::min<long>(remaining, kBlock));
        if (!event->setControlFrame(frame) || !omitted->setControlFrame(frame))
            throw std::runtime_error("Funky release frame rejected");
        event->render(changed.data(), n);
        omitted->render(held.data(), n);
        remaining -= n;
    }
    std::printf("Funky1+2 note-off exact gate: rate=%d quality=%d policy=%s off=%.2fs, held/released distinguished\n",
                rate, quality, pure ? "pure" : "source", noteOffSeconds);
}

void bassSoundingTail(Results& result, const ws1::Tolerances& tol, int rate, bool pure)
{
    const auto frame = bassPhysicalPanel(pure);
    auto event = initialized(rate, frame);
    auto omitted = initialized(rate, frame);
    for (int note = 48; note < 56; ++note)
    {
        event->noteOn(note, .8f, 1);
        omitted->noteOn(note, .8f, 1);
    }

    // The fixture is selected by an independent SOURCE-STATE predicate,
    // not a click waveform or prospective score: at least one genuine
    // nonzero band-limited WSC residual must be pending in selected voice0.
    std::uint64_t prefix = renderSame(*event, *omitted, frame, 64);
    bool populated = false;
    std::array<float, 2> a {}, b {};
    for (int step = 0; step < 4096; ++step)
    {
        const int populatedEntries = nonzeroResiduals(event->getRankOscillatorState(0, 0))
            + nonzeroResiduals(event->getRankOscillatorState(0, 1));
        populated = populatedEntries >= tol["minimum_nonzero_wsc_residual_entries_at_sounding_tail_retrigger"];
        if (populated) break;
        event->render(a.data(), 1);
        omitted->render(b.data(), 1);
        for (int channel = 0; channel < 2; ++channel)
            prefix += !exact::sameBits(a[static_cast<std::size_t>(channel)],
                                       b[static_cast<std::size_t>(channel)]);
    }
    result.require(populated, "Bass retained sounding WSC residual exists before reuse");
    result.require(prefix <= tol["no_event_control_pre_event_pcm_bit_mismatches_max"],
                   "Bass true/omitted state-identical PCM prefix");
    result.require(event->getVoiceInfo(0).held && event->getVoiceInfo(0).active,
                   "Bass voice0 is active-held before release");
    for (int rank : { 0, 1 })
        compareExactly(result, tol, exact::observe(*event, 0, rank),
                       exact::observe(*omitted, 0, rank),
                       "Bass sounding rank pre-event cross-control equality");

    const auto beforeI = exact::observe(*event, 0, 0);
    const auto beforeII = exact::observe(*event, 0, 1);
    event->noteOff(48, 1);
    omitted->noteOff(48, 1);
    result.require(event->getVoiceInfo(0).active && !event->getVoiceInfo(0).held,
                   "Bass selected sounding tail released but not retired");
    result.require(EngineEventContinuityProbe::nextAvailable(*event) == 0
                   && EngineEventContinuityProbe::pending(*event, 0)[0] == 0,
                   "Bass active-tail allocator selects voice0 with no pending idle clock");
    compareExactly(result, tol, beforeI, exact::observe(*event, 0, 0),
                   "Bass sounding tail release preserves rank I");
    compareExactly(result, tol, beforeII, exact::observe(*event, 0, 1),
                   "Bass sounding tail release preserves rank II");

    event->noteOn(60, .8f, 1); // nextVoice cycled to released voice0
    result.require(event->getVoiceInfo(0).active && event->getVoiceInfo(0).held
                   && event->getVoiceInfo(0).note == 60,
                   "Bass sounding-tail retrigger reassigns voice0");
    compareExactly(result, tol, beforeI, exact::observe(*event, 0, 0),
                   "Bass rank I WSC residual survives zero-time sounding-tail retrigger");
    compareExactly(result, tol, beforeII, exact::observe(*event, 0, 1),
                   "Bass rank II WSC residual survives zero-time sounding-tail retrigger");
    std::printf("Bass active-tail zero-time gate: rate=%d policy=%s, populated WSC residual retained\n",
                rate, pure ? "pure" : "source");
}

struct Event { double seconds; bool on; int note; };
std::vector<Event> bassPhrasePrefix()
{
    // Identical to engine_audit.cpp::phrase through its T::retrigOn2 note-on.
    std::vector<Event> out { { .20, true, 60 }, { 1.40, false, 60 } };
    for (int note : { 48, 52, 55, 60 }) out.push_back({ 3.00, true, note });
    for (int note : { 48, 52, 55, 60 }) out.push_back({ 4.40, false, note });
    out.push_back({ 6.00, true, 60 });
    out.push_back({ 6.20, false, 60 });
    std::stable_sort(out.begin(), out.end(), [](auto a, auto b) {
        return a.seconds < b.seconds;
    });
    return out;
}

void originalIdleBassRetrigger(Results& result, const ws1::Tolerances& tol,
                               const ws1::Tolerances& timeTol, int rate, bool pure)
{
    const auto frame = bassPhysicalPanel(pure);
    auto observed = initialized(rate, frame);
    auto wakeControl = initialized(rate, frame);
    auto plain = initialized(rate, frame);
    std::array<std::array<double, 2>, 8> initialElapsed {};
    for (int voice = 0; voice < 8; ++voice)
        for (int rank : { 0, 1 })
            initialElapsed[voice][rank] = observed->getRankOscillatorState(voice, rank).elapsedSeconds;
    long hostFrame = 0;
    std::uint64_t prefix = 0;
    for (const auto event : bassPhrasePrefix())
    {
        const long eventFrame = frameFor(event.seconds, rate);
        prefix += renderSame(*observed, *wakeControl, *plain, frame,
                             eventFrame - hostFrame);
        hostFrame = eventFrame;
        for (auto* engine : { observed.get(), wakeControl.get(), plain.get() })
        {
            if (event.on) engine->noteOn(event.note, .8f, 1);
            else engine->noteOff(event.note, 1);
        }
    }
    const long retriggerFrame = frameFor(6.25, rate);
    prefix += renderSame(*observed, *wakeControl, *plain, frame,
                         retriggerFrame - hostFrame);
    result.require(prefix <= tol["no_event_control_pre_event_pcm_bit_mismatches_max"],
                   "original physical Bass pre-retrigger 3-way PCM bit identity");

    const int selected = EngineEventContinuityProbe::nextAvailable(*observed);
    result.require(selected == EngineEventContinuityProbe::nextAvailable(*wakeControl)
                   && selected == EngineEventContinuityProbe::nextAvailable(*plain),
                   "Bass predicted selected voice matches all three independent engines");
    // Exact fixture identity freezes the source phrase/allocator. Should the
    // allocator change, inspect source and preregister a replacement before
    // accepting the new run; do not silently select a different test voice.
    result.require(selected == 6, "historical physical Bass retrigger selects voice6");
    if (selected < 0 || selected >= 8) return;
    result.require(!observed->getVoiceInfo(selected).active,
                   "original Bass selected circuit is inactive and needs idle catchup");
    const auto [idleClock, idleSteps] = EngineEventContinuityProbe::pending(*wakeControl, selected);
    result.require(idleClock >= tol["minimum_original_bass_pending_idle_internal_samples"]
                   && idleSteps > 0,
                   "original Bass selected circuit has pending idle time");

    for (int rank : { 0, 1 })
        compareExactly(result, tol, exact::observe(*observed, selected, rank),
                       exact::observe(*wakeControl, selected, rank),
                       "Bass selected stale state equal before reference wake");

    const auto staleI = exact::observe(*wakeControl, selected, 0);
    const auto staleII = exact::observe(*wakeControl, selected, 1);
    EngineEventContinuityProbe::completeReferenceIdleTime(*wakeControl, selected);
    const auto caughtI = exact::observe(*wakeControl, selected, 0);
    const auto caughtII = exact::observe(*wakeControl, selected, 1);
    for (int rank : { 0, 1 })
    {
        const double expectedElapsed = initialElapsed[selected][rank]
            + static_cast<double>(retriggerFrame * observed->getOversamplingFactor())
                / observed->getInternalSampleRate();
        const double actualElapsed = wakeControl->getRankOscillatorState(selected, rank).elapsedSeconds;
        result.require(std::abs(actualElapsed - expectedElapsed) <= timeTol["elapsed_seconds_error_max"],
                       "Bass reference wake accounts only independently counted elapsed samples");
    }
    result.require(exact::compare(staleI, caughtI).total() > 0
                   || exact::compare(staleII, caughtII).total() > 0,
                   "Bass wake reference legitimately advances an old idle snapshot");

    observed->noteOn(60, .8f, 1);
    plain->noteOn(60, .8f, 1);
    result.require(observed->getVoiceInfo(selected).held
                   && observed->getVoiceInfo(selected).note == 60,
                   "Bass original retrigger assigned selected inactive circuit");
    compareExactly(result, tol, caughtI, exact::observe(*observed, selected, 0),
                   "Bass rank I oscillator/residual/filter/EG/VCA exactly unchanged after wake+assignment");
    compareExactly(result, tol, caughtII, exact::observe(*observed, selected, 1),
                   "Bass rank II oscillator/residual/filter/EG/VCA exactly unchanged after wake+assignment");
    for (int rank : { 0, 1 })
    {
        const double expectedElapsed = initialElapsed[selected][rank]
            + static_cast<double>(retriggerFrame * observed->getOversamplingFactor())
                / observed->getInternalSampleRate();
        const double actualElapsed = observed->getRankOscillatorState(selected, rank).elapsedSeconds;
        result.require(std::abs(actualElapsed - expectedElapsed) <= timeTol["elapsed_seconds_error_max"],
                       "Bass assignment accounts only independently counted elapsed samples");
    }

    const auto postEventPCM = renderSame(*observed, *plain, frame,
                                        static_cast<long>(std::llround(.008 * rate)));
    result.require(postEventPCM <= tol["nonmutating_probe_post_event_pcm_bit_mismatches_max"],
                   "original Bass test observation does not change any post-event PCM bit");
    std::printf("Bass cold-retrigger exact gate: rate=%d policy=%s, selected=%d, pending=%d internal samples, %d steps\n",
                rate, pure ? "pure" : "source", selected, idleClock, idleSteps);
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::string path = argc > 1 ? argv[1]
            : "tests/parity/tolerances-v2/click-residual-continuity.json";
        requireRegistered(path);
        const ws1::Tolerances tol(path, "proposed_additive_gate");
        const ws1::Tolerances timeTol(argc > 2 ? argv[2]
            : "tests/parity/tolerances-v2/cold-idle-timing.json", "schema_version");
        // Focused suites are for isolated source mutation qualification. CTest
        // always runs all registered fixtures, including both source policies.
        const std::string suite = argc > 3 ? argv[3] : "all";
        if (suite != "all" && suite != "noteoff" && suite != "tail"
            && suite != "idle" && suite != "wsc")
            throw std::runtime_error("unknown click-continuity fixture suite");
        Results result;
        for (const char* key : {
                 "wsc_full_exposed_state_bit_mismatches_max",
                 "wsc_cached_restarted_output_bit_mismatches_max",
                 "nodal_stage_history_bit_mismatches_max",
                 "envelope_charge_bit_mismatches_max",
                 "vca_li_coupling_bit_mismatches_max",
                 "no_event_control_pre_event_pcm_bit_mismatches_max",
                 "nonmutating_probe_post_event_pcm_bit_mismatches_max" })
            result.require(tol[key] == 0, "registered bit-continuity limits must remain exactly zero");
        result.require(exact::exposedFieldsPerRank == tol["complete_exposed_continuation_fields_per_rank"],
                       "all 72 named exposed fields/rank are compared");
        for (bool pure : { false, true })
        {
            for (int rate : { 44100, 48000, 96000 })
            {
                if (suite == "all" || suite == "noteoff")
                    for (int quality : { 0, 1, 2 })
                    {
                        funkyNoteOff(result, tol, rate, quality, 1.40, pure); // original phrase
                        funkyNoteOff(result, tol, rate, quality, .22, pure);  // nonzero-charge fixture
                    }
                if (suite == "all" || suite == "tail")
                    bassSoundingTail(result, tol, rate, pure);
            }
            if (suite == "all" || suite == "idle")
                for (int rate : { 48000, 96000 })
                    originalIdleBassRetrigger(result, tol, timeTol, rate, pure);
        }
        if (suite == "all" || suite == "wsc")
            for (int internalRate : { 96000, 192000 })
                wscCachedBranchEvent(result, tol, internalRate);
        std::printf("Click event continuation: %llu checks, %llu failures\n",
                    result.checks, result.failures);
        return result.failures ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Click event continuity fixture error: %s\n", error.what());
        return 2;
    }
}
