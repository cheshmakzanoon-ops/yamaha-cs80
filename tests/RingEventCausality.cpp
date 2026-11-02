// Descriptive targeted-event controls. Reuse the exact production audit's
// fixture adapters, phrase schedule and score calculation without changing it.
#include "RingEventProbe.h"
#include "ExactContinuationComparator.h"
#define main cs80_original_audit_entry_not_called
#include "../src/tools/engine_audit.cpp"
#undef main

#include <bit>
#include <memory>

namespace ring_causality {
using Probe = cs80::EngineEventContinuityProbe;
enum class Variant { Historical, PhaseOnly, ChargeOnly, Both, Omitted, Production };
const char* tag(Variant v)
{
    switch (v)
    {
        case Variant::Historical: return "historical";
        case Variant::PhaseOnly: return "phase_only";
        case Variant::ChargeOnly: return "charge_only";
        case Variant::Both: return "phase_and_charge";
        case Variant::Omitted: return "omitted_target";
        case Variant::Production: return "production";
    }
    return "unknown";
}
struct Peak { double jump = 0; long sample = 0; int channel = 0; };
Peak jumpPeak(const Rendered& a, long begin, long end)
{
    Peak p;
    for (long i = std::max(1L, begin); i < end; ++i)
        for (int ch : {0, 1})
        {
            const auto& samples = ch ? a.r : a.l;
            const double jump = std::fabs(samples[static_cast<std::size_t>(i)] - samples[static_cast<std::size_t>(i - 1)]);
            if (jump > p.jump) p = {jump, i, ch};
        }
    return p;
}
unsigned long long differences(const Rendered& a, const Rendered& b, long begin, long end)
{
    unsigned long long count = 0;
    for (long i = begin; i < end; ++i)
    {
        const auto n = static_cast<std::size_t>(i);
        count += !click_continuity::sameBits(a.l[n], b.l[n]);
        count += !click_continuity::sameBits(a.r[n], b.r[n]);
    }
    return count;
}
long firstDifference(const Rendered& a, const Rendered& b)
{
    for (std::size_t i = 0; i < a.l.size(); ++i)
        if (!click_continuity::sameBits(a.l[i], b.l[i]) || !click_continuity::sameBits(a.r[i], b.r[i]))
            return static_cast<long>(i);
    return -1;
}
void stateRow(std::ofstream& out, const Setup& setup, int rate, Variant v, int note, const char* side,
              long frame, const Engine& engine)
{
    const auto ring = Probe::ring(engine);
    int active = 0, held = 0, released = 0;
    for (int voice = 0; voice < Probe::voiceCount; ++voice)
    {
        const auto info = engine.getVoiceInfo(voice);
        active += info.active; held += info.held || info.pedalHeld;
        released += info.active && !info.held && !info.pedalHeld;
    }
    for (int voice = 0; voice < Probe::voiceCount; ++voice)
    {
        const auto info = engine.getVoiceInfo(voice);
        for (int rank : {0, 1})
        {
            const auto amp = Probe::rank(engine, voice, rank);
            out << setup.name << ',' << rate << ',' << tag(v) << ',' << note << ',' << side << ',' << frame << ','
                << ring.phase << ',' << std::sin(kTwoPi * ring.phase) << ',' << ring.output << ','
                << ring.charge.charge << ',' << std::bit_cast<std::uint64_t>(ring.charge.charge) << ','
                << ring.frequency << ',' << ring.keyDown << ',' << active << ',' << held << ',' << released << ','
                << voice << ',' << rank << ',' << info.note << ',' << info.active << ',' << info.held << ','
                << info.pedalHeld << ',' << info.assignmentSerial << ',' << amp.visible.value << ','
                << amp.charge << ',' << static_cast<int>(amp.visible.stage) << '\n';
        }
    }
}

Rendered replay(const Setup& setup, int rate, Variant variant, double target,
                std::ofstream& state, bool historicalRanks, std::ofstream* continuation = nullptr)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate);
    auto frame = ControlFrame::programmer(setup.values, setup.sources);
    require(engine->setControlFrame(frame), "causal frame rejected");
    engine->reset();
    Rendered audio;
    audio.rate = rate;
    const long frames = static_cast<long>(std::llround(T::length * rate));
    audio.l.resize(static_cast<std::size_t>(frames)); audio.r.resize(static_cast<std::size_t>(frames));
    std::array<float, kBlock * 2> block {};
    const auto schedule = phrase();
    std::size_t next = 0;
    for (long pos = 0; pos < frames; pos += kBlock)
    {
        const int count = static_cast<int>(std::min<long>(kBlock, frames - pos));
        while (next < schedule.size() && static_cast<long>(std::ceil(schedule[next].t * rate / kBlock)) * kBlock <= pos)
        {
            const auto& event = schedule[next++];
            const bool targeted = event.on && event.t == target;
            if (targeted) stateRow(state, setup, rate, variant, event.note, "before", pos, *engine);
            if (targeted && variant == Variant::Omitted)
            {
                stateRow(state, setup, rate, variant, event.note, "after-omitted", pos, *engine);
                continue;
            }
            const auto before = Probe::ring(*engine);
            const auto assignments = Probe::assignments(*engine);
            std::array<std::array<click_continuity::Continuation, 2>, Probe::voiceCount> priorRanks {};
            std::array<std::array<double, 2>, Probe::voiceCount> priorCharge {};
            std::array<bool, Probe::voiceCount> priorActive {};
            if (targeted && continuation)
                for (int voice = 0; voice < Probe::voiceCount; ++voice)
                {
                    priorActive[voice] = engine->getVoiceInfo(voice).active;
                    for (int rank : {0, 1})
                    {
                        priorRanks[voice][rank] = click_continuity::observe(*engine, voice, rank);
                        priorCharge[voice][rank] = Probe::rank(*engine, voice, rank).charge;
                    }
                }
            if (event.on) engine->noteOn(event.note, event.vel, 1); else engine->noteOff(event.note, 1);
            if (variant != Variant::Production)
            {
                if (historicalRanks && event.on) Probe::historicalRankPrecision(*engine, assignments);
                if (event.on && !before.keyDown && Probe::ring(*engine).keyDown)
                {
                    Probe::historicalRingReset(*engine);
                    if (targeted)
                        Probe::retainedRingReference(*engine, before,
                            variant == Variant::PhaseOnly || variant == Variant::Both,
                            variant == Variant::ChargeOnly || variant == Variant::Both);
                }
            }
            if (targeted) stateRow(state, setup, rate, variant, event.note, "after", pos, *engine);
            if (targeted && continuation)
            {
                const auto assigned = Probe::assignments(*engine);
                for (int voice = 0; voice < Probe::voiceCount; ++voice)
                    if (assigned[voice] != assignments[voice])
                        for (int rank : {0, 1})
                        {
                            const auto diff = click_continuity::compare(priorRanks[voice][rank],
                                click_continuity::observe(*engine, voice, rank));
                            const auto amp = Probe::rank(*engine, voice, rank);
                            *continuation << setup.name << ',' << rate << ',' << tag(variant) << ',' << pos << ','
                                << event.note << ',' << voice << ',' << rank << ',' << priorActive[voice] << ','
                                << diff.oscillator << ',' << diff.nodal << ',' << diff.envelopes << ',' << diff.vca << ','
                                << diff.phase << ',' << diff.total() << ','
                                << std::bit_cast<std::uint64_t>(priorCharge[voice][rank]) << ','
                                << std::bit_cast<std::uint64_t>(amp.charge) << '\n';
                        }
            }
        }
        require(engine->setControlFrame(frame), "causal repeated frame rejected");
        engine->render(block.data(), count);
        for (int i = 0; i < count; ++i)
        {
            const auto targetIndex = static_cast<std::size_t>(pos + i);
            const float l = block[static_cast<std::size_t>(i) * 2], r = block[static_cast<std::size_t>(i) * 2 + 1];
            require(std::isfinite(l) && std::isfinite(r), "nonfinite causal replay");
            audio.l[targetIndex] = l; audio.r[targetIndex] = r;
        }
    }
    audio.voicesAtEnd = engine->activeVoices();
    return audio;
}

