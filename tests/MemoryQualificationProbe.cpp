// Explicit research probe, not a shipping/CTest accuracy certification.
#include "experimental/LoadedMemoryReceivers.h"
#include "reference/MemoryPanelPortCompiler.h"
#include "reference/MemoryReceiverNetwork.h"

#include <chrono>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ex = cs80::experimental;
namespace ref = cs80::reference::memory_panel;
namespace sf = cs80::source_factory;
using Inputs = ex::MemoryReceiverInputs;
using Buses = ex::MemoryPanelPortMap::Voltages;
namespace
{
struct Limits { double bus, cv, q, seconds, level; };
double number(const std::string& json, const char* key)
{
    const auto at = json.find(std::string("\"") + key + "\""), colon = json.find(':', at);
    if (at == std::string::npos || colon == std::string::npos) throw std::runtime_error("missing study limit");
    const double v = std::strtod(json.c_str() + colon + 1, nullptr);
    if (!std::isfinite(v) || v <= 0.0) throw std::runtime_error("invalid study limit");
    return v;
}
void require(bool yes, const char* label) { if (!yes) throw std::runtime_error(label); }
ref::ReceiverDrives physical(const Inputs& input)
{
    ref::ReceiverDrives out; out.masterResVolts = 10.0 * (1.0 - input.masterResonance);
    for (std::size_t i = 0; i < 8; ++i)
        out.voices[i] = {(5.0 / 3.0) * input.voices[i].filterEnvelopeZ, input.voices[i].velocity, input.voices[i].pressure,
                        input.keyboardCutoffVolts[i], input.keyboardLevelVolts[i]};
    return out;
}
ref::Setup source(int rank, int selector, double pot, int shape)
{
    ref::Setup out; out.rank = rank; out.selector = selector; out.electrical.potOhms = pot;
    for (std::size_t bank = 0; bank < 3; ++bank)
        for (std::size_t row = 0; row < 26; ++row)
            out.banks[bank][row] = shape < 5 ? shape * 0.25
                : static_cast<double>((row * (shape == 5 ? 7 : 5) + bank * 3) % 9) / 8.0;
    return out;
}
Inputs target(int profile, double amount)
{
    Inputs out;
    if (profile == 3 || profile == 4) out.masterResonance = 1.0 - amount;
    for (std::size_t v = 0; v < 8; ++v)
    {
        if (profile == 0 || profile == 1 || profile == 4) out.voices[v].filterEnvelopeZ = (profile == 1 ? -3.0 : 3.0) * amount;
        if (profile == 2 || profile == 4) { out.voices[v].velocity = amount * (v + 1) / 8.0; out.voices[v].pressure = amount * (8 - v) / 8.0; }
        if (profile == 4) { out.keyboardCutoffVolts[v] = amount * (static_cast<double>(v) - 3.5) * 0.2; out.keyboardLevelVolts[v] = amount * v * 0.1; }
    }
    return out; // profile 5 is the zero-drive destination
}
Inputs between(const Inputs& a, const Inputs& b, double fraction)
{
    Inputs out; const auto mix = [fraction](double x, double y) { return x + fraction * (y - x); };
    out.masterResonance = mix(a.masterResonance, b.masterResonance);
    for (std::size_t i = 0; i < 8; ++i)
    {
        out.voices[i] = {mix(a.voices[i].filterEnvelopeZ, b.voices[i].filterEnvelopeZ),
                        mix(a.voices[i].velocity, b.voices[i].velocity), mix(a.voices[i].pressure, b.voices[i].pressure)};
        out.keyboardCutoffVolts[i] = mix(a.keyboardCutoffVolts[i], b.keyboardCutoffVolts[i]);
        out.keyboardLevelVolts[i] = mix(a.keyboardLevelVolts[i], b.keyboardLevelVolts[i]);
    }
    return out;
}
ex::MemoryControlFrame prepare(const ref::Setup& source, const Inputs& center)
{
    const auto truth = ref::solveReceivers(source, physical(center));
    require(truth.converged, "nonlinear preparation failed");
    auto setup = source; setup.injectedAmps = truth.injectionAmps;
    ex::MemoryControlFrame out;
    require(ref::compileLoadedPortMap(setup, out.ports), "port map preparation failed");
    out.electrical = setup.electrical; out.frameVersion = ex::memoryControlFrameVersion;
    out.adapterVersion = ex::memory_control::version; out.sourceModelVersion = sf::modelVersion; out.sourceHash = sf::sourceFingerprint;
    out.ready = true; out.selectedSwitches = {true, true};
    return out;
}
struct Observed
{
    Buses bus {};
    std::array<std::array<double, 2>, 8> cv {}, q {};
    std::array<double, 8> level {};
    std::array<double, 6> seconds {};
};
// Cheap per-voice algebra with fresh inputs and caller-supplied shared buses.
// Used only by this scheduling experiment, never installed as an Engine path.
void localReadout(const Buses& bus, const Inputs& input, Observed& out)
{
    out.bus = bus;
    for (std::size_t voice = 0; voice < 8; ++voice)
    {
        const auto& d = input.voices[voice];
        const double touch = bus[22] * d.velocity + bus[24] * d.pressure + input.keyboardCutoffVolts[voice];
        for (std::size_t side = 0; side < 2; ++side)
        {
            const bool hp = side == 0;
            const double gc = 1.0 / 39000.0, gt = 1.0 / (hp ? 120000.0 : 39000.0), ge = 1.0 / (hp ? 100000.0 : 47000.0);
            // Dividing the pin equation by gc/diagonal cancels the diagonal.
            // This is the same explicitly named effective-CV convention.
            out.cv[voice][side] = (hp ? sf::alignment.hpEffectiveCv - sf::alignment.hpBusVolts
                                       : sf::alignment.lpEffectiveCv - sf::alignment.lpBusVolts)
                + bus[6 + 2 * side] + (gt / gc) * touch + (ge / gc) * ((5.0 / 3.0) * d.filterEnvelopeZ);
            const double qv = 0.5 * (bus[7 + 2 * side] + 10.0 * (1.0 - input.masterResonance));
            out.q[voice][side] = 1.0 / (0.2 + 0.18 * std::clamp(qv, 0.0, 10.0));
        }
        const double touchLevel = bus[23] * d.velocity + bus[25] * d.pressure + input.keyboardLevelVolts[voice];
        out.level[voice] = (bus[21] + (560000.0 / 270000.0) * touchLevel) / 10.0;
    }
}
void stageReadout(const Buses& bus, Observed& out)
{
    const std::array<std::size_t, 6> rows {{12, 13, 14, 17, 18, 20}};
    for (std::size_t i = 0; i < rows.size(); ++i)
        out.seconds[i] = (i == 0 || i == 3 ? 1.0 : 10.0) * std::pow(10.0, -3.0 * std::clamp(bus[rows[i]] / 10.0, 0.0, 1.0));
}
Observed observe(const Buses& bus, const Inputs& input)
{
    Observed out; localReadout(bus, input, out); stageReadout(bus, out); return out;
}
Observed observe(const ex::MemoryReceiverReadout& input)
{
    Observed out; out.bus = input.busVolts; out.cv = input.cutoff.effectiveCv; out.q = input.q; out.level = input.levelGain;
    const auto& f = input.direct.filterEnvelope; const auto& a = input.direct.amplitudeEnvelope;
    out.seconds = {f.attackSeconds, f.decaySeconds, f.releaseSeconds, a.attackSeconds, a.decaySeconds, a.releaseSeconds};
    return out;
}
struct Error
{
    double bus = 0.0, cv = 0.0, q = 0.0, seconds = 0.0, level = 0.0;
    bool within(const Limits& l) const { return bus <= l.bus && cv <= l.cv && q <= l.q && seconds <= l.seconds && level <= l.level; }
    void add(const Error& e) { bus = std::max(bus, e.bus); cv = std::max(cv, e.cv); q = std::max(q, e.q); seconds = std::max(seconds, e.seconds); level = std::max(level, e.level); }
};
Error compare(const Observed& actual, const Observed& truth)
{
    Error e;
    const auto add = [](double& maximum, double error) {
        require(std::isfinite(error), "nonfinite study comparison");
        maximum = std::max(maximum, error);
    };
    for (std::size_t i = 0; i < 26; ++i) add(e.bus, std::abs(actual.bus[i] - truth.bus[i]));
    for (std::size_t v = 0; v < 8; ++v)
    {
        for (std::size_t s = 0; s < 2; ++s)
        {
            add(e.cv, std::abs(actual.cv[v][s] - truth.cv[v][s]));
            require(std::isfinite(actual.q[v][s]) && std::isfinite(truth.q[v][s])
                && actual.q[v][s] > 0.0 && truth.q[v][s] > 0.0, "invalid study Q");
            add(e.q, std::abs(actual.q[v][s] / truth.q[v][s] - 1.0));
        }
        add(e.level, std::abs(actual.level[v] - truth.level[v]));
    }
    for (std::size_t i = 0; i < 6; ++i)
    {
        require(std::isfinite(actual.seconds[i]) && std::isfinite(truth.seconds[i])
            && actual.seconds[i] > 0.0 && truth.seconds[i] > 0.0, "invalid study duration");
        add(e.seconds, std::abs(actual.seconds[i] / truth.seconds[i] - 1.0));
    }
    return e;
}
void comparisonSelfTests()
{
    Observed baseline;
    for (auto& row : baseline.q) row.fill(1.0);
    baseline.seconds.fill(1.0);
    const auto zero = compare(baseline, baseline);
    require(zero.bus == 0.0 && zero.cv == 0.0 && zero.q == 0.0 && zero.seconds == 0.0 && zero.level == 0.0,
            "comparison baseline failed");
    for (int kind = 0; kind < 12; ++kind)
    {
        auto actual = baseline, truth = baseline;
        auto& bad = kind < 5 ? actual : truth;
        const double invalid = kind < 5 ? std::numeric_limits<double>::quiet_NaN() : std::numeric_limits<double>::infinity();
        if (kind % 5 == 0) bad.bus[0] = invalid;
        if (kind % 5 == 1) bad.cv[7][1] = invalid;
        if (kind % 5 == 2) bad.q[6][0] = invalid;
        if (kind % 5 == 3) bad.seconds[5] = invalid;
        if (kind % 5 == 4) bad.level[3] = invalid;
        if (kind == 10) { truth = baseline; truth.q[0][0] = 0.0; }
        if (kind == 11) { truth = baseline; truth.seconds[0] = -1.0; }
        bool rejected = false;
        try { (void) compare(actual, truth); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "invalid readout escaped the comparison guard");
    }
}
void csvError(std::ostream& out, const Error& e) { out << ',' << e.bus << ',' << e.cv << ',' << e.q << ',' << e.seconds << ',' << e.level; }
struct Aggregate
{
    int count = 0, accepted = 0;
    double modelDelta = 0.0, trueDelta = 0.0;
    Error worst;
    void add(const Error& e, const Limits& l, double model, double truth)
    {
        require(std::isfinite(model) && std::isfinite(truth), "nonfinite study current distance");
        ++count; accepted += e.within(l); modelDelta = std::max(modelDelta, model); trueDelta = std::max(trueDelta, truth); worst.add(e);
    }
};
std::ofstream csv(const std::filesystem::path& path)
{
    std::ofstream out(path); require(out.good(), "cannot open study output"); out << std::setprecision(12); return out;
}
template <class T> void materialize(T& value)
{
    // Keep every output field live in the measured GNU/Clang build, not only
    // the few checksum fields. No executable instruction is added by this asm.
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : "+m" (value) : : "memory");
#else
    volatile const void* address = &value; (void) address;
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}
void sweep(const Limits& limits, const std::filesystem::path& directory)
{
    constexpr std::array<double, 9> amounts {{0.0, 0.001, 0.003, 0.01, 0.03, 0.05, 0.1, 0.3, 1.0}};
    constexpr std::array<double, 8> radii {{0.1e-6, 0.3e-6, 1e-6, 3e-6, 5e-6, 10e-6, 30e-6, 100e-6}};
    std::array<std::array<std::array<Aggregate, 9>, 6>, 2> aggregates {};
    std::array<std::array<Aggregate, 8>, 2> radius {};
    auto failures = csv(directory / "nonlinear-counterexamples.csv");
    failures << "rank,selector,pot_ohms,shape,center,profile,amount,model_delta_A,true_delta_A,bus_V,cutoff_CV,q_relative,stage_relative,level_absolute\n";
    std::array<double, 2> smallestFailure {{1e9, 1e9}};
    int cases = 0, accepted = 0;
    for (int rank : {0, 1}) for (int selector : {11, 12, 13}) for (double pot : {10000.0, 50000.0, 100000.0}) for (int shape = 0; shape < 7; ++shape)
    {
        const auto config = source(rank, selector, pot, shape);
        for (std::size_t center = 0; center < 2; ++center)
        {
            const auto initial = center == 0 ? Inputs{} : target(4, 0.5);
            const auto frame = prepare(config, initial);
            for (int profile = 0; profile < (center == 0 ? 5 : 6); ++profile)
                for (std::size_t a = 0; a < amounts.size(); ++a)
            {
                const auto input = between(initial, target(profile, 1.0), amounts[a]);
                const auto truth = ref::solveReceivers(config, physical(input));
                require(truth.converged && truth.network.residualAmps <= 1e-12, "nonlinear sweep truth did not converge");
                ex::MemoryReceiverReadout linear;
                require(ex::evaluateMemoryReceivers(frame, input, linear), "linear receiver sweep failed");
                const auto error = compare(observe(linear), observe(truth.network.busVolts, input));
                double trueDelta = 0.0;
                for (std::size_t row = 0; row < 26; ++row) trueDelta = std::max(trueDelta, std::abs(truth.injectionAmps[row] - frame.ports.referenceCurrents[row]));
                aggregates[center][static_cast<std::size_t>(profile)][a].add(error, limits, linear.maxDeltaAmps, trueDelta);
                if (linear.maxDeltaAmps > 1e-15)
                    for (std::size_t r = 0; r < radii.size(); ++r)
                        if (linear.maxDeltaAmps <= radii[r]) radius[center][r].add(error, limits, linear.maxDeltaAmps, trueDelta);
                if (!error.within(limits) && linear.maxDeltaAmps < smallestFailure[center])
                {
                    smallestFailure[center] = linear.maxDeltaAmps;
                    failures << rank << ',' << selector << ',' << pot << ',' << shape << ',' << center << ',' << profile << ',' << amounts[a]
                             << ',' << linear.maxDeltaAmps << ',' << trueDelta; csvError(failures, error); failures << '\n';
                }
                ++cases; accepted += error.within(limits);
            }
        }
    }
    auto output = csv(directory / "nonlinear-summary.csv");
    output << "center,profile,amount,cases,within_budget,max_model_delta_A,max_true_delta_A,bus_V,cutoff_CV,q_relative,stage_relative,level_absolute\n";
    for (std::size_t c = 0; c < 2; ++c) for (std::size_t p = 0; p < 6; ++p) for (std::size_t a = 0; a < amounts.size(); ++a)
    {
        const auto& v = aggregates[c][p][a]; if (v.count == 0) continue;
        output << c << ',' << p << ',' << amounts[a] << ',' << v.count << ',' << v.accepted << ',' << v.modelDelta << ',' << v.trueDelta;
        csvError(output, v.worst); output << '\n';
    }
    auto ranges = csv(directory / "nonlinear-radius.csv");
    ranges << "center,candidate_model_delta_A,nonzero_cases,within_budget,max_model_delta_A,max_true_delta_A,bus_V,cutoff_CV,q_relative,stage_relative,level_absolute\n";
    for (std::size_t c = 0; c < 2; ++c) for (std::size_t r = 0; r < radii.size(); ++r)
    {
        const auto& v = radius[c][r];
        ranges << c << ',' << radii[r] << ',' << v.count << ',' << v.accepted << ',' << v.modelDelta << ',' << v.trueDelta;
        csvError(ranges, v.worst); ranges << '\n';
    }
    std::cout << "Nonlinear sweep: " << cases << " cases, " << accepted << " within candidate budget; smallest sampled failure delta "
              << smallestFailure[0] << " / " << smallestFailure[1] << " A (zero / nonzero center)\n";
}

