// Archived exploratory replay of Bass Stack physical PANEL and Funky tabs.
// Standalone read-only research, not a production or acceptance-gate change.
#define main cs80_original_engine_audit_main
#include "../../src/tools/engine_audit.cpp"
#undef main
#include "../../tests/ExactContinuationComparator.h"

#include <array>
#include <bit>
#include <cstdio>
#include <iostream>

namespace cs80 {
// Same declared limited friend used by the independently committed
// ClickEventContinuationTests; only inspect allocator and perform the actual
// source Engine::wakeIdleVoice on the separate reference engine.
struct EngineEventContinuityProbe {
    static int nextAvailable(const Engine& e) noexcept {
        for (int offset = 0; offset < Engine::kNumVoices; ++offset) {
            const int i = (e.nextVoice + offset) % Engine::kNumVoices;
            const auto& voice = e.voices[(std::size_t) i];
            if (!voice.held && !voice.pedalHeld) return i;
        }
        return -1;
    }
    static std::array<int, 2> pending(const Engine& e, int voice) noexcept {
        const auto& v = e.voices[(std::size_t) voice];
        return {v.idleClock, v.idleSteps};
    }
    static void wake(Engine& e, int voice) noexcept {
        e.wakeIdleVoice(e.voices[(std::size_t) voice]);
    }
};
} // namespace cs80

