// Programmer circuit integration against independent multi-source nodal truth.
// Limits registered in programmer-integration.json before evaluation.
#include "core/Engine.h"
#include "reference/MemoryPanelNetwork.h"
#include "Ws1Tolerances.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <vector>
using namespace cs80;
namespace mp = cs80::memory_panels;
namespace sf = cs80::source_factory;
namespace truth = cs80::reference::memory_panel;
namespace {
int checks = 0, failures = 0;
void check(bool pass, const char* label)
{
    ++checks;
    if (!pass) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
std::vector<float> render(Engine& engine, int frames)
{
    std::vector<float> out(static_cast<size_t>(frames) * 2);
    engine.render(out.data(), frames);
    return out;
}
double difference(const std::vector<float>& a, const std::vector<float>& b)
{
    double peak = 0;
    for (size_t i = 0; i < a.size(); ++i) peak = std::max(peak, double(std::abs(a[i] - b[i])));
    return peak;
}
void prepare(Engine& engine, const ControlFrame& frame, double rate = 48000)
{
    engine.prepare(rate);
    check(engine.setControlFrame(frame), "frame accepted");
    engine.reset();
}
void mapping(const ws1::Tolerances& tol)
{
    // An independent row transcription, intentionally not mp::rowChannels.
    constexpr std::array<ChannelParam, 26> rows {{
        CP_pwmSpeed, CP_pwmDepth, CP_pw, CP_pulse, CP_saw, CP_noise,
        CP_hpfCutoff, CP_hpfRes, CP_lpfCutoff, CP_lpfRes, CP_fIl, CP_fAl,
        CP_fA, CP_fD, CP_fR, CP_vcfLevel, CP_sine, CP_vA, CP_vD, CP_vS,
        CP_vR, CP_vLevel, CP_initBrill, CP_initLevel, CP_afterBrill, CP_afterLevel
    }};
    for (int rank : {0, 1}) for (double fraction : {0.25, 0.5, 0.65})
    {
        ParamValues values;
        for (size_t row = 0; row < rows.size(); ++row)
        {
            const auto parameter = rows[row];
            double value = fraction;
            switch (parameter)
            {
                case CP_pwmSpeed: value = 0.05 * std::pow(400.0, fraction); break;
                case CP_pw: value = 0.5 + 0.4 * fraction; break;
                case CP_hpfRes: case CP_lpfRes: value = 1 - fraction; break;
                case CP_fA: case CP_vA: value = std::pow(10.0, -3 * fraction); break;
                case CP_fD: case CP_fR: case CP_vD: case CP_vR:
                    value = 10 * std::pow(10.0, -3 * fraction); break;
                case CP_hpfCutoff: value = 62.5 * std::exp2(4.05640725 + 10 * fraction - 4.0); break;
                case CP_lpfCutoff: value = 62.5 * std::exp2(4.07460544 + 10 * fraction - 3.3); break;
                case CP_saw: case CP_pulse: value = fraction >= 0.5 ? 1 : 0; break;
                default: break;
            }
            values[chParam(rank, parameter)] = static_cast<float>(value);
        }
        const auto actual = mp::positionsFromParameters(values, rank);
        for (size_t row = 0; row < rows.size(); ++row)
        {
            const double expected = row == 3 || row == 4 ? (fraction >= 0.5 ? 1 : 0) : fraction;
            check(std::abs(actual[row] - expected) <= tol["mapping_max_abs"], "all26 named parameters reach the correct electrical row and polarity");
        }
    }
    check(mp::positionFor(CP_fA, 20.0) == 0 && mp::positionFor(CP_fA, .001) == 1,
          "attack ends saturate at the declared hardware voltage range");
    check(mp::positionFor(CP_vR, 20.0) == 0 && mp::positionFor(CP_vR, .001) == 1,
          "release ends saturate at the declared hardware voltage range");
    check(mp::positionFor(CP_hpfRes, 0) == 1 && mp::positionFor(CP_hpfRes, 1) == 0,
          "resonance voltage is inverted");
}
void busesAndImpedance(const ws1::Tolerances& tol)
{
    double worstBus = 0, worstReconstruction = 0, worstImpedance = 0;
    for (int rank : {0, 1}) for (int selector : {11, 12, 13})
    {
        const int bank = selector - 11;
        for (int trial = 0; trial < 4; ++trial)
        {
            truth::Setup reference;
            reference.rank = rank; reference.selector = selector;
            for (int b = 0; b < 3; ++b) for (int row = 0; row < 26; ++row)
                reference.banks[b][row] = double((13 * row + 7 * b + 5 * trial + 3) % 29) / 28.0;
            const auto expected = truth::solve(reference);
            check(expected.converged, "full programmer reference converges");
            const auto model = mp::compileBuses(rank, selector, reference.banks[bank]);
            ControlFrame frame;
            frame.effectiveValues[P_sourcePerformance] = 1.0f;
            frame.hardwareSources[rank] = selector;
            frame.hasProgrammerPositions[rank] = true;
            frame.programmerPositions[rank] = reference.banks[bank];
            Engine engine;
            prepare(engine, frame);
            check(engine.getSourceToneInfo(rank).provenance.programmerMappingVersion == mp::mappingVersion,
                  "engine accepted the electrical programmer source");
            const auto actual = engine.getSourceBusReadout(rank);
            for (int row = 1; row <= 26; ++row)
            {
                if (mp::isSwitchRow(row)) continue;
                const size_t i = static_cast<size_t>(row - 1);
                worstBus = std::max(worstBus, std::abs(actual[i] - expected.busVolts[i]));
                const double load = sf::compiledReceiverSiemens(row, sf::Grade::B, sf::Grade::B);
                const double reconstructed = model.sources[i].volts / (1 + load * model.sources[i].ohms);
                worstReconstruction = std::max(worstReconstruction, std::abs(reconstructed - model.busVolts[i]));
            }
            const auto before = engine.getSourceCompileCount();
            check(engine.setControlFrame(frame), "unchanged programmer frame accepted");
            check(double(engine.getSourceCompileCount() - before) <= tol["unchanged_frame_compile_delta"],
                  "unchanged frame reuses compiled pots, diodes and receiver maps");
        }
        for (double fraction : {0.2, 0.5, 0.8})
        {
            truth::Setup reference;
            reference.rank = rank; reference.selector = selector;
            for (auto& b : reference.banks) b.fill(fraction);
            const auto model = mp::compileBuses(rank, selector, reference.banks[bank]);
            constexpr double current = 1e-8;
            for (int row = 1; row <= 26; ++row)
            {
                if (mp::isSwitchRow(row)) continue;
                const size_t i = static_cast<size_t>(row - 1);
                reference.injectedAmps[i] = current;
                const auto plus = truth::solve(reference);
                reference.injectedAmps[i] = -current;
                const auto minus = truth::solve(reference);
                reference.injectedAmps[i] = 0;
                check(plus.converged && minus.converged, "differential source reference converges");
                const double expected = (plus.busVolts[i] - minus.busVolts[i]) / (2 * current);
                const double load = sf::compiledReceiverSiemens(row, sf::Grade::B, sf::Grade::B);
                const double actual = model.sources[i].ohms / (1 + load * model.sources[i].ohms);
                worstImpedance = std::max(worstImpedance, std::abs(actual / expected - 1));
            }
        }
    }
    std::printf("programmer bus max %.12g V; source reconstruction %.12g V; diagonal impedance relative %.12g\n",
                worstBus, worstReconstruction, worstImpedance);
    check(worstBus <= tol["bus_volts_max_abs"], "engine source buses meet unchanged ws6 truth limit");
    check(worstReconstruction <= tol["source_reconstruction_max_abs"], "receiver conductance removed exactly once");
    check(worstImpedance <= tol["diagonal_impedance_rel"], "pot, diode and common-rail differential source impedance matches full network");
}
void audioAndTransactions(const ws1::Tolerances& tol)
{
    ParamValues values;
    values[P_chorusOn] = values[P_drift] = values[P_voiceDispersion] = 0;
    for (int rank : {0, 1})
    {
        values[chParam(rank, CP_vA)] = 0.01f;
        values[chParam(rank, CP_vS)] = 0.8f;
    }
    for (double rate : {44100.0, 48000.0, 96000.0}) for (bool pure : {false, true})
        for (int selector : {11, 12, 13})
        {
            values[P_sourcePerformance] = 1; values[P_parityMode] = pure ? 1.0f : 0.0f;
            auto on = ControlFrame::programmer(values, {selector, selector}), off = on;
            for (auto& p : off.programmerPositions) p[3] = p[4] = 0;
            Engine a, b;
            prepare(a, on, rate); prepare(b, off, rate);
            a.noteOn(60, .8f); b.noteOn(60, .8f);
            check(difference(render(a, int(.12 * rate)), render(b, int(.12 * rate))) >= tol["audio_effect_min_peak_abs"],
                  "selected programmer waveform gates drive circuit and Pure audio");
            auto low = on;
            low.programmerPositions[0][8] = 0.2;
            low.programmerPositions[1][21] = 0.1;
            prepare(a, on, rate); prepare(b, low, rate);
            a.noteOn(60, .8f); b.noteOn(60, .8f);
            check(difference(render(a, int(.12 * rate)), render(b, int(.12 * rate))) >= tol["audio_effect_min_peak_abs"],
                  "independent programmer cutoff and level changes reach audio");
        }
    values[P_parityMode] = values[P_sourcePerformance] = 0;
    auto frame = ControlFrame::programmer(values, {11, 12}), changed = frame;
    changed.hasProgrammerPositions = {true, true};
    for (auto& p : changed.programmerPositions) p.fill(0.9);
    Engine a, b;
    prepare(a, frame); prepare(b, changed); a.noteOn(60, .8f); b.noteOn(60, .8f);
    check(difference(render(a, 4096), render(b, 4096)) <= tol["legacy_audio_difference_max_abs"],
          "Legacy ignores electrical programmer positions exactly");
    values[P_sourcePerformance] = 1;
    a.prepare(48000); a.setParams(values); a.reset();
    prepare(b, {values, {13, 13}}); a.noteOn(60, .8f); b.noteOn(60, .8f);
    check(render(a, 4096) == render(b, 4096), "setParams and unannotated frames retain the same physical-unit API");
    frame = ControlFrame::programmer(values, {11, 12});
    prepare(a, frame); prepare(b, frame); a.noteOn(60, .8f); b.noteOn(60, .8f);
    check(render(a, 1024) == render(b, 1024), "transaction control is identical before bad frame");
    const auto count = a.getSourceCompileCount();
    const auto originalBuses = a.getSourceBusReadout(0);
    for (double invalid : {-0.01, 1.01, std::numeric_limits<double>::quiet_NaN()})
    {
        changed = frame; changed.programmerPositions[1][12] = invalid;
        changed.effectiveValues[P_volume] = 0.001f;
        check(!a.setControlFrame(changed), "bad programmer fraction rejects whole frame");
        check(a.getSourceCompileCount() == count && a.getSourceBusReadout(0) == originalBuses
                  && a.getParams().v == b.getParams().v, "rejection preserves both compiled ranks and parameter snapshot");
    }
    check(difference(render(a, 1024), render(b, 1024)) <= tol["invalid_transaction_state_delta"],
          "rejected frame leaves subsequent audio exactly unchanged");
    frame = ControlFrame::programmer(values, {3, 3}); changed = frame;
    changed.hasProgrammerPositions = {true, true};
    for (auto& p : changed.programmerPositions) p.fill(0.8);
    prepare(a, frame); prepare(b, changed); a.noteOn(60, .8f); b.noteOn(60, .8f);
    check(render(a, 4096) == render(b, 4096), "unselected programmer cannot alter factory-tab audio");
}
}
int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tol(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/programmer-integration.json", "programmer_integration");
        mapping(tol); busesAndImpedance(tol); audioAndTransactions(tol);
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
    std::printf("Programmer integration: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