struct TimedInput { Inputs input; bool event = false; };
TimedInput trajectory(int profile, int sample, double rate)
{
    TimedInput out; const double t = sample / rate;
    if (profile == 0)
    {
        out.input.masterResonance = 0.5 + 0.3 * std::sin(6.283185307179586 * 2.0 * t);
        for (std::size_t v = 0; v < 8; ++v)
            out.input.voices[v] = {2.0 * std::sin(6.283185307179586 * 20.0 * t + v * 0.1), (v + 1) / 8.0,
                                  0.3 + 0.3 * std::sin(6.283185307179586 * 5.0 * t + v * 0.2)};
    }
    else if (profile == 1)
    {
        // Deliberately offset/coprime event timing: an aligned gate grid can
        // make an otherwise unsafe sample-and-hold schedule appear accurate.
        const int phase = (sample + 11) % 389;
        const double z = phase < 97 ? -3.0 + 6.0 * phase / 97.0 : 3.0 - 6.0 * (phase - 97) / 292.0;
        out.input.masterResonance = 0.5;
        for (auto& d : out.input.voices) d = {z, ((sample + 11) / 389) % 2 ? 0.25 : 1.0, 0.1};
        out.event = phase == 0;
    }
    else
    {
        out.input.masterResonance = ((sample + 7) / 67) % 2 ? 0.2 : 0.8;
        for (auto& d : out.input.voices) d = {0.0, ((sample + 11) / 257) % 2 ? 0.0 : 1.0, ((sample + 5) / 131) % 2 ? 1.0 : 0.0};
        out.event = (sample + 7) % 67 == 0 || (sample + 11) % 257 == 0 || (sample + 5) % 131 == 0;
    }
    return out;
}
void schedules(const Limits& limits, const std::filesystem::path& directory)
{
    constexpr int samples = 2048; constexpr double rate = 96000.0;
    constexpr std::array<int, 8> intervals {{1, 2, 4, 8, 12, 24, 48, 96}};
    auto output = csv(directory / "scheduling.csv");
    output << "pot_ohms,shape,profile,interval_samples,event_updates,samples,electrical_updates,linear_budget_pass,total_budget_pass,schedule_bus_V,schedule_CV,schedule_q_relative,schedule_stage_relative,schedule_level,combined_bus_V,combined_CV,combined_q_relative,combined_stage_relative,combined_level,linearization_bus_V,linearization_CV,linearization_q_relative,linearization_stage_relative,linearization_level\n";
    for (double pot : {10000.0, 50000.0}) for (int shape : {2, 5}) for (int profile = 0; profile < 3; ++profile)
    {
        const auto config = source(0, 11, pot, shape);
        const auto frame = prepare(config, trajectory(profile, 0, rate).input);
        std::vector<TimedInput> inputs; std::vector<Observed> linear, truth;
        inputs.reserve(samples); linear.reserve(samples); truth.reserve(samples);
        Error baseline;
        for (int sample = 0; sample < samples; ++sample)
        {
            inputs.push_back(trajectory(profile, sample, rate));
            const auto nonlinear = ref::solveReceivers(config, physical(inputs.back().input));
            require(nonlinear.converged, "trajectory nonlinear truth failed");
            ex::MemoryReceiverReadout readout;
            require(ex::evaluateMemoryReceivers(frame, inputs.back().input, readout), "trajectory linear readout failed");
            linear.push_back(observe(readout)); truth.push_back(observe(nonlinear.network.busVolts, inputs.back().input));
            baseline.add(compare(linear.back(), truth.back()));
        }
        for (int interval : intervals) for (bool eventAware : {false, true})
        {
            Observed held; Error scheduling, combined; int updates = 0, linearPass = 0, totalPass = 0;
            for (int sample = 0; sample < samples; ++sample)
            {
                const auto i = static_cast<std::size_t>(sample);
                if (sample % interval == 0 || (eventAware && inputs[i].event)) { held = linear[i]; ++updates; }
                Observed current = held; localReadout(held.bus, inputs[i].input, current);
                const auto scheduledError = compare(current, linear[i]), totalError = compare(current, truth[i]);
                scheduling.add(scheduledError); combined.add(totalError);
                linearPass += scheduledError.within(limits); totalPass += totalError.within(limits);
            }
            output << pot << ',' << shape << ',' << profile << ',' << interval << ',' << eventAware << ',' << samples << ',' << updates
                   << ',' << linearPass << ',' << totalPass; csvError(output, scheduling); csvError(output, combined); csvError(output, baseline); output << '\n';
        }
        std::cout << "Scheduling profile complete: " << pot << " ohms, shape " << shape << ", profile " << profile << '\n';
    }
}