namespace {
namespace exact = click_continuity;

enum class Kind { bassFirst, bassRetrigger, funkyOff };
struct Case { Kind kind; const char* label; Setup setup; double seconds; bool on; };

cs80::ControlFrame makeFrame(const Setup& setup) {
    cs80::ControlFrame frame;
    frame.effectiveValues = setup.values;
    frame.hardwareSources = setup.sources;
    if (setup.useProgrammer)
        frame = cs80::ControlFrame::programmer(frame.effectiveValues, frame.hardwareSources);
    return frame;
}

long hostEventFrame(double seconds, int rate) {
    return (long) std::ceil(seconds * rate / kBlock) * kBlock;
}

int zeroTimeDifference(const Case& scenario, int rate) {
    auto live = std::make_unique<cs80::Engine>();
    auto reference = std::make_unique<cs80::Engine>();
    const auto frame = makeFrame(scenario.setup);
    for (auto* e : {live.get(), reference.get()}) {
        e->prepare(rate);
        require(e->setControlFrame(frame), "source control frame rejected");
        e->reset();
    }
    const auto events = phrase();
    const long stop = hostEventFrame(scenario.seconds, rate);
    std::size_t next = 0;
    std::array<float, kBlock * 2> a {}, b {};
    long preBitDiff = 0;
    for (long f = 0; f < stop; f += kBlock) {
        while (next < events.size() && hostEventFrame(events[next].t, rate) <= f) {
            const auto& ev = events[next++];
            for (auto* e : {live.get(), reference.get()}) {
                if (ev.on) e->noteOn(ev.note, ev.vel, 1);
                else e->noteOff(ev.note, 1);
            }
        }
        require(live->setControlFrame(frame) && reference->setControlFrame(frame), "frame rejected");
        live->render(a.data(), kBlock);
        reference->render(b.data(), kBlock);
        for (std::size_t i = 0; i < a.size(); ++i)
            preBitDiff += std::bit_cast<std::uint32_t>(a[i]) != std::bit_cast<std::uint32_t>(b[i]);
    }
    require(preBitDiff == 0, "two-engine pre-event PCM drift");
    const auto target = std::find_if(events.begin(), events.end(), [&](const Event& ev) {
        return ev.t == scenario.seconds && ev.on == scenario.on && ev.note == 60;
    });
    require(target != events.end(), "selected MIDI event not in original phrase");
    int selected = -1, compared = 0, mismatches = 0, wakeDelta = 0, pendingClock = 0, pendingSteps = 0;
    if (scenario.on) {
        selected = cs80::EngineEventContinuityProbe::nextAvailable(*live);
        require(selected >= 0 && selected < 8 &&
                selected == cs80::EngineEventContinuityProbe::nextAvailable(*reference),
                "source voice selector diverged");
        const auto pending = cs80::EngineEventContinuityProbe::pending(*reference, selected);
        pendingClock = pending[0]; pendingSteps = pending[1];
        std::array<exact::Continuation, 2> stale, caught;
        for (int rank = 0; rank < 2; ++rank)
            stale[(std::size_t)rank] = exact::observe(*reference, selected, rank);
        if (!reference->getVoiceInfo(selected).active)
            cs80::EngineEventContinuityProbe::wake(*reference, selected);
        for (int rank = 0; rank < 2; ++rank) {
            caught[(std::size_t)rank] = exact::observe(*reference, selected, rank);
            wakeDelta += exact::compare(stale[(std::size_t)rank], caught[(std::size_t)rank]).total();
        }
        live->noteOn(60, .8f, 1);
        require(live->setControlFrame(frame), "post-on control frame rejected");
        for (int rank = 0; rank < 2; ++rank) {
            const auto diff = exact::compare(caught[(std::size_t)rank], exact::observe(*live, selected, rank));
            mismatches += diff.total(); compared += exact::exposedFieldsPerRank;
            std::cout << "zero_state " << scenario.label << " " << rate << " voice=" << selected
                      << " rank=" << rank << " wsc=" << diff.oscillator
                      << " nodal=" << diff.nodal << " eg=" << diff.envelopes
                      << " vca=" << diff.vca << " phase=" << diff.phase << '\n';
        }
    } else {
        // Actual Funky key-up changes the gate stage and metadata, not charge.
        std::array<std::array<exact::Continuation, 2>, 8> pre;
        for (int voice = 0; voice < 8; ++voice)
            for (int rank = 0; rank < 2; ++rank)
                pre[(std::size_t)voice][(std::size_t)rank] = exact::observe(*live, voice, rank);
        live->noteOff(60, 1);
        require(live->setControlFrame(frame), "post-off control frame rejected");
        for (int voice = 0; voice < 8; ++voice)
            for (int rank = 0; rank < 2; ++rank) {
                const auto diff = exact::compare(pre[(std::size_t)voice][(std::size_t)rank],
                                                 exact::observe(*live, voice, rank));
                mismatches += diff.total(); compared += exact::exposedFieldsPerRank;
            }
        require(!live->getVoiceInfo(0).held && reference->getVoiceInfo(0).held,
                "Funky note-off did not change held-key stage");
    }
    std::cout << "state_summary " << scenario.label << " rate=" << rate
              << " selected_voice=" << selected << " pending_clock=" << pendingClock
              << " pending_steps=" << pendingSteps << " reference_wake_delta=" << wakeDelta
              << " compared_fields=" << compared << " mismatches=" << mismatches
              << " pre_event_pcm_bit_mismatches=" << preBitDiff << '\n';
    require(mismatches == 0, "zero-time exposed audio state change");
    return mismatches;
}

void derivativeProfile(const Case& scenario, int rate) {
    const auto original = phrase();
    auto omitted = original;
    const auto it = std::remove_if(omitted.begin(), omitted.end(), [&](const Event& ev) {
        return ev.t == scenario.seconds && ev.on == scenario.on && ev.note == 60;
    });
    require(std::distance(it, omitted.end()) == 1, "not exactly one omitted event");
    omitted.erase(it, omitted.end());
    const long f = hostEventFrame(scenario.seconds, rate);
    const long window = (long) std::llround(.008 * rate);
    const auto baseline = render(scenario.setup, rate, original, scenario.seconds + .03);
    const auto control = render(scenario.setup, rate, omitted, scenario.seconds + .03);
    require(!baseline.nonfinite && !control.nonfinite, "nonfinite event diagnostic output");
    for (long sample = 0; sample < f; ++sample)
        for (auto channel : {0, 1}) {
            const auto& a = channel ? baseline.r : baseline.l;
            const auto& b = channel ? control.r : control.l;
            require(std::bit_cast<std::uint32_t>(a[(std::size_t) sample]) ==
                    std::bit_cast<std::uint32_t>(b[(std::size_t) sample]),
                    "omission changed pre-event PCM");
        }
    const double ref = scenario.on
        ? std::max(maxJump(baseline, idx(baseline, T::chordOn), idx(baseline, T::chordOff)), 1.e-7)
        : std::max(maxJump(baseline, f - (long)(.06 * rate), f), 1.e-7);
    double baselineMax = 0, omittedMax = 0, addedMax = 0, firstPeakFrac = 0;
    long peakAt = f;
    for (long sample = f; sample < f + window; ++sample)
        for (int channel = 0; channel < 2; ++channel) {
            const auto& b = channel ? baseline.r : baseline.l;
            const auto& c = channel ? control.r : control.l;
            const std::size_t j=(std::size_t)sample, prev=(std::size_t)(sample-1);
            const double bd=(double)(b[j]-b[prev]), cd=(double)(c[j]-c[prev]);
            const double mag=std::fabs(bd);
            if (mag>baselineMax){ baselineMax=mag; peakAt=sample; firstPeakFrac=std::fabs(bd-cd)/std::max(mag,1.e-12); }
            omittedMax=std::max(omittedMax,std::fabs(cd));
            addedMax=std::max(addedMax,std::fabs(bd-cd));
        }
    std::cout << "score_summary " << scenario.label << " rate=" << rate
              << " original=" << (baselineMax/ref)
              << " omit=" << (omittedMax/ref)
              << " event_only_max=" << (addedMax/ref)
              << " peak_offset_frames=" << (peakAt-f)
              << " peak_offset_ms=" << ((peakAt-f)*1000.0/rate)
              << " event_fraction_at_peak=" << firstPeakFrac << '\n';
    const std::array<std::array<double,2>,5> bands {{{0.,.001},{.001,.002},{.002,.004},{.004,.006},{.006,.008}}};
    for (const auto band : bands) {
        double jBase=0,jAdded=0,jOmit=0;
        const long from=f+(long)std::llround(band[0]*rate);
        const long to=std::min(f+window, f+(long)std::llround(band[1]*rate));
        for (long sample=from;sample<to;++sample)
            for (int channel:{0,1}) {
                const auto& b=channel?baseline.r:baseline.l;
                const auto& c=channel?control.r:control.l;
                const std::size_t j=(std::size_t)sample,prev=(std::size_t)(sample-1);
                const double bd=(double)(b[j]-b[prev]),cd=(double)(c[j]-c[prev]);
                jBase=std::max(jBase,std::fabs(bd));
                jOmit=std::max(jOmit,std::fabs(cd));
                jAdded=std::max(jAdded,std::fabs(bd-cd));
            }
        std::cout << "window " << scenario.label << " rate=" << rate
                  << " seconds=" << band[0] << ':' << band[1]
                  << " baseline=" << jBase/ref << " omitted=" << jOmit/ref
                  << " event_only=" << jAdded/ref << '\n';
    }
}
} // namespace

