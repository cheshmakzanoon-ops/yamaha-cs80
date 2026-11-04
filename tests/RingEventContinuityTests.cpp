// Preregistered global-ring and exact amplitude-charge event invariants.
// This test does not change the original phrase score or its <= 1 criterion.
#include "RingEventProbe.h"
#include "ExactContinuationComparator.h"
#include "Ws1Tolerances.h"
#include "core/RingModulatorCircuit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace cs80;
using Probe = EngineEventContinuityProbe;
using click_continuity::sameBits;
enum class Policy { Source, Pure, Legacy };
const char* name(Policy p) { return p == Policy::Source ? "Source" : p == Policy::Pure ? "Pure" : "Legacy"; }
bool source(Policy p) { return p != Policy::Legacy; }
struct Results
{
    unsigned long long checks = 0, failures = 0;
    std::map<std::string, int> coverage;
    void require(bool ok, const std::string& label)
    {
        ++checks;
        if (!ok)
        {
            ++failures;
            if (failures <= 100) std::fprintf(stderr, "RING-CONTINUITY FAIL: %s\n", label.c_str());
        }
    }
    template<class T> void exact(T a, T b, const std::string& label, const std::string& category = {})
    {
        if (!category.empty()) ++coverage[category];
        require(sameBits(a, b), label);
    }
    void covered(const std::string& category, int expected)
    {
        require(coverage[category] == expected, "registered coverage " + category);
        std::printf("COVERAGE %s %d/%d\n", category.c_str(), coverage[category], expected);
    }
};

struct Trace
{
    std::ofstream out;
    explicit Trace(const std::string& path)
    {
        if (path.empty()) return;
        out.open(path);
        if (!out) throw std::runtime_error("cannot create witness trace " + path);
        out << "suite,policy,rate,quality,event,rank,phase_before,phase_after,float_before,float_after,"
               "precise_before,precise_after,precise_bits_before,precise_bits_after,attack_origin_after,"
               "stage_before,stage_after,telemetry_before,telemetry_after\n";
        out << std::setprecision(17);
    }
    void envelope(const char* suite, Policy p, double rate, int quality, const char* event, int rank,
                  const Probe::Envelope& a, const Probe::Envelope& b,
                  float phaseA = 0.0f, float phaseB = 0.0f, float envA = 0.0f, float envB = 0.0f)
    {
        if (!out.is_open()) return;
        out << suite << ',' << name(p) << ',' << rate << ',' << quality << ',' << event << ',' << rank << ','
            << phaseA << ',' << phaseB << ',' << a.visible.value << ',' << b.visible.value << ','
            << a.charge << ',' << b.charge << ',' << std::bit_cast<std::uint64_t>(a.charge) << ','
            << std::bit_cast<std::uint64_t>(b.charge) << ',' << b.attackOrigin << ','
            << static_cast<int>(a.visible.stage) << ',' << static_cast<int>(b.visible.stage) << ','
            << envA << ',' << envB << '\n';
    }
};

void registered(const std::string& path)
{
    std::ifstream in(path);
    if (!in) throw std::runtime_error("missing published continuation contract");
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (text.find("\"status\": \"REGISTERED\"") == std::string::npos)
        throw std::runtime_error("continuation contract is not registered");
    ws1::Tolerances limits(path, "limits");
    for (const char* key : {
        "source_zero_time_phase_bit_mismatches_max", "source_zero_time_eg_charge_bit_mismatches_max",
        "source_zero_time_eg_precise_charge_bit_mismatches_max", "source_attack_origin_precise_charge_bit_mismatches_max",
        "source_zero_time_envelope_telemetry_bit_mismatches_max", "source_zero_time_fixed_input_ring_blend_bit_mismatches_max",
        "nonmutating_probe_pcm_bit_mismatches_max", "independent_retained_state_replay_pcm_bit_mismatches_max",
        "neutral_amount_zero_pcm_bit_mismatches_max", "source_rank_retrigger_precise_charge_bit_mismatches_max",
        "source_rank_retrigger_attack_origin_bit_mismatches_max" })
        if (limits[key] != 0.0) throw std::runtime_error(std::string("registered bit bound must remain zero: ") + key);
}

