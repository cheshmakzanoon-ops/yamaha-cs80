// Archived exploratory zero-time Clavichord 96k event attribution. This is
// deliberately NOT an acceptance gate or a new source-model sound law.
// Compile manually as documented in docs/evidence/CLICK_BANK_COUNTERFACTUAL_20261102.md.
#define main cs80_original_engine_audit_main
#include "../../src/tools/engine_audit.cpp"
#undef main
#include "../../tests/ExactContinuationComparator.h"

#include <array>
#include <iostream>

int main()
{
    try {
        const int rate = 96000;
        auto tabs = tabSetups(true);
        const auto setup = std::find_if(tabs.begin(), tabs.end(), [](const Setup& s) {
            return s.name == "Clavichord 1 + Clavichord 2";
        });
        require(setup != tabs.end(), "Clavichord tab fixture absent");
        cs80::Engine e;
        e.prepare(rate);
        cs80::ControlFrame frame;
        frame.effectiveValues = setup->values;
        frame.hardwareSources = setup->sources;
        frame = cs80::ControlFrame::programmer(frame.effectiveValues, frame.hardwareSources);
        require(e.setControlFrame(frame), "control frame rejected");
        e.reset();
        std::array<float, kBlock * 2> output {};
        const auto events = phrase();
        std::size_t next = 0;
        const long eventFrame = (long) std::ceil(T::stealOn * rate / kBlock) * kBlock;
        for (long f = 0; f < eventFrame; f += kBlock) {
            while (next < events.size() &&
                   (long) std::ceil(events[next].t * rate / kBlock) * kBlock <= f) {
                const auto& ev = events[next++];
                if (ev.on) e.noteOn(ev.note, ev.vel, 1);
                else e.noteOff(ev.note, 1);
            }
            require(e.setControlFrame(frame), "frame rejected during replay");
            e.render(output.data(), kBlock);
        }
        require(next < events.size() && events[next].t == T::stealOn, "event sequence misaligned");
        std::array<std::array<click_continuity::Continuation, 2>, 8> before;
        std::array<cs80::Engine::VoiceInfo, 8> oldInfo;
        for (int v = 0; v < 8; ++v) {
            oldInfo[(std::size_t) v] = e.getVoiceInfo(v);
            for (int rank = 0; rank < 2; ++rank)
                before[(std::size_t) v][(std::size_t) rank] = click_continuity::observe(e, v, rank);
        }
        for (int note : {48, 52, 55, 60}) e.noteOn(note, .8f, 1);
        require(e.setControlFrame(frame), "event-frame control rejected");
        int changedMetadata = 0, mismatchAll = 0;
        for (int v = 0; v < 8; ++v) {
            const auto& b = oldInfo[(std::size_t) v];
            const auto a = e.getVoiceInfo(v);
            if (b.held != a.held || b.note != a.note || b.assignmentSerial != a.assignmentSerial)
                ++changedMetadata;
            for (int rank = 0; rank < 2; ++rank) {
                const auto after = click_continuity::observe(e, v, rank);
                const auto diff = click_continuity::compare(before[(std::size_t) v][(std::size_t) rank], after);
                mismatchAll += diff.total();
                std::cout << "voice=" << v << " rank=" << rank
                          << " old_note=" << b.note << " old_active=" << b.active << " old_held=" << b.held
                          << " new_note=" << a.note << " new_active=" << a.active << " new_held=" << a.held
                          << " oscillator_mismatch=" << diff.oscillator
                          << " nodal_mismatch=" << diff.nodal
                          << " EG_mismatch=" << diff.envelopes
                          << " VCA_mismatch=" << diff.vca
                          << " phase_mismatch=" << diff.phase << '\n';
            }
        }
        std::cout << "event_frame=" << eventFrame << " affected_voice_slots=" << changedMetadata
                  << " compared_exposed_fields=" << (8 * 2 * click_continuity::exposedFieldsPerRank)
                  << " zero_time_state_bit_mismatches=" << mismatchAll << '\n';
        require(changedMetadata == 4, "expected four assigned new notes");
        require(mismatchAll == 0, "instantaneous oscillator/filter/EG/VCA state modification");

        // Host output difference onset, a separate diagnostic from the exact
        // zero-time state probe. Use unchanged original event render and a
        // control omitting exactly the four steal-on events.
        auto omitted = events;
        const auto it = std::remove_if(omitted.begin(), omitted.end(), [](const Event& event) {
            return event.on && event.t == T::stealOn &&
                   (event.note == 48 || event.note == 52 || event.note == 55 || event.note == 60);
        });
        const int removed = (int) std::distance(it, omitted.end());
        omitted.erase(it, omitted.end());
        require(removed == 4, "unexpected omitted steal event count");
        const auto baseline = render(*setup, rate, events, T::stealOn + .02);
        const auto control = render(*setup, rate, omitted, T::stealOn + .02);
        const double ref = std::max(maxJump(baseline, idx(baseline, T::bigOn), idx(baseline, T::bigOff)), 1e-7);
        for (const auto bounds : std::array<std::array<int, 2>, 6> {{{0, 63}, {63, 128}, {128, 256},
                                                                     {256, 384}, {384, 512}, {512, 768}}}) {
            double maxAddedDerivative = 0, maxBaselineDerivative = 0;
            for (long sample = eventFrame + bounds[0]; sample < eventFrame + bounds[1]; ++sample)
                for (const auto channel : {0, 1}) {
                    const auto& b = channel ? baseline.r : baseline.l;
                    const auto& c = channel ? control.r : control.l;
                    const auto bs = (std::size_t) sample, prior = (std::size_t) (sample - 1);
                    const double db = (double) b[bs] - b[prior], dc = (double) c[bs] - c[prior];
                    maxAddedDerivative = std::max(maxAddedDerivative, std::fabs(db - dc));
                    maxBaselineDerivative = std::max(maxBaselineDerivative, std::fabs(db));
                }
            std::cout << "derivative_window_offset_frames=" << bounds[0] << ':' << bounds[1]
                      << " event_only_max_normalized=" << (maxAddedDerivative / ref)
                      << " baseline_max_normalized=" << (maxBaselineDerivative / ref) << '\n';
        }
        return 0;
    } catch (const std::exception& err) {
        std::cerr << "ClavStealZeroTime: " << err.what() << '\n';
        return 1;
    }
}