int main(int argc, char** argv) {
    std::ofstream archive;
    auto* originalStdout = std::cout.rdbuf();
    try {
        if (argc != 1) {
            require(argc == 3 && std::string(argv[1]) == "--out",
                    "usage: BassFunkyEventZoom [--out evidence.txt]");
            archive.open(argv[2]);
            require((bool) archive, "cannot create exploratory evidence output");
            std::cout.rdbuf(archive.rdbuf());
        }
        const auto programs = programSetups(true);
        const auto tabs = tabSetups(true);
        const auto bass = std::find_if(programs.begin(), programs.end(), [](const Setup& s){return s.name=="Bass Stack";});
        const auto funky = std::find_if(tabs.begin(), tabs.end(), [](const Setup& s){return s.name=="Funky 1 + Funky 2";});
        require(bass!=programs.end() && funky!=tabs.end(), "bank/tab fixtures missing");
        auto physical=*bass; physical.useProgrammer=false;
        const Case first { Kind::bassFirst, "bass_first_on", physical, T::retrigOn1, true };
        const Case retrig { Kind::bassRetrigger, "bass_retrigger_on", physical, T::retrigOn2, true };
        const Case off { Kind::funkyOff, "funky_single_off", *funky, T::singleOff, false };
        std::cout << std::setprecision(12);
        for (int rate : {48000,96000}) {
            zeroTimeDifference(first, rate);
            derivativeProfile(first, rate);
            zeroTimeDifference(retrig, rate);
            derivativeProfile(retrig, rate);
        }
        for (int rate : {44100,48000,96000}) {
            zeroTimeDifference(off, rate);
            derivativeProfile(off, rate);
        }
        std::cout.flush();
        std::cout.rdbuf(originalStdout);
        return 0;
    } catch(const std::exception& e){
        std::cout.rdbuf(originalStdout);
        std::cerr<<"BassFunkyZoom: "<<e.what()<<'\n'; return 1;
    }
}