ParamValues patch(Policy policy, int quality, float amount = 0.7f)
{
    ParamValues p;
    p[P_sourcePerformance] = source(policy) ? 1.0f : 0.0f;
    p[P_parityMode] = policy == Policy::Pure ? 1.0f : 0.0f;
    p[P_oversampling] = static_cast<float>(quality);
    p[P_ringSpeed] = 77.0f; p[P_ringDepth] = 0.5f; p[P_ringAmount] = amount;
    p[P_ringAttack] = 0.08f; p[P_ringDecay] = 0.45f;
    p[P_chorusOn] = p[P_drift] = p[P_voiceDispersion] = p[P_masterSustain] = 0.0f;
    p[P_keyRelease] = p[P_sustainMode] = p[P_kasScanTiming] = 0.0f;
    for (int rank : {0, 1})
    {
        p[chParam(rank, CP_saw)] = 1.0f;
        p[chParam(rank, CP_pulse)] = p[chParam(rank, CP_noise)] = p[chParam(rank, CP_sine)] = 0.0f;
        p[chParam(rank, CP_vA)] = 0.02f; p[chParam(rank, CP_vD)] = 0.7f;
        p[chParam(rank, CP_vS)] = 0.55f; p[chParam(rank, CP_vR)] = 0.7f;
        p[chParam(rank, CP_vLevel)] = 0.65f;
    }
    return p;
}
std::unique_ptr<Engine> prepared(double rate, const ParamValues& p)
{
    auto e = std::make_unique<Engine>(); e->setParams(p); e->prepare(rate); return e;
}
std::vector<float> render(Engine& e, int frames)
{
    std::vector<float> audio(static_cast<std::size_t>(frames) * 2);
    for (int pos = 0; pos < frames; pos += 64)
        e.render(audio.data() + static_cast<std::size_t>(pos) * 2, std::min(64, frames - pos));
    return audio;
}
bool pcmEqual(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) if (!sameBits(a[i], b[i])) return false;
    return true;
}
void nonfloat(Results& r, const Probe::Envelope& e, const std::string& label)
{
    r.require(e.visible.value > 0.0f && e.charge > 0.0, label + " positive charge");
    r.require(e.charge != static_cast<double>(e.visible.value), label + " non-float-representable charge");
}
void sameEnvelopeSelection(Results& r, const Probe::Envelope& a, const Probe::Envelope& b, const std::string& label)
{
    r.require(a.visible.stage == b.visible.stage, label + " stage");
    r.exact(a.visible.elapsed, b.visible.elapsed, label + " float elapsed");
    r.exact(a.elapsed, b.elapsed, label + " double elapsed");
    r.exact(a.attackOrigin, b.attackOrigin, label + " attack origin");
}
void attack(Results& r, const Probe::Envelope& e, const std::string& label)
{
    r.require(e.visible.stage == Adsr::Stage::Attack, label + " enters Attack");
    r.exact(e.visible.elapsed, 0.0f, label + " starts float clock at zero");
    r.exact(e.elapsed, 0.0, label + " starts double clock at zero");
}

struct Event { bool on; int note; int nextFrames; const char* label; };
constexpr std::array<Event, 5> events {{
    {true, 60, 1024, "first-key"}, {true, 64, 64, "legato"},
    {false, 64, 256, "partial-release"}, {false, 60, 64, "last-release"},
    {true, 67, 512, "released-audio-rekey"}
}};
void apply(Engine& e, const Event& event) { if (event.on) e.noteOn(event.note, 0.8f); else e.noteOff(event.note); }