void run(const fs::path& directory, const std::vector<int>& rates, bool expectHistoricalProduction)
{
    fs::create_directories(directory);
    std::ofstream states(directory / "event-states.csv"), scores(directory / "event-scores.csv");
    require(states.good() && scores.good(), "cannot create causal evidence");
    states << "name,rate,variant,note,boundary,host_frame,ring_phase,carrier,ring_float_charge,ring_precise_charge,"
              "ring_precise_bits,ring_frequency,ring_key_down,active_voices,held_voices,released_voices,voice,rank,"
              "voice_note,active,held,pedal_held,assignment_serial,amp_float,amp_precise,amp_stage\n";
    scores << "name,rate,variant,event_frame,peak_frame,peak_channel,numerator,historical_denominator,common_reference_score,"
              "prefix_bit_mismatches,first_pcm_difference,changed_pcm_samples,delta_peak_frame,delta_peak_jump,"
              "native_audit_retrigger,native_audit_steal,ring_amount\n";
    states << std::setprecision(17); scores << std::setprecision(17);
    const auto programs = programSetups(true);
    const auto laser = std::find_if(programs.begin(), programs.end(), [](const Setup& s) { return s.name == "Laser Sweep"; });
    require(laser != programs.end(), "Laser Sweep missing");
    for (int rate : rates)
    {
        const long eventFrame = static_cast<long>(std::ceil(T::retrigOn2 * rate / kBlock)) * kBlock;
        const long window = static_cast<long>(std::llround(.008 * rate));
        const auto originalProduction = render(*laser, rate, phrase(), T::length);
        const auto baseline = replay(*laser, rate, Variant::Historical, T::retrigOn2, states, true);
        if (expectHistoricalProduction)
            require(differences(baseline, originalProduction, 0, static_cast<long>(baseline.l.size())) == 0,
                    "historical replay differs from untouched production audit");
        const double denominator = std::max(maxJump(baseline, idx(baseline, T::chordOn), idx(baseline, T::chordOff)), 1e-7);
        for (Variant variant : {Variant::Historical, Variant::PhaseOnly, Variant::ChargeOnly,
                                Variant::Both, Variant::Omitted, Variant::Production})
        {
            const auto audio = variant == Variant::Historical ? baseline
                : replay(*laser, rate, variant, T::retrigOn2, states, true);
            if (variant == Variant::Production)
                require(differences(audio, originalProduction, 0, static_cast<long>(audio.l.size())) == 0,
                        "instrumented production differs from untouched audit PCM");
            const auto prefix = differences(audio, baseline, 0, eventFrame);
            if (variant != Variant::Production) require(prefix == 0, "targeted ring replay prefix mismatch");
            const auto peak = jumpPeak(audio, eventFrame, eventFrame + window);
            Rendered difference = audio;
            for (std::size_t i = 0; i < audio.l.size(); ++i)
            {
                difference.l[i] -= baseline.l[i]; difference.r[i] -= baseline.r[i];
            }
            const auto changedPeak = jumpPeak(difference, eventFrame, eventFrame + window);
            const auto row = analyse(*laser, audio);
            scores << laser->name << ',' << rate << ',' << tag(variant) << ',' << eventFrame << ','
                   << peak.sample << ',' << peak.channel << ',' << peak.jump << ',' << denominator << ','
                   << peak.jump / denominator << ',' << prefix << ',' << firstDifference(audio, baseline) << ','
                   << differences(audio, baseline, eventFrame, static_cast<long>(audio.l.size())) << ','
                   << changedPeak.sample << ',' << changedPeak.jump << ',' << row.clickRetrig << ','
                   << row.clickSteal << ',' << laser->values[P_ringAmount] << '\n';
            scores.flush(); states.flush();
        }
    }
}