void cpu(const std::filesystem::path& directory)
{
    const auto config = source(0, 11, 10000.0, 5);
    const auto center = trajectory(0, 0, 96000.0).input;
    const auto frame = prepare(config, center);
    constexpr int iterations = 500000;
    std::array<TimedInput, 1024> inputs;
    for (int i = 0; i < static_cast<int>(inputs.size()); ++i) inputs[static_cast<std::size_t>(i)] = trajectory(0, i, 96000.0);
    volatile double checksum = 0.0;
    auto output = csv(directory / "cpu.csv");
    output << "kind,interval_samples,round,ns_per_rank_frame,projected_two_ranks_48k_seconds_per_second,projected_two_ranks_96k_seconds_per_second\n";
    for (int round = 0; round < 7; ++round)
        for (int position = 0; position < 6; ++position)
    {
        constexpr std::array<int, 6> periods {{0, 1, 8, 12, 24, 96}};
        const int interval = periods[static_cast<std::size_t>(round % 2 ? 5 - position : position)];
        ex::MemoryReceiverReadout full; Observed held, observed;
        require(ex::evaluateMemoryReceivers(frame, inputs[0].input, full), "benchmark warmup failed"); held = observe(full);
        bool good = true; const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i)
        {
            const auto& input = inputs[static_cast<std::size_t>(i % 1024)].input;
            if (interval == 0)
            {
                good &= ex::evaluateMemoryReceivers(frame, input, full);
                materialize(full);
                checksum = checksum + full.cutoff.effectiveCv[0][0] + full.direct.filterEnvelope.attackSeconds;
            }
            else
            {
                if (i % interval == 0) { good &= ex::evaluateMemoryReceivers(frame, input, full); held = observe(full); }
                observed = held; localReadout(held.bus, input, observed);
                materialize(observed);
                checksum = checksum + observed.cv[0][0] + observed.seconds[0];
            }
        }
        const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / iterations;
        require(good && std::isfinite(checksum), "benchmark evaluation failed");
        output << (interval == 0 ? "full" : "held_bus_fresh_local") << ',' << interval << ',' << round << ',' << ns
               << ',' << ns * 2.0 * 48000.0 / 1e9 << ',' << ns * 2.0 * 96000.0 / 1e9 << '\n';
    }
    auto preparation = csv(directory / "preparation-cpu.csv");
    preparation << "pot_ohms,shape,round,microseconds_full_nonlinear_plus_map\n";
    for (int round = 0; round < 5; ++round) for (double pot : {10000.0, 50000.0}) for (int shape : {0, 2, 4, 5})
    {
        const auto setup = source(0, 11, pot, shape);
        const auto start = std::chrono::steady_clock::now(); const auto prepared = prepare(setup, center);
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
        checksum = checksum + prepared.ports.operatingPoint[0];
        preparation << pot << ',' << shape << ',' << round << ',' << us << '\n';
    }
    std::cout << "Shared-machine CPU probe complete; checksum " << checksum << " (not instrument/WCET qualification)\n";
}
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc != 4) { std::cerr << "usage: " << argv[0] << " limits.json output-directory sweep|schedule|cpu|check\n"; return 2; }
        comparisonSelfTests();
        std::ifstream input(argv[1]); std::stringstream stream; stream << input.rdbuf(); const auto json = stream.str();
        const Limits limits {number(json, "bus_absolute_volts"), number(json, "cutoff_effective_cv_absolute"), number(json, "q_relative"),
                             number(json, "stage_seconds_relative"), number(json, "level_gain_absolute")};
        const std::filesystem::path directory(argv[2]); std::filesystem::create_directories(directory);
        const std::string mode(argv[3]);
        if (mode == "sweep") sweep(limits, directory);
        else if (mode == "schedule") schedules(limits, directory);
        else if (mode == "cpu") cpu(directory);
        else if (mode == "check") std::cout << "Comparison self-tests: finite baseline passed; 12 invalid readouts rejected\n";
        else throw std::runtime_error("unknown probe mode");
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "STUDY FAILED: " << error.what() << '\n'; return 2; }
}