void ringFixture(Results& r, Trace& trace, Policy policy, double rate, int quality)
{
    const auto p = patch(policy, quality);
    auto actual = prepared(rate, p), reference = prepared(rate, p), untouched = prepared(rate, p);
    const std::string prefix = std::string(name(policy)) + " " + std::to_string(static_cast<int>(rate)) + "/" + std::to_string(quality);
    std::vector<float> previous;
    const auto advance = [&](int frames)
    {
        previous = render(*actual, frames);
        const auto independent = render(*reference, frames), ordinary = render(*untouched, frames);
        r.require(pcmEqual(previous, ordinary), prefix + " nonmutating observer PCM");
        r.require(pcmEqual(previous, independent), prefix + " independent retained-state replay PCM");
    };
    advance(513);
    for (const auto& event : events)
    {
        const std::string label = prefix + " " + event.label;
        const auto before = Probe::ring(*actual), refBefore = Probe::ring(*reference);
        const float beforeCarrier = std::sin(kTwoPi * before.phase);
        r.require(before.phase != 0.0f && beforeCarrier != 0.0f, label + " nonzero carrier witness");
        if (event.note == 67)
        {
            r.require(before.charge.visible.value > 0.0f, label + " positive released ring envelope");
            r.exact(before.output, before.charge.visible.value, label + " charge/telemetry witness");
            if (source(policy)) nonfloat(r, before.charge, label);
            bool tail = false;
            for (int voice = 0; voice < Probe::voiceCount; ++voice)
            {
                const auto info = actual->getVoiceInfo(voice);
                tail |= info.active && !info.held && !info.pedalHeld
                    && actual->getRankEnvelopeState(voice, 0).amplitude.value > 0.0f
                    && actual->getRankEnvelopeState(voice, 1).amplitude.value > 0.0f;
            }
            r.require(tail, label + " active released audio in both ranks");
            r.require(std::any_of(previous.begin(), previous.end(), [](float x) { return x != 0.0f; }),
                      label + " nonzero preceding 64-frame PCM");
        }
        apply(*actual, event); apply(*reference, event); apply(*untouched, event);
        if (source(policy) && event.on && !refBefore.keyDown)
            Probe::retainedRingReference(*reference, refBefore);
        // OBSERVER_INERT_POINT: no elapsed time belongs between event snapshots.
        const auto after = Probe::ring(*actual);
        trace.envelope("ring", policy, rate, quality, event.label, -1, before.charge, after.charge,
                       before.phase, after.phase, before.output, after.output);
        const bool firstKey = event.on && !before.keyDown;
        r.exact(before.frequency, after.frequency, label + " frequency cache changes only with rendered time");
        if (source(policy))
        {
            const std::string suffix = name(policy);
            r.exact(before.phase, after.phase, label + " source carrier phase", "phase_" + suffix);
            r.exact(before.charge.visible.value, after.charge.visible.value, label + " source float charge", "float_charge_" + suffix);
            r.exact(before.charge.charge, after.charge.charge, label + " source precise charge", "precise_charge_" + suffix);
            r.exact(before.output, after.output, label + " source envelope telemetry", "telemetry_" + suffix);
            const float afterCarrier = std::sin(kTwoPi * after.phase);
            for (float dry : {0.0f, -0.1f, 0.1f})
                r.exact(RingModulatorCircuit::blend(dry, beforeCarrier, 0.7f),
                        RingModulatorCircuit::blend(dry, afterCarrier, 0.7f),
                        label + " fixed-input ring blend", "blend_" + suffix);
            if (firstKey)
                r.exact(after.charge.attackOrigin, before.charge.charge,
                        label + " retained precise attack origin", "attack_origin_" + suffix);
        }
        else if (firstKey)
        {
            r.exact(after.phase, 0.0f, label + " historical Legacy phase reset");
            r.exact(after.output, 0.0f, label + " historical Legacy envelope reset");
            r.exact(after.charge.visible.value, 0.0f, label + " historical Legacy float reset");
            r.exact(after.charge.charge, 0.0, label + " historical Legacy precise reset");
        }
        else r.exact(before.phase, after.phase, label + " Legacy nontrigger phase retention");
        if (firstKey)
        {
            attack(r, after.charge, label);
            r.require(after.keyDown, label + " opens ring gate");
        }
        else if (event.label == std::string("last-release"))
        {
            r.require(!after.keyDown && after.charge.visible.stage == Adsr::Stage::Release,
                      label + " last release selects Release and clears gate");
            r.exact(after.charge.visible.elapsed, 0.0f, label + " release float clock starts at zero");
            r.exact(after.charge.elapsed, 0.0, label + " release double clock starts at zero");
        }
        else
        {
            r.require(before.keyDown == after.keyDown, label + " retains ring gate");
            sameEnvelopeSelection(r, before.charge, after.charge, label + " nontrigger selection");
        }
        advance(event.nextFrames);
    }
    actual->reset();
    const auto cleared = Probe::ring(*actual);
    r.exact(cleared.phase, 0.0f, prefix + " explicit reset phase");
    r.exact(cleared.output, 0.0f, prefix + " explicit reset telemetry");
    r.exact(cleared.charge.visible.value, 0.0f, prefix + " explicit reset float charge");
    r.exact(cleared.charge.charge, 0.0, prefix + " explicit reset precise charge");
    r.require(!cleared.keyDown && cleared.charge.visible.stage == Adsr::Stage::Idle, prefix + " explicit reset gate/stage");
}

