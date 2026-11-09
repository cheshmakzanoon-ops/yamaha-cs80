#include "experimental/LoadedMemoryReceivers.h"
#include "reference/MemoryControlFrameCompiler.h"
#include "reference/MemoryPanelPorts.h"

#include <atomic>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>

namespace sf = cs80::source_factory;
namespace ref = cs80::reference::memory_panel;
namespace ex = cs80::experimental;
namespace adapter = ex::memory_control;
namespace
{
int checks = 0, failures = 0;
std::atomic<bool> countAllocations { false };
std::atomic<unsigned> allocations { 0 };
void check(bool good, const char* label)
{
    ++checks;
    if (!good) { if (failures < 20) std::printf("FAIL: %s\n", label); ++failures; }
}
double limit(const std::string& json, const char* key)
{
    const auto at = json.find(std::string("\"") + key + "\""), colon = json.find(':', at);
    if (at == std::string::npos || colon == std::string::npos) std::exit(2);
    const double value = std::strtod(json.c_str() + colon + 1, nullptr);
    if (!std::isfinite(value) || value <= 0.0) std::exit(2);
    return value;
}
void* allocate(std::size_t n)
{
    if (countAllocations.load()) ++allocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
adapter::ChannelValues defaults()
{
    adapter::ChannelValues out {};
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = cs80::kChannelDefs[i].def;
    return out;
}
// Deliberately explicit expectations, not production identity/roles lookup.
constexpr std::array<int, 26> expectedParams {{
    cs80::CP_pwmSpeed, cs80::CP_pwmDepth, cs80::CP_pw, cs80::CP_pulse, cs80::CP_saw,
    cs80::CP_noise, cs80::CP_hpfCutoff, cs80::CP_hpfRes, cs80::CP_lpfCutoff, cs80::CP_lpfRes,
    cs80::CP_fIl, cs80::CP_fAl, cs80::CP_fA, cs80::CP_fD, cs80::CP_fR,
    cs80::CP_vcfLevel, cs80::CP_sine, cs80::CP_vA, cs80::CP_vD, cs80::CP_vS,
    cs80::CP_vR, cs80::CP_vLevel, cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel
}};
double forward(int row, double travel)
{
    if (row == 1) return 0.05 * std::pow(400.0, travel);
    if (row == 3) return 0.5 + 0.4 * travel;
    if (row == 7 || row == 9)
        return 62.5 * std::exp2(10.0 * travel + (row == 7 ? sf::alignment.hpEffectiveCv - sf::alignment.hpBusVolts
                                                                           : sf::alignment.lpEffectiveCv - sf::alignment.lpBusVolts));
    if (row == 8 || row == 10) return 1.0 - travel;
    if (row == 13 || row == 18) return std::pow(10.0, -3.0 * travel);
    if (row == 14 || row == 15 || row == 19 || row == 21) return 10.0 * std::pow(10.0, -3.0 * travel);
    return travel;
}
// Independently reduce the documented resistor graph, not compileCutoffSide.
struct Coefficients { double bus, touch, envelope; };
Coefficients coefficients(bool hp)
{
    const double gBus = 1.0 / 39000.0, gTouch = 1.0 / (hp ? 120000.0 : 39000.0), gEnv = 1.0 / (hp ? 100000.0 : 47000.0);
    const double total = 1.0 / (hp ? 2200.0 : 2700.0) + 5.0 * gTouch + 1.0 / (hp ? 680000.0 : 220000.0)
        + gEnv + gBus + 1.0 / 175000.0;
    return {gBus / total, gTouch / total, gEnv / total};
}
ex::MemoryReceiverInputs sanitize(ex::MemoryReceiverInputs input)
{
    input.masterResonance = std::clamp(input.masterResonance, 0.0, 1.0);
    for (auto& d : input.voices)
    {
        d.velocity = std::clamp(d.velocity, 0.0, 1.0); d.pressure = std::clamp(d.pressure, 0.0, 1.0);
        d.filterEnvelopeZ = std::clamp(d.filterEnvelopeZ, -3.0, 3.0);
    }
    return input;
}
// Independent dense solve of V = V0 + G*(c + L*V - Iref), 24 bus unknowns.
bool denseReadout(const ref::Ports& oracle, const ex::MemoryPanelPortMap::Currents& reference,
                  ex::MemoryReceiverInputs input, ex::MemoryPanelPortMap::Voltages& result)
{
    input = sanitize(input);
    std::array<std::array<double, 26>, 26> feedback {};
    std::array<double, 26> constant {};
    double velocity = 0.0, pressure = 0.0, env = 0.0, kc = 0.0, kb = 0.0;
    for (std::size_t v = 0; v < input.voices.size(); ++v)
    {
        velocity += input.voices[v].velocity; pressure += input.voices[v].pressure;
        env += (5.0 / 3.0) * input.voices[v].filterEnvelopeZ;
        kc += input.keyboardCutoffVolts[v]; kb += input.keyboardLevelVolts[v];
    }
    for (int side = 0; side < 2; ++side)
    {
        const auto m = coefficients(side == 0);
        const auto row = static_cast<std::size_t>(6 + 2 * side);
        constant[row] = (m.touch * kc + m.envelope * env) / 39000.0;
        feedback[row][22] = m.touch * velocity / 39000.0;
        feedback[row][24] = m.touch * pressure / 39000.0;
        constant[row + 1] = 8.0 * 10.0 * (1.0 - input.masterResonance) / 440000.0;
    }
    const double diagonal = 1.0 / 180000.0 + 1.0 / 560000.0 + 3.0 / 270000.0;
    const double touch = (1.0 / 270000.0) / diagonal;
    constant[21] = touch * kb / 560000.0;
    feedback[21][23] = touch * velocity / 560000.0; feedback[21][25] = touch * pressure / 560000.0;
    std::array<std::array<double, 25>, 24> a {};
    for (std::size_t i = 0; i < 24; ++i)
    {
        const auto row = static_cast<std::size_t>(oracle.rows[i] - 1);
        a[i][i] = 1.0; a[i][24] = oracle.operatingPoint.busVolts[row];
        for (std::size_t j = 0; j < 24; ++j)
        {
            const auto injection = static_cast<std::size_t>(oracle.rows[j] - 1);
            const double g = oracle.loadedOhms[i * 24 + j];
            a[i][24] += g * (constant[injection] - reference[injection]);
            for (std::size_t k = 0; k < 24; ++k)
                a[i][k] -= g * feedback[injection][static_cast<std::size_t>(oracle.rows[k] - 1)];
        }
    }
    for (std::size_t col = 0; col < 24; ++col)
    {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < 24; ++row)
            if (std::abs(a[row][col]) > std::abs(a[pivot][col])) pivot = row;
        std::swap(a[col], a[pivot]);
        const double d = a[col][col];
        if (!std::isfinite(d) || std::abs(d) < 1e-12) return false;
        for (double& value : a[col]) value /= d;
        for (std::size_t row = 0; row < 24; ++row)
        {
            if (row == col) continue;
            const double factor = a[row][col];
            for (std::size_t k = 0; k < 25; ++k) a[row][k] -= factor * a[col][k];
        }
    }
    result = {};
    for (std::size_t i = 0; i < 24; ++i) result[static_cast<std::size_t>(oracle.rows[i] - 1)] = a[i][24];
    return true;
}
ex::MemoryReceiverInputs inputs(int mode)
{
    ex::MemoryReceiverInputs out;
    if (mode == 0) return out;
    out.masterResonance = mode == 5 ? -1.0 : mode == 3 ? 0.0 : 0.25 * (mode % 4);
    for (int v = 0; v < sf::voiceCount; ++v)
    {
        auto& d = out.voices[static_cast<std::size_t>(v)];
        if (mode == 1) d = {-0.1, 0.25, 0.1};
        if (mode == 2) d = {(v - 3.5) * 0.5, (v + 1) / 8.0, (7 - v) / 7.0};
        if (mode == 3) d = {3.0, 1.0, 1.0};
        if (mode == 4) d = {-3.0, 0.0, 0.0};
        if (mode == 5) d = {v % 2 ? -9.0 : 9.0, v % 2 ? 2.0 : -0.1, 2.0};
        out.keyboardCutoffVolts[static_cast<std::size_t>(v)] = 0.02 * (v - 3);
        out.keyboardLevelVolts[static_cast<std::size_t>(v)] = 0.05 * v;
    }
    return out;
}
} // namespace
void* operator new(std::size_t n) { return allocate(n); }
void* operator new[](std::size_t n) { return allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::ifstream file(argv[1]); std::stringstream stream; stream << file.rdbuf(); const auto json = stream.str();
    const double semanticAbs = limit(json, "semantic_absolute"), semanticRel = limit(json, "semantic_relative");
    const double voltageAbs = limit(json, "voltage_absolute"), voltageRel = limit(json, "voltage_relative");
    check(limit(json, "feedback_current_residual_amperes") == ex::memory_receiver_detail::maximumResidualAmps
        && limit(json, "feedback_matrix_infinity_norm_limit") == ex::memory_receiver_detail::maximumFeedbackNorm,
        "runtime guards match preregistered constants");
    auto values = defaults(); adapter::AdaptedBank base;
    check(adapter::adapt(values, base), "declared defaults adapt");
    int realizable = 0, clipped = 0;
    for (int row = 1; row <= 26; ++row)
    {
        const auto index = static_cast<std::size_t>(row - 1), param = static_cast<std::size_t>(expectedParams[index]);
        const auto& def = cs80::kChannelDefs[param];
        for (int step = 0; step <= 20; ++step)
        {
            auto edited = values;
            edited[param] = static_cast<double>(def.min) + (static_cast<double>(def.max) - def.min) * (step / 20.0);
            adapter::AdaptedBank actual;
            check(adapter::adapt(edited, actual), "all represented in-range values adapt");
            for (std::size_t other = 0; other < 26; ++other)
                if (other != index) check(actual.positions[other] == base.positions[other], "one channel control edits only its physical row");
            if (row == 4 || row == 5)
            {
                check(actual.positions[index] == (edited[param] >= 0.5 ? 1.0 : 0.0)
                    && actual.requestedNominalVolts[index] == 0.0, "waveform gates remain digital");
            }
            else if ((actual.saturatedRows & (1u << index)) == 0)
            {
                const double error = std::abs(forward(row, actual.positions[index]) - edited[param]);
                check(error <= semanticAbs + semanticRel * std::abs(edited[param]), "realizable semantic inverse round trip");
                ++realizable;
            }
            else
            {
                check((actual.positions[index] == 0.0 && actual.requestedNominalVolts[index] < 0.0)
                    || (actual.positions[index] == 1.0 && actual.requestedNominalVolts[index] > 10.0), "unrealizable target is explicitly clipped and retained");
                ++clipped;
            }
        }
    }
    check(realizable > 350 && clipped > 50, "semantic coverage includes realizable and saturated targets");
    for (int param : {cs80::CP_feet, cs80::CP_pan, cs80::CP_hpfOn, cs80::CP_lpfOn})
    {
        auto ignored = values; ignored[static_cast<std::size_t>(param)] = std::numeric_limits<double>::quiet_NaN();
        adapter::AdaptedBank out;
        check(adapter::adapt(ignored, out) && out.positions == base.positions, "excluded controls are not programmer inputs");
    }
    for (int param : expectedParams)
        for (double invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -100.0, 100000.0})
        {
            auto edited = values; edited[static_cast<std::size_t>(param)] = invalid; auto kept = base;
            check(!adapter::adapt(edited, kept) && kept.positions == base.positions && kept.saturatedRows == base.saturatedRows,
                "bad represented input preserves adapted bank");
        }

    double worstVoltage = 0.0, worstResidual = 0.0, largestFeedback = 0.0;
    int points = 0, frames = 0;
    ex::MemoryControlFrame last;
    ex::MemoryReceiverReadout output;
    const auto near = [&](double actual, double expected, const char* label) {
        const double error = std::abs(actual - expected); worstVoltage = std::max(worstVoltage, error);
        check(std::isfinite(error) && error <= voltageAbs + voltageRel * std::abs(expected), label);
    };
    for (int rank : {0, 1}) for (int selector : {11, 12, 13}) for (double pot : {10000.0, 50000.0}) for (int scenario = 0; scenario < 3; ++scenario)
    {
        ref::Setup setup; setup.rank = rank; setup.selector = selector; setup.electrical.potOhms = pot;
        for (std::size_t bank = 0; bank < setup.banks.size(); ++bank)
            for (std::size_t row = 0; row < 26; ++row)
                setup.banks[bank][row] = scenario == 0 ? 0.5 : static_cast<double>((row * 7 + bank * 3) % 9) / 8.0;
        if (scenario == 2) { setup.injectedAmps[6] = 1e-5; setup.injectedAmps[22] = 2e-6; setup.injectedAmps[25] = -1e-6; }
        const auto oracle = ref::linearise(setup);
        ex::MemoryControlFrame frame; frame.electrical = setup.electrical;
        check(oracle.valid && ref::compileLoadedPortMap(setup, frame.ports), "source operating point compiles");
        frame.ready = true; frame.frameVersion = ex::memoryControlFrameVersion;
        frame.adapterVersion = adapter::version; frame.sourceModelVersion = sf::modelVersion; frame.sourceHash = sf::sourceFingerprint;
        frame.selectedSwitches = {true, false}; ++points;
        for (int mode = 0; mode < 6; ++mode)
        {
            const auto input = inputs(mode); const auto clean = sanitize(input);
            ex::MemoryPanelPortMap::Voltages expected;
            check(denseReadout(oracle, setup.injectedAmps, input, expected), "independent 24-bus feedback oracle solves");
            const bool ok = ex::evaluateMemoryReceivers(frame, input, output);
            check(ok, "receiver route accepts complete test frame");
            if (!ok) continue;
            ++frames;
            for (std::size_t row = 0; row < 26; ++row) near(output.busVolts[row], expected[row], "loaded feedback matches dense 24-bus solution");
            for (std::size_t voice = 0; voice < sf::voiceCount; ++voice)
            {
                const auto& d = clean.voices[voice];
                const double touch = expected[22] * d.velocity + expected[24] * d.pressure + input.keyboardCutoffVolts[voice];
                for (std::size_t side = 0; side < 2; ++side)
                {
                    const auto m = coefficients(side == 0);
                    const double pin = m.bus * expected[6 + 2 * side] + m.touch * touch + m.envelope * (5.0 / 3.0) * d.filterEnvelopeZ;
                    const double offset = side == 0 ? sf::alignment.hpEffectiveCv : sf::alignment.lpEffectiveCv;
                    const double referenceBus = side == 0 ? sf::alignment.hpBusVolts : sf::alignment.lpBusVolts;
                    near(output.cutoff.pinVolts[voice][side], pin, "cutoff pin uses loaded bus exactly once");
                    near(output.cutoff.effectiveCv[voice][side], offset + (pin - m.bus * referenceBus) / m.bus, "pin voltage remains distinct from effective cutoff CV");
                    const double qv = 0.5 * (expected[7 + 2 * side] + 10.0 * (1.0 - clean.masterResonance));
                    near(output.qVolts[voice][side], qv, "Q pin sees loaded rank bus and one stiff master");
                    near(output.q[voice][side], 1.0 / (0.2 + 0.18 * std::clamp(qv, 0.0, 10.0)), "Q retains nominal damping law");
                }
                const double diagonal = 1.0 / 180000.0 + 1.0 / 560000.0 + 3.0 / 270000.0;
                const double level = (expected[21] / 560000.0 + (expected[23] * d.velocity + expected[25] * d.pressure + input.keyboardLevelVolts[voice]) / 270000.0) / diagonal;
                near(output.levelPinVolts[voice], level, "level pin uses loaded bus exactly once");
                near(output.levelGain[voice], level / (10.0 / (560000.0 * diagonal)), "level pin has separate nominal gain scale");
            }
            check(output.touchReceiverIds == sf::touchReceiverIds[static_cast<std::size_t>(rank)], "rank-specific touch receiver identity retained");
            check(output.direct.wsc.pulseEnabled && !output.direct.wsc.sawEnabled, "selected waveform switches stay separate from zero digital buses");
            const auto unit = [&](std::size_t row) { return std::clamp(expected[row] / 10.0, 0.0, 1.0); };
            near(output.direct.dutyProxy, 0.5 + 0.4 * unit(2), "direct PW uses loaded bus and named duty proxy");
            near(output.direct.wsc.pulseThresholdVolts, 1.525 * (1.0 - 2.0 * (0.5 + 0.4 * unit(2))), "duty proxy and oscillator threshold remain distinct");
            near(output.direct.pwmRateHz, 0.05 * std::pow(400.0, unit(0)), "loaded PWM rate uses nominal law");
            near(output.direct.pwmDepth, unit(1), "loaded PWM depth uses nominal law");
            near(output.direct.noiseGain, unit(5), "loaded noise gain uses nominal law");
            near(output.direct.filterPathGain, unit(15), "loaded filter gain uses nominal law");
            near(output.direct.sinePathGain, unit(16), "loaded sine gain uses nominal law");
            near(output.direct.filterEnvelope.initialVolts, -5.0 * unit(10), "filter initial target remains in volts");
            near(output.direct.filterEnvelope.attackVolts, 5.0 * unit(11), "filter attack target remains in volts");
            near(output.direct.filterEnvelope.attackSeconds, std::pow(10.0, -3.0 * unit(12)), "loaded filter attack duration");
            near(output.direct.filterEnvelope.decaySeconds, 10.0 * std::pow(10.0, -3.0 * unit(13)), "loaded filter decay duration");
            near(output.direct.filterEnvelope.releaseSeconds, 10.0 * std::pow(10.0, -3.0 * unit(14)), "loaded filter release duration");
            near(output.direct.amplitudeEnvelope.attackSeconds, std::pow(10.0, -3.0 * unit(17)), "loaded amplitude attack duration");
            near(output.direct.amplitudeEnvelope.decaySeconds, 10.0 * std::pow(10.0, -3.0 * unit(18)), "loaded amplitude decay duration");
            near(output.direct.amplitudeEnvelope.releaseSeconds, 10.0 * std::pow(10.0, -3.0 * unit(20)), "loaded amplitude release duration");
            near(output.direct.amplitudeEnvelope.sustainRatio, unit(19), "amplitude sustain remains dimensionless");
            check(output.masterResSaturated == (mode == 5) && output.cutoff.saturatedVoices == (mode == 5 ? 255U : 0U), "input saturation is reported without rewriting stored state");
            worstResidual = std::max(worstResidual, output.residualAmps); largestFeedback = std::max(largestFeedback, output.feedbackNorm);
            // Reordering all per-voice drives cannot change the shared buses.
            auto reversed = input; std::reverse(reversed.voices.begin(), reversed.voices.end());
            std::reverse(reversed.keyboardCutoffVolts.begin(), reversed.keyboardCutoffVolts.end());
            std::reverse(reversed.keyboardLevelVolts.begin(), reversed.keyboardLevelVolts.end());
            ex::MemoryReceiverReadout reordered;
            check(ex::evaluateMemoryReceivers(frame, reversed, reordered), "permuted complete frame evaluates");
            for (std::size_t row = 0; row < 26; ++row) near(reordered.busVolts[row], output.busVolts[row], "shared solution independent of voice ordering");
        }
        last = frame;
    }
    check(points == 36 && frames == 216, "complete receiver grid evaluated");

    std::array<adapter::ChannelValues, 3> banks {{values, values, values}};
    for (std::size_t bank = 0; bank < banks.size(); ++bank)
    {
        banks[bank][cs80::CP_saw] = bank == 1 ? 1.0 : 0.0; banks[bank][cs80::CP_pulse] = bank == 2 ? 1.0 : 0.0;
        banks[bank][cs80::CP_pwmDepth] = 0.2 + 0.2 * bank;
        banks[bank][cs80::CP_hpfCutoff] = 100.0 + 200.0 * bank;
        banks[bank][cs80::CP_afterLevel] = 0.3 * bank;
    }
    cs80::memory_panels::Electrical electrical; electrical.potOhms = 10000.0;
    for (int rank : {0, 1}) for (int selector : {11, 12, 13})
    {
        ex::MemoryControlFrame frame;
        check(ref::compileControlFrame(rank, selector, banks, electrical, frame), "complete raw-bank preparation succeeds offline");
        check(frame.ports.rank == rank && frame.ports.selector == selector && frame.selectedSwitches.saw == (selector == 12)
            && frame.selectedSwitches.pulse == (selector == 13) && frame.electrical.potOhms == electrical.potOhms,
            "frame retains rank, selector, selected bank switches and electrical assumptions");
        check(ex::evaluateMemoryReceivers(frame, inputs(2), output), "prepared semantic frame routes to typed loaded receivers");
        ref::Setup setup; setup.rank = rank; setup.selector = selector; setup.electrical = electrical;
        for (std::size_t bank = 0; bank < banks.size(); ++bank)
        {
            adapter::AdaptedBank expected; adapter::adapt(banks[bank], expected);
            setup.banks[bank] = expected.positions;
            check(frame.saturatedRows[bank] == expected.saturatedRows, "frame retains every bank's conversion limits");
        }
        const auto nonlinear = ref::solve(setup);
        check(nonlinear.converged && nonlinear.busVolts == frame.ports.operatingPoint, "all three unequal banks reach the reference network unchanged");
        ex::MemoryReceiverReadout zero;
        check(ex::evaluateMemoryReceivers(frame, inputs(0), zero), "zero-drive prepared frame evaluates");
        for (std::size_t row = 0; row < 26; ++row)
            check(std::bit_cast<std::uint64_t>(zero.busVolts[row]) == std::bit_cast<std::uint64_t>(frame.ports.operatingPoint[row]),
                "zero-drive routing retains exact prepared operating-point bits");
        std::array<unsigned char, sizeof(frame)> preserved; std::memcpy(preserved.data(), &frame, sizeof(frame));
        auto badBanks = banks; badBanks[2][cs80::CP_pwmDepth] = std::numeric_limits<double>::quiet_NaN();
        check(!ref::compileControlFrame(rank, selector, badBanks, electrical, frame)
            && std::memcmp(preserved.data(), &frame, sizeof(frame)) == 0, "invalid inactive bank preserves the entire prior prepared frame");
    }
    const auto before = output;
    std::array<unsigned char, sizeof(output)> bytes; std::memcpy(bytes.data(), &output, sizeof(output));
    for (int kind = 0; kind < 11; ++kind)
    {
        auto frame = last; auto input = inputs(2);
        const double nan = std::numeric_limits<double>::quiet_NaN();
        if (kind == 0) frame.ready = false;
        if (kind == 1) frame.adapterVersion = 999;
        if (kind == 2) input.voices[0].filterEnvelopeZ = nan;
        if (kind == 3) input.keyboardLevelVolts[7] = nan;
        if (kind == 4) input.masterResonance = nan;
        if (kind == 5) frame.ports.operatingPoint[0] = nan;
        if (kind == 6) for (auto& row : frame.ports.currentToRail) for (double& v : row) v *= 1e9;
        if (kind == 7) input.keyboardCutoffVolts[0] = std::numeric_limits<double>::max();
        if (kind == 8) frame.ports.selector = 0;
        if (kind == 9) frame.sourceHash = 0;
        if (kind == 10) frame.frameVersion = 0;
        check(!ex::evaluateMemoryReceivers(frame, input, output) && std::memcmp(bytes.data(), &output, sizeof(output)) == 0,
            "invalid or unqualified frame preserves complete prior readout");
    }
    output = before;
    auto input = inputs(2); allocations = 0; countAllocations = true;
    const auto start = std::chrono::steady_clock::now(); bool good = true; volatile double checksum = 0.0;
    for (int i = 0; i < 20000; ++i)
    {
        input.voices[0].pressure = (i & 1) ? 0.2 : 0.8;
        good &= ex::evaluateMemoryReceivers(last, input, output); checksum = checksum + output.busVolts[0];
    }
    const auto stop = std::chrono::steady_clock::now(); countAllocations = false;
    check(good && std::isfinite(checksum) && allocations == 0, "twenty thousand typed receiver evaluations allocate no ordinary C++ memory");
    const double ns = std::chrono::duration<double, std::nano>(stop - start).count() / 20000.0;
    std::printf("Memory routing: %d points, %d frames, %d checks, %d failures; semantic %d realizable / %d clipped\n"
        "worst comparison %.9g; residual %.9g A; max feedback norm %.9g; %.3f ns/evaluation (shared machine, not instrument qualification)\n",
        points, frames, checks, failures, realizable, clipped, worstVoltage, worstResidual, largestFeedback, ns);
    return failures != 0;
}
