#include "reference/MemoryReceiverNetwork.h"
#include "reference/MemoryPanelPortCompiler.h"
#include "experimental/LoadedMemoryReceivers.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace ref = cs80::reference::memory_panel;
namespace
{
int checks = 0, failures = 0;
void check(bool good, const char* label)
{
    ++checks; if (!good) { if (failures < 20) std::printf("FAIL: %s\n", label); ++failures; }
}
double limit(const std::string& json, const char* key)
{
    const auto at = json.find(std::string("\"") + key + "\""), colon = json.find(':', at);
    if (at == std::string::npos || colon == std::string::npos) std::exit(2);
    const double v = std::strtod(json.c_str() + colon + 1, nullptr);
    if (!std::isfinite(v) || v <= 0.0) std::exit(2);
    return v;
}
double nominalControl(int row, double travel)
{
    namespace sf = cs80::source_factory;
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
ref::ReceiverDrives physical(const cs80::experimental::MemoryReceiverInputs& input)
{
    ref::ReceiverDrives out; out.masterResVolts = 10.0 * (1.0 - input.masterResonance);
    for (std::size_t v = 0; v < 8; ++v)
        out.voices[v] = {(5.0 / 3.0) * input.voices[v].filterEnvelopeZ, input.voices[v].velocity, input.voices[v].pressure,
                        input.keyboardCutoffVolts[v], input.keyboardLevelVolts[v]};
    return out;
}
}
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::ifstream file(argv[1]); std::stringstream stream; stream << file.rdbuf();
    const double tolerance = limit(stream.str(), "truth_comparison_absolute_volts");
    const double residualLimit = limit(stream.str(), "truth_residual_amperes");
    const double candidateBusLimit = limit(stream.str(), "bus_absolute_volts");
    double worst = 0.0, residual = 0.0; int cases = 0, maxIterations = 0;
    for (int rank : {0, 1}) for (int selector : {11, 12, 13}) for (double pot : {10000.0, 50000.0})
    {
        ref::Setup source; source.rank = rank; source.selector = selector; source.electrical.potOhms = pot;
        for (std::size_t bank = 0; bank < source.banks.size(); ++bank)
            for (std::size_t row = 0; row < 26; ++row) source.banks[bank][row] = ((row * 7 + bank * 3) % 9) / 8.0;
        ref::Jacobian a, b; ref::BusCurrentFeedback zero;
        const auto old = ref::solve(source, &a), explicitZero = ref::solve(source, &b, &zero);
        check(old.converged && explicitZero.converged, "zero-feedback reference converges");
        check(std::memcmp(old.busVolts.data(), explicitZero.busVolts.data(), sizeof(old.busVolts)) == 0
            && std::memcmp(old.railVolts.data(), explicitZero.railVolts.data(), sizeof(old.railVolts)) == 0,
            "explicit zero feedback preserves original result bits");
        check(a.dimension == b.dimension && a.values.size() == b.values.size()
            && std::memcmp(a.values.data(), b.values.data(), a.values.size() * sizeof(double)) == 0,
            "explicit zero feedback preserves original Jacobian bits");
        for (int mode = 0; mode < 4; ++mode)
        {
            ref::ReceiverDrives drives; drives.masterResVolts = mode * (10.0 / 3.0);
            for (int voice = 0; voice < 8; ++voice)
            {
                auto& d = drives.voices[static_cast<std::size_t>(voice)];
                d.envelopeVolts = mode == 0 ? 0.0 : (voice % 2 ? -1.0 : 1.0) * (mode / 3.0) * 5.0;
                d.velocity = mode == 0 ? 0.0 : (voice + 1) / 8.0;
                d.pressure = mode / 3.0;
                d.keyboardCutoffVolts = mode * (voice - 3.5) * 0.1;
                d.keyboardLevelVolts = mode * voice * 0.1;
            }
            const auto dense = ref::solveReceivers(source, drives);
            check(dense.converged && dense.network.residualAmps <= residualLimit, "full nonlinear receiver KCL converges");
            if (!dense.converged) continue;
            residual = std::max(residual, dense.network.residualAmps);
            const auto injection = ref::receiverInjection(drives);
            auto last = old; bool converged = false;
            for (int iteration = 0; iteration < 16; ++iteration)
            {
                auto setup = source;
                for (std::size_t row = 0; row < 26; ++row)
                {
                    setup.injectedAmps[row] += injection.constantAmps[row];
                    for (std::size_t col = 0; col < 26; ++col)
                        setup.injectedAmps[row] += injection.feedback.siemens[row][col] * last.busVolts[col];
                }
                const auto next = ref::solve(setup);
                if (!next.converged) break;
                double difference = 0.0;
                for (std::size_t row = 0; row < 26; ++row) difference = std::max(difference, std::abs(next.busVolts[row] - last.busVolts[row]));
                last = next;
                if (difference < 1e-11) { converged = true; maxIterations = std::max(maxIterations, iteration + 1); break; }
            }
            check(converged, "independent repeated nonlinear source solves converge");
            for (std::size_t row = 0; row < 26; ++row)
            {
                const double error = std::abs(last.busVolts[row] - dense.network.busVolts[row]);
                check(error <= tolerance, "dense feedback Newton matches independently updated nonlinear sources");
                worst = std::max(worst, error);
            }
            ++cases;
        }
    }
    ref::Setup source;
    for (auto& bank : source.banks) bank.fill(0.5);
    for (int kind = 0; kind < 4; ++kind)
    {
        ref::BusCurrentFeedback bad;
        if (kind == 0) bad.siemens[3][0] = 1e-6;
        if (kind == 1) bad.siemens[0][4] = 1e-6;
        if (kind == 2) bad.siemens[0][0] = std::numeric_limits<double>::quiet_NaN();
        if (kind == 3) bad.siemens[7][9] = std::numeric_limits<double>::infinity();
        ref::Jacobian jac;
        check(!ref::solve(source, &jac, &bad).converged && jac.values.empty(), "invalid feedback cannot publish a converged derivative");
    }
    for (int kind = 0; kind < 4; ++kind)
    {
        ref::ReceiverDrives bad;
        if (kind == 0) bad.voices[0].velocity = 1.01;
        if (kind == 1) bad.voices[7].envelopeVolts = -5.01;
        if (kind == 2) bad.masterResVolts = -0.01;
        if (kind == 3) bad.voices[2].keyboardLevelVolts = std::numeric_limits<double>::quiet_NaN();
        check(!ref::solveReceivers(source, bad).converged, "invalid physical receiver boundary is rejected");
    }
    // Concrete counterexample made entirely from valid stored float controls.
    // It proves a tiny affine receiver-closure residual is NOT a nonlinear
    // source-error certificate. This is an expected rejection of the proposed
    // approximation budget, not a claim that inaccurate audio is acceptable.
    namespace ex = cs80::experimental;
    namespace sf = cs80::source_factory;
    ref::Setup reachable; reachable.rank = 0; reachable.selector = 11; reachable.electrical.potOhms = 10000.0;
    for (std::size_t bank = 0; bank < 3; ++bank)
    {
        ex::memory_control::ChannelValues values;
        for (std::size_t p = 0; p < values.size(); ++p) values[p] = cs80::kChannelDefs[p].def;
        const std::size_t pattern = bank == 0 ? 2 : bank == 2 ? 0 : 1;
        for (int row = 1; row <= 26; ++row)
        {
            const auto parameter = static_cast<std::size_t>(cs80::reference::programmer_identity::channelParamForOutput(row));
            const double travel = std::clamp(static_cast<double>(((row - 1) * 7 + pattern * 3) % 9) / 8.0, 0.001, 0.999);
            const auto& def = cs80::kChannelDefs[parameter];
            values[parameter] = static_cast<float>(std::clamp(nominalControl(row, travel), static_cast<double>(def.min), static_cast<double>(def.max)));
        }
        ex::memory_control::AdaptedBank adapted;
        check(ex::memory_control::adapt(values, adapted) && adapted.saturatedRows == 0,
              "counterexample bank is realizable through valid float host controls without clipping");
        reachable.banks[bank] = adapted.positions;
    }
    ex::MemoryReceiverInputs center; center.masterResonance = 0.5;
    for (std::size_t v = 0; v < 8; ++v)
    {
        center.voices[v] = {1.5, 0.5 * (v + 1) / 8.0, 0.5 * (8 - v) / 8.0};
        center.keyboardCutoffVolts[v] = 0.1 * (static_cast<double>(v) - 3.5);
        center.keyboardLevelVolts[v] = 0.05 * v;
    }
    const auto operating = ref::solveReceivers(reachable, physical(center));
    auto biased = reachable; biased.injectedAmps = operating.injectionAmps;
    ex::MemoryControlFrame frame;
    check(operating.converged && ref::compileLoadedPortMap(biased, frame.ports), "counterexample map prepares at its actual receiver operating point");
    frame.ready = true; frame.frameVersion = ex::memoryControlFrameVersion; frame.adapterVersion = ex::memory_control::version;
    frame.sourceModelVersion = sf::modelVersion; frame.sourceHash = sf::sourceFingerprint; frame.electrical = reachable.electrical;
    auto moved = center; moved.masterResonance *= 0.95;
    for (std::size_t v = 0; v < 8; ++v)
    {
        moved.voices[v].filterEnvelopeZ *= 0.95; moved.voices[v].velocity *= 0.95; moved.voices[v].pressure *= 0.95;
        moved.keyboardCutoffVolts[v] *= 0.95; moved.keyboardLevelVolts[v] *= 0.95;
    }
    const auto nonlinear = ref::solveReceivers(reachable, physical(moved));
    ex::MemoryReceiverReadout linear;
    check(nonlinear.converged && ex::evaluateMemoryReceivers(frame, moved, linear), "both counterexample models evaluate successfully");
    const double counterError = std::abs(nonlinear.network.busVolts[7] - linear.busVolts[7]);
    check(linear.residualAmps <= residualLimit && counterError > candidateBusLimit,
          "affine residual success cannot certify the nonlinear 1mV source budget");
    check(linear.maxDeltaAmps > 3e-6 && linear.maxDeltaAmps < 5e-6, "counterexample remains a small but unqualified current excursion");
    check(cases == 48, "complete nonlinear truth comparison grid");
    std::printf("Memory receiver truth: %d cases, %d checks, %d failures; worst %.9g V, KCL %.9g A, fixed-point solves <= %d\n",
                cases, checks, failures, worst, residual, maxIterations);
    std::printf("Reachable memory counterexample: row8 error %.9g V at %.9g A, affine closure %.9g A\n", counterError, linear.maxDeltaAmps, linear.residualAmps);
    return failures != 0;
}