void neutralFixture(Results& r, Policy policy, double rate, int quality)
{
    auto actual = prepared(rate, patch(policy, quality, 0.0f));
    auto historical = prepared(rate, patch(policy, quality, 0.0f));
    const std::string label = std::string(name(policy)) + " neutral ring PCM";
    const auto advance = [&](int frames) { r.require(pcmEqual(render(*actual, frames), render(*historical, frames)), label); };
    advance(513);
    for (const auto& event : events)
    {
        const bool firstKey = event.on && !Probe::ring(*historical).keyDown;
        apply(*actual, event); apply(*historical, event);
        if (firstKey) Probe::historicalRingReset(*historical);
        advance(event.nextFrames);
    }
}

void rankFixture(Results& r, Trace& trace, Policy policy, double rate, int quality)
{
    auto e = prepared(rate, patch(policy, quality, 0.0f));
    for (int note = 48; note < 56; ++note) e->noteOn(note, 0.8f);
    render(*e, 997); e->noteOff(48); render(*e, 31);
    const auto info = e->getVoiceInfo(0);
    const std::string prefix = std::string(name(policy)) + " rank " + std::to_string(static_cast<int>(rate)) + "/" + std::to_string(quality);
    r.require(info.active && !info.held && !info.pedalHeld && info.note == 48, prefix + " voice0 sounding release witness");
    int held = 0;
    for (int voice = 0; voice < Probe::voiceCount; ++voice) held += e->getVoiceInfo(voice).held;
    r.require(held == 7, prefix + " seven other keys prevent global re-trigger");
    std::array<Probe::Envelope, 2> before {Probe::rank(*e, 0, 0), Probe::rank(*e, 0, 1)};
    for (int rank : {0, 1})
    {
        r.require(before[rank].visible.value > 0.0f, prefix + " positive rank float charge");
        if (source(policy)) nonfloat(r, before[rank], prefix + " rank" + std::to_string(rank));
    }
    const auto globalBefore = Probe::ring(*e);
    e->noteOn(72, 0.8f);
    r.require(e->getVoiceInfo(0).note == 72 && e->getVoiceInfo(0).held, prefix + " reassignment selects voice0");
    const auto globalAfter = Probe::ring(*e);
    r.exact(globalBefore.phase, globalAfter.phase, prefix + " isolates global carrier");
    r.exact(globalBefore.charge.charge, globalAfter.charge.charge, prefix + " isolates global charge");
    for (int rank : {0, 1})
    {
        const auto after = Probe::rank(*e, 0, rank);
        const std::string label = prefix + " rank" + std::to_string(rank);
        trace.envelope("rank", policy, rate, quality, "released-tail-rekey", rank, before[rank], after);
        attack(r, after, label);
        if (source(policy))
        {
            const std::string suffix = std::string(name(policy)) + "_" + std::to_string(rank);
            r.exact(before[rank].visible.value, after.visible.value, label + " source float retention");
            r.exact(before[rank].charge, after.charge, label + " source precise retention", "rank_charge_" + suffix);
            r.exact(before[rank].charge, after.attackOrigin, label + " precise attack origin", "rank_origin_" + suffix);
        }
        else
        {
            r.exact(after.visible.value, 0.0f, label + " Legacy float reset");
            r.exact(after.charge, 0.0, label + " Legacy precise reset");
            r.exact(after.attackOrigin, 0.0, label + " Legacy attack origin reset");
        }
    }
}