// Descriptive follow-up for the pre-existing Clavichord steal metric. This
// does not add or change a gate; observe whether its intended tail is present.
void clavichord(const fs::path& directory, const std::vector<int>& rates)
{
    fs::create_directories(directory);
    std::ofstream states(directory / "clavichord-states.csv"), scores(directory / "clavichord-scores.csv");
    std::ofstream continuation(directory / "clavichord-continuation.csv");
    continuation << "name,rate,variant,event_frame,note,voice,rank,was_active,wsc_mismatches,nodal_mismatches,"
                    "envelope_mismatches,vca_mismatches,phase_proxy_mismatches,total_exposed_mismatches,"
                    "precise_before_bits,precise_after_bits\n";
    states << "name,rate,variant,note,boundary,host_frame,ring_phase,carrier,ring_float_charge,ring_precise_charge,"
              "ring_precise_bits,ring_frequency,ring_key_down,active_voices,held_voices,released_voices,voice,rank,"
              "voice_note,active,held,pedal_held,assignment_serial,amp_float,amp_precise,amp_stage\n";
    scores << "name,rate,variant,event_frame,peak_frame,numerator,denominator,score,prefix_bit_mismatches,"
              "production_full_pcm_mismatches,ring_amount\n";
    states << std::setprecision(17); scores << std::setprecision(17);
    const auto tabs = tabSetups(true);
    const auto selected = std::find_if(tabs.begin(), tabs.end(), [](const Setup& s) {
        return s.name == "Clavichord 1 + Clavichord 2";
    });
    require(selected != tabs.end(), "Clavichord paired tabs missing");
    for (int rate : rates)
    {
        const auto production = render(*selected, rate, phrase(), T::length);
        const auto historical = replay(*selected, rate, Variant::Historical, T::stealOn, states, true, &continuation);
        const long event = static_cast<long>(std::ceil(T::stealOn * rate / kBlock)) * kBlock;
        const long window = static_cast<long>(std::llround(.008 * rate));
        const double reference = std::max(maxJump(historical, idx(historical, T::bigOn), idx(historical, T::bigOff)), 1e-7);
        for (Variant variant : {Variant::Historical, Variant::Production, Variant::Omitted})
        {
            const auto audio = variant == Variant::Historical ? historical
                : replay(*selected, rate, variant, T::stealOn, states, true, &continuation);
            const auto prefix = differences(audio, historical, 0, event);
            if (variant == Variant::Omitted) require(prefix == 0, "omitted Clavichord prefix mismatch");
            if (variant == Variant::Production)
                require(differences(audio, production, 0, static_cast<long>(audio.l.size())) == 0,
                        "Clavichord probe changes ordinary production PCM");
            const auto peak = jumpPeak(audio, event, event + window);
            scores << selected->name << ',' << rate << ',' << tag(variant) << ',' << event << ','
                   << peak.sample << ',' << peak.jump << ',' << reference << ',' << peak.jump / reference << ','
                   << prefix << ',' << differences(audio, production, 0, static_cast<long>(audio.l.size())) << ','
                   << selected->values[P_ringAmount] << '\n';
            scores.flush(); states.flush();
        }
    }
}
} // namespace ring_causality

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2) throw std::runtime_error("usage: causal OUT_DIR [--rates 44100,48000,96000] [--expect-historical-production]");
        std::vector<int> rates {44100, 48000, 96000}; bool historical = false, clavichord = false;
        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--rates" && i + 1 < argc) rates = parseRates(argv[++i]);
            else if (arg == "--expect-historical-production") historical = true;
            else if (arg == "--clavichord") clavichord = true;
            else throw std::runtime_error("unknown argument " + arg);
        }
        if (clavichord) ring_causality::clavichord(argv[1], rates);
        else ring_causality::run(argv[1], rates, historical);
        std::printf("Original phrase ring counterfactuals written; existing <=1 requirements remain unchanged.\n");
        return 0;
    }
    catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