void primitiveFixture(Results& r)
{
    for (float rate : {88200.0f, 96000.0f, 192000.0f, 384000.0f})
        for (bool release : {false, true})
        {
            Adsr e; e.noteOn(0.08f, 0.45f, 0.55f, 0.7f);
            for (int n = 0; n < 997; ++n) e.tick(rate, false, true);
            if (release) { e.noteOff(); for (int n = 0; n < 31; ++n) e.tick(rate, false, true); }
            const std::string label = std::string("primitive ") + (release ? "released " : "held ") + std::to_string(static_cast<int>(rate));
            const auto before = Probe::envelope(e);
            nonfloat(r, before, label);
            Adsr oldDefault = e;
            e.noteOn(0.08f, 0.45f, 0.55f, 0.7f, true);
            const auto after = Probe::envelope(e);
            r.exact(before.visible.value, after.visible.value, label + " preserving float charge");
            r.exact(before.charge, after.charge, label + " preserving precise charge");
            r.exact(before.charge, after.attackOrigin, label + " preserving precise attack origin");
            attack(r, after, label);
            oldDefault.noteOn(0.08f, 0.45f, 0.55f, 0.7f);
            const auto old = Probe::envelope(oldDefault);
            r.exact(old.charge, static_cast<double>(before.visible.value), label + " historical default double rounding");
            r.exact(old.attackOrigin, static_cast<double>(before.visible.value), label + " historical default attack origin");
        }
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2) throw std::runtime_error("usage: gate CONTRACT [--suite all|ring|rank|primitive] [--trace CSV]");
        std::string suite = "all", tracePath;
        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--suite" && i + 1 < argc) suite = argv[++i];
            else if (arg == "--trace" && i + 1 < argc) tracePath = argv[++i];
            else throw std::runtime_error("unknown argument " + arg);
        }
        if (suite != "all" && suite != "ring" && suite != "rank" && suite != "primitive")
            throw std::runtime_error("unknown suite " + suite);
        registered(argv[1]); Results r; Trace trace(tracePath);
        for (Policy policy : {Policy::Source, Policy::Pure, Policy::Legacy})
            for (double rate : {44100.0, 48000.0, 96000.0})
                for (int quality : {0, 1, 2})
                {
                    if (suite == "all" || suite == "ring")
                    {
                        ringFixture(r, trace, policy, rate, quality);
                        if (source(policy)) neutralFixture(r, policy, rate, quality);
                    }
                    if (suite == "all" || suite == "rank") rankFixture(r, trace, policy, rate, quality);
                }
        for (const std::string policy : {"Source", "Pure"})
        {
            if (suite == "all" || suite == "ring")
            {
                for (const std::string field : {"phase_", "float_charge_", "precise_charge_", "telemetry_"}) r.covered(field + policy, 45);
                r.covered("blend_" + policy, 135); r.covered("attack_origin_" + policy, 18);
            }
            if (suite == "all" || suite == "rank")
                for (int rank : {0, 1})
                {
                    const std::string suffix = policy + "_" + std::to_string(rank);
                    r.covered("rank_charge_" + suffix, 9); r.covered("rank_origin_" + suffix, 9);
                }
        }
        if (suite == "all" || suite == "primitive") primitiveFixture(r);
        std::printf("Ring event continuity: %llu assertions, %llu failures\n", r.checks, r.failures);
        return r.failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "RING-CONTINUITY CONTRACT ERROR: %s\n", e.what()); return 2;
    }
}
